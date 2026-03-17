#include "totp.h"
#include "globals.h"
#include "credential_store.h"
#include <mbedtls/md.h>
#include <time.h>

// ---- Base32 decoder ----

static int base32CharVal(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;
}

static std::vector<uint8_t> base32Decode(const String& input) {
    std::vector<uint8_t> out;
    int bits = 0, val = 0;
    for (int i = 0; i < (int)input.length(); i++) {
        char c = input[i];
        if (c == '=' || c == ' ') continue;
        int v = base32CharVal(c);
        if (v < 0) continue;
        val = (val << 5) | v;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)(val >> bits));
            val &= (1 << bits) - 1;
        }
    }
    return out;
}

// ---- RFC 4226 HOTP ----

static uint32_t hotp(const uint8_t* key, size_t keyLen, uint64_t counter, int digits) {
    // Counter as big-endian 8-byte value
    uint8_t msg[8];
    for (int i = 7; i >= 0; i--) {
        msg[i] = (uint8_t)(counter & 0xFF);
        counter >>= 8;
    }

    uint8_t hmac[20];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
    mbedtls_md_hmac_starts(&ctx, key, keyLen);
    mbedtls_md_hmac_update(&ctx, msg, 8);
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);

    // Dynamic truncation (RFC 4226 §5.4)
    int offset = hmac[19] & 0x0F;
    uint32_t code = ((uint32_t)(hmac[offset]     & 0x7F) << 24)
                  | ((uint32_t)(hmac[offset + 1] & 0xFF) << 16)
                  | ((uint32_t)(hmac[offset + 2] & 0xFF) <<  8)
                  | ((uint32_t)(hmac[offset + 3] & 0xFF));

    static const uint32_t powTen[] = {1,10,100,1000,10000,100000,1000000,10000000,100000000};
    return code % powTen[digits < 9 ? digits : 6];
}

// ---- Public TOTP math ----

uint32_t totpCompute(const String& base32Secret, time_t utcNow, int period, int digits) {
    std::vector<uint8_t> key = base32Decode(base32Secret);
    if (key.empty()) return 0;
    uint64_t counter = (uint64_t)utcNow / period;
    return hotp(key.data(), key.size(), counter, digits);
}

int totpSecondsRemaining(time_t utcNow, int period) {
    return period - (int)(utcNow % period);
}

bool totpTimeReady() {
    return time(nullptr) > 1000000000L;
}

// ---- NVS account store ----
// Namespace "kprox_totp"
// Keys: "n" = count, "nm<i>" = name, "sc<i>" = secret,
//       "dg<i>" = digits, "pd<i>" = period
// Gate keys: "gate" = CSGateMode byte, "gsec" = gate TOTP secret

static const char* TOTP_NS = "kprox_totp";

void totpInit() {
    // Nothing to do at runtime; NVS loaded on demand
}

int totpCount() {
    preferences.begin(TOTP_NS, true);
    int n = preferences.getInt("n", 0);
    preferences.end();
    return n;
}

std::vector<TOTPAccount> totpListAccounts() {
    preferences.begin(TOTP_NS, true);
    int n = preferences.getInt("n", 0);
    std::vector<TOTPAccount> out;
    for (int i = 0; i < n; i++) {
        TOTPAccount a;
        a.name   = preferences.getString(("nm" + String(i)).c_str(), "");
        a.digits = preferences.getInt(   ("dg" + String(i)).c_str(), 6);
        a.period = preferences.getInt(   ("pd" + String(i)).c_str(), 30);
        // Only decrypt secret when the credential store is unlocked
        if (!credStoreLocked && !credStoreRuntimeKey.isEmpty()) {
            String enc = preferences.getString(("sc" + String(i)).c_str(), "");
            a.secret = enc.isEmpty() ? "" : credDecrypt(enc, credStoreRuntimeKey);
        }
        // secret left empty when locked — callers check for empty
        if (!a.name.isEmpty()) out.push_back(a);
    }
    preferences.end();
    return out;
}

bool totpAddAccount(const TOTPAccount& acct) {
    if (acct.name.isEmpty() || acct.secret.isEmpty()) return false;
    if (base32Decode(acct.secret).size() < 10) return false;

    // Encrypt the secret before storing. If the store is locked we cannot
    // encrypt so refuse to add until the store is unlocked.
    if (credStoreLocked || credStoreRuntimeKey.isEmpty()) return false;
    String encSecret = credEncrypt(acct.secret, credStoreRuntimeKey);
    if (encSecret.isEmpty()) return false;

    preferences.begin(TOTP_NS, false);
    int n = preferences.getInt("n", 0);
    for (int i = 0; i < n; i++) {
        if (preferences.getString(("nm" + String(i)).c_str(), "").equalsIgnoreCase(acct.name)) {
            preferences.putString(("sc" + String(i)).c_str(), encSecret);
            preferences.putInt(   ("dg" + String(i)).c_str(), acct.digits);
            preferences.putInt(   ("pd" + String(i)).c_str(), acct.period);
            preferences.end();
            return true;
        }
    }
    preferences.putString(("nm" + String(n)).c_str(), acct.name);
    preferences.putString(("sc" + String(n)).c_str(), encSecret);
    preferences.putInt(   ("dg" + String(n)).c_str(), acct.digits);
    preferences.putInt(   ("pd" + String(n)).c_str(), acct.period);
    preferences.putInt("n", n + 1);
    preferences.end();
    return true;
}

bool totpDeleteAccount(const String& name) {
    auto accounts = totpListAccounts();
    int found = -1;
    for (int i = 0; i < (int)accounts.size(); i++) {
        if (accounts[i].name.equalsIgnoreCase(name)) { found = i; break; }
    }
    if (found < 0) return false;
    accounts.erase(accounts.begin() + found);

    preferences.begin(TOTP_NS, false);
    // Preserve gate settings — only rewrite account keys
    preferences.putInt("n", (int)accounts.size());
    for (int i = 0; i < (int)accounts.size(); i++) {
        preferences.putString(("nm" + String(i)).c_str(), accounts[i].name);
        preferences.putString(("sc" + String(i)).c_str(), accounts[i].secret);
        preferences.putInt(   ("dg" + String(i)).c_str(), accounts[i].digits);
        preferences.putInt(   ("pd" + String(i)).c_str(), accounts[i].period);
    }
    // Clear trailing stale key
    preferences.remove(("nm" + String(accounts.size())).c_str());
    preferences.remove(("sc" + String(accounts.size())).c_str());
    preferences.remove(("dg" + String(accounts.size())).c_str());
    preferences.remove(("pd" + String(accounts.size())).c_str());
    preferences.end();
    return true;
}

void totpWipe() {
    preferences.begin(TOTP_NS, false);
    preferences.clear();
    preferences.end();
}

int32_t totpGetCode(const String& name) {
    auto accounts = totpListAccounts();
    for (auto& a : accounts) {
        if (a.name.equalsIgnoreCase(name)) {
            if (!totpTimeReady()) return -1;
            return (int32_t)totpCompute(a.secret, time(nullptr), a.period, a.digits);
        }
    }
    return -1;
}

// ---- Credential store gate ----

CSGateMode csGateGetMode() {
    preferences.begin(TOTP_NS, true);
    uint8_t m = preferences.getUChar("gate", 0);
    preferences.end();
    return (CSGateMode)m;
}

void csGateSetMode(CSGateMode mode) {
    preferences.begin(TOTP_NS, false);
    preferences.putUChar("gate", (uint8_t)mode);
    preferences.end();
}

String csGateGetSecret() {
    preferences.begin(TOTP_NS, true);
    String s = preferences.getString("gsec", "");
    preferences.end();
    return s;
}

void csGateSetSecret(const String& b32) {
    preferences.begin(TOTP_NS, false);
    preferences.putString("gsec", b32);
    preferences.end();
}

bool csGateVerifyCode(const String& codeStr) {
    if (!totpTimeReady()) return false;
    String secret = csGateGetSecret();
    if (secret.isEmpty()) return false;
    if (codeStr.length() < 6) return false;

    uint32_t entered = (uint32_t)codeStr.toInt();
    time_t   now     = time(nullptr);

    // Accept current window and ±1 for clock skew
    for (int delta = -1; delta <= 1; delta++) {
        if (totpCompute(secret, now + delta * 30) == entered) return true;
    }
    return false;
}
