#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEHIDDevice.h>
#include "HIDTypes.h"
#include <driver/adc.h>
#include "sdkconfig.h"

#include "BleConnectionStatus.h"
#include "KeyboardOutputCallbacks.h"
#include "BleComboKeyboard.h"

#include "esp_log.h"
static const char* LOG_TAG = "BLEDevice";

#define KEYBOARD_ID    0x01
#define MEDIA_KEYS_ID  0x02
#define MOUSE_ID       0x03
#define SYSTEM_KEYS_ID 0x04
#define EXT_KEYS_ID    0x05

static const uint8_t _hidReportDescriptor[] = {
  USAGE_PAGE(1),      0x01,          // USAGE_PAGE (Generic Desktop Ctrls)
  USAGE(1),           0x06,          // USAGE (Keyboard)
  COLLECTION(1),      0x01,          // COLLECTION (Application)
  // ------------------------------------------------- Keyboard
  REPORT_ID(1),       KEYBOARD_ID,
  USAGE_PAGE(1),      0x07,
  USAGE_MINIMUM(1),   0xE0,
  USAGE_MAXIMUM(1),   0xE7,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1),     0x01,
  REPORT_COUNT(1),    0x08,
  HIDINPUT(1),        0x02,
  REPORT_COUNT(1),    0x01,
  REPORT_SIZE(1),     0x08,
  HIDINPUT(1),        0x01,
  REPORT_COUNT(1),    0x05,
  REPORT_SIZE(1),     0x01,
  USAGE_PAGE(1),      0x08,
  USAGE_MINIMUM(1),   0x01,
  USAGE_MAXIMUM(1),   0x05,
  HIDOUTPUT(1),       0x02,
  REPORT_COUNT(1),    0x01,
  REPORT_SIZE(1),     0x03,
  HIDOUTPUT(1),       0x01,
  REPORT_COUNT(1),    0x06,
  REPORT_SIZE(1),     0x08,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(2), 0xE7, 0x00,
  USAGE_PAGE(1),      0x07,
  USAGE_MINIMUM(1),   0x00,
  USAGE_MAXIMUM(2),   0xE7, 0x00,
  HIDINPUT(1),        0x00,
  END_COLLECTION(0),
  // ------------------------------------------------- Media Keys
  USAGE_PAGE(1),      0x0C,
  USAGE(1),           0x01,
  COLLECTION(1),      0x01,
  REPORT_ID(1),       MEDIA_KEYS_ID,
  USAGE_PAGE(1),      0x0C,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1),     0x01,
  REPORT_COUNT(1),    0x19,
  USAGE(1),           0xB5,
  USAGE(1),           0xB6,
  USAGE(1),           0xB7,
  USAGE(1),           0xCD,
  USAGE(1),           0xE2,
  USAGE(1),           0xE9,
  USAGE(1),           0xEA,
  USAGE(2),           0x23, 0x02,
  USAGE(2),           0x94, 0x01,
  USAGE(2),           0x92, 0x01,
  USAGE(2),           0x2A, 0x02,
  USAGE(2),           0x21, 0x02,
  USAGE(2),           0x26, 0x02,
  USAGE(2),           0x24, 0x02,
  USAGE(2),           0x83, 0x01,
  USAGE(2),           0x8A, 0x01,
  USAGE(2),           0x25, 0x02,
  USAGE(2),           0x27, 0x02,
  USAGE(1),           0x6F,
  USAGE(1),           0x70,
  USAGE(1),           0x77,
  USAGE(1),           0x78,
  USAGE(1),           0x79,
  USAGE(1),           0xB8,
  USAGE(2),           0x9E, 0x01,
  HIDINPUT(1),        0x02,
  REPORT_COUNT(1),    0x07,
  HIDINPUT(1),        0x03,
  END_COLLECTION(0),
  // ------------------------------------------------- Mouse
  USAGE_PAGE(1),       0x01,
  USAGE(1),            0x02,
  COLLECTION(1),       0x01,
  USAGE(1),            0x01,
  COLLECTION(1),       0x00,
  REPORT_ID(1),        MOUSE_ID,
  USAGE_PAGE(1),       0x09,
  USAGE_MINIMUM(1),    0x01,
  USAGE_MAXIMUM(1),    0x05,
  LOGICAL_MINIMUM(1),  0x00,
  LOGICAL_MAXIMUM(1),  0x01,
  REPORT_SIZE(1),      0x01,
  REPORT_COUNT(1),     0x05,
  HIDINPUT(1),         0x02,
  REPORT_SIZE(1),      0x03,
  REPORT_COUNT(1),     0x01,
  HIDINPUT(1),         0x03,
  USAGE_PAGE(1),       0x01,
  USAGE(1),            0x30,
  USAGE(1),            0x31,
  USAGE(1),            0x38,
  LOGICAL_MINIMUM(1),  0x81,
  LOGICAL_MAXIMUM(1),  0x7f,
  REPORT_SIZE(1),      0x08,
  REPORT_COUNT(1),     0x03,
  HIDINPUT(1),         0x06,
  USAGE_PAGE(1),       0x0c,
  USAGE(2),      0x38, 0x02,
  LOGICAL_MINIMUM(1),  0x81,
  LOGICAL_MAXIMUM(1),  0x7f,
  REPORT_SIZE(1),      0x08,
  REPORT_COUNT(1),     0x01,
  HIDINPUT(1),         0x06,
  END_COLLECTION(0),
  END_COLLECTION(0),
  // ------------------------------------------------- System Keys
  USAGE_PAGE(1),     0x01,
  USAGE(1),          0x80,
  COLLECTION(1),     0x01,
  REPORT_ID(1),      SYSTEM_KEYS_ID,
  LOGICAL_MINIMUM(1),0x00,
  LOGICAL_MAXIMUM(1),0x01,
  REPORT_SIZE(1),    0x01,
  REPORT_COUNT(1),   0x03,
  USAGE(1),          0x81,
  USAGE(1),          0x82,
  USAGE(1),          0x83,
  HIDINPUT(1),       0x02,
  REPORT_COUNT(1),   0x01,
  REPORT_SIZE(1),    0x05,
  HIDINPUT(1),       0x03,
  END_COLLECTION(0),
  // ------------------------------------------------- Extended Keys
  USAGE_PAGE(1),     0x07,
  USAGE(1),          0x01,
  COLLECTION(1),     0x01,
  REPORT_ID(1),      EXT_KEYS_ID,
  USAGE_PAGE(1),     0x07,
  LOGICAL_MINIMUM(1),0x00,
  LOGICAL_MAXIMUM(1),0x01,
  REPORT_SIZE(1),    0x01,
  REPORT_COUNT(1),   0x0B,
  USAGE(1),          0x85,
  USAGE(1),          0x87,
  USAGE(1),          0x88,
  USAGE(1),          0x89,
  USAGE(1),          0x8A,
  USAGE(1),          0x8B,
  USAGE(1),          0x90,
  USAGE(1),          0x91,
  USAGE(1),          0x92,
  USAGE(1),          0x93,
  USAGE(1),          0x94,
  HIDINPUT(1),       0x02,
  REPORT_COUNT(1),   0x05,
  HIDINPUT(1),       0x03,
  END_COLLECTION(0)
};


BleComboKeyboard::BleComboKeyboard(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel)
  : hid(nullptr)
{
  this->deviceName         = deviceName;
  this->deviceManufacturer = deviceManufacturer;
  this->batteryLevel       = batteryLevel;
  this->connectionStatus   = new BleConnectionStatus();
  this->inputKeyboard      = nullptr;
  this->outputKeyboard     = nullptr;
  this->inputMediaKeys     = nullptr;
  this->inputSystemKeys    = nullptr;
  this->inputExtKeys       = nullptr;
  this->inputMouse         = nullptr;
  this->_systemKeyReport   = 0;
  this->_begun             = false;
}

void BleComboKeyboard::begin(void) {
  if (_begun) {
    NimBLEDevice::startAdvertising();
    return;
  }
  _begun = true;

  NimBLEDevice::init(deviceName);
  // Bonding only — no MITM so host OS uses "Just Works" pairing automatically.
  NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND);
  // NO_INPUT_OUTPUT = "Just Works"; without this the default IO cap can require
  // a passkey that no HID device can display, making the device invisible on iOS.
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(connectionStatus);

  hid = new NimBLEHIDDevice(pServer);

  // v2.x: configure properties before server start.
  // pnp() was removed; PnP metadata is optional and not needed for HID function.
  hid->setReportMap((uint8_t*)_hidReportDescriptor, sizeof(_hidReportDescriptor));
  hid->setManufacturer(deviceManufacturer);
  hid->setHidInfo(0x00, 0x01);

  // getInputReport()/getOutputReport() create characteristics before server start.
  inputKeyboard   = hid->getInputReport(KEYBOARD_ID);
  outputKeyboard  = hid->getOutputReport(KEYBOARD_ID);
  inputMediaKeys  = hid->getInputReport(MEDIA_KEYS_ID);
  inputSystemKeys = hid->getInputReport(SYSTEM_KEYS_ID);
  inputExtKeys    = hid->getInputReport(EXT_KEYS_ID);
  inputMouse      = hid->getInputReport(MOUSE_ID);

  connectionStatus->inputKeyboard  = inputKeyboard;
  connectionStatus->outputKeyboard = outputKeyboard;
  connectionStatus->inputMouse     = inputMouse;

  if (outputKeyboard) outputKeyboard->setCallbacks(new KeyboardOutputCallbacks());

  // v2.x: pServer->start() replaces the deprecated hid->startServices().
  pServer->start();

  // Advertising packet layout (31-byte ADV_IND limit):
  //   flags(3B) + complete 16-bit UUID list(4B) + appearance(4B) = 11B
  //   remaining 20B is enough for names up to 18 chars.
  // Android does NOT send a scan request before listing a device — it shows
  // devices from ADV_IND only. Name must be in the primary packet, not scan response.
  // iOS and Windows also require 0x1812 in ADV_IND to classify as HID keyboard.

  // Truncate name to 18 chars to guarantee it fits in the primary ADV_IND.
  std::string advName = deviceName.length() > 18 ? deviceName.substr(0, 18) : deviceName;

  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);                           // LE General Discoverable | BR/EDR Not Supported
  advData.addServiceUUID(NimBLEUUID((uint16_t)0x1812));
  advData.setAppearance(HID_KEYBOARD);
  advData.setName(advName);

  // Scan response: full name if truncated, otherwise empty (saves airtime).
  NimBLEAdvertisementData scanData;
  if (deviceName.length() > 18) scanData.setName(deviceName);

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setScanResponseData(scanData);
  pAdvertising->setMinInterval(160);   // 160 × 0.625ms = 100ms
  pAdvertising->setMaxInterval(256);   // 256 × 0.625ms = 160ms
  pAdvertising->start();
  hid->setBatteryLevel(batteryLevel);

  ESP_LOGD(LOG_TAG, "NimBLE HID advertising started");
}

void BleComboKeyboard::end(void) {
  if (_begun) NimBLEDevice::getAdvertising()->stop();
}

bool BleComboKeyboard::isConnected(void) {
  return connectionStatus->connected;
}

void BleComboKeyboard::setBatteryLevel(uint8_t level) {
  batteryLevel = level;
  if (hid) hid->setBatteryLevel(level);
}

void BleComboKeyboard::sendReport(BleKeyReport* keys) {
  if (isConnected()) {
    inputKeyboard->setValue((uint8_t*)keys, sizeof(BleKeyReport));
    inputKeyboard->notify();
  }
}

void BleComboKeyboard::sendReport(MediaKeyReport* keys) {
  if (isConnected() && inputMediaKeys) {
    inputMediaKeys->setValue((uint8_t*)keys, sizeof(MediaKeyReport));
    inputMediaKeys->notify();
  }
}

void BleComboKeyboard::sendSystemReport(SystemKeyReport* keys) {
  if (isConnected() && inputSystemKeys) {
    inputSystemKeys->setValue((uint8_t*)keys, sizeof(SystemKeyReport));
    inputSystemKeys->notify();
  }
}

void BleComboKeyboard::writeExtKey(uint8_t b0, uint8_t b1) {
  if (!isConnected() || !inputExtKeys) return;
  uint8_t report[2] = {b0, b1};
  inputExtKeys->setValue(report, 2);
  inputExtKeys->notify();
  uint8_t zero[2] = {0, 0};
  inputExtKeys->setValue(zero, 2);
  inputExtKeys->notify();
}

extern const uint8_t _asciimap[128] PROGMEM;

#define SHIFT 0x80
const uint8_t _asciimap[128] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x2a, 0x2b, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x2c,          // ' '
	0x1e|SHIFT,    // !
	0x34|SHIFT,    // "
	0x20|SHIFT,    // #
	0x21|SHIFT,    // $
	0x22|SHIFT,    // %
	0x24|SHIFT,    // &
	0x34,          // '
	0x26|SHIFT,    // (
	0x27|SHIFT,    // )
	0x25|SHIFT,    // *
	0x2e|SHIFT,    // +
	0x36,          // ,
	0x2d,          // -
	0x37,          // .
	0x38,          // /
	0x27, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,  // 0-9
	0x33|SHIFT,    // :
	0x33,          // ;
	0x36|SHIFT,    // <
	0x2e,          // =
	0x37|SHIFT,    // >
	0x38|SHIFT,    // ?
	0x1f|SHIFT,    // @
	0x04|SHIFT, 0x05|SHIFT, 0x06|SHIFT, 0x07|SHIFT, 0x08|SHIFT,  // A-E
	0x09|SHIFT, 0x0a|SHIFT, 0x0b|SHIFT, 0x0c|SHIFT, 0x0d|SHIFT,  // F-J
	0x0e|SHIFT, 0x0f|SHIFT, 0x10|SHIFT, 0x11|SHIFT, 0x12|SHIFT,  // K-O
	0x13|SHIFT, 0x14|SHIFT, 0x15|SHIFT, 0x16|SHIFT, 0x17|SHIFT,  // P-T
	0x18|SHIFT, 0x19|SHIFT, 0x1a|SHIFT, 0x1b|SHIFT, 0x1c|SHIFT,  // U-Y
	0x1d|SHIFT,    // Z
	0x2f,          // [
	0x31,          // backslash
	0x30,          // ]
	0x23|SHIFT,    // ^
	0x2d|SHIFT,    // _
	0x35,          // `
	0x04, 0x05, 0x06, 0x07, 0x08,  // a-e
	0x09, 0x0a, 0x0b, 0x0c, 0x0d,  // f-j
	0x0e, 0x0f, 0x10, 0x11, 0x12,  // k-o
	0x13, 0x14, 0x15, 0x16, 0x17,  // p-t
	0x18, 0x19, 0x1a, 0x1b, 0x1c,  // u-y
	0x1d,          // z
	0x2f|SHIFT,    // {
	0x31|SHIFT,    // |
	0x30|SHIFT,    // }
	0x35|SHIFT,    // ~
	0              // DEL
};

size_t BleComboKeyboard::press(uint8_t k) {
	if (k >= 136) {
		k = k - 136;
	} else if (k >= 128) {
		_keyReport.modifiers |= (1<<(k-128));
		k = 0;
	} else {
		k = pgm_read_byte(_asciimap + k);
		if (!k) { setWriteError(); return 0; }
		if (k & 0x80) { _keyReport.modifiers |= 0x02; k &= 0x7F; }
	}
	for (uint8_t i = 0; i < 6; i++) {
		if (_keyReport.keys[i] == k) return 1;
	}
	for (uint8_t i = 0; i < 6; i++) {
		if (_keyReport.keys[i] == 0) { _keyReport.keys[i] = k; sendReport(&_keyReport); return 1; }
	}
	setWriteError();
	return 0;
}

size_t BleComboKeyboard::press(const MediaKeyReport k) {
	_mediaKeyReport[0] |= k[0]; _mediaKeyReport[1] |= k[1];
	_mediaKeyReport[2] |= k[2]; _mediaKeyReport[3] |= k[3];
	sendReport(&_mediaKeyReport);
	return 1;
}

size_t BleComboKeyboard::release(uint8_t k) {
	if (k >= 136) {
		k = k - 136;
	} else if (k >= 128) {
		_keyReport.modifiers &= ~(1<<(k-128));
		k = 0;
	} else {
		k = pgm_read_byte(_asciimap + k);
		if (!k) return 0;
		if (k & 0x80) { _keyReport.modifiers &= ~(0x02); k &= 0x7F; }
	}
	for (uint8_t i = 0; i < 6; i++) {
		if (_keyReport.keys[i] == k) _keyReport.keys[i] = 0;
	}
	sendReport(&_keyReport);
	return 1;
}

size_t BleComboKeyboard::release(const MediaKeyReport k) {
	_mediaKeyReport[0] &= ~k[0]; _mediaKeyReport[1] &= ~k[1];
	_mediaKeyReport[2] &= ~k[2]; _mediaKeyReport[3] &= ~k[3];
	sendReport(&_mediaKeyReport);
	return 1;
}

void BleComboKeyboard::releaseAll(void) {
	memset(&_keyReport, 0, sizeof(_keyReport));
	memset(_mediaKeyReport, 0, sizeof(_mediaKeyReport));
	_systemKeyReport = 0;
	sendReport(&_keyReport);
	sendReport(&_mediaKeyReport);
	sendSystemReport(&_systemKeyReport);
}

size_t BleComboKeyboard::write(uint8_t c)               { uint8_t p = press(c); release(c); return p; }
size_t BleComboKeyboard::write(const MediaKeyReport c)   { uint16_t p = press(c); release(c); return p; }
size_t BleComboKeyboard::pressSystemKey(SystemKeyReport k)  { _systemKeyReport |= k;  sendSystemReport(&_systemKeyReport); return 1; }
size_t BleComboKeyboard::releaseSystemKey(SystemKeyReport k){ _systemKeyReport &= ~k; sendSystemReport(&_systemKeyReport); return 1; }
size_t BleComboKeyboard::writeSystemKey(SystemKeyReport c)  { pressSystemKey(c); releaseSystemKey(c); return 1; }

size_t BleComboKeyboard::write(const uint8_t *buffer, size_t size) {
	size_t n = 0;
	while (size--) {
		if (*buffer != '\r') { if (write(*buffer)) n++; else break; }
		buffer++;
	}
	return n;
}
