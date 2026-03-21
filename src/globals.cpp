#include "globals.h"

// ---- Runtime-configurable HID timing (ms) ----
int g_keyPressDelay        = KEY_PRESS_DELAY;
int g_keyReleaseDelay      = KEY_RELEASE_DELAY;
int g_betweenKeysDelay     = BETWEEN_KEYS_DELAY;
int g_betweenSendTextDelay = BETWEEN_SEND_TEXT_DELAY;
int g_specialKeyDelay      = SPECIAL_KEY_DELAY;
int g_tokenDelay           = TOKEN_DELAY;

// ---- Runtime-configurable identity ----
String hostnameStr     = HOSTNAME;
String usbSerialNumber = USB_SERIAL_NUMBER;

// ---- Default startup app ----
int defaultAppIndex = 1;

// ---- App display order and visibility ----
std::vector<int>  appOrder;
std::vector<bool> appHidden;

// ---- Credential store security settings ----
int           csAutoLockSecs        = 0;
int           csAutoWipeAttempts    = 0;
unsigned long credStoreLastActivity = 0;

String csStorageLocation = "nvs";

// ---- Sink ----
int maxSinkSize = 0; // 0 = unlimited

// ---- Boot register ----
bool   bootRegEnabled    = false;
int    bootRegIndex      = 0;
int    bootRegLimit      = 0;
int    bootRegFiredCount = 0;
