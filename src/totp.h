#pragma once
#include <Arduino.h>
#include <vector>

// ---- TOTP config stored per account ----
struct TOTPAccount {
    String name;        // display name / label
    String secret;      // Base32-encoded secret
    int    period = 30; // seconds per window (always 30 for RFC 6238)
    int    digits = 6;  // code length (6 or 8)
};

// ---- Credential store TOTP gate settings ----
// Persisted in NVS namespace "kprox_totp"
enum class CSGateMode : uint8_t {
    NONE  = 0,  // no gate — key only
    TOTP  = 1,  // TOTP required after PIN (second factor)
    TOTP_ONLY = 2, // TOTP replaces PIN (secret stored in kprox_totp NVS)
};

// TOTP math
uint32_t totpCompute(const String& base32Secret, time_t utcNow,
                     int period = 30, int digits = 6);
// Returns -1 if secret is invalid
int      totpSecondsRemaining(time_t utcNow, int period = 30);

// Account store (NVS namespace kprox_totp)
void totpInit();
int  totpCount();
std::vector<TOTPAccount> totpListAccounts();
bool totpAddAccount(const TOTPAccount& acct);
bool totpDeleteAccount(const String& name);
// Wipes all totp NVS data
void totpWipe();

// Compute current code for a named account (-1 = not found / bad secret)
int32_t totpGetCode(const String& name);

// Credential store gate
CSGateMode csGateGetMode();
void       csGateSetMode(CSGateMode mode);
// For TOTP_ONLY mode: secret stored separately
String     csGateGetSecret();
void       csGateSetSecret(const String& b32);
// Verify: returns true if the supplied 6-char code is valid right now
// (or within one window either side for clock skew)
bool       csGateVerifyCode(const String& codeStr);
// Is time currently synced? (epoch > 1000000000)
bool       totpTimeReady();
