#include "crypto_utils.h"
#include <mbedtls/md.h>

// Payload: base64( iv[16] + ciphertext[n] + hmac[32] )
// AES-256-CTR with counter starting at 1, HMAC-SHA256 over iv+ciphertext for integrity.
String encryptResponse(const String& plaintext) {
    uint8_t key[32];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, (const uint8_t*)apiKey.c_str(), apiKey.length());
    mbedtls_sha256_finish(&sha, key);
    mbedtls_sha256_free(&sha);

    // Random 16-byte IV (CTR uses full 128-bit block as counter seed)
    uint8_t iv[16];
    for (int i = 0; i < 16; i++) iv[i] = (uint8_t)esp_random();

    const uint8_t* input = (const uint8_t*)plaintext.c_str();
    size_t inputLen = plaintext.length();

    uint8_t* ciphertext = (uint8_t*)malloc(inputLen);
    if (!ciphertext) return "";

    // AES-256-CTR encrypt, counter starts at 1 (iv[15] += 1 for first block)
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 256) != 0) {
        mbedtls_aes_free(&aes); free(ciphertext); return "";
    }
    uint8_t ctrBlock[16];
    memcpy(ctrBlock, iv, 16);
    ctrBlock[15] = (ctrBlock[15] & 0xfe) | 0x01; // start counter at 1
    size_t ncOff = 0;
    uint8_t streamBlock[16] = {0};
    mbedtls_aes_crypt_ctr(&aes, inputLen, &ncOff, ctrBlock, streamBlock, input, ciphertext);
    mbedtls_aes_free(&aes);

    // HMAC-SHA256(key, iv || ciphertext) for integrity
    uint8_t hmac[32];
    mbedtls_md_context_t mdCtx;
    mbedtls_md_init(&mdCtx);
    mbedtls_md_setup(&mdCtx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&mdCtx, key, 32);
    mbedtls_md_hmac_update(&mdCtx, iv, 16);
    mbedtls_md_hmac_update(&mdCtx, ciphertext, inputLen);
    mbedtls_md_hmac_finish(&mdCtx, hmac);
    mbedtls_md_free(&mdCtx);

    // Pack: iv(16) + ciphertext(n) + hmac(32)
    size_t blobLen = 16 + inputLen + 32;
    uint8_t* blob  = (uint8_t*)malloc(blobLen);
    if (!blob) { free(ciphertext); return ""; }
    memcpy(blob,            iv,         16);
    memcpy(blob + 16,       ciphertext, inputLen);
    memcpy(blob + 16 + inputLen, hmac,  32);
    free(ciphertext);

    size_t b64Len = 0;
    mbedtls_base64_encode(nullptr, 0, &b64Len, blob, blobLen);
    uint8_t* b64 = (uint8_t*)malloc(b64Len + 1);
    if (!b64) { free(blob); return ""; }
    mbedtls_base64_encode(b64, b64Len + 1, &b64Len, blob, blobLen);
    free(blob);
    b64[b64Len] = '\0';
    String result = String((char*)b64);
    free(b64);
    return result;
}

String generateNonce() {
    uint8_t bytes[16];
    for (int i = 0; i < 16; i++) bytes[i] = (uint8_t)esp_random();
    char buf[33];
    for (int i = 0; i < 16; i++) snprintf(buf + i * 2, 3, "%02x", bytes[i]);
    return String(buf);
}

bool verifyHMAC(const String& nonce, const String& hmacHex) {
    if (hmacHex.length() != 64) return false;

    // Compute HMAC-SHA256(apiKey, nonce)
    uint8_t expected[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(&ctx, info, 1) != 0) { mbedtls_md_free(&ctx); return false; }
    mbedtls_md_hmac_starts(&ctx,
        (const uint8_t*)apiKey.c_str(), apiKey.length());
    mbedtls_md_hmac_update(&ctx,
        (const uint8_t*)nonce.c_str(), nonce.length());
    mbedtls_md_hmac_finish(&ctx, expected);
    mbedtls_md_free(&ctx);

    // Decode submitted hex
    if (hmacHex.length() != 64) return false;
    uint8_t submitted[32];
    for (int i = 0; i < 32; i++) {
        submitted[i] = (uint8_t)strtol(hmacHex.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
    }

    // Constant-time compare
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= expected[i] ^ submitted[i];
    return diff == 0;
}

// Decrypts an encrypted request body (same format as encryptResponse).
// Returns decrypted plaintext or "" on failure.
String decryptRequest(const String& b64Body) {
    // base64 decode
    size_t b64Len = b64Body.length();
    size_t outLen = 0;
    mbedtls_base64_decode(nullptr, 0, &outLen,
        (const uint8_t*)b64Body.c_str(), b64Len);
    if (outLen < 48) return "";

    uint8_t* raw = (uint8_t*)malloc(outLen);
    if (!raw) return "";
    mbedtls_base64_decode(raw, outLen, &outLen,
        (const uint8_t*)b64Body.c_str(), b64Len);

    uint8_t* iv         = raw;
    uint8_t* ciphertext = raw + 16;
    size_t   cipherLen  = outLen - 48;
    uint8_t* tag        = raw + 16 + cipherLen;

    // Derive AES key
    uint8_t key[32];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, (const uint8_t*)apiKey.c_str(), apiKey.length());
    mbedtls_sha256_finish(&sha, key);
    mbedtls_sha256_free(&sha);

    // Verify HMAC(key, iv || ciphertext)
    uint8_t expected[32];
    mbedtls_md_context_t mdCtx;
    mbedtls_md_init(&mdCtx);
    mbedtls_md_setup(&mdCtx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&mdCtx, key, 32);
    mbedtls_md_hmac_update(&mdCtx, iv, 16);
    mbedtls_md_hmac_update(&mdCtx, ciphertext, cipherLen);
    mbedtls_md_hmac_finish(&mdCtx, expected);
    mbedtls_md_free(&mdCtx);

    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= expected[i] ^ tag[i];
    if (diff != 0) { free(raw); return ""; }

    // Decrypt AES-256-CTR
    uint8_t ctrBlock[16];
    memcpy(ctrBlock, iv, 16);
    ctrBlock[15] = (ctrBlock[15] & 0xfe) | 0x01;

    uint8_t* plain = (uint8_t*)malloc(cipherLen + 1);
    if (!plain) { free(raw); return ""; }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 256);
    size_t ncOff = 0;
    uint8_t streamBlock[16] = {0};
    mbedtls_aes_crypt_ctr(&aes, cipherLen, &ncOff, ctrBlock, streamBlock, ciphertext, plain);
    mbedtls_aes_free(&aes);
    free(raw);

    plain[cipherLen] = '\0';
    String result = String((char*)plain);
    free(plain);
    return result;
}
