#include "storage.h"
#include "led.h"
#include "mtls.h"
#include "keymap.h"
#include "keymap.h"
#include "credential_store.h"

void saveWiFiSettings() {
    preferences.begin("kprox", false);
    preferences.putString("wifiSSID",     wifiSSID);
    preferences.putString("wifiPassword", wifiPassword);
    preferences.end();
    blinkLED(3, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void loadWiFiSettings() {
    preferences.begin("kprox", false);
    wifiSSID     = preferences.getString("wifiSSID",     DEFAULT_WIFI_SSID);
    wifiPassword = preferences.getString("wifiPassword", DEFAULT_WIFI_PASSWORD);
    preferences.end();
    if (wifiSSID.isEmpty())     wifiSSID     = DEFAULT_WIFI_SSID;
    if (wifiPassword.isEmpty()) wifiPassword = DEFAULT_WIFI_PASSWORD;
}

void saveWifiEnabledSettings() {
    preferences.begin("kprox", false);
    preferences.putBool("wifiEnabled", wifiEnabled);
    preferences.end();
}

void loadWifiEnabledSettings() {
    preferences.begin("kprox", false);
    wifiEnabled = preferences.getBool("wifiEnabled", true);
    preferences.end();
}

void saveBtSettings() {
    preferences.begin("kprox", false);
    preferences.putBool("btEnabled",   bluetoothEnabled);
    preferences.putBool("bleKbEn",     bleKeyboardEnabled);
    preferences.putBool("bleMouseEn",  bleMouseEnabled);
    preferences.putBool("bleIntlKbEn", bleIntlKeyboardEnabled);
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void loadBtSettings() {
    preferences.begin("kprox", false);
    bluetoothEnabled       = preferences.getBool("btEnabled",   true);
    bleKeyboardEnabled     = preferences.getBool("bleKbEn",     true);
    bleMouseEnabled        = preferences.getBool("bleMouseEn",  true);
    bleIntlKeyboardEnabled = preferences.getBool("bleIntlKbEn", true);
    preferences.end();
}

#ifdef BOARD_HAS_USB_HID
void saveUSBSettings() {
    preferences.begin("kprox", false);
    preferences.putBool("usbEnabled",    usbEnabled);
    preferences.putBool("usbKbEn",       usbKeyboardEnabled);
    preferences.putBool("usbMouseEn",    usbMouseEnabled);
    preferences.putBool("usbIntlKbEn",   usbIntlKeyboardEnabled);
    preferences.putBool("fido2En",       fido2Enabled);
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void loadUSBSettings() {
    preferences.begin("kprox", false);
    usbEnabled             = preferences.getBool("usbEnabled",  true);
    usbKeyboardEnabled     = preferences.getBool("usbKbEn",     true);
    usbMouseEnabled        = preferences.getBool("usbMouseEn",  true);
    usbIntlKeyboardEnabled = preferences.getBool("usbIntlKbEn", true);
    fido2Enabled           = preferences.getBool("fido2En",     false);
    preferences.end();
}
#else
void saveUSBSettings() {}
void loadUSBSettings() {}
#endif

void saveUSBIdentitySettings() {
    preferences.begin("kprox", false);
    preferences.putString("usbMfg",     usbManufacturer);
    preferences.putString("usbProduct", usbProduct);
#ifdef BOARD_HAS_USB_HID
    preferences.putUShort("usbVid", usbVidOverride);
    preferences.putUShort("usbPid", usbPidOverride);
#endif
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void loadUSBIdentitySettings() {
    preferences.begin("kprox", false);
    usbManufacturer = preferences.getString("usbMfg",     DEFAULT_MANUFACTURER);
    usbProduct      = preferences.getString("usbProduct", DEFAULT_PRODUCT_NAME);
#ifdef BOARD_HAS_USB_HID
    usbVidOverride  = preferences.getUShort("usbVid", DEFAULT_USB_VID);
    usbPidOverride  = preferences.getUShort("usbPid", DEFAULT_USB_PID);
#endif
    preferences.end();
}

void saveSinkSettings() {
    preferences.begin("kprox", false);
    preferences.putInt("sinkMaxSize", maxSinkSize);
    preferences.end();
}

void loadSinkSettings() {
    preferences.begin("kprox", false);
    maxSinkSize = preferences.getInt("sinkMaxSize", 0);
    preferences.end();
}

void saveUtcOffsetSettings() {
    preferences.begin("kprox", false);
    preferences.putLong("utcOffset", utcOffsetSeconds);
    preferences.end();
}

void loadUtcOffsetSettings() {
    preferences.begin("kprox", false);
    utcOffsetSeconds = preferences.getLong("utcOffset", 0);
    preferences.end();
}

void saveApiKeySettings() {
    preferences.begin("kprox", false);
    preferences.putString("apiKey", apiKey);
    preferences.end();
    blinkLED(2, LED_COLOR_SAVE);
}

void loadApiKeySettings() {
    preferences.begin("kprox", false);
    apiKey = preferences.getString("apiKey", DEFAULT_API_KEY);
    preferences.end();
    if (apiKey.isEmpty()) apiKey = DEFAULT_API_KEY;
}

void saveLEDSettings() {
    preferences.begin("kprox", false);
    preferences.putBool("ledEnabled", ledEnabled);
    preferences.putUChar("ledColorR", ledColorR);
    preferences.putUChar("ledColorG", ledColorG);
    preferences.putUChar("ledColorB", ledColorB);
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void loadLEDSettings() {
    preferences.begin("kprox", false);
    ledEnabled = preferences.getBool("ledEnabled", true);
    ledColorR  = preferences.getUChar("ledColorR", 0);
    ledColorG  = preferences.getUChar("ledColorG", 255);
    ledColorB  = preferences.getUChar("ledColorB", 0);
    preferences.end();
}

void saveKeymapSettings() {
    preferences.begin("kprox", false);
    preferences.putString("keymap", activeKeymap);
    preferences.end();
}

void loadKeymapSettings() {
    preferences.begin("kprox", false);
    activeKeymap = preferences.getString("keymap", "en");
    preferences.end();
    if (activeKeymap.isEmpty()) activeKeymap = "en";
}

void saveTimingSettings() {
    preferences.begin("kprox", false);
    preferences.putInt("kpressDelay",   g_keyPressDelay);
    preferences.putInt("krelDelay",     g_keyReleaseDelay);
    preferences.putInt("btwKeysDelay",  g_betweenKeysDelay);
    preferences.putInt("btwSendDelay",  g_betweenSendTextDelay);
    preferences.putInt("specKeyDelay",  g_specialKeyDelay);
    preferences.putInt("tokenDelay",    g_tokenDelay);
    preferences.end();
    blinkLED(3, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void loadTimingSettings() {
    preferences.begin("kprox", false);
    g_keyPressDelay        = preferences.getInt("kpressDelay",  KEY_PRESS_DELAY);
    g_keyReleaseDelay      = preferences.getInt("krelDelay",    KEY_RELEASE_DELAY);
    g_betweenKeysDelay     = preferences.getInt("btwKeysDelay", BETWEEN_KEYS_DELAY);
    g_betweenSendTextDelay = preferences.getInt("btwSendDelay", BETWEEN_SEND_TEXT_DELAY);
    g_specialKeyDelay      = preferences.getInt("specKeyDelay", SPECIAL_KEY_DELAY);
    g_tokenDelay           = preferences.getInt("tokenDelay",   TOKEN_DELAY);
    preferences.end();
}

void saveHostnameSettings() {
    preferences.begin("kprox", false);
    preferences.putString("hostname",  hostnameStr);
    preferences.putString("usbSerial", usbSerialNumber);
    preferences.end();
    blinkLED(3, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void loadHostnameSettings() {
    preferences.begin("kprox", false);
    hostnameStr    = preferences.getString("hostname",  HOSTNAME);
    usbSerialNumber = preferences.getString("usbSerial", USB_SERIAL_NUMBER);
    preferences.end();
    if (hostnameStr.isEmpty())     hostnameStr     = HOSTNAME;
    if (usbSerialNumber.isEmpty()) usbSerialNumber = USB_SERIAL_NUMBER;
    hostname = hostnameStr.c_str();
}

void saveDefaultAppSettings() {
    preferences.begin("kprox", false);
    preferences.putInt("defaultApp", defaultAppIndex);
    preferences.end();
}

void loadDefaultAppSettings() {
    preferences.begin("kprox", false);
    defaultAppIndex = preferences.getInt("defaultApp", 1);
    preferences.end();
    if (defaultAppIndex < 1) defaultAppIndex = 1;
}

// appOrder  — stored as comma-separated ints, e.g. "1,2,3,4,5,6,7,8,9,10,11"
// appHidden — stored as comma-separated 0/1 flags in the same index order
void saveAppLayout() {
    // 22 apps × 3 chars (max "22,") + null — no heap churn from String concat loop.
    char order[96]  = {};
    char hidden[96] = {};
    char *op = order, *hp = hidden;
    for (size_t i = 0; i < appOrder.size(); i++) {
        if (i) { *op++ = ','; *hp++ = ','; }
        op += snprintf(op, 4, "%d", appOrder[i]);
        hp += snprintf(hp, 2, "%d", (appHidden.size() > i && appHidden[i]) ? 1 : 0);
    }
    preferences.begin("kprox", false);
    preferences.putString("appOrder",  order);
    preferences.putString("appHidden", hidden);
    preferences.end();
}

void loadAppLayout(int numApps) {
    preferences.begin("kprox", false);
    String order  = preferences.getString("appOrder",  "");
    String hidden = preferences.getString("appHidden", "");
    preferences.end();

    appOrder.clear();
    appHidden.clear();

    // Parse order
    if (order.length() > 0) {
        int start = 0;
        while (start < (int)order.length()) {
            int comma = order.indexOf(',', start);
            int val = (comma < 0) ? order.substring(start).toInt()
                                  : order.substring(start, comma).toInt();
            if (val >= 1 && val <= numApps) appOrder.push_back(val);
            if (comma < 0) break;
            start = comma + 1;
        }
    }
    // Parse hidden flags
    if (hidden.length() > 0) {
        int start = 0;
        int idx   = 0;
        while (start < (int)hidden.length()) {
            int comma = hidden.indexOf(',', start);
            int val = (comma < 0) ? hidden.substring(start).toInt()
                                  : hidden.substring(start, comma).toInt();
            if (idx < (int)appOrder.size()) appHidden.push_back(val != 0);
            idx++;
            if (comma < 0) break;
            start = comma + 1;
        }
    }

    // Ensure appOrder contains every valid index 1..numApps exactly once,
    // filling in any that are missing (newly registered apps after an upgrade).
    for (int i = 1; i <= numApps; i++) {
        bool found = false;
        for (int v : appOrder) if (v == i) { found = true; break; }
        if (!found) {
            appOrder.push_back(i);
            // NostrProx (18) and IRCProx (19) are hidden by default
            appHidden.push_back(i == 18 || i == 19);
        }
    }
    // Ensure appHidden is same length as appOrder, defaulting to false.
    while ((int)appHidden.size() < (int)appOrder.size()) appHidden.push_back(false);
    // Settings app (last registered index = numApps) is never hideable.
    for (size_t i = 0; i < appOrder.size(); i++) {
        if (appOrder[i] == numApps) appHidden[i] = false;
    }
}

void saveAllSettings() {
    saveWiFiSettings();
    saveWifiEnabledSettings();
    saveBtSettings();
    saveUSBSettings();
    saveUSBIdentitySettings();
    saveApiKeySettings();
    saveUtcOffsetSettings();
    saveLEDSettings();
    saveKeymapSettings();
    saveSinkSettings();
    saveTimingSettings();
    saveHostnameSettings();
    saveDefaultAppSettings();
    saveAppLayout();
    saveCsSecuritySettings();
    saveCsStorageLocation();
    saveBootRegSettings();
    saveTimerProxSettings();
    saveDisplaySettings();
}

void serializeAllSettings(JsonObject& obj) {
    obj["wifiSSID"]               = wifiSSID;
    obj["wifiPassword"]           = wifiPassword;
    obj["wifiEnabled"]            = wifiEnabled;
    obj["bluetoothEnabled"]       = bluetoothEnabled;
    obj["bleKeyboardEnabled"]     = bleKeyboardEnabled;
    obj["bleMouseEnabled"]        = bleMouseEnabled;
    obj["bleIntlKeyboardEnabled"] = bleIntlKeyboardEnabled;
#ifdef BOARD_HAS_USB_HID
    obj["usbEnabled"]             = usbEnabled;
    obj["usbKeyboardEnabled"]     = usbKeyboardEnabled;
    obj["usbMouseEnabled"]        = usbMouseEnabled;
    obj["usbIntlKeyboardEnabled"] = usbIntlKeyboardEnabled;
    obj["fido2Enabled"]           = fido2Enabled;
#endif
    obj["usbManufacturer"]        = usbManufacturer;
    obj["usbProduct"]             = usbProduct;
#ifdef BOARD_HAS_USB_HID
    obj["usbVid"]                 = usbVidOverride;
    obj["usbPid"]                 = usbPidOverride;
#endif
    obj["hostnameStr"]            = hostnameStr;
    obj["usbSerialNumber"]        = usbSerialNumber;
    obj["apiKey"]                 = apiKey;
    obj["utcOffsetSeconds"]       = utcOffsetSeconds;
    obj["ledEnabled"]             = ledEnabled;
    obj["ledColorR"]              = ledColorR;
    obj["ledColorG"]              = ledColorG;
    obj["ledColorB"]              = ledColorB;
    obj["activeKeymap"]           = activeKeymap;
    obj["maxSinkSize"]            = maxSinkSize;
    obj["keyPressDelay"]          = g_keyPressDelay;
    obj["keyReleaseDelay"]        = g_keyReleaseDelay;
    obj["betweenKeysDelay"]       = g_betweenKeysDelay;
    obj["betweenSendTextDelay"]   = g_betweenSendTextDelay;
    obj["specialKeyDelay"]        = g_specialKeyDelay;
    obj["tokenDelay"]             = g_tokenDelay;
    obj["defaultApp"]             = defaultAppIndex;
    obj["csAutoLockSecs"]         = csAutoLockSecs;
    obj["csAutoWipeAttempts"]     = csAutoWipeAttempts;
    obj["csStorageLocation"]      = csStorageLocation;
    obj["bootRegEnabled"]         = bootRegEnabled;
    obj["bootRegIndex"]           = bootRegIndex;
    obj["bootRegLimit"]           = bootRegLimit;
    obj["bootRegFiredCount"]      = bootRegFiredCount;
    obj["timerProxRegIdx"]        = timerProxRegIdx;
    obj["timerProxFireH"]         = timerProxFireH;
    obj["timerProxFireM"]         = timerProxFireM;
    obj["timerProxFireS"]         = timerProxFireS;
    obj["timerProxHaltH"]         = timerProxHaltH;
    obj["timerProxHaltM"]         = timerProxHaltM;
    obj["timerProxHaltS"]         = timerProxHaltS;
    obj["timerProxRepH"]          = timerProxRepH;
    obj["timerProxRepM"]          = timerProxRepM;
    obj["timerProxRepS"]          = timerProxRepS;
    obj["displayBrightness"]      = g_displayBrightness;
    obj["screenTimeoutMs"]        = (uint32_t)g_screenTimeoutMs;

    JsonArray orderArr = obj["appOrder"].to<JsonArray>();
    JsonArray hidArr   = obj["appHidden"].to<JsonArray>();
    for (size_t i = 0; i < appOrder.size(); i++) {
        orderArr.add(appOrder[i]);
        hidArr.add(i < appHidden.size() ? appHidden[i] : false);
    }
}

void deserializeAllSettings(const JsonObject& obj) {
    if (!obj["wifiSSID"].isNull())     wifiSSID     = obj["wifiSSID"].as<String>();
    if (!obj["wifiPassword"].isNull()) wifiPassword = obj["wifiPassword"].as<String>();
    wifiEnabled            = obj["wifiEnabled"]            | wifiEnabled;
    bluetoothEnabled       = obj["bluetoothEnabled"]       | bluetoothEnabled;
    bleKeyboardEnabled     = obj["bleKeyboardEnabled"]     | bleKeyboardEnabled;
    bleMouseEnabled        = obj["bleMouseEnabled"]        | bleMouseEnabled;
    bleIntlKeyboardEnabled = obj["bleIntlKeyboardEnabled"] | bleIntlKeyboardEnabled;
#ifdef BOARD_HAS_USB_HID
    usbEnabled             = obj["usbEnabled"]             | usbEnabled;
    usbKeyboardEnabled     = obj["usbKeyboardEnabled"]     | usbKeyboardEnabled;
    usbMouseEnabled        = obj["usbMouseEnabled"]        | usbMouseEnabled;
    usbIntlKeyboardEnabled = obj["usbIntlKeyboardEnabled"] | usbIntlKeyboardEnabled;
    fido2Enabled           = obj["fido2Enabled"]           | fido2Enabled;
#endif
    if (!obj["usbManufacturer"].isNull())   usbManufacturer  = obj["usbManufacturer"].as<String>();
    if (!obj["usbProduct"].isNull())        usbProduct       = obj["usbProduct"].as<String>();
#ifdef BOARD_HAS_USB_HID
    if (!obj["usbVid"].isNull())            usbVidOverride   = obj["usbVid"].as<uint16_t>();
    if (!obj["usbPid"].isNull())            usbPidOverride   = obj["usbPid"].as<uint16_t>();
#endif
    if (!obj["hostnameStr"].isNull())       { hostnameStr = obj["hostnameStr"].as<String>(); hostname = hostnameStr.c_str(); }
    if (!obj["usbSerialNumber"].isNull())   usbSerialNumber  = obj["usbSerialNumber"].as<String>();
    if (!obj["apiKey"].isNull())            apiKey           = obj["apiKey"].as<String>();
    utcOffsetSeconds       = obj["utcOffsetSeconds"]       | utcOffsetSeconds;
    ledEnabled             = obj["ledEnabled"]             | ledEnabled;
    ledColorR              = obj["ledColorR"]              | ledColorR;
    ledColorG              = obj["ledColorG"]              | ledColorG;
    ledColorB              = obj["ledColorB"]              | ledColorB;
    if (!obj["activeKeymap"].isNull())      activeKeymap     = obj["activeKeymap"].as<String>();
    maxSinkSize            = obj["maxSinkSize"]            | maxSinkSize;
    g_keyPressDelay        = obj["keyPressDelay"]          | g_keyPressDelay;
    g_keyReleaseDelay      = obj["keyReleaseDelay"]        | g_keyReleaseDelay;
    g_betweenKeysDelay     = obj["betweenKeysDelay"]       | g_betweenKeysDelay;
    g_betweenSendTextDelay = obj["betweenSendTextDelay"]   | g_betweenSendTextDelay;
    g_specialKeyDelay      = obj["specialKeyDelay"]        | g_specialKeyDelay;
    g_tokenDelay           = obj["tokenDelay"]             | g_tokenDelay;
    defaultAppIndex        = obj["defaultApp"]             | defaultAppIndex;
    csAutoLockSecs         = obj["csAutoLockSecs"]         | csAutoLockSecs;
    csAutoWipeAttempts     = obj["csAutoWipeAttempts"]     | csAutoWipeAttempts;
    if (!obj["csStorageLocation"].isNull()) csStorageLocation = obj["csStorageLocation"].as<String>();
    bootRegEnabled         = obj["bootRegEnabled"]         | bootRegEnabled;
    bootRegIndex           = obj["bootRegIndex"]           | bootRegIndex;
    bootRegLimit           = obj["bootRegLimit"]           | bootRegLimit;
    bootRegFiredCount      = obj["bootRegFiredCount"]      | bootRegFiredCount;
    timerProxRegIdx        = obj["timerProxRegIdx"]        | timerProxRegIdx;
    timerProxFireH         = obj["timerProxFireH"]         | timerProxFireH;
    timerProxFireM         = obj["timerProxFireM"]         | timerProxFireM;
    timerProxFireS         = obj["timerProxFireS"]         | timerProxFireS;
    timerProxHaltH         = obj["timerProxHaltH"]         | timerProxHaltH;
    timerProxHaltM         = obj["timerProxHaltM"]         | timerProxHaltM;
    timerProxHaltS         = obj["timerProxHaltS"]         | timerProxHaltS;
    timerProxRepH          = obj["timerProxRepH"]          | timerProxRepH;
    timerProxRepM          = obj["timerProxRepM"]          | timerProxRepM;
    timerProxRepS          = obj["timerProxRepS"]          | timerProxRepS;
    g_displayBrightness    = obj["displayBrightness"]      | g_displayBrightness;
    if (!obj["screenTimeoutMs"].isNull())
        g_screenTimeoutMs  = obj["screenTimeoutMs"].as<uint32_t>();

    if (obj["appOrder"].is<JsonArray>() && obj["appHidden"].is<JsonArray>()) {
        appOrder.clear(); appHidden.clear();
        for (int v  : obj["appOrder"].as<JsonArray>())  appOrder.push_back(v);
        for (bool v : obj["appHidden"].as<JsonArray>()) appHidden.push_back(v);
    }
}

void wipeAllSettings() {
    preferences.begin("kprox", false);
    preferences.clear();
    preferences.end();
    preferences.begin("kprox_usb_id", false);
    preferences.clear();
    preferences.end();
    preferences.begin(MTLS_PREF_NS, false);
    preferences.clear();
    preferences.end();
    mtlsEnabled  = false;
    serverCert   = "";
    serverKey    = "";
    caCert       = "";
    activeKeymap = "en";
    credStoreWipe();
}

void saveCsSecuritySettings() {
    preferences.begin("kprox", false);
    preferences.putInt("csAutoLock",  csAutoLockSecs);
    preferences.putInt("csAutoWipe",  csAutoWipeAttempts);
    preferences.end();
}

void loadCsSecuritySettings() {
    preferences.begin("kprox", false);
    csAutoLockSecs      = preferences.getInt("csAutoLock", 0);
    csAutoWipeAttempts  = preferences.getInt("csAutoWipe", 0);
    preferences.end();
}

void saveCsStorageLocation() {
    preferences.begin("kprox", false);
    preferences.putString("csStoreLoc", csStorageLocation);
    preferences.end();
}

void loadCsStorageLocation() {
    preferences.begin("kprox", false);
    csStorageLocation = preferences.getString("csStoreLoc", "nvs");
    preferences.end();
    if (csStorageLocation != "sd") csStorageLocation = "nvs";
}

void saveBootRegSettings() {
    preferences.begin("kprox", false);
    preferences.putBool("bootRegEn",    bootRegEnabled);
    preferences.putInt( "bootRegIdx",   bootRegIndex);
    preferences.putInt( "bootRegLim",   bootRegLimit);
    preferences.putInt( "bootRegFired", bootRegFiredCount);
    preferences.end();
}

void loadBootRegSettings() {
    preferences.begin("kprox", false);
    bootRegEnabled    = preferences.getBool("bootRegEn",    false);
    bootRegIndex      = preferences.getInt( "bootRegIdx",   0);
    bootRegLimit      = preferences.getInt( "bootRegLim",   0);
    bootRegFiredCount = preferences.getInt( "bootRegFired", 0);
    preferences.end();
}

void saveTimerProxSettings() {
    Preferences preferences;
    preferences.begin("kprox", false);
    preferences.putInt("tpReg",   timerProxRegIdx);
    preferences.putInt("tpFireH", timerProxFireH);
    preferences.putInt("tpFireM", timerProxFireM);
    preferences.putInt("tpFireS", timerProxFireS);
    preferences.putInt("tpHaltH", timerProxHaltH);
    preferences.putInt("tpHaltM", timerProxHaltM);
    preferences.putInt("tpHaltS", timerProxHaltS);
    preferences.putInt("tpRepH",  timerProxRepH);
    preferences.putInt("tpRepM",  timerProxRepM);
    preferences.putInt("tpRepS",  timerProxRepS);
    preferences.end();
}

void loadTimerProxSettings() {
    Preferences preferences;
    preferences.begin("kprox", true);
    timerProxRegIdx = preferences.getInt("tpReg",   0);
    timerProxFireH  = preferences.getInt("tpFireH", 0);
    timerProxFireM  = preferences.getInt("tpFireM", 5);
    timerProxFireS  = preferences.getInt("tpFireS", 0);
    timerProxHaltH  = preferences.getInt("tpHaltH", 0);
    timerProxHaltM  = preferences.getInt("tpHaltM", 0);
    timerProxHaltS  = preferences.getInt("tpHaltS", 0);
    timerProxRepH   = preferences.getInt("tpRepH",  0);
    timerProxRepM   = preferences.getInt("tpRepM",  0);
    timerProxRepS   = preferences.getInt("tpRepS",  0);
    preferences.end();
}

void saveDisplaySettings() {
    Preferences preferences;
    preferences.begin("kprox", false);
    preferences.putInt("dispBright",  g_displayBrightness);
    preferences.putULong("dispTimeout", g_screenTimeoutMs);
    preferences.end();
}

void loadDisplaySettings() {
    Preferences preferences;
    preferences.begin("kprox", true);
    g_displayBrightness = preferences.getInt("dispBright",    128);
    g_screenTimeoutMs   = preferences.getULong("dispTimeout", 60000);
    preferences.end();
    // Clamp brightness to a usable range
    if (g_displayBrightness < 16)  g_displayBrightness = 128;
    if (g_displayBrightness > 255) g_displayBrightness = 255;
    // Valid values: 5000, 10000, 30000, 60000. Reject anything outside that range.
    if (g_screenTimeoutMs < 5000 || g_screenTimeoutMs > 60000) g_screenTimeoutMs = 60000;
}

