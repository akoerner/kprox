#pragma once

#include <Arduino.h>
#include <esp_chip_info.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <FastLED.h>
#include <vector>
#include <map>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <SPIFFS.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/error.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <WebSocketsServer.h>
// For Cardputer: M5Cardputer must be included before BLE Combo because both define
// overlapping KEY_* names. M5Cardputer uses #define macros; BLE Combo uses const uint8_t.
// Including M5Cardputer first then undefing the macros lets BLE Combo declare its consts
// cleanly. All other translation units include globals.h first, so M5Cardputer.h's include
// guard fires on re-inclusion and the macros are not re-instated.
#ifdef BOARD_M5STACK_CARDPUTER
#  include <M5Cardputer.h>
#  undef KEY_LEFT_CTRL
#  undef KEY_LEFT_SHIFT
#  undef KEY_LEFT_ALT
#  undef KEY_LEFT_GUI
#  undef KEY_RIGHT_CTRL
#  undef KEY_RIGHT_SHIFT
#  undef KEY_RIGHT_ALT
#  undef KEY_RIGHT_GUI
#  undef KEY_BACKSPACE
#  undef KEY_TAB
#  undef KEY_RETURN
#  undef KEY_ESC
#  undef KEY_INSERT
#  undef KEY_DELETE
#  undef KEY_HOME
#  undef KEY_END
#  undef KEY_PAGE_UP
#  undef KEY_PAGE_DOWN
#  undef KEY_UP_ARROW
#  undef KEY_DOWN_ARROW
#  undef KEY_LEFT_ARROW
#  undef KEY_RIGHT_ARROW
#  undef KEY_CAPS_LOCK
#  undef KEY_F1
#  undef KEY_F2
#  undef KEY_F3
#  undef KEY_F4
#  undef KEY_F5
#  undef KEY_F6
#  undef KEY_F7
#  undef KEY_F8
#  undef KEY_F9
#  undef KEY_F10
#  undef KEY_F11
#  undef KEY_F12
#endif

#include "ble/BleComboKeyboard.h"
#include "ble/BleComboMouse.h"

#include "config.h"
#include "constants.h"
#include "credential_store.h"

#ifdef BOARD_HAS_USB_HID
#  include "USB.h"
#  include "USBHIDKeyboard.h"
#  include "USBHIDMouse.h"
#  include "ble/USBHIDFIDO2.h"
#  include "KProxConsumerHID.h"
#endif

struct MouseBatch {
    int16_t accumulatedX = 0;
    int16_t accumulatedY = 0;
    unsigned long lastUpdate = 0;
    bool hasMovement = false;
};

// ---- Hardware objects ----
extern WebServer        server;     // HTTP on 80; TLS terminated externally when mTLS is configured
extern WebServer        serverHTTP; // reserved for HTTP->HTTPS redirect when mTLS active
extern WebSocketsServer webSocket;
extern WiFiUDP          udp;
extern Preferences      preferences;
#ifdef BOARD_M5STACK_CARDPUTER
extern BleComboKeyboard* Keyboard;
extern BleComboMouse*    Mouse;
#define BLE_KEYBOARD (*Keyboard)
#define BLE_MOUSE    (*Mouse)
#define BLE_KEYBOARD_VALID (Keyboard != nullptr)
#else
extern BleComboKeyboard Keyboard;
extern BleComboMouse    Mouse;
#define BLE_KEYBOARD Keyboard
#define BLE_MOUSE    Mouse
#define BLE_KEYBOARD_VALID true
#endif
extern CRGB             leds[];

#ifdef BOARD_HAS_USB_HID
extern USBHIDKeyboard USBKeyboard;
extern USBHIDMouse    USBMouse;
#endif

// ---- Settings / identity ----
extern String wifiSSID;
extern String wifiPassword;
extern String apiKey;
extern String usbManufacturer;
extern String usbProduct;
extern const char* hostname;   // points to hostnameStr.c_str()
extern const char* deviceName;

// ---- BLE / USB state ----
extern bool bluetoothEnabled;
extern bool bluetoothInitialized;
extern bool bleKeyboardEnabled;    // BLE keyboard sub-enable (requires bluetoothEnabled)
extern bool bleMouseEnabled;       // BLE mouse sub-enable (requires bluetoothEnabled)
extern bool bleIntlKeyboardEnabled; // BLE extended (international) keyboard report enable

#ifdef BOARD_HAS_USB_HID
extern bool usbEnabled;
extern bool usbInitialized;
extern bool usbKeyboardReady;
extern bool usbMouseReady;
extern bool usbKeyboardEnabled;
extern bool usbMouseEnabled;
extern bool usbIntlKeyboardEnabled; // USB extended (international) keyboard report enable
extern bool fido2Enabled;
#endif

// ---- Network state ----
extern bool          wifiEnabled;
extern bool          mdnsEnabled;
extern bool          udpEnabled;
extern unsigned long lastUdpBroadcast;
extern unsigned long lastWifiCheck;

// ---- Execution state ----
extern bool          isHalted;
extern bool          requestInProgress;
extern bool          isLooping;
extern unsigned long loopDuration;
extern unsigned long loopStartTime;
extern int           loopingRegister;
extern std::vector<String> pendingTokenStrings;

// ---- Time ----
extern long utcOffsetSeconds;

// ---- Keymap ----
extern String activeKeymap;

// ---- Register state ----
extern int                  activeRegister;
extern std::vector<String>  registers;
extern std::vector<String>  registerNames;
extern bool                 registersLoaded;

// ---- LED state ----
extern bool    ledEnabled;
extern uint8_t ledColorR;
extern uint8_t ledColorG;
extern uint8_t ledColorB;

// ---- Mouse state ----
extern MouseBatch mouseBatch;
extern int        currentMouseX;
extern int        currentMouseY;

// ---- Sink ----
extern int maxSinkSize; // 0 = unlimited

// ---- TimerProx persisted settings ----
extern int timerProxRegIdx;
extern int timerProxFireH, timerProxFireM, timerProxFireS;
extern int timerProxHaltH, timerProxHaltM, timerProxHaltS;
extern int timerProxRepH,  timerProxRepM,  timerProxRepS;

extern int           g_displayBrightness;  // 0-255, default 128
extern unsigned long g_screenTimeoutMs;    // ms, default 60000

// ---- Boot register ----
extern bool   bootRegEnabled;
extern int    bootRegIndex;    // 0-based register index
extern int    bootRegLimit;    // 0 = every boot; N = fire N times then disable
extern int    bootRegFiredCount; // how many times it has fired

// Credential store security settings
extern int           csAutoLockSecs;       // 0 = disabled
extern int           csAutoWipeAttempts;   // 0 = disabled
extern unsigned long credStoreLastActivity; // millis() of last HID output or button press

// Credential store backend: "nvs" (default) or "sd"
extern String csStorageLocation;

// ---- Runtime-configurable HID timing (ms) ----
extern int g_keyPressDelay;
extern int g_keyReleaseDelay;
extern int g_betweenKeysDelay;
extern int g_betweenSendTextDelay;
extern int g_specialKeyDelay;
extern int g_tokenDelay;

// ---- Runtime-configurable identity ----
extern String hostnameStr;
extern String usbSerialNumber;

// ---- Default startup app (Cardputer launcher index) ----
extern int defaultAppIndex;

// App display order and visibility (indices 1..N_APPS into registered apps).
// appOrder[i] = registered app index to show at launcher position i.
// appHidden[i] = true if that registered app index should not appear in launcher.
// Settings app (last registered) can never be hidden.
extern std::vector<int>  appOrder;
extern std::vector<bool> appHidden;
// Cleared at the start of putTokenString() before each new run.
extern bool g_parserAbort;

// Non-zero means haltAllOperations() should be called at or after this millis() timestamp.
// Set by AppTimerProx when the register fires; checked by checkParseInterrupt() so
// the halt triggers even while token execution is blocking the main loop.
extern unsigned long g_haltDeadlineMs;

// Optional hook called by checkParseInterrupt() on every tick during token execution.
// Used by AppTimerProx to update the HALTING countdown display while the main loop is blocked.
// Must be cleared before the installing object is destroyed.
extern void (*g_parseInterruptHook)();

// Set by checkParseInterrupt() when BtnA specifically triggered the abort.
// KProx reads and clears this to skip the corresponding wasReleased event.
extern bool g_btnAHaltedPlayback;
extern bool g_needsDisplayRedraw; // set by token_parser to request current app repaint
void feedWatchdog();
void initWatchdog();

// ---- Nonce state (defined in api.cpp) ----
extern String currentNonce;

// ---- mTLS state (defined in mtls.cpp) ----
extern bool   mtlsEnabled;
extern String serverCert;
extern String serverKey;
extern String caCert;

// ---- Keymap (defined in keymap.cpp) ----
extern String activeKeymap;
