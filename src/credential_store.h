#pragma once

#include "globals.h"
#include <vector>

struct Credential {
    String label;
    String encValue;
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

// Returns "" if locked or not found
String credStoreGet(const String& label);

// Upsert: adds or replaces credential; returns false if locked
bool   credStoreSet(const String& label, const String& value);
bool   credStoreDelete(const String& label);

// Low-level symmetric AES-256-CTR+HMAC keyed from an arbitrary key string
String credEncrypt(const String& plaintext, const String& key);
String credDecrypt(const String& b64, const String& key);

// Failed-attempt tracking (persisted across reboots in kprox_cs NVS)
int  csGetFailedAttempts();
void csResetFailedAttempts();
