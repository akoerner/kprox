#ifndef BOARD_M5STACK_CARDPUTER
#include "globals.h"
#include "led.h"
#include "hid.h"
#include "storage.h"
#include "registers.h"
#include "connection.h"
#include "token_parser.h"
#include "api.h"
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
bool usbKeyboardEnabled = true;
bool usbMouseEnabled    = true;
bool fido2Enabled       = false;
#endif

bool bleKeyboardEnabled = true;
bool bleMouseEnabled    = true;

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
std::vector<String> pendingTokenStrings;
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
        if (fido2Enabled) FIDO2Device.begin();
        USB.begin();
        KProxConsumer.begin();  // creates send semaphore; addDevice() ran in constructor
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

    feedWatchdog();
}

// ---- Loop ----

void loop() {
    feedWatchdog();
    server.handleClient();
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

    if (ESP.getFreeHeap() < 5000) {
        blinkLED(5, LED_COLOR_MEMORY_WDT, LED_MEMORY_WDT_DUTY_CYCLE);
        delay(1000);
        ESP.restart();
    }

    delay(10);
}
#endif // BOARD_M5STACK_CARDPUTER
