#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "../led.h"
#include "../hid.h"
#include "../storage.h"
#include "../registers.h"
#include "../connection.h"
#include "../token_parser.h"
#include "../api.h"
#include "../mtls.h"
#include "../keymap.h"
#include "../credential_store.h"
#include "../scheduled_tasks.h"
#include "../totp.h"
#include "../storage.h"
#include "ui_manager.h"
#include "app_launcher.h"
#include "app_kprox.h"
#include "app_keyboard_hid.h"
#include "app_clock.h"
#include "app_settings.h"
#include "app_qrprox.h"
#include "app_schedprox.h"
#include "app_credstore.h"
#include "app_gadgets.h"
#include "app_regedit.h"
#include "app_fuzzyprox.h"
#include "app_sinkprox.h"
#include "app_totprox.h"
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
    uint8_t usbEn = 1;
    nvs_handle_t h;
    if (nvs_open("kprox", NVS_READONLY, &h) == ESP_OK) {
        size_t len;
        nvs_get_u8(h, "usbEnabled", &usbEn);
        len = sizeof(mfg);  nvs_get_str(h, "usbMfg",    mfg,  &len);
        len = sizeof(prod); nvs_get_str(h, "usbProduct", prod, &len);
        nvs_close(h);
    }
    if (usbEn) {
        USB.manufacturerName(mfg);
        USB.productName(prod);
        USB.serialNumber(USB_SERIAL_NUMBER);
    }
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

// Constructed in setup() after settings are loaded so the BT device name
// and battery level reflect persisted values on every boot.
BleComboKeyboard* Keyboard = nullptr;
BleComboMouse*    Mouse    = nullptr;

USBHIDKeyboard USBKeyboard;
USBHIDMouse    USBMouse;
bool usbEnabled         = true;
bool usbInitialized     = false;
bool usbKeyboardReady   = false;
bool usbMouseReady      = false;
bool usbKeyboardEnabled = true;
bool usbMouseEnabled    = true;
bool fido2Enabled       = false;

bool bleKeyboardEnabled = true;
bool bleMouseEnabled    = true;

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
    delay(2000);
}

// ---- Setup ----

void setup() {
    delay(1000);

    M5Cardputer.begin(true);

    auto& disp = M5Cardputer.Display;
    disp.setBrightness(128);

    initWatchdog();
    randomSeed(esp_random());

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(80);
    FastLED.clear();
    FastLED.show();

    loadRegisters();
    loadBtSettings();
    loadWiFiSettings();
    loadWifiEnabledSettings();
    loadApiKeySettings();
    loadUtcOffsetSettings();
    loadSinkSettings();
    loadTimingSettings();
    loadCsSecuritySettings();
    loadHostnameSettings();
    loadDefaultAppSettings();
    loadMTLSSettings();
    loadKeymapSettings();
    loadUSBSettings();
    loadUSBIdentitySettings();

    if (ledEnabled) {
        for (int i = 0; i < 3; i++) { setLED(LED_COLOR_BOOT, 200); delay(200); feedWatchdog(); }
    }

    showSplash();

    if (!SPIFFS.begin(true)) { /* SPIFFS mount failed */ }
    feedWatchdog();
    keymapInit();
    credStoreInit();
    totpInit();
    loadScheduledTasks();

    // Construct BLE objects here — after settings are loaded — so the device
    // name and manufacturer are the persisted values, not compile-time defaults.
    // Battery level is read live on Cardputer; other boards report 100.
    uint8_t batLevel = (uint8_t)constrain(M5Cardputer.Power.getBatteryLevel(), 0, 100);
    Keyboard = new BleComboKeyboard(usbProduct.c_str(), usbManufacturer.c_str(), batLevel);
    Mouse    = new BleComboMouse(Keyboard);

    if (bluetoothEnabled) {
        Keyboard->begin();
        Mouse->begin();
        bluetoothInitialized = true;
    }

    if (usbEnabled) {
        USB.begin();
        KProxConsumer.begin();  // creates send semaphore; addDevice() ran in constructor
        if (usbKeyboardEnabled) { USBKeyboard.begin(); usbKeyboardReady = true; }
        if (usbMouseEnabled)    { USBMouse.begin();    usbMouseReady    = true; }
        usbInitialized   = true;
    }

    delay(500);
    if (bluetoothEnabled) BLE_KEYBOARD.releaseAll();
    if (usbEnabled && usbInitialized) { if (usbKeyboardReady) USBKeyboard.releaseAll(); if (KProxConsumer.isReady()) { KProxConsumer.sendConsumer(0,0); KProxConsumer.sendSystem(0); } }
    feedWatchdog();

    mouseBatch.accumulatedX = 0;
    mouseBatch.accumulatedY = 0;
    mouseBatch.lastUpdate   = 0;
    mouseBatch.hasMovement  = false;

    WiFi.setHostname(hostname);

    if (wifiEnabled) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    // WiFi connection screen — styled to match the app theme
    const uint16_t WS_BG     = disp.color565(18,  18,  28);
    const uint16_t WS_BAR    = disp.color565(20,  60, 130);
    const uint16_t WS_LABEL  = disp.color565(130, 130, 130);
    const uint16_t WS_VALUE  = disp.color565(100, 200, 255);
    const uint16_t WS_DOT    = disp.color565(80,  140, 220);
    const uint16_t WS_WARN   = disp.color565(220, 160, 0);
    const uint16_t WS_OK     = disp.color565(80,  220, 80);
    const uint16_t WS_ERR    = disp.color565(220, 80,  80);

    disp.fillScreen(WS_BG);

    // Header bar
    disp.fillRect(0, 0, disp.width(), 18, WS_BAR);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, WS_BAR);
    disp.drawString("KProx", 4, 3);
    disp.setTextColor(disp.color565(160, 200, 255), WS_BAR);
    disp.drawString("Connecting...", disp.width() - disp.textWidth("Connecting...") - 4, 3);

    // SSID row
    int y = 24;
    disp.setTextColor(WS_LABEL, WS_BG);
    disp.drawString("SSID ", 4, y);
    disp.setTextColor(WS_VALUE, WS_BG);
    String ssidDisp = wifiSSID;
    if ((int)ssidDisp.length() > 26) ssidDisp = ssidDisp.substring(0,23) + "...";
    disp.drawString(ssidDisp, 4 + disp.textWidth("SSID "), y);
    y += 14;

    // Default password warning — show the actual password
    if (wifiPassword == DEFAULT_WIFI_PASSWORD) {
        disp.setTextColor(WS_WARN, WS_BG);
        disp.drawString("! Default pw: " + wifiPassword, 4, y);
        y += 14;
        disp.drawString("  Change in Settings app", 4, y);
        y += 14;
    }

    // Progress dots row
    y += 4;
    disp.setTextColor(WS_LABEL, WS_BG);
    disp.drawString("Connecting", 4, y);
    int dotsX = 4 + disp.textWidth("Connecting") + 4;
    int dotsY = y;

    for (int attempts = 0; WiFi.status() != WL_CONNECTED && attempts < 30; attempts++) {
        delay(500);
        feedWatchdog();
        // Draw a dot for each attempt, wrapping if needed
        int dx = dotsX + (attempts % 20) * 7;
        int dy = dotsY + (attempts / 20) * 14;
        disp.fillCircle(dx + 2, dy + 5, 2, WS_DOT);
        if (attempts == 15) {
            WiFi.disconnect();
            delay(500);
            WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        }
    }

    // Result row
    y = dotsY + 20;
    if (WiFi.status() == WL_CONNECTED) {
        if (ledEnabled) { setLED(LED_COLOR_WIFI_CONNECTED, 500); blinkLED(10, LED_COLOR_WIFI_CONNECTED, LED_COLOR_WIFI_CONNECTED_DUTY_CYCLE); }
        initNTP();
        if (udpEnabled) udp.begin(UDP_DISCOVERY_PORT);
        // IP row
        disp.setTextColor(WS_LABEL, WS_BG);
        disp.drawString("IP   ", 4, y);
        disp.setTextColor(WS_OK, WS_BG);
        disp.drawString(WiFi.localIP().toString(), 4 + disp.textWidth("IP   "), y);
        y += 14;
        disp.setTextColor(WS_OK, WS_BG);
        disp.drawString("Connected", 4, y);
    } else {
        if (ledEnabled) setLED(LED_COLOR_WIFI_ERROR, 500);
        disp.setTextColor(WS_ERR, WS_BG);
        disp.drawString("Connection failed", 4, y);
        y += 14;
        disp.setTextColor(WS_WARN, WS_BG);
        disp.drawString("Continuing without WiFi...", 4, y);
    }

    delay(800);
    feedWatchdog();
    } else {
        WiFi.mode(WIFI_OFF);
        // Brief "no WiFi" notice before proceeding to startup app
        const uint16_t NW_BG  = disp.color565(18, 18, 28);
        const uint16_t NW_BAR = disp.color565(80, 50, 10);
        disp.fillScreen(NW_BG);
        disp.fillRect(0, 0, disp.width(), 18, NW_BAR);
        disp.setTextSize(1);
        disp.setTextColor(TFT_WHITE, NW_BAR);
        disp.drawString("KProx", 4, 3);
        disp.setTextColor(disp.color565(220, 160, 0), NW_BG);
        disp.drawString("WiFi disabled.", 4, 26);
        disp.setTextColor(disp.color565(130, 130, 130), NW_BG);
        disp.drawString("Enable in Settings > WiFi.", 4, 42);
        disp.drawString("Web interface unavailable.", 4, 56);
        delay(1500);
        feedWatchdog();
    }

    if (registers.empty()) {
        addRegister("{LEFT}{SLEEP 1000}{RIGHT}{SLEEP 1000}{UP}{SLEEP 1000}{DOWN}{SLEEP 1000}{ENTER}");
        addRegister("{LOOP}{MOVEMOUSE {RAND -100 100} {RAND -100 100}}{SLEEP {RAND 1000 3000}}{ENDLOOP}");
        addRegister("Hello World{SLEEP 1000}{ENTER}Testing tokens: {RAND 1 100}{ENTER}{F1}{SLEEP 500}{ESC}");
        addRegister("{CHORD CTRL+A}{SLEEP 500}{CHORD CTRL+C}{SLEEP 500}{CHORD CTRL+V}");
    }

    if (wifiEnabled) {
        setupRoutes();
        server.begin();
    }

    feedWatchdog();

    if (ledEnabled && !registers.empty()) {
        delay(1000);
        blinkLED(activeRegister + 1, LED_COLOR_REG_CHANGE);
    }

    feedWatchdog();

    // Register apps: Launcher first, then real apps in display order
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
    static Cardputer::AppSettings    appSettings;

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
    Cardputer::uiManager.addApp(&appTOTProx);  // 11
    Cardputer::uiManager.addApp(&appSettings); // 12

    // Load persisted app order/visibility (12 user apps, indices 1..12)
    loadAppLayout(12);

    int numApps = (int)Cardputer::uiManager.apps().size();
    int startApp = (defaultAppIndex >= 1 && defaultAppIndex < numApps) ? defaultAppIndex : 1;
    Cardputer::uiManager.launchApp(startApp);
    Cardputer::uiManager.notifyInteraction();
}

// ---- Loop ----

void loop() {
    feedWatchdog();
    if (wifiEnabled) {
        server.handleClient();
        if (mtlsEnabled) serverHTTP.handleClient();
        MDNS_UPDATE();
    }

    if (wifiEnabled && udpEnabled && WiFi.status() == WL_CONNECTED && millis() - lastUdpBroadcast > UDP_BROADCAST_INTERVAL) {
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

    if (!pendingTokenStrings.empty()) {
        credStoreLastActivity = millis();  // any HID output resets CS inactivity timer
        for (int i = (int)pendingTokenStrings.size() - 1; i >= 0; i--) {
            if (!pendingTokenStrings[i].startsWith("SCHED|")) {
                String tok = pendingTokenStrings[i];
                pendingTokenStrings.erase(pendingTokenStrings.begin() + i);
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

    checkScheduledTasks();

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

    if (ESP.getFreeHeap() < 8000) {
        delay(1000);
        ESP.restart();
    }

    // Update UI
    Cardputer::uiManager.update();

    delay(10);
}

#endif // BOARD_M5STACK_CARDPUTER
