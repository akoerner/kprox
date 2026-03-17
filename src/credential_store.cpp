#include "credential_store.h"
#include "totp.h"
#include <ArduinoJson.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include <algorithm>

// ============================================================
// KeePass KDBX 3.1 credential store
// ============================================================
// Storage: NVS namespace "kprox_db" as chunked blobs — survives firmware flash.
// Key layout: cs_db_n (total byte count) + cs_db_0, cs_db_1, ... (3900-byte chunks)
// The KDBX file is a standards-compliant KDBX 3.1 file openable by KeePass / KeePassXC.
//
// All credential metadata (labels, count, values) is inside the AES-256-CBC payload.
// ============================================================

// ---- KDBX magic numbers and constants ----
static const uint32_t KDBX_SIG1     = 0x9AA2D903UL;
static const uint32_t KDBX_SIG2     = 0xB54BFB67UL;
static const uint16_t KDBX_VER_MIN  = 0x0001;
static const uint16_t KDBX_VER_MAJ  = 0x0003;

// AES256-CBC cipher UUID as stored in KDBX (verified against KeePass2 source)
static const uint8_t CIPHER_AES256_UUID[16] = {
    0x31,0xC1,0xF2,0xE6, 0xBF,0x71,0x43,0x50,
    0xBE,0x58,0x05,0x21, 0x6A,0xFC,0x5A,0xFF
};

// Salsa20 IV (fixed per KeePass spec: first 8 bytes of SHA256("KeePassSalsa20IV"))
static const uint8_t SALSA20_IV[8] = {
    0xE8,0x30,0x09,0x4B, 0x97,0x20,0x5D,0x2A
};

// ---- Globals ----
String credStoreRuntimeKey = "";
bool   credStoreLocked     = true;

static std::vector<Credential> _creds;
static bool                    _loaded = false;

// ---- AES-256-CTR+HMAC for TOTP secret encryption ----
// (kept for TOTP module which calls credEncrypt/credDecrypt directly)

static void deriveKey32(const String& keyStr, uint8_t out[32]) {
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, (const uint8_t*)keyStr.c_str(), keyStr.length());
    mbedtls_sha256_finish(&sha, out);
    mbedtls_sha256_free(&sha);
}

String credEncrypt(const String& plaintext, const String& key) {
    uint8_t aesKey[32];
    deriveKey32(key, aesKey);

    uint8_t iv[16];
    for (int i = 0; i < 16; i++) iv[i] = (uint8_t)esp_random();

    size_t len = plaintext.length();
    uint8_t* ct = (uint8_t*)malloc(len);
    if (!ct) return "";

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, aesKey, 256) != 0) {
        mbedtls_aes_free(&aes); free(ct); return "";
    }
    uint8_t ctr[16]; memcpy(ctr, iv, 16); ctr[15] = (ctr[15] & 0xfe) | 0x01;
    size_t ncOff = 0; uint8_t stream[16] = {0};
    mbedtls_aes_crypt_ctr(&aes, len, &ncOff, ctr, stream, (const uint8_t*)plaintext.c_str(), ct);
    mbedtls_aes_free(&aes);

    uint8_t hmac[32];
    mbedtls_md_context_t md;
    mbedtls_md_init(&md);
    mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&md, aesKey, 32);
    mbedtls_md_hmac_update(&md, iv, 16);
    mbedtls_md_hmac_update(&md, ct, len);
    mbedtls_md_hmac_finish(&md, hmac);
    mbedtls_md_free(&md);

    size_t blobLen = 16 + len + 32;
    uint8_t* blob = (uint8_t*)malloc(blobLen);
    if (!blob) { free(ct); return ""; }
    memcpy(blob, iv, 16); memcpy(blob+16, ct, len); memcpy(blob+16+len, hmac, 32);
    free(ct);

    size_t b64Len = 0;
    mbedtls_base64_encode(nullptr, 0, &b64Len, blob, blobLen);
    uint8_t* b64 = (uint8_t*)malloc(b64Len + 1);
    if (!b64) { free(blob); return ""; }
    mbedtls_base64_encode(b64, b64Len+1, &b64Len, blob, blobLen);
    free(blob); b64[b64Len] = '\0';
    String r((char*)b64); free(b64); return r;
}

String credDecrypt(const String& b64, const String& key) {
    size_t outLen = 0;
    mbedtls_base64_decode(nullptr, 0, &outLen, (const uint8_t*)b64.c_str(), b64.length());
    if (outLen < 48) return "";

    uint8_t* raw = (uint8_t*)malloc(outLen);
    if (!raw) return "";
    mbedtls_base64_decode(raw, outLen, &outLen, (const uint8_t*)b64.c_str(), b64.length());

    uint8_t* iv_ = raw; uint8_t* ct_ = raw+16;
    size_t ctLen = outLen - 48; uint8_t* tag = raw+16+ctLen;

    uint8_t aesKey[32]; deriveKey32(key, aesKey);

    uint8_t expected[32];
    mbedtls_md_context_t md; mbedtls_md_init(&md);
    mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&md, aesKey, 32);
    mbedtls_md_hmac_update(&md, iv_, 16);
    mbedtls_md_hmac_update(&md, ct_, ctLen);
    mbedtls_md_hmac_finish(&md, expected);
    mbedtls_md_free(&md);

    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= expected[i] ^ tag[i];
    if (diff) { free(raw); return ""; }

    uint8_t ctr[16]; memcpy(ctr, iv_, 16); ctr[15] = (ctr[15] & 0xfe) | 0x01;
    uint8_t* plain = (uint8_t*)malloc(ctLen+1);
    if (!plain) { free(raw); return ""; }

    mbedtls_aes_context aes; mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aesKey, 256);
    size_t ncOff = 0; uint8_t stream[16] = {0};
    mbedtls_aes_crypt_ctr(&aes, ctLen, &ncOff, ctr, stream, ct_, plain);
    mbedtls_aes_free(&aes); free(raw);
    plain[ctLen] = '\0'; String r((char*)plain); free(plain); return r;
}

// ============================================================
// Salsa20 stream cipher (inner stream for KDBX protected values)
// ============================================================

struct Salsa20State {
    uint32_t x[16];
    uint8_t  buf[64];
    int      bufPos;
};

static inline uint32_t rotl32(uint32_t v, int n) { return (v << n) | (v >> (32-n)); }

static void salsa20Block(uint32_t out[16], const uint32_t in[16]) {
    uint32_t x[16];
    memcpy(x, in, 64);
    for (int i = 0; i < 10; i++) {
        x[ 4] ^= rotl32(x[ 0]+x[12], 7);  x[ 8] ^= rotl32(x[ 4]+x[ 0], 9);
        x[12] ^= rotl32(x[ 8]+x[ 4],13);  x[ 0] ^= rotl32(x[12]+x[ 8],18);
        x[ 9] ^= rotl32(x[ 5]+x[ 1], 7);  x[13] ^= rotl32(x[ 9]+x[ 5], 9);
        x[ 1] ^= rotl32(x[13]+x[ 9],13);  x[ 5] ^= rotl32(x[ 1]+x[13],18);
        x[14] ^= rotl32(x[10]+x[ 6], 7);  x[ 2] ^= rotl32(x[14]+x[10], 9);
        x[ 6] ^= rotl32(x[ 2]+x[14],13);  x[10] ^= rotl32(x[ 6]+x[ 2],18);
        x[ 3] ^= rotl32(x[15]+x[11], 7);  x[ 7] ^= rotl32(x[ 3]+x[15], 9);
        x[11] ^= rotl32(x[ 7]+x[ 3],13);  x[15] ^= rotl32(x[11]+x[ 7],18);
        x[ 1] ^= rotl32(x[ 0]+x[ 3], 7);  x[ 2] ^= rotl32(x[ 1]+x[ 0], 9);
        x[ 3] ^= rotl32(x[ 2]+x[ 1],13);  x[ 0] ^= rotl32(x[ 3]+x[ 2],18);
        x[ 6] ^= rotl32(x[ 5]+x[ 4], 7);  x[ 7] ^= rotl32(x[ 6]+x[ 5], 9);
        x[ 4] ^= rotl32(x[ 7]+x[ 6],13);  x[ 5] ^= rotl32(x[ 4]+x[ 7],18);
        x[11] ^= rotl32(x[10]+x[ 9], 7);  x[ 8] ^= rotl32(x[11]+x[10], 9);
        x[ 9] ^= rotl32(x[ 8]+x[11],13);  x[10] ^= rotl32(x[ 9]+x[ 8],18);
        x[12] ^= rotl32(x[15]+x[14], 7);  x[13] ^= rotl32(x[12]+x[15], 9);
        x[14] ^= rotl32(x[13]+x[12],13);  x[15] ^= rotl32(x[14]+x[13],18);
    }
    for (int i = 0; i < 16; i++) out[i] = x[i] + in[i];
}

static void salsa20Init(Salsa20State& s, const uint8_t key[32], const uint8_t iv[8]) {
    static const uint8_t SIGMA[16] = {
        'e','x','p','a','n','d',' ','3','2','-','b','y','t','e',' ','k'
    };
    auto u32le = [](const uint8_t* p) -> uint32_t {
        return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
    };
    s.x[ 0] = u32le(SIGMA);    s.x[ 1] = u32le(key);
    s.x[ 2] = u32le(key+4);    s.x[ 3] = u32le(key+8);
    s.x[ 4] = u32le(key+12);   s.x[ 5] = u32le(SIGMA+4);
    s.x[ 6] = u32le(iv);       s.x[ 7] = u32le(iv+4);
    s.x[ 8] = 0;               s.x[ 9] = 0;
    s.x[10] = u32le(SIGMA+8);  s.x[11] = u32le(key+16);
    s.x[12] = u32le(key+20);   s.x[13] = u32le(key+24);
    s.x[14] = u32le(key+28);   s.x[15] = u32le(SIGMA+12);
    s.bufPos = 64; // force block generation on first use
}

static void salsa20Xor(Salsa20State& s, uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (s.bufPos == 64) {
            uint32_t block[16];
            salsa20Block(block, s.x);
            for (int j = 0; j < 16; j++) {
                s.buf[j*4]   = (uint8_t)(block[j]);
                s.buf[j*4+1] = (uint8_t)(block[j]>>8);
                s.buf[j*4+2] = (uint8_t)(block[j]>>16);
                s.buf[j*4+3] = (uint8_t)(block[j]>>24);
            }
            // increment counter (words 8-9)
            if (++s.x[8] == 0) ++s.x[9];
            s.bufPos = 0;
        }
        buf[i] ^= s.buf[s.bufPos++];
    }
}

// ============================================================
// KDBX 3.1 writer
// ============================================================

static void writeU8(std::vector<uint8_t>& v, uint8_t b)  { v.push_back(b); }
static void writeU16LE(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(x & 0xFF); v.push_back(x >> 8);
}
static void writeU32LE(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; i++) { v.push_back(x & 0xFF); x >>= 8; }
}
static void writeU64LE(std::vector<uint8_t>& v, uint64_t x) {
    for (int i = 0; i < 8; i++) { v.push_back(x & 0xFF); x >>= 8; }
}
static void writeBytes(std::vector<uint8_t>& v, const uint8_t* d, size_t n) {
    v.insert(v.end(), d, d+n);
}
static void writeField(std::vector<uint8_t>& v, uint8_t type,
                       const uint8_t* data, uint16_t len) {
    writeU8(v, type);
    writeU16LE(v, len);
    writeBytes(v, data, len);
}

// AES-KDF: transform composite_key for TransformRounds rounds
static void aesKdfTransform(const uint8_t transformSeed[32],
                             uint64_t rounds, uint8_t transformedKey[32]) {
    // Operate on first 16 and second 16 bytes separately (ECB mode)
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, transformSeed, 256);

    uint8_t a[16], b[16];
    memcpy(a, transformedKey,    16);
    memcpy(b, transformedKey+16, 16);

    for (uint64_t i = 0; i < rounds; i++) {
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, a, a);
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, b, b);
        feedWatchdog();
        if ((i & 0x1FFFF) == 0) feedWatchdog(); // feed WDT frequently
    }
    mbedtls_aes_free(&aes);

    memcpy(transformedKey,    a, 16);
    memcpy(transformedKey+16, b, 16);
}

static void deriveMasterKey(const String& password,
                             const uint8_t masterSeed[32],
                             const uint8_t transformSeed[32],
                             uint64_t      transformRounds,
                             uint8_t       masterKey[32]) {
    // composite_key = SHA256(SHA256(password))
    uint8_t ck[32];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, (const uint8_t*)password.c_str(), password.length());
    mbedtls_sha256_finish(&sha, ck);
    // second SHA256
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, ck, 32);
    mbedtls_sha256_finish(&sha, ck);
    mbedtls_sha256_free(&sha);

    // AES-KDF
    aesKdfTransform(transformSeed, transformRounds, ck);

    // master_key = SHA256(master_seed || SHA256(transformed_key))
    uint8_t tkHash[32];
    mbedtls_sha256_context sha2;
    mbedtls_sha256_init(&sha2);
    mbedtls_sha256_starts(&sha2, 0);
    mbedtls_sha256_update(&sha2, ck, 32);
    mbedtls_sha256_finish(&sha2, tkHash);

    mbedtls_sha256_starts(&sha2, 0);
    mbedtls_sha256_update(&sha2, masterSeed, 32);
    mbedtls_sha256_update(&sha2, tkHash, 32);
    mbedtls_sha256_finish(&sha2, masterKey);
    mbedtls_sha256_free(&sha2);
}

// Base64-encode a block of data, return String
static String b64Encode(const uint8_t* data, size_t len) {
    size_t out = 0;
    mbedtls_base64_encode(nullptr, 0, &out, data, len);
    uint8_t* buf = (uint8_t*)malloc(out + 1);
    if (!buf) return "";
    mbedtls_base64_encode(buf, out+1, &out, data, len);
    buf[out] = '\0';
    String s((char*)buf); free(buf); return s;
}

static String b64Decode(const String& s, std::vector<uint8_t>& out) {
    size_t outLen = 0;
    mbedtls_base64_decode(nullptr, 0, &outLen, (const uint8_t*)s.c_str(), s.length());
    out.resize(outLen);
    mbedtls_base64_decode(out.data(), outLen, &outLen, (const uint8_t*)s.c_str(), s.length());
    out.resize(outLen);
    return "";
}

// XML-escape a string
static String xmlEsc(const String& s) {
    String r;
    r.reserve(s.length() + 16);
    for (char c : s) {
        if      (c == '&')  r += "&amp;";
        else if (c == '<')  r += "&lt;";
        else if (c == '>')  r += "&gt;";
        else if (c == '"')  r += "&quot;";
        else                r += c;
    }
    return r;
}

static String xmlUnescape(const String& s) {
    String r = s;
    r.replace("&amp;",  "&");
    r.replace("&lt;",   "<");
    r.replace("&gt;",   ">");
    r.replace("&quot;", "\"");
    r.replace("&apos;", "'");
    return r;
}

// Build KeePass XML payload
static String buildXML(const std::vector<Credential>& creds,
                        Salsa20State& innerStream) {
    auto makeUUID = []() -> String {
        uint8_t uuid[16];
        for (int i = 0; i < 16; i++) uuid[i] = (uint8_t)esp_random();
        return b64Encode(uuid, 16);
    };

    String xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    xml += "<KeePassFile>\n";
    xml += "<Meta>\n";
    xml += "<Generator>KProx</Generator>\n";
    xml += "<DatabaseName>KProx Credentials</DatabaseName>\n";
    xml += "<DatabaseDescription></DatabaseDescription>\n";
    xml += "<MemoryProtection><ProtectPassword>True</ProtectPassword></MemoryProtection>\n";
    xml += "</Meta>\n";
    xml += "<Root>\n";
    xml += "<Group>\n";
    xml += "<UUID>" + makeUUID() + "</UUID>\n";
    xml += "<Name>KProx</Name>\n";
    xml += "<IsExpanded>True</IsExpanded>\n";

    for (auto& c : creds) {
        xml += "<Entry>\n";
        xml += "<UUID>" + makeUUID() + "</UUID>\n";
        xml += "<String><Key>Title</Key><Value>";
        xml += xmlEsc(c.label);
        xml += "</Value></String>\n";
        xml += "<String><Key>UserName</Key><Value></Value></String>\n";
        xml += "<String><Key>Password</Key><Value Protected=\"True\">";
        String plain = credDecrypt(c.encValue, credStoreRuntimeKey);
        std::vector<uint8_t> plainBytes(plain.length());
        memcpy(plainBytes.data(), plain.c_str(), plain.length());
        salsa20Xor(innerStream, plainBytes.data(), plainBytes.size());
        xml += b64Encode(plainBytes.data(), plainBytes.size());
        xml += "</Value></String>\n";
        xml += "<String><Key>URL</Key><Value></Value></String>\n";
        xml += "<String><Key>Notes</Key><Value></Value></String>\n";
        xml += "</Entry>\n";
    }

    xml += "</Group>\n";
    xml += "</Root>\n";
    xml += "</KeePassFile>\n";
    return xml;
}

// ---- KDBX 3.1 hash-block stream ----
// After AES-CBC decryption, KDBX3.1 wraps the XML in a series of blocks:
//   [4] block index (LE)  [32] SHA-256(block)  [4] block size  [N] data
// Terminated by a block with size == 0 and hash == all zeros.

static std::vector<uint8_t> makeHashBlocks(const uint8_t* data, size_t len) {
    std::vector<uint8_t> out;
    static const size_t BLOCK_SIZE = 1024 * 1024; // 1 MB
    uint32_t idx = 0;

    auto appendU32 = [&](uint32_t v) {
        for (int i = 0; i < 4; i++) { out.push_back(v & 0xFF); v >>= 8; }
    };

    size_t off = 0;
    while (off < len) {
        size_t chunkLen = std::min(BLOCK_SIZE, len - off);
        const uint8_t* chunk = data + off; off += chunkLen;

        uint8_t hash[32];
        mbedtls_sha256_context sha;
        mbedtls_sha256_init(&sha);
        mbedtls_sha256_starts(&sha, 0);
        mbedtls_sha256_update(&sha, chunk, chunkLen);
        mbedtls_sha256_finish(&sha, hash);
        mbedtls_sha256_free(&sha);

        appendU32(idx++);
        out.insert(out.end(), hash, hash + 32);
        appendU32((uint32_t)chunkLen);
        out.insert(out.end(), chunk, chunk + chunkLen);
    }
    // Terminator block
    appendU32(idx);
    for (int i = 0; i < 32; i++) out.push_back(0);
    appendU32(0);

    return out;
}

static std::vector<uint8_t> readHashBlocks(const uint8_t* data, size_t len) {
    std::vector<uint8_t> out;
    size_t pos = 0;

    while (pos + 40 <= len) {
        // uint32 index (skip), 32 bytes hash, uint32 size
        pos += 4;  // skip block index
        const uint8_t* hashExpected = data + pos; pos += 32;
        uint32_t size = 0;
        for (int i = 0; i < 4; i++) size |= ((uint32_t)data[pos++]) << (8*i);

        if (size == 0) break; // terminator

        if (pos + size > len) break; // truncated

        // Verify hash
        uint8_t hashActual[32];
        mbedtls_sha256_context sha;
        mbedtls_sha256_init(&sha);
        mbedtls_sha256_starts(&sha, 0);
        mbedtls_sha256_update(&sha, data + pos, size);
        mbedtls_sha256_finish(&sha, hashActual);
        mbedtls_sha256_free(&sha);

        uint8_t diff = 0;
        for (int i = 0; i < 32; i++) diff |= hashActual[i] ^ hashExpected[i];
        if (diff != 0) { out.clear(); return out; } // block corrupted

        out.insert(out.end(), data + pos, data + pos + size);
        pos += size;
    }
    return out;
}

// Write KDBX to NVS as chunked blobs
static bool writeKDBX(const String& password) {
    // Random material
    uint8_t masterSeed[32], transformSeed[32], encIV[16],
            protStreamKey[32], streamStartBytes[32];
    for (int i = 0; i < 32; i++) {
        masterSeed[i]    = (uint8_t)esp_random();
        transformSeed[i] = (uint8_t)esp_random();
        protStreamKey[i] = (uint8_t)esp_random();
        streamStartBytes[i] = (uint8_t)esp_random();
    }
    for (int i = 0; i < 16; i++) encIV[i] = (uint8_t)esp_random();

    static const uint64_t TRANSFORM_ROUNDS = 6000; // balance security vs ESP32 speed

    // ---- Build outer header ----
    std::vector<uint8_t> header;
    writeU32LE(header, KDBX_SIG1);
    writeU32LE(header, KDBX_SIG2);
    writeU16LE(header, KDBX_VER_MIN);
    writeU16LE(header, KDBX_VER_MAJ);

    // CipherID
    writeField(header, 0x02, CIPHER_AES256_UUID, 16);
    // CompressionFlags = 0 (none)
    uint8_t comp[4] = {0,0,0,0};
    writeField(header, 0x03, comp, 4);
    // MasterSeed
    writeField(header, 0x04, masterSeed, 32);
    // TransformSeed
    writeField(header, 0x05, transformSeed, 32);
    // TransformRounds
    uint8_t trBytes[8];
    for (int i = 0; i < 8; i++) trBytes[i] = (TRANSFORM_ROUNDS >> (8*i)) & 0xFF;
    writeField(header, 0x06, trBytes, 8);
    // EncryptionIV
    writeField(header, 0x07, encIV, 16);
    // ProtectedStreamKey
    writeField(header, 0x08, protStreamKey, 32);
    // StreamStartBytes
    writeField(header, 0x09, streamStartBytes, 32);
    // InnerRandomStreamID = 2 (Salsa20)
    uint8_t rsid[4] = {2,0,0,0};
    writeField(header, 0x0A, rsid, 4);
    // EndOfHeader
    uint8_t eoh[4] = {0x0D,0x0A,0x0D,0x0A};
    writeField(header, 0x00, eoh, 4);

    // ---- Derive master key ----
    uint8_t masterKey[32];
    deriveMasterKey(password, masterSeed, transformSeed, TRANSFORM_ROUNDS, masterKey);
    feedWatchdog();

    // ---- Build inner (Salsa20) stream key ----
    // inner stream key = SHA256(ProtectedStreamKey)
    uint8_t innerKey[32];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, protStreamKey, 32);
    mbedtls_sha256_finish(&sha, innerKey);
    mbedtls_sha256_free(&sha);

    Salsa20State innerStream;
    salsa20Init(innerStream, innerKey, SALSA20_IV);

    // ---- Build plaintext payload = StreamStartBytes || HashBlocks(XML) ----
    String xmlPayload = buildXML(_creds, innerStream);
    std::vector<uint8_t> blocks = makeHashBlocks(
        (const uint8_t*)xmlPayload.c_str(), xmlPayload.length());

    size_t payloadLen = 32 + blocks.size();
    uint8_t* payload = (uint8_t*)malloc(payloadLen);
    if (!payload) return false;
    memcpy(payload, streamStartBytes, 32);
    memcpy(payload + 32, blocks.data(), blocks.size());

    // ---- AES-256-CBC encrypt payload (PKCS7 padded) ----
    size_t paddedLen = ((payloadLen + 15) / 16) * 16;
    uint8_t padByte  = (uint8_t)(paddedLen - payloadLen);
    uint8_t* padded = (uint8_t*)malloc(paddedLen);
    if (!padded) { free(payload); return false; }
    memcpy(padded, payload, payloadLen);
    memset(padded + payloadLen, padByte, paddedLen - payloadLen);
    free(payload);

    uint8_t* encrypted = (uint8_t*)malloc(paddedLen);
    if (!encrypted) { free(padded); return false; }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, masterKey, 256) != 0) {
        mbedtls_aes_free(&aes); free(padded); free(encrypted); return false;
    }
    uint8_t iv_copy[16]; memcpy(iv_copy, encIV, 16);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, iv_copy, padded, encrypted);
    mbedtls_aes_free(&aes);
    free(padded);
    feedWatchdog();

    // ---- Write to NVS as chunked blobs (survives firmware flash) ----
    // Key layout: "cs_db_n" = total byte count, "cs_db_0".."cs_db_N" = 3900-byte chunks
    static const size_t CHUNK = 3900;
    size_t total = header.size() + paddedLen;
    uint8_t* fullBuf = (uint8_t*)malloc(total);
    if (!fullBuf) { free(encrypted); return false; }
    memcpy(fullBuf,               header.data(), header.size());
    memcpy(fullBuf + header.size(), encrypted,   paddedLen);
    free(encrypted);

    preferences.begin("kprox_db", false);
    preferences.putInt("cs_db_n", (int)total);
    int chunkIdx = 0;
    for (size_t off = 0; off < total; off += CHUNK, chunkIdx++) {
        size_t len = std::min(CHUNK, total - off);
        char key[12]; snprintf(key, sizeof(key), "cs_db_%d", chunkIdx);
        preferences.putBytes(key, fullBuf + off, len);
    }
    // Remove any stale extra chunks from a previous (larger) database
    while (true) {
        char key[12]; snprintf(key, sizeof(key), "cs_db_%d", chunkIdx);
        if (!preferences.isKey(key)) break;
        preferences.remove(key); chunkIdx++;
    }
    preferences.end();
    free(fullBuf);
    return true;
}

// ============================================================
// KDBX 3.1 reader
// ============================================================

// Read a null-terminated XML tag value
static String xmlTagValue(const String& xml, const String& tag) {
    String openTag  = "<" + tag + ">";
    String closeTag = "</" + tag + ">";
    int start = xml.indexOf(openTag);
    if (start < 0) return "";
    start += openTag.length();
    int end = xml.indexOf(closeTag, start);
    if (end < 0) return "";
    return xml.substring(start, end);
}

// Parse KeePass XML and load credentials
static void parseXML(const String& xml, const String& key,
                     Salsa20State& innerStream) {
    _creds.clear();
    int pos = 0;
    while (true) {
        int entryStart = xml.indexOf("<Entry>", pos);
        if (entryStart < 0) break;
        int entryEnd = xml.indexOf("</Entry>", entryStart);
        if (entryEnd < 0) break;

        String entry = xml.substring(entryStart, entryEnd + 8);
        pos = entryEnd + 8;

        // Extract title and password
        String label, encValue;

        // Walk through <String> elements
        int strPos = 0;
        while (true) {
            int sStart = entry.indexOf("<String>", strPos);
            if (sStart < 0) break;
            int sEnd = entry.indexOf("</String>", sStart);
            if (sEnd < 0) break;
            String strElem = entry.substring(sStart, sEnd);
            strPos = sEnd + 9;

            String k = xmlTagValue(strElem, "Key");

            // Check if Value has Protected="True"
            int vOpen = strElem.indexOf("<Value");
            int vClose = strElem.indexOf("</Value>");
            if (vOpen < 0 || vClose < 0) continue;

            int vContent = strElem.indexOf(">", vOpen) + 1;
            String vText = strElem.substring(vContent, vClose);
            bool isProtected = (strElem.indexOf("Protected=\"True\"") >= 0);

            if (k.equalsIgnoreCase("Title")) {
                label = xmlUnescape(vText);
            } else if (k.equalsIgnoreCase("Password")) {
                if (isProtected && !vText.isEmpty()) {
                    // Decode base64 then XOR with Salsa20 stream
                    std::vector<uint8_t> raw;
                    b64Decode(vText, raw);
                    salsa20Xor(innerStream, raw.data(), raw.size());
                    String plain;
                    plain.reserve(raw.size());
                    for (uint8_t b : raw) plain += (char)b;
                    encValue = credEncrypt(plain, key);
                } else if (!vText.isEmpty()) {
                    encValue = credEncrypt(xmlUnescape(vText), key);
                }
            }
        }

        if (!label.isEmpty()) {
            Credential c;
            c.label    = label;
            c.encValue = encValue;
            _creds.push_back(c);
        }
    }
}

static bool readKDBX(const String& password) {
    // ---- Read from NVS chunked blobs ----
    preferences.begin("kprox_db", true);
    int total = preferences.getInt("cs_db_n", 0);
    preferences.end();
    if (total <= 0) return false;

    uint8_t* fbuf = (uint8_t*)malloc(total);
    if (!fbuf) return false;

    static const size_t CHUNK = 3900;
    preferences.begin("kprox_db", true);
    int chunkIdx = 0;
    size_t loaded = 0;
    while (loaded < (size_t)total) {
        char key[12]; snprintf(key, sizeof(key), "cs_db_%d", chunkIdx);
        size_t got = preferences.getBytesLength(key);
        if (got == 0) break;
        preferences.getBytes(key, fbuf + loaded, got);
        loaded += got; chunkIdx++;
    }
    preferences.end();

    if (loaded != (size_t)total) { free(fbuf); return false; }
    size_t fsize = total;

    size_t pos = 0;
    auto readU32 = [&]() -> uint32_t {
        if (pos + 4 > fsize) return 0;
        uint32_t v = (uint32_t)fbuf[pos] | ((uint32_t)fbuf[pos+1]<<8) |
                     ((uint32_t)fbuf[pos+2]<<16) | ((uint32_t)fbuf[pos+3]<<24);
        pos += 4; return v;
    };
    auto readU16 = [&]() -> uint16_t {
        if (pos + 2 > fsize) return 0;
        uint16_t v = fbuf[pos] | (fbuf[pos+1]<<8); pos += 2; return v;
    };

    // Verify signatures
    if (readU32() != KDBX_SIG1 || readU32() != KDBX_SIG2) { free(fbuf); return false; }
    readU16(); readU16(); // version minor/major (skip)

    // Parse header fields
    uint8_t masterSeed[32]={}, transformSeed[32]={}, encIV[16]={};
    uint8_t protStreamKey[32]={}, streamStartBytes[32]={};
    uint64_t transformRounds = 6000;

    while (pos < fsize) {
        if (pos + 3 > fsize) break;
        uint8_t type = fbuf[pos++];
        uint16_t len = fbuf[pos] | (fbuf[pos+1]<<8); pos += 2;
        if (type == 0x00) { pos += len; break; }
        if (pos + len > fsize) break;
        uint8_t* data = fbuf + pos; pos += len;

        switch (type) {
            case 0x04: if (len==32) memcpy(masterSeed,    data, 32); break;
            case 0x05: if (len==32) memcpy(transformSeed, data, 32); break;
            case 0x06: if (len==8) {
                transformRounds = 0;
                for (int i = 0; i < 8; i++) transformRounds |= ((uint64_t)data[i] << (8*i));
                break;
            }
            case 0x07: if (len==16) memcpy(encIV,           data, 16); break;
            case 0x08: if (len==32) memcpy(protStreamKey,   data, 32); break;
            case 0x09: if (len==32) memcpy(streamStartBytes, data, 32); break;
        }
    }

    // Derive master key
    uint8_t masterKey[32];
    deriveMasterKey(password, masterSeed, transformSeed, transformRounds, masterKey);
    feedWatchdog();

    // Decrypt payload
    size_t encLen = fsize - pos;
    if (encLen < 32 || (encLen % 16) != 0) { free(fbuf); return false; }

    uint8_t* decrypted = (uint8_t*)malloc(encLen);
    if (!decrypted) { free(fbuf); return false; }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_dec(&aes, masterKey, 256) != 0) {
        mbedtls_aes_free(&aes); free(fbuf); free(decrypted); return false;
    }
    uint8_t iv_copy[16]; memcpy(iv_copy, encIV, 16);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, encLen, iv_copy, fbuf+pos, decrypted);
    mbedtls_aes_free(&aes);
    free(fbuf);
    feedWatchdog();

    // Verify StreamStartBytes
    if (memcmp(decrypted, streamStartBytes, 32) != 0) {
        free(decrypted);
        return false; // wrong password
    }

    // Build inner Salsa20 key
    uint8_t innerKey[32];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, protStreamKey, 32);
    mbedtls_sha256_finish(&sha, innerKey);
    mbedtls_sha256_free(&sha);

    Salsa20State innerStream;
    salsa20Init(innerStream, innerKey, SALSA20_IV);

    // Remove PKCS7 padding from decrypted block
    size_t plainLen = encLen;
    if (plainLen > 0) {
        uint8_t padByte = decrypted[encLen-1];
        if (padByte >= 1 && padByte <= 16) plainLen -= padByte;
    }

    // Unwrap hash blocks (starts at byte 32, after StreamStartBytes)
    size_t blocksLen = plainLen - 32;
    std::vector<uint8_t> xmlBytes = readHashBlocks(decrypted + 32, blocksLen);
    free(decrypted);

    if (xmlBytes.empty()) return false;

    String xml((char*)xmlBytes.data(), xmlBytes.size());
    parseXML(xml, password, innerStream);
    return true;
}

// ---- Failed-attempt counter (persisted in kprox_cs, survives settings wipe) ----

static int _getFailedAttempts() {
    preferences.begin("kprox_cs", true);
    int n = preferences.getInt("cs_fails", 0);
    preferences.end();
    return n;
}

static void _setFailedAttempts(int n) {
    preferences.begin("kprox_cs", false);
    preferences.putInt("cs_fails", n);
    preferences.end();
}

int csGetFailedAttempts()  { return _getFailedAttempts(); }
void csResetFailedAttempts() { _setFailedAttempts(0); }

// Wipe all keying material: KDBX file + TOTP namespace
static void _securityWipe() {
    credStoreWipe();
    // Wipe TOTP NVS namespace
    preferences.begin("kprox_totp", false);
    preferences.clear();
    preferences.end();
    _setFailedAttempts(0);
}

// ============================================================
// Public credential store API
// ============================================================

void credStoreInit() {
    _loaded             = false;
    credStoreLocked     = true;
    credStoreRuntimeKey = "";
    _creds.clear();
}

void credStoreWipe() {
    preferences.begin("kprox_db", false);
    preferences.clear();
    preferences.end();
    // Also clear old-style NVS keycheck if present
    preferences.begin("kprox_cs", false);
    preferences.remove("cs_kc");
    preferences.end();
    _creds.clear();
    _loaded             = false;
    credStoreLocked     = true;
    credStoreRuntimeKey = "";
}

bool credStoreUnlock(const String& key) {
    return credStoreUnlockWithTOTP(key, "");
}

bool credStoreUnlockWithTOTP(const String& key, const String& totpCode) {
    CSGateMode gate = csGateGetMode();

    auto onFail = [&]() -> bool {
        int fails = _getFailedAttempts() + 1;
        _setFailedAttempts(fails);
        if (csAutoWipeAttempts > 0 && fails >= csAutoWipeAttempts) {
            _securityWipe();
        }
        return false;
    };
    auto onSuccess = [&]() {
        csResetFailedAttempts();
        credStoreLastActivity = millis();
    };

    if (gate == CSGateMode::TOTP_ONLY) {
        if (!totpTimeReady()) return onFail();
        if (!csGateVerifyCode(totpCode)) return onFail();
        String secret = csGateGetSecret();
        bool ok = readKDBX(secret);
        if (!ok) {
            credStoreRuntimeKey = secret;
            credStoreLocked     = false;
            _loaded             = true;
            _creds.clear();
            writeKDBX(secret);
            onSuccess();
            return true;
        }
        credStoreRuntimeKey = secret;
        credStoreLocked     = false;
        _loaded             = true;
        writeKDBX(secret);
        onSuccess();
        return true;
    }

    if (gate == CSGateMode::TOTP) {
        if (!totpTimeReady() || !csGateVerifyCode(totpCode)) return onFail();
    }

    bool ok = readKDBX(key);
    if (!ok) {
        // DB exists but wrong key
        {
            preferences.begin("kprox_db", true);
            bool hasDb = preferences.getInt("cs_db_n", 0) > 0;
            preferences.end();
            if (hasDb) return onFail();
        }
        // First-time: create empty database with this key
        credStoreRuntimeKey = key;
        credStoreLocked     = false;
        _loaded             = true;
        _creds.clear();
        writeKDBX(key);
        onSuccess();
        return true;
    }
    credStoreRuntimeKey = key;
    credStoreLocked     = false;
    _loaded             = true;
    writeKDBX(key);
    onSuccess();
    return true;
}

void credStoreLock() {
    credStoreRuntimeKey = "";
    credStoreLocked     = true;
    _creds.clear();
    _loaded = false;
}

bool credStoreRekey(const String& oldKey, const String& newKey) {
    // Verify old key by attempting to read
    if (!readKDBX(oldKey)) return false;
    credStoreRuntimeKey = newKey;
    return writeKDBX(newKey);
}

int credStoreCount() {
    return credStoreLocked ? 0 : (int)_creds.size();
}

bool credStoreLabelExists(const String& label) {
    for (auto& c : _creds)
        if (c.label.equalsIgnoreCase(label)) return true;
    return false;
}

std::vector<String> credStoreListLabels() {
    std::vector<String> labels;
    if (!credStoreLocked)
        for (auto& c : _creds) labels.push_back(c.label);
    return labels;
}

String credStoreGet(const String& label) {
    if (credStoreLocked) return "";
    for (auto& c : _creds)
        if (c.label.equalsIgnoreCase(label))
            return credDecrypt(c.encValue, credStoreRuntimeKey);
    return "";
}

bool credStoreSet(const String& label, const String& value) {
    if (credStoreLocked) return false;
    String enc = credEncrypt(value, credStoreRuntimeKey);
    for (auto& c : _creds) {
        if (c.label.equalsIgnoreCase(label)) {
            c.encValue = enc;
            return writeKDBX(credStoreRuntimeKey);
        }
    }
    Credential c; c.label = label; c.encValue = enc;
    _creds.push_back(c);
    return writeKDBX(credStoreRuntimeKey);
}

bool credStoreDelete(const String& label) {
    if (credStoreLocked) return false;
    for (auto it = _creds.begin(); it != _creds.end(); ++it) {
        if (it->label.equalsIgnoreCase(label)) {
            _creds.erase(it);
            return writeKDBX(credStoreRuntimeKey);
        }
    }
    return false;
}
