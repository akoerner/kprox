#pragma once

#include "globals.h"
#include <vector>

enum class CredField : uint8_t {
    PASSWORD = 0,
    USERNAME = 1,
    NOTES    = 2,
};

struct Credential {
    String label;
    String encPassword;
    String encUsername;
    String encNotes;

    // Legacy accessor — maps old encValue references
    String& encValue() { return encPassword; }
};

// Volatile runtime key — never persisted to flash
extern String credStoreRuntimeKey;
extern bool   credStoreLocked;

void   credStoreInit();
void   credStoreWipe();

// Returns false if key doesn't match the stored keycheck
bool   credStoreUnlock(const String& key);
// Full unlock with optional TOTP second factor / TOTP-only mode
bool   credStoreUnlockWithTOTP(const String& key, const String& totpCode);
void   credStoreLock();

// Re-encrypts all values with newKey; oldKey must be current runtime key
bool   credStoreRekey(const String& oldKey, const String& newKey);

int    credStoreCount();
bool   credStoreLabelExists(const String& label);
std::vector<String> credStoreListLabels();
std::vector<String> credStoreListAllLabels(); // includes __totp__: entries

// Returns "" if locked or not found.
// credStoreGet without field returns the password (default field).
String credStoreGet(const String& label);
String credStoreGet(const String& label, CredField field);

// Upsert: adds or replaces credential; returns false if locked.
// credStoreSet without field sets the password.
bool   credStoreSet(const String& label, const String& value);
bool   credStoreSet(const String& label, const String& value, CredField field);
bool   credStoreDelete(const String& label);

// Low-level symmetric AES-256-CTR+HMAC keyed from an arbitrary key string
String credEncrypt(const String& plaintext, const String& key);
String credDecrypt(const String& b64, const String& key);

// Failed-attempt tracking (persisted across reboots in kprox_cs NVS)
int  csGetFailedAttempts();
void csResetFailedAttempts();

// SD card helpers (always compiled; no-op on non-SD builds)
bool sdMount();
void sdUnmount();
bool sdAvailable();
bool sdExists();
bool sdFormat();
void sdRemove();

// Internal — exposed for storage migration in api.cpp
bool writeKDBX(const String& password);
