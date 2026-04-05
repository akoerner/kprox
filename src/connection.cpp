#include "connection.h"
#include "led.h"
#include "storage.h"
#include "crypto_utils.h"

// ---- mDNS compatibility ----
static void mdnsUpdate() {
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

// ---- NTP ----

void initNTP() {
    configTime(utcOffsetSeconds, 0, "pool.ntp.org", "time.nist.gov");
}

// ---- Bluetooth ----

void enableBluetooth() {
    if (bluetoothEnabled) return;
    bluetoothEnabled = true;
    if (!bluetoothInitialized && BLE_KEYBOARD_VALID) {
        BLE_KEYBOARD.begin();
        BLE_MOUSE.begin();
        bluetoothInitialized = true;
    }
    saveBtSettings();
    if (ledEnabled) setLED(LED_COLOR_BT_ENABLE, 500);
}

void disableBluetooth() {
    if (!bluetoothEnabled) return;
    bluetoothEnabled = false;
    if (bluetoothInitialized && BLE_KEYBOARD_VALID) {
        BLE_KEYBOARD.end();
        bluetoothInitialized = false;
        delay(1000);
    }
    saveBtSettings();
    if (ledEnabled) setLED(LED_COLOR_BT_DISABLE, 500);
}

// ---- USB ----

#ifdef BOARD_HAS_USB_HID
void enableUSB() {
    if (usbEnabled) return;
    usbEnabled = true;
    if (!usbInitialized) {
        USB.manufacturerName(usbManufacturer.c_str());
        USB.productName(usbProduct.c_str());
        USB.serialNumber(USB_SERIAL_NUMBER);
        if (fido2Enabled) FIDO2Device.begin();
        USB.begin();
        KProxConsumer.begin();  // creates send semaphore; addDevice() ran in constructor
        if (usbKeyboardEnabled) { USBKeyboard.begin(); usbKeyboardReady = true; }
        if (usbMouseEnabled)    { USBMouse.begin();    usbMouseReady    = true; }
        usbInitialized = true;
        delay(1000);
    } else {
        // Already initialised — just update ready flags
        usbKeyboardReady = usbKeyboardEnabled;
        usbMouseReady    = usbMouseEnabled;
    }
    saveUSBSettings();
    if (ledEnabled) setLED(LED_COLOR_USB_ENABLE, 500);
}

void disableUSB() {
    if (!usbEnabled) return;
    usbEnabled = false;
    if (usbInitialized) {
        if (usbKeyboardReady) USBKeyboard.releaseAll(); if (KProxConsumer.isReady()) { KProxConsumer.sendConsumer(0,0); KProxConsumer.sendSystem(0); }
        usbKeyboardReady = false;
        usbMouseReady    = false;
    }
    saveUSBSettings();
    if (ledEnabled) setLED(LED_COLOR_USB_DISABLE, 500);
}
#else
void enableUSB()  {}
void disableUSB() {}
#endif

// ---- WiFi ----

bool connectToNewWiFi(const String& newSSID, const String& newPassword) {
    if (ledEnabled) setLED(LED_COLOR_WIFI_CONNECTING, 500);

    WiFi.disconnect();
    delay(1000);
    WiFi.begin(newSSID.c_str(), newPassword.c_str());

    for (int attempts = 0; WiFi.status() != WL_CONNECTED && attempts < 20; attempts++) {
        delay(500);
        feedWatchdog();
        if (ledEnabled) setLED(LED_COLOR_WIFI_CONNECTING, 100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiSSID     = newSSID;
        wifiPassword = newPassword;
        saveWiFiSettings();
        if (ledEnabled) setLED(LED_COLOR_WIFI_SUCCESS, 1000);
        return true;
    }

    if (ledEnabled) setLED(LED_COLOR_WIFI_FAILED, 1000);
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    return false;
}

void cleanupConnections() {
    feedWatchdog();
    unsigned long now = millis();
    if (WiFi.status() != WL_CONNECTED) {
        if (now - lastWifiCheck > WIFI_RECONNECT_INTERVAL) {
            WiFi.reconnect();
            lastWifiCheck = now;
        }
    } else {
        lastWifiCheck = now;
    }
}

// ---- mDNS ----

void setupMDNS() {
    if (WiFi.status() != WL_CONNECTED) return;

    bool started = false;
    for (int attempt = 0; attempt < 3 && !started; attempt++) {
        if (attempt > 0) { delay(500); feedWatchdog(); }
        started = MDNS.begin(hostname);
    }

    if (started) {
        mdnsEnabled = true;
        MDNS.addService("http",  "tcp", 80);
        MDNS.addServiceTxt("http",  "tcp", "version", "12.1");
        MDNS.addServiceTxt("http",  "tcp", "device",  "kprox");
        MDNS.addService("kprox", "tcp", 80);
        MDNS.addServiceTxt("kprox", "tcp", "type", "hid-controller");
        if (ledEnabled) setLED(LED_COLOR_MDNS_SUCCESS, 300);
    } else {
        mdnsEnabled = false;
        if (ledEnabled) setLED(LED_COLOR_MDNS_FAILED, 300);
    }
}

// ---- UDP Discovery ----

void broadcastDiscovery() {
    if (!udpEnabled || WiFi.status() != WL_CONNECTED) return;

    JsonDocument doc;
    doc["protocol"]    = "KProx-Discovery";
    doc["version"]     = "1.0";
    doc["device_type"] = "KProx_HID_Controller";
    doc["device_id"]   = WiFi.macAddress();
    doc["device_name"] = String(BLUETOOTH_DEVICE_NAME);
    doc["hostname"]    = String(hostname);
    doc["ip"]          = WiFi.localIP().toString();
    doc["http_port"]   = 80;
    doc["timestamp"]   = millis();

    String message;
    serializeJson(doc, message);

    String encrypted = encryptResponse(message);
    if (encrypted.length() == 0) return;

    IPAddress broadcastAddr = ~WiFi.subnetMask() | WiFi.gatewayIP();
    udp.beginPacket(broadcastAddr, UDP_DISCOVERY_PORT);
    udp.write((const uint8_t*)encrypted.c_str(), encrypted.length());
    udp.endPacket();
}
