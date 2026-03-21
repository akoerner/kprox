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

// ---- TOTP account store ----
// Accounts are stored entirely in the credential store using the label prefix
// "__totp__:" so that name, secret, digits, and period are all encrypted at rest.
// Label format: "__totp__:<name>"
// Value format: "<base32_secret>:<digits>:<period>"
//
// Gate settings (gate mode + gate TOTP secret) remain in the "kprox_totp" NVS
// namespace because they are needed before the credential store can be unlocked.
//
// Migration: on first totpListAccounts() call after a successful unlock, any
// accounts still present in the old NVS keys (nm<i>/sc<i>/dg<i>/pd<i>) are
// migrated into the credential store and the old keys are erased.

static const char* TOTP_NS     = "kprox_totp";
static const char* TOTP_PREFIX = "__totp__:";

void totpInit() {}

static String _totpLabel(const String& name) {
    return String(TOTP_PREFIX) + name;
}

static bool _isTotpLabel(const String& label) {
    return label.startsWith(TOTP_PREFIX);
}

static String _totpNameFromLabel(const String& label) {
    return label.substring(strlen(TOTP_PREFIX));
}

static String _encodeValue(const TOTPAccount& a) {
    return a.secret + ":" + String(a.digits) + ":" + String(a.period);
}

static TOTPAccount _decodeValue(const String& name, const String& value) {
    TOTPAccount a;
    a.name = name;
    int c1 = value.indexOf(':');
    int c2 = (c1 >= 0) ? value.indexOf(':', c1 + 1) : -1;
    if (c1 > 0) {
        a.secret = value.substring(0, c1);
        a.digits = (c2 > c1) ? value.substring(c1 + 1, c2).toInt() : 6;
        a.period = (c2 > 0) ? value.substring(c2 + 1).toInt() : 30;
    } else {
        a.secret = value;
        a.digits = 6;
        a.period = 30;
    }
    if (a.digits < 6 || a.digits > 8) a.digits = 6;
    if (a.period < 15 || a.period > 120) a.period = 30;
    return a;
}

// Migrate any accounts still stored in the old NVS layout into the credential store.
static void _migrateNvsAccounts() {
    preferences.begin(TOTP_NS, true);
    int n = preferences.getInt("n", 0);
    preferences.end();
    if (n <= 0) return;

    preferences.begin(TOTP_NS, false);
    for (int i = 0; i < n; i++) {
        String name   = preferences.getString(("nm" + String(i)).c_str(), "");
        String encSec = preferences.getString(("sc" + String(i)).c_str(), "");
        int    digits = preferences.getInt(   ("dg" + String(i)).c_str(), 6);
        int    period = preferences.getInt(   ("pd" + String(i)).c_str(), 30);
        if (name.isEmpty()) continue;

        String secret = encSec.isEmpty() ? "" : credDecrypt(encSec, credStoreRuntimeKey);
        if (secret.isEmpty()) continue;

        TOTPAccount a; a.name = name; a.secret = secret; a.digits = digits; a.period = period;
        credStoreSet(_totpLabel(name), _encodeValue(a));

        preferences.remove(("nm" + String(i)).c_str());
        preferences.remove(("sc" + String(i)).c_str());
        preferences.remove(("dg" + String(i)).c_str());
        preferences.remove(("pd" + String(i)).c_str());
    }
    preferences.putInt("n", 0);
    preferences.end();
}

int totpCount() {
    if (credStoreLocked) return 0;
    _migrateNvsAccounts();
    int count = 0;
    for (auto& lbl : credStoreListAllLabels())
        if (_isTotpLabel(lbl)) count++;
    return count;
}

std::vector<TOTPAccount> totpListAccounts() {
    if (credStoreLocked) return {};
    _migrateNvsAccounts();
    std::vector<TOTPAccount> out;
    for (auto& lbl : credStoreListAllLabels()) {
        if (!_isTotpLabel(lbl)) continue;
        String name  = _totpNameFromLabel(lbl);
        String value = credStoreGet(lbl);
        if (value.isEmpty()) continue;
        out.push_back(_decodeValue(name, value));
    }
    return out;
}

bool totpAddAccount(const TOTPAccount& acct) {
    if (acct.name.isEmpty() || acct.secret.isEmpty()) return false;
    if (base32Decode(acct.secret).size() < 10) return false;
    if (credStoreLocked) return false;
    _migrateNvsAccounts();
    return credStoreSet(_totpLabel(acct.name), _encodeValue(acct));
}

bool totpDeleteAccount(const String& name) {
    if (credStoreLocked) return false;
    _migrateNvsAccounts();
    return credStoreDelete(_totpLabel(name));
}

void totpWipe() {
    // Remove all __totp__: entries from the credential store
    if (!credStoreLocked) {
        for (auto& lbl : credStoreListAllLabels())
            if (_isTotpLabel(lbl)) credStoreDelete(lbl);
    }
    // Also clear legacy NVS entries
    preferences.begin(TOTP_NS, false);
    int n = preferences.getInt("n", 0);
    for (int i = 0; i < n; i++) {
        preferences.remove(("nm" + String(i)).c_str());
        preferences.remove(("sc" + String(i)).c_str());
        preferences.remove(("dg" + String(i)).c_str());
        preferences.remove(("pd" + String(i)).c_str());
    }
    preferences.putInt("n", 0);
    preferences.end();
}

int32_t totpGetCode(const String& name) {
    if (credStoreLocked) return -1;
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
