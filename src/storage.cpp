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
    preferences.putBool("btEnabled",  bluetoothEnabled);
    preferences.putBool("bleKbEn",    bleKeyboardEnabled);
    preferences.putBool("bleMouseEn", bleMouseEnabled);
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void loadBtSettings() {
    preferences.begin("kprox", false);
    bluetoothEnabled  = preferences.getBool("btEnabled",  true);
    bleKeyboardEnabled = preferences.getBool("bleKbEn",   true);
    bleMouseEnabled    = preferences.getBool("bleMouseEn",true);
    preferences.end();
}

#ifdef BOARD_HAS_USB_HID
void saveUSBSettings() {
    preferences.begin("kprox", false);
    preferences.putBool("usbEnabled",    usbEnabled);
    preferences.putBool("usbKbEn",       usbKeyboardEnabled);
    preferences.putBool("usbMouseEn",    usbMouseEnabled);
    preferences.putBool("fido2En",       fido2Enabled);
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void loadUSBSettings() {
    preferences.begin("kprox", false);
    usbEnabled         = preferences.getBool("usbEnabled",  true);
    usbKeyboardEnabled = preferences.getBool("usbKbEn",     true);
    usbMouseEnabled    = preferences.getBool("usbMouseEn",  true);
    fido2Enabled       = preferences.getBool("fido2En",     false);
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
    preferences.end();
    blinkLED(10, LED_COLOR_SAVE, LED_SAVE_DUTY_CYCLE);
}

void loadUSBIdentitySettings() {
    preferences.begin("kprox", false);
    usbManufacturer = preferences.getString("usbMfg",     DEFAULT_MANUFACTURER);
    usbProduct      = preferences.getString("usbProduct", DEFAULT_PRODUCT_NAME);
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
    String order, hidden;
    for (size_t i = 0; i < appOrder.size(); i++) {
        if (i) { order += ','; hidden += ','; }
        order  += String(appOrder[i]);
        hidden += String(appHidden.size() > i && appHidden[i] ? 1 : 0);
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
        if (!found) appOrder.push_back(i);
    }
    // Ensure appHidden is same length as appOrder, defaulting to false.
    while ((int)appHidden.size() < (int)appOrder.size()) appHidden.push_back(false);
    // Settings app (last registered index = numApps) is never hideable.
    for (size_t i = 0; i < appOrder.size(); i++) {
        if (appOrder[i] == numApps) appHidden[i] = false;
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
