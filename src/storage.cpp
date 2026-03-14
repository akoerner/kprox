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
