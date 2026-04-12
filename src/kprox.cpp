#ifndef BOARD_M5STACK_CARDPUTER
#include "globals.h"
#include "led.h"
#include "hid.h"
#include "storage.h"
#include "registers.h"
#include "connection.h"
#include "token_parser.h"
#include "api.h"
#include "ws_mouse.h"
#include "mtls.h"
#include "keymap.h"
// ---- mDNS version compatibility ----
class MDNSHelper {
public:
    static void safeUpdate() {
#ifdef ESP_ARDUINO_VERSION_MAJOR
#  if ESP_ARDUINO_VERSION_MAJOR < 2
        MDNS.update();
#  endif
#else
#  ifdef ARDUINO_ESP32_DEV
        MDNS.update();
#  endif
#endif
    }
};
#define MDNS_UPDATE() MDNSHelper::safeUpdate()
// ---- Global definitions ----
WebServer        server(80);
WebServer        serverHTTP(443);
WiFiUDP          udp;
Preferences      preferences;
CRGB             leds[NUM_LEDS];
BleComboKeyboard Keyboard(DEFAULT_PRODUCT_NAME, DEFAULT_MANUFACTURER, BATTERY_LEVEL);
BleComboMouse    Mouse(&Keyboard);
#ifdef BOARD_M5STACK_ATOMS3
USBHIDKeyboard USBKeyboard;
USBHIDMouse    USBMouse;
bool usbEnabled       = true;
bool usbInitialized   = false;
bool usbKeyboardReady = false;
bool usbMouseReady    = false;
bool usbKeyboardEnabled    = true;
bool usbMouseEnabled       = true;
bool usbIntlKeyboardEnabled = true;
bool usbConsumerEnabled     = true;
bool usbSystemEnabled       = true;
bool fido2Enabled          = false;
#ifdef BOARD_HAS_USB_HID
uint16_t usbVidOverride = DEFAULT_USB_VID;
uint16_t usbPidOverride = DEFAULT_USB_PID;
#endif
#endif
bool bleKeyboardEnabled    = true;
bool bleMouseEnabled       = true;
bool bleIntlKeyboardEnabled = true;
bool bleConsumerEnabled     = true;
bool bleSystemEnabled       = true;
// maxSinkSize defined in globals.cpp
String wifiSSID      = DEFAULT_WIFI_SSID;
String wifiPassword  = DEFAULT_WIFI_PASSWORD;
String apiKey        = DEFAULT_API_KEY;
String usbManufacturer = DEFAULT_MANUFACTURER;
String usbProduct    = DEFAULT_PRODUCT_NAME;
const char* hostname   = hostnameStr.c_str();
const char* deviceName = DEFAULT_PRODUCT_NAME;
bool bluetoothEnabled     = true;
bool bluetoothInitialized = false;
bool wifiEnabled          = true;
bool mdnsEnabled          = false;
bool isLooping            = false;
bool isHalted             = false;
bool requestInProgress    = false;
bool udpEnabled           = true;
bool ledEnabled           = true;
bool registersLoaded      = false;
unsigned long loopDuration    = 0;
unsigned long loopStartTime   = 0;
unsigned long lastUdpBroadcast = 0;
unsigned long lastWifiCheck   = 0;
unsigned long lastStatusPrint = 0;
int loopingRegister = -1;
long                utcOffsetSeconds = 0;
int activeRegister  = 0;
int currentMouseX   = 0;
int currentMouseY   = 0;
uint8_t ledColorR = 0;
uint8_t ledColorG = 255;
uint8_t ledColorB = 0;
std::vector<String> registers;
std::vector<String> registerNames;
MouseBatch mouseBatch;
// ---- Watchdog ----
void initWatchdog() {
    esp_task_wdt_deinit();
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    esp_task_wdt_config_t cfg = {
        .timeout_ms     = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic  = true
    };
    esp_task_wdt_init(&cfg);
#else
    esp_task_wdt_init(WDT_TIMEOUT, true);
#endif
    esp_task_wdt_add(NULL);
}
void feedWatchdog() {
    esp_task_wdt_reset();
}
// ---- Button handling ----
static unsigned long lastButtonPress   = 0;
static unsigned long lastButtonRelease = 0;
static bool          buttonPressed     = false;
static bool          haltTriggered     = false;
static bool          deleteAllTriggered = false;
static int           buttonPressCount  = 0;
static void checkButton() {
    bool currentState = digitalRead(BUTTON_PIN) == LOW;
    unsigned long now = millis();
    if (currentState && !buttonPressed && (now - lastButtonRelease > BUTTON_DEBOUNCE)) {
        buttonPressed       = true;
        lastButtonPress     = now;
        haltTriggered       = false;
        deleteAllTriggered  = false;
    } else if (!currentState && buttonPressed && (now - lastButtonPress > BUTTON_DEBOUNCE)) {
        buttonPressed      = false;
        lastButtonRelease  = now;
        if (!haltTriggered && !deleteAllTriggered) buttonPressCount++;
    }
    if (buttonPressed && !haltTriggered && !deleteAllTriggered &&
        now - lastButtonPress >= BUTTON_HALT_THRESHOLD &&
        now - lastButtonPress <  BUTTON_DELETE_ALL_THRESHOLD) {
        haltTriggered    = true;
        buttonPressCount = 0;
        isHalted ? resumeOperations() : haltAllOperations();
    }
    if (buttonPressed && !deleteAllTriggered && now - lastButtonPress >= BUTTON_DELETE_ALL_THRESHOLD) {
        deleteAllTriggered = true;
        haltTriggered      = true;
        buttonPressCount   = 0;
        deleteAllRegisters();
    }
    if (buttonPressCount > 0 && (now - lastButtonRelease > DOUBLE_CLICK_THRESHOLD)) {
        if (buttonPressCount == 1) {
            if (!registers.empty() && !isHalted) playRegister(activeRegister);
        } else {
            if (!registers.empty()) {
                activeRegister = (activeRegister + 1) % registers.size();
                saveActiveRegister();
                blinkLED(activeRegister + 1, LED_COLOR_REG_CHANGE);
            }
        }
        buttonPressCount = 0;
    }
}
// ---- Loop playback ----
static void processLoop() {
    if (!isLooping || isHalted || loopingRegister < 0 || (size_t)loopingRegister >= registers.size()) return;
    unsigned long now = millis();
    if (loopDuration > 0 && (now - loopStartTime >= loopDuration)) {
        isLooping = false; loopingRegister = -1; return;
    }
    if (requestInProgress) return;
    playRegister(loopingRegister);
    delay(100);
    feedWatchdog();
}
// ---- Setup ----
void setup() {
    delay(1000);
    initWatchdog();
    randomSeed(esp_random());
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
    FastLED.clear();
    FastLED.show();
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    loadRegisters();
    loadBtSettings();
    loadWiFiSettings();
    loadApiKeySettings();
    loadUtcOffsetSettings();
    loadSinkSettings();
    loadTimingSettings();
    loadLEDSettings();
    loadCsSecuritySettings();
    loadCsStorageLocation();
    loadBootRegSettings();
    loadTimerProxSettings();
    loadHostnameSettings();
    loadMTLSSettings();
    loadKeymapSettings();
#ifdef BOARD_M5STACK_ATOMS3
    loadUSBSettings();
    loadUSBIdentitySettings();
#endif
    if (ledEnabled) {
        for (int i = 0; i < 3; i++) { setLED(LED_COLOR_BOOT, 200); delay(200); feedWatchdog(); }
    }
    if (!SPIFFS.begin(true)) { /* SPIFFS mount failed — web UI unavailable */ }
    feedWatchdog();
    keymapInit();
    if (bluetoothEnabled) { Keyboard.begin(); Mouse.begin(); bluetoothInitialized = true; }
#ifdef BOARD_M5STACK_ATOMS3
    if (usbEnabled) {
        USB.manufacturerName(usbManufacturer.c_str());
        USB.productName(usbProduct.c_str());
        USB.serialNumber(usbSerialNumber.c_str());
        KProxConsumer.preinit(usbConsumerEnabled, usbSystemEnabled, usbIntlKeyboardEnabled);
        if (fido2Enabled) FIDO2Device.begin();
        USB.begin();
        KProxConsumer.begin();
        if (usbKeyboardEnabled) { USBKeyboard.begin(); usbKeyboardReady = true; }
        if (usbMouseEnabled)    { USBMouse.begin();    usbMouseReady    = true; }
        usbInitialized = true;
    }
#endif
    delay(500);
    if (bluetoothEnabled) Keyboard.releaseAll();
#ifdef BOARD_M5STACK_ATOMS3
    if (usbEnabled && usbInitialized) { if (usbKeyboardReady) USBKeyboard.releaseAll(); if (KProxConsumer.isReady()) { KProxConsumer.sendConsumer(0,0); KProxConsumer.sendSystem(0); } }
#endif
    feedWatchdog();
    mouseBatch.accumulatedX = 0;
    mouseBatch.accumulatedY = 0;
    mouseBatch.lastUpdate   = 0;
    mouseBatch.hasMovement  = false;
    WiFi.setHostname(hostname);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    for (int attempts = 0; WiFi.status() != WL_CONNECTED && attempts < 30; attempts++) {
        delay(500);
        feedWatchdog();
        if (attempts == 15) { WiFi.disconnect(); delay(1000); WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str()); }
    }
    if (WiFi.status() == WL_CONNECTED) {
        if (ledEnabled) { setLED(LED_COLOR_WIFI_CONNECTED, 500); blinkLED(10, LED_COLOR_WIFI_CONNECTED, LED_COLOR_WIFI_CONNECTED_DUTY_CYCLE); }
        initNTP();
        if (udpEnabled) udp.begin(UDP_DISCOVERY_PORT);
    } else {
        if (ledEnabled) setLED(LED_COLOR_WIFI_ERROR, 500);
    }
    feedWatchdog();
    setupRoutes();
    // server always runs HTTP on port 80.
    // mTLS is enforced at the application layer: when mtlsEnabled, all API
    // endpoints require a valid client cert presented via the X-Client-Cert
    // header (base64 PEM), verified against the stored CA cert in handleMTLS.
    // For full transport-layer TLS, terminate with a reverse proxy (nginx/HAProxy)
    // in front of this device, or use the ESP32-HTTPS-Server library as a drop-in.
    server.begin();
    webSocket.begin();
    webSocket.onEvent(handleSendMouseWebSocket);
    if (registers.empty()) {
        addRegister("{LEFT}{SLEEP 1000}{RIGHT}{SLEEP 1000}{UP}{SLEEP 1000}{DOWN}{SLEEP 1000}{ENTER}");
        addRegister("{LOOP}{MOVEMOUSE {RAND -100 100} {RAND -100 100}}{SLEEP {RAND 1000 3000}}{ENDLOOP}");
        addRegister("Hello World{SLEEP 1000}{ENTER}Testing tokens: {RAND 1 100}{ENTER}{F1}{SLEEP 500}{ESC}");
        addRegister("{CHORD CTRL+A}{SLEEP 500}{CHORD CTRL+C}{SLEEP 500}{CHORD CTRL+V}");
        addRegister("{HID 0x02 0x04}{SLEEP 500}{HID 0x28}{SLEEP 500}{ASCII 65}{ASCII 66}{ASCII 67}");
        addRegister("{SETMOUSE 400 300}{MOUSECLICK}{SLEEP 500}{MOUSEDOUBLECLICK}{SLEEP 500}{MOUSEPRESS 1}{SLEEP 1000}{MOUSERELEASE 1}");
    }
    if (ledEnabled && !registers.empty()) {
        delay(1000);
        blinkLED(activeRegister + 1, LED_COLOR_REG_CHANGE);
    }
    // Fire boot register if enabled and within limit
    if (bootRegEnabled && !registers.empty() &&
        bootRegIndex >= 0 && bootRegIndex < (int)registers.size()) {
        bool shouldFire = (bootRegLimit == 0) || (bootRegFiredCount < bootRegLimit);
        if (shouldFire) {
            bootRegFiredCount++;
            if (bootRegLimit > 0 && bootRegFiredCount >= bootRegLimit) {
                bootRegEnabled = false;
            }
            saveBootRegSettings();
            delay(500);
            playRegister(bootRegIndex);
        }
    }
    feedWatchdog();
}
// ---- Loop ----
void loop() {
    feedWatchdog();
    server.handleClient();
    webSocket.loop();
    if (mtlsEnabled) serverHTTP.handleClient();
    MDNS_UPDATE();
    if (udpEnabled && WiFi.status() == WL_CONNECTED && millis() - lastUdpBroadcast > UDP_BROADCAST_INTERVAL) {
        broadcastDiscovery();
        lastUdpBroadcast = millis();
    }
    static bool          mdnsSetupAttempted = false;
    static unsigned long wifiConnectedTime  = 0;
    if (WiFi.status() == WL_CONNECTED && !mdnsEnabled && !mdnsSetupAttempted) {
        if (!wifiConnectedTime) wifiConnectedTime = millis();
        else if (millis() - wifiConnectedTime > 10000) { mdnsSetupAttempted = true; setupMDNS(); }
    } else if (WiFi.status() != WL_CONNECTED) {
        wifiConnectedTime = 0;
    }
    checkButton();
    if (!isHalted) processLoop();
    if (!isHalted && !pendingTokenStrings.empty()) {
        // Convert any non-SCHED entries to SCHED entries immediately so all
        // scheduled events are registered regardless of queue position.
        for (int i = (int)pendingTokenStrings.size() - 1; i >= 0; i--) {
            if (!pendingTokenStrings[i].startsWith("SCHED|")) {
                String tok = pendingTokenStrings[i];
                pendingTokenStrings.erase(pendingTokenStrings.begin() + i);
                putTokenString(tok);
            }
        }
        // Scan all SCHED entries and fire any whose time has arrived.
        time_t now = time(nullptr);
        if (now > 100000) {
            struct tm* t = localtime(&now);
            for (int i = (int)pendingTokenStrings.size() - 1; i >= 0; i--) {
                String& tok = pendingTokenStrings[i];
                if (!tok.startsWith("SCHED|")) continue;
                int p1 = tok.indexOf('|', 6);
                int p2 = tok.indexOf('|', p1 + 1);
                int p3 = tok.indexOf('|', p2 + 1);
                int targetH = tok.substring(6,      p1).toInt();
                int targetM = tok.substring(p1 + 1, p2).toInt();
                int targetS = tok.substring(p2 + 1, p3).toInt();
                int nowMins = t->tm_hour * 60 + t->tm_min;
                int tgtMins = targetH    * 60 + targetM;
                if (nowMins == tgtMins && t->tm_sec >= targetS) {
                    String remainder = tok.substring(p3 + 1);
                    pendingTokenStrings.erase(pendingTokenStrings.begin() + i);
                    if (remainder.length() > 0) putTokenString(remainder);
                }
            }
        }
    }
    cleanupConnections();
    if (!isHalted && mouseBatch.hasMovement && millis() - mouseBatch.lastUpdate > MOUSE_BATCH_TIMEOUT) {
        sendBatchedMouseMovement();
    }
    static unsigned long lastPeriodicCleanup = 0;
    if (millis() - lastPeriodicCleanup > 60000) {
        if (!requestInProgress && !isHalted) { hidReleaseAll(); delay(KEY_RELEASE_DELAY); }
        lastPeriodicCleanup = millis();
        feedWatchdog();
    }
    if (ESP.getFreeHeap() < MIN_HEAP_FREE) {
        blinkLED(5, LED_COLOR_MEMORY_WDT, LED_MEMORY_WDT_DUTY_CYCLE);
        delay(1000);
        ESP.restart();
    }
    delay(10);
}
#endif // BOARD_M5STACK_CARDPUTER
#ifdef BOARD_M5STACK_CARDPUTER
#include "globals.h"
#include "led.h"
#include "hid.h"
#include "storage.h"
#include "registers.h"
#include "connection.h"
#include "token_parser.h"
#include "api.h"
#include "ws_mouse.h"
#include "mtls.h"
#include "keymap.h"
#include "credential_store.h"
#include "sd_card.h"
#include "scheduled_tasks.h"
#include "totp.h"
#include "storage.h"
#include "cardputer/ui_manager.h"
#include "cardputer/app_launcher.h"
#include "cardputer/app_kprox.h"
#include "cardputer/app_keyboard_hid.h"
#include "cardputer/app_clock.h"
#include "cardputer/app_settings.h"
#include "cardputer/app_qrprox.h"
#include "cardputer/app_schedprox.h"
#include "cardputer/app_credstore.h"
#include "cardputer/app_gadgets.h"
#include "cardputer/app_regedit.h"
#include "cardputer/app_fuzzyprox.h"
#include "cardputer/app_sinkprox.h"
#include "cardputer/app_totprox.h"
#include "cardputer/app_filebrowser.h"
#include "cardputer/app_kpsrunner.h"
#include "cardputer/app_bootprox.h"
#include "cardputer/app_timerprox.h"
#include "cardputer/app_combatprox.h"
#include "cardputer/app_mediacontrol.h"
#include "cardputer/app_nostrprox.h"
#include "cardputer/app_ircprox.h"
#include "cardputer/app_kproxchat.h"
#include "debug_hid.h"
#include <M5Cardputer.h>
#include "nvs_flash.h"
#include "nvs.h"
#ifdef BOARD_HAS_USB_HID
static void usbPreInit() __attribute__((constructor(110)));
static void usbPreInit() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    char mfg[64]  = DEFAULT_MANUFACTURER;
    char prod[64] = DEFAULT_PRODUCT_NAME;
    uint8_t  usbEn = 1;
    uint16_t vid   = DEFAULT_USB_VID;
    uint16_t pid   = DEFAULT_USB_PID;
    nvs_handle_t h;
    if (nvs_open("kprox", NVS_READONLY, &h) == ESP_OK) {
        size_t len;
        nvs_get_u8(h, "usbEnabled", &usbEn);
        len = sizeof(mfg);  nvs_get_str(h, "usbMfg",    mfg,  &len);
        len = sizeof(prod); nvs_get_str(h, "usbProduct", prod, &len);
        nvs_get_u16(h, "usbVid", &vid);
        nvs_get_u16(h, "usbPid", &pid);
        nvs_close(h);
    }
    if (usbEn) {
        USB.manufacturerName(mfg);
        USB.productName(prod);
        USB.serialNumber(USB_SERIAL_NUMBER);
    }
    usbVidOverride = vid;
    usbPidOverride = pid;
}
#endif
// ---- mDNS version compatibility ----
class MDNSHelper {
public:
    static void safeUpdate() {
#ifdef ESP_ARDUINO_VERSION_MAJOR
#  if ESP_ARDUINO_VERSION_MAJOR < 2
        MDNS.update();
#  endif
#else
#  ifdef ARDUINO_ESP32_DEV
        MDNS.update();
#  endif
#endif
    }
};
#define MDNS_UPDATE() MDNSHelper::safeUpdate()
// ---- Global definitions ----
WebServer        server(80);
WebServer        serverHTTP(443);
WiFiUDP          udp;
Preferences      preferences;
CRGB             leds[NUM_LEDS];
// Storage in BSS; constructed via placement new in setup() after settings load
// so the BT device name / battery level reflect persisted values on every boot.
alignas(BleComboKeyboard) static uint8_t _keyboardBuf[sizeof(BleComboKeyboard)];
alignas(BleComboMouse)    static uint8_t _mouseBuf[sizeof(BleComboMouse)];
BleComboKeyboard* Keyboard = nullptr;
BleComboMouse*    Mouse    = nullptr;
USBHIDKeyboard USBKeyboard;
KProxConsumerHID KProxConsumer;  // after USBKeyboard: keyboard takes TinyUSB slot 0
USBHIDMouse    USBMouse;
bool usbEnabled         = true;
bool usbInitialized     = false;
#ifdef BOARD_HAS_USB_HID
uint16_t usbVidOverride = DEFAULT_USB_VID;
uint16_t usbPidOverride = DEFAULT_USB_PID;
#endif
bool usbKeyboardReady   = false;
bool usbMouseReady         = false;
bool usbKeyboardEnabled    = true;
bool usbMouseEnabled       = true;
bool usbIntlKeyboardEnabled = true;
bool usbConsumerEnabled     = true;
bool usbSystemEnabled       = true;
bool fido2Enabled          = false;
bool bleKeyboardEnabled    = true;
bool bleMouseEnabled       = true;
bool bleIntlKeyboardEnabled = true;
bool bleConsumerEnabled     = true;
bool bleSystemEnabled       = true;
// maxSinkSize defined in globals.cpp
String wifiSSID      = DEFAULT_WIFI_SSID;
String wifiPassword  = DEFAULT_WIFI_PASSWORD;
String apiKey        = DEFAULT_API_KEY;
String usbManufacturer = DEFAULT_MANUFACTURER;
String usbProduct    = DEFAULT_PRODUCT_NAME;
bool wifiEnabled     = true;
const char* hostname   = HOSTNAME;
const char* deviceName = DEFAULT_PRODUCT_NAME;
bool bluetoothEnabled     = true;
bool bluetoothInitialized = false;
bool mdnsEnabled          = false;
bool isLooping            = false;
bool isHalted             = false;
bool requestInProgress    = false;
bool udpEnabled           = true;
bool ledEnabled           = true;
bool registersLoaded      = false;
unsigned long loopDuration    = 0;
unsigned long loopStartTime   = 0;
unsigned long lastUdpBroadcast = 0;
unsigned long lastWifiCheck   = 0;
unsigned long lastStatusPrint = 0;
int loopingRegister = -1;
long                utcOffsetSeconds = 0;
int activeRegister  = 0;
int currentMouseX   = 0;
int currentMouseY   = 0;
uint8_t ledColorR = 0;
uint8_t ledColorG = 255;
uint8_t ledColorB = 0;
std::vector<String> registers;
std::vector<String> registerNames;
MouseBatch mouseBatch;
// ---- Watchdog ----
void initWatchdog() {
    esp_task_wdt_deinit();
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    esp_task_wdt_config_t cfg = {
        .timeout_ms     = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic  = true
    };
    esp_task_wdt_init(&cfg);
#else
    esp_task_wdt_init(WDT_TIMEOUT, true);
#endif
    esp_task_wdt_add(NULL);
}
void feedWatchdog() {
    esp_task_wdt_reset();
}
// ---- Splash screen ----
static void showSplash() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TFT_BLACK);
    // Large "KProx" text, vertically and horizontally centred
    disp.setTextDatum(MC_DATUM);
    disp.setTextColor(disp.color565(50, 140, 255), TFT_BLACK);
    disp.setTextSize(4);
    disp.drawString("KProx", disp.width() / 2, disp.height() / 2 - 8);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(80, 80, 80), TFT_BLACK);
    disp.drawString("HID Automation", disp.width() / 2, disp.height() / 2 + 22);
    disp.setTextDatum(TL_DATUM);
    delay(500);
}
// ---- Setup ----
void usbBiosCompatPatchDescriptors(void);  // defined in usb_bios_compat.cpp
void setup() {
    // Device on bus at ~400ms — before M5Cardputer.begin() (~736ms).
    // loadUSBSettings() reads NVS consumer/system/intl flags before USB.begin()
    // so preinit() registers only the requested HID interfaces in the descriptor.
    loadUSBSettings();
    KProxConsumer.preinit(usbConsumerEnabled, usbSystemEnabled, usbIntlKeyboardEnabled);
    USB.begin();
    usbBiosCompatPatchDescriptors();  // patch bDeviceClass=0, SubClass=1, Protocol=1
    USBKeyboard.begin();
    KProxConsumer.begin();
    usbKeyboardReady = true;
    usbInitialized   = true;
    debugHidInit();
    DHID_HEAP("BOOT");
    DHID("SETUP", "M5 begin");
    M5Cardputer.begin(true);
    DHID_HEAP("M5");
    DHID("SETUP", "sdInit");
    sdInit();
    DHID_HEAP("SD");
    M5Cardputer.Speaker.setVolume(160);
    auto& disp = M5Cardputer.Display;
    disp.setBrightness(g_displayBrightness);
    DHID("SETUP", "watchdog+rng");
    initWatchdog();
    randomSeed(esp_random());
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(80);
    FastLED.clear();
    FastLED.show();
    DHID("SETUP", "load settings");
    loadRegisters();
    loadBtSettings();
    loadWiFiSettings();
    loadWifiEnabledSettings();
    loadApiKeySettings();
    loadUtcOffsetSettings();
    loadSinkSettings();
    loadTimingSettings();
    loadLEDSettings();
    loadCsSecuritySettings();
    loadCsStorageLocation();
    loadBootRegSettings();
    loadTimerProxSettings();
    loadDisplaySettings();
    disp.setBrightness((uint8_t)g_displayBrightness);
    loadHostnameSettings();
    loadDefaultAppSettings();
    loadMTLSSettings();
    loadKeymapSettings();
    loadUSBSettings();
    loadUSBIdentitySettings();
    DHID_HEAP("CFG");
    if (ledEnabled) {
        for (int i = 0; i < 3; i++) { setLED(LED_COLOR_BOOT, 200); delay(200); feedWatchdog(); }
    }
    // Start WiFi immediately after settings load so it connects in the background
    // while BLE init, USB async tasks, and SPIFFS provisioning run in parallel.
    // Previously WiFi started at ~18s; now it starts at ~2s, saving ~16s.
    if (wifiEnabled) {
        WiFi.setHostname(hostname);
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        DHID("WIFI", "begin (early)");
    } else {
        WiFi.mode(WIFI_OFF);
    }
    DHID("SETUP", "splash");
    if (!bootRegEnabled) showSplash();
    DHID("SETUP", "SPIFFS");
    if (!SPIFFS.begin(true)) { DHID("SPIFFS", "FAILED"); }
    feedWatchdog();
    DHID("SETUP", "keymapInit");
    keymapInit();
    DHID("SETUP", "credStoreInit");
    credStoreInit();
    DHID("SETUP", "totpInit");
    totpInit();
    DHID("SETUP", "loadScheduled");
    loadScheduledTasks();
    DHID_HEAP("PRE-BLE");
    // Construct BLE objects here — after settings are loaded — so the device
    // name and manufacturer are the persisted values, not compile-time defaults.
    // Battery level is read live on Cardputer; other boards report 100.
    DHID("BLE", "new keyboard");
    uint8_t batLevel = (uint8_t)constrain(M5Cardputer.Power.getBatteryLevel(), 0, 100);
    Keyboard = new(_keyboardBuf) BleComboKeyboard(usbProduct.c_str(), usbManufacturer.c_str(), batLevel);
    DHID("BLE", "new mouse");
    Mouse    = new(_mouseBuf) BleComboMouse(Keyboard);
    DHID_HEAP("BLE-OBJ");
    if (bluetoothEnabled) {
        DHID("BLE", "keyboard begin");
        Keyboard->begin();
        DHID("BLE", "mouse begin");
        Mouse->begin();
        bluetoothInitialized = true;
        DHID_HEAP("BLE-INIT");
    }
    if (usbEnabled) {
        // USB.begin()/USBKeyboard.begin()/KProxConsumer.begin() already called
        // at the top of setup() for BIOS/GRUB timing. Apply per-device enables
        // now that settings are loaded.
        usbKeyboardReady = usbKeyboardEnabled;
        if (usbMouseEnabled) { USBMouse.begin(); usbMouseReady = true; }
        if (usbKeyboardReady) debugHidFlush();
        DHID_HEAP("USB-DONE");
    } else {
        usbKeyboardReady = false;
        usbInitialized   = false;
    }
    if (bluetoothEnabled) BLE_KEYBOARD.releaseAll();
    if (usbEnabled && usbInitialized) { if (usbKeyboardReady) USBKeyboard.releaseAll(); if (KProxConsumer.isReady()) { KProxConsumer.sendConsumer(0,0); KProxConsumer.sendSystem(0); } }
    feedWatchdog();
    // Fire boot register — HID ready, splash already shown
    if (bootRegEnabled && !registers.empty() &&
        bootRegIndex >= 0 && bootRegIndex < (int)registers.size()) {
        bool shouldFire = (bootRegLimit == 0) || (bootRegFiredCount < bootRegLimit);
        if (shouldFire) {
            bootRegFiredCount++;
            if (bootRegLimit > 0 && bootRegFiredCount >= bootRegLimit)
                bootRegEnabled = false;
            saveBootRegSettings();
            // Draw boot summary screen
            {
                auto& disp = M5Cardputer.Display;
                disp.fillScreen(TFT_BLACK);
                uint16_t hdrBg = disp.color565(0, 60, 120);
                disp.fillRect(0, 0, disp.width(), 16, hdrBg);
                disp.setTextDatum(TL_DATUM);
                disp.setTextSize(1);
                disp.setTextColor(TFT_WHITE, hdrBg);
                disp.drawString("BootProx", 4, 3);
                String cntStr = bootRegLimit > 0
                    ? String(bootRegFiredCount) + "/" + String(bootRegLimit)
                    : "Fire " + String(bootRegFiredCount);
                int cw = disp.textWidth(cntStr);
                disp.setTextColor(disp.color565(180, 255, 180), hdrBg);
                disp.drawString(cntStr, disp.width() - cw - 4, 3);
                int y = 22;
                disp.setTextColor(disp.color565(100, 160, 220), TFT_BLACK);
                String regLabel = "Reg " + String(bootRegIndex + 1);
                if (bootRegIndex < (int)registerNames.size() && !registerNames[bootRegIndex].isEmpty())
                    regLabel += "  " + registerNames[bootRegIndex];
                if ((int)regLabel.length() > 26) regLabel = regLabel.substring(0, 24) + "..";
                disp.drawString(regLabel, 4, y); y += 14;
                disp.setTextColor(disp.color565(160, 160, 160), TFT_BLACK);
                String preview = registers[bootRegIndex];
                if ((int)preview.length() > 34) preview = preview.substring(0, 31) + "...";
                disp.drawString(preview, 4, y);
                int botY = disp.height() - 13;
                disp.fillRect(0, botY, disp.width(), 13, disp.color565(16, 16, 16));
                disp.setTextColor(disp.color565(110, 110, 110), disp.color565(16, 16, 16));
                disp.drawString("BtnG0 or ESC to cancel", 2, botY + 2);
            }
            playRegister(bootRegIndex);
            M5Cardputer.Display.fillScreen(TFT_BLACK);
        }
    }
    mouseBatch.accumulatedX = 0;
    mouseBatch.accumulatedY = 0;
    mouseBatch.lastUpdate   = 0;
    mouseBatch.hasMovement  = false;
    if (wifiEnabled && udpEnabled) {
        udp.begin(UDP_DISCOVERY_PORT);
    }
    if (registers.empty()) {
        addRegister("{LEFT}{SLEEP 1000}{RIGHT}{SLEEP 1000}{UP}{SLEEP 1000}{DOWN}{SLEEP 1000}{ENTER}");
        addRegister("{LOOP}{MOVEMOUSE {RAND -100 100} {RAND -100 100}}{SLEEP {RAND 1000 3000}}{ENDLOOP}");
        addRegister("Hello World{SLEEP 1000}{ENTER}Testing tokens: {RAND 1 100}{ENTER}{F1}{SLEEP 500}{ESC}");
        addRegister("{CHORD CTRL+A}{SLEEP 500}{CHORD CTRL+C}{SLEEP 500}{CHORD CTRL+V}");
    }
    if (wifiEnabled) {
        DHID("WIFI", "setupRoutes");
        setupRoutes();
        // server.begin() / webSocket.begin() are deferred to loop() once
        // WiFi reports WL_CONNECTED, keeping setup() non-blocking.
    }
    feedWatchdog();
    if (ledEnabled && !registers.empty()) {
        blinkLED(activeRegister + 1, LED_COLOR_REG_CHANGE);
    }
    feedWatchdog();
    DHID_HEAP("PRE-APPS");
    // Register apps: Launcher first, then real apps in display order.
    // Reserve the exact count upfront — eliminates 5-6 realloc cycles that
    // would otherwise fragment the heap during registration.
    Cardputer::uiManager.reserveApps(22);
    static Cardputer::AppLauncher    launcher;
    static Cardputer::AppKProx       appKProx;
    static Cardputer::AppFuzzyProx   appFuzzyProx;
    static Cardputer::AppRegEdit     appRegEdit;
    static Cardputer::AppCredStore   appCredStore;
    static Cardputer::AppGadgets     appGadgets;
    static Cardputer::AppSinkProx    appSinkProx;
    static Cardputer::AppKeyboardHID appKeyboard;
    static Cardputer::AppClock       appClock;
    static Cardputer::AppQRProx      appQRProx;
    static Cardputer::AppSchedProx   appSchedProx;
    static Cardputer::AppTOTProx     appTOTProx;
    static Cardputer::AppFileBrowser appFileBrowser;
    static Cardputer::AppKPSRunner   appKPSRunner;
    static Cardputer::AppBootProx    appBootProx;
    static Cardputer::AppTimerProx   appTimerProx;
    static Cardputer::AppCombatProx  appCombatProx;
    static Cardputer::AppMediaControl appMediaControl;
    static Cardputer::AppNostrProx   appNostrProx;
    static Cardputer::AppIRCProx     appIRCProx;
    static Cardputer::AppKProxChat   appKProxChat;
    static Cardputer::AppSettings    appSettings;
    DHID_HEAP("APPS-CTORS");
    // Registration order determines launcher icon index (0 = launcher, 1..N = user apps)
    Cardputer::uiManager.addApp(&launcher);    // 0
    Cardputer::uiManager.addApp(&appKProx);    // 1
    Cardputer::uiManager.addApp(&appFuzzyProx);// 2
    Cardputer::uiManager.addApp(&appRegEdit);  // 3
    Cardputer::uiManager.addApp(&appCredStore);// 4
    Cardputer::uiManager.addApp(&appGadgets);  // 5
    Cardputer::uiManager.addApp(&appSinkProx); // 6
    Cardputer::uiManager.addApp(&appKeyboard); // 7
    Cardputer::uiManager.addApp(&appClock);    // 8
    Cardputer::uiManager.addApp(&appQRProx);   // 9
    Cardputer::uiManager.addApp(&appSchedProx);// 10
    Cardputer::uiManager.addApp(&appTOTProx);      // 11
    Cardputer::uiManager.addApp(&appFileBrowser);  // 12
    Cardputer::uiManager.addApp(&appKPSRunner);    // 13
    Cardputer::uiManager.addApp(&appBootProx);     // 14
    Cardputer::uiManager.addApp(&appTimerProx);    // 15
    Cardputer::uiManager.addApp(&appCombatProx);   // 16
    Cardputer::uiManager.addApp(&appMediaControl); // 17
    Cardputer::uiManager.addApp(&appNostrProx);    // 18 — hidden by default
    Cardputer::uiManager.addApp(&appIRCProx);      // 19 — hidden by default
    Cardputer::uiManager.addApp(&appKProxChat);    // 20
    Cardputer::uiManager.addApp(&appSettings); // 21
    // Load persisted app order/visibility
    loadAppLayout((int)Cardputer::uiManager.apps().size() - 1);
    // On first flash (no saved layout), hide NostrProx (18) and IRCProx (19) by default.
    // loadAppLayout appends new apps at the end of appOrder as visible; we override here
    // if these are freshly added (user can always unhide via Settings).
    {
        preferences.begin("kprox", true);
        bool hasLayout = preferences.isKey("appOrder");
        preferences.end();
        if (!hasLayout) {
            // Mark indices 18 and 19 hidden in appOrder
            for (size_t i = 0; i < appOrder.size(); i++) {
                if (appOrder[i] == 18 || appOrder[i] == 19)
                    appHidden[i] = true;
            }
        }
    }
    int numApps = (int)Cardputer::uiManager.apps().size();
    // On first flash the "defaultApp" NVS key has never been written.
    // Default to QRProx (index 9) so the user immediately sees the web URL.
    {
        preferences.begin("kprox", true);
        bool hasDefault = preferences.isKey("defaultApp");
        preferences.end();
        if (!hasDefault) {
            defaultAppIndex = 9;  // QRProx
            saveDefaultAppSettings();
        }
    }
    int startApp = (defaultAppIndex >= 1 && defaultAppIndex < numApps) ? defaultAppIndex : 1;
    DHID_HEAP("PRE-LAUNCH");
    Cardputer::uiManager.launchApp(startApp);
    Cardputer::uiManager.notifyInteraction();
    DHID_HEAP("SETUP-END");
}
// ---- Loop ----
void loop() {
    feedWatchdog();
    static bool serverStarted = false;
    static bool loopLogged    = false;
    if (!loopLogged) { loopLogged = true; DHID_HEAP("LOOP-1st"); }
    if (wifiEnabled) {
        if (serverStarted) {
            server.handleClient();
            webSocket.loop();
        }
        if (mtlsEnabled) serverHTTP.handleClient();
        MDNS_UPDATE();
    }
    if (wifiEnabled && udpEnabled && WiFi.status() == WL_CONNECTED && millis() - lastUdpBroadcast > UDP_BROADCAST_INTERVAL) {
        broadcastDiscovery();
        lastUdpBroadcast = millis();
    }
    static bool          mdnsSetupAttempted = false;
    static bool          ntpSyncAttempted   = false;
    static unsigned long wifiConnectedTime  = 0;
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiConnectedTime) wifiConnectedTime = millis();
        if (!serverStarted) {
            serverStarted = true;
            DHID("WIFI", "connected - starting servers");
            DHID_HEAP("PRE-SRV");
            server.begin();
            webSocket.begin();
            webSocket.onEvent(handleSendMouseWebSocket);
            DHID_HEAP("SRV-UP");
        }
        // NTP: configTime() is cheap (~200B); run whenever heap > 6KB.
        if (!ntpSyncAttempted && millis() - wifiConnectedTime > 2000) {
            ntpSyncAttempted = true;
            if (ESP.getFreeHeap() > 6000) initNTP();
            else DHID("NTP", "skipped - low heap");
        }
        // MDNS: MDNS.begin() allocates a 4KB FreeRTOS task + sockets + queues = ~10KB.
        // Only start it if there is comfortable headroom above that cost.
        if (!mdnsEnabled && !mdnsSetupAttempted && millis() - wifiConnectedTime > 10000) {
            mdnsSetupAttempted = true;
            if (ESP.getFreeHeap() > 25000) setupMDNS();
            else DHID("MDNS", "skipped - low heap");
        }
    } else if (WiFi.status() != WL_CONNECTED) {
        wifiConnectedTime  = 0;
        ntpSyncAttempted   = false;  // reset so it retries on next connection
    }
    if (!isHalted) {
        // Loop playback
        if (isLooping && loopingRegister >= 0 && (size_t)loopingRegister < registers.size()) {
            unsigned long now = millis();
            if (loopDuration == 0 || (now - loopStartTime < loopDuration)) {
                if (!requestInProgress) {
                    playRegister(loopingRegister);
                    delay(100);
                    feedWatchdog();
                }
            } else {
                isLooping = false;
                loopingRegister = -1;
            }
        }
    }
    checkScheduledTasks();
    if (!pendingTokenStrings.empty()) {
        credStoreLastActivity = millis();  // any HID output resets CS inactivity timer
        for (int i = (int)pendingTokenStrings.size() - 1; i >= 0; i--) {
            if (!pendingTokenStrings[i].startsWith("SCHED|")) {
                String tok = pendingTokenStrings[i];
                pendingTokenStrings.erase(pendingTokenStrings.begin() + i);
                isHalted = false;
                g_parserAbort = false;
                putTokenString(tok);
            }
        }
        time_t now = time(nullptr);
        if (!isHalted && now > 100000) {
            struct tm* t = localtime(&now);
            for (int i = (int)pendingTokenStrings.size() - 1; i >= 0; i--) {
                String& tok = pendingTokenStrings[i];
                if (!tok.startsWith("SCHED|")) continue;
                int p1 = tok.indexOf('|', 6);
                int p2 = tok.indexOf('|', p1 + 1);
                int p3 = tok.indexOf('|', p2 + 1);
                int targetH = tok.substring(6,      p1).toInt();
                int targetM = tok.substring(p1 + 1, p2).toInt();
                int targetS = tok.substring(p2 + 1, p3).toInt();
                int nowMins = t->tm_hour * 60 + t->tm_min;
                int tgtMins = targetH    * 60 + targetM;
                if (nowMins == tgtMins && t->tm_sec >= targetS) {
                    String remainder = tok.substring(p3 + 1);
                    pendingTokenStrings.erase(pendingTokenStrings.begin() + i);
                    if (remainder.length() > 0) putTokenString(remainder);
                }
            }
        }
    }
    cleanupConnections();
    if (!isHalted && mouseBatch.hasMovement && millis() - mouseBatch.lastUpdate > MOUSE_BATCH_TIMEOUT) {
        sendBatchedMouseMovement();
    }
    static unsigned long lastPeriodicCleanup = 0;
    if (millis() - lastPeriodicCleanup > 60000) {
        if (!requestInProgress && !isHalted) { hidReleaseAll(); delay(KEY_RELEASE_DELAY); }
        lastPeriodicCleanup = millis();
        feedWatchdog();
    }
    // Update BLE battery level every 30 seconds
    static unsigned long lastBatUpdate = 0;
    if (Keyboard && bluetoothInitialized && millis() - lastBatUpdate > 30000) {
        uint8_t bat = (uint8_t)constrain(M5Cardputer.Power.getBatteryLevel(), 0, 100);
        BLE_KEYBOARD.setBatteryLevel(bat);
        lastBatUpdate = millis();
    }
    if (ESP.getFreeHeap() < MIN_HEAP_FREE) {
        delay(1000);
        ESP.restart();
    }
    // Update UI
    Cardputer::uiManager.update();
    delay(10);
}
#endif // BOARD_M5STACK_CARDPUTER
