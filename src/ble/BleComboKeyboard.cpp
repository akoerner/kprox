#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include <driver/adc.h>
#include "sdkconfig.h"

#include "BleConnectionStatus.h"
#include "KeyboardOutputCallbacks.h"
#include "BleComboKeyboard.h"

#if defined(CONFIG_ARDUHAL_ESP_LOG)
  #include "esp32-hal-log.h"
  #define LOG_TAG ""
#else
  #include "esp_log.h"
  static const char* LOG_TAG = "BLEDevice";
#endif


// Report IDs:
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
  REPORT_ID(1),       KEYBOARD_ID,   //   REPORT_ID (1)
  USAGE_PAGE(1),      0x07,          //   USAGE_PAGE (Kbrd/Keypad)
  USAGE_MINIMUM(1),   0xE0,          //   USAGE_MINIMUM (0xE0)
  USAGE_MAXIMUM(1),   0xE7,          //   USAGE_MAXIMUM (0xE7)
  LOGICAL_MINIMUM(1), 0x00,          //   LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1), 0x01,          //   Logical Maximum (1)
  REPORT_SIZE(1),     0x01,          //   REPORT_SIZE (1)
  REPORT_COUNT(1),    0x08,          //   REPORT_COUNT (8)
  HIDINPUT(1),        0x02,          //   INPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  REPORT_COUNT(1),    0x01,          //   REPORT_COUNT (1) ; 1 byte (Reserved)
  REPORT_SIZE(1),     0x08,          //   REPORT_SIZE (8)
  HIDINPUT(1),        0x01,          //   INPUT (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
  REPORT_COUNT(1),    0x05,          //   REPORT_COUNT (5) ; 5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
  REPORT_SIZE(1),     0x01,          //   REPORT_SIZE (1)
  USAGE_PAGE(1),      0x08,          //   USAGE_PAGE (LEDs)
  USAGE_MINIMUM(1),   0x01,          //   USAGE_MINIMUM (0x01) ; Num Lock
  USAGE_MAXIMUM(1),   0x05,          //   USAGE_MAXIMUM (0x05) ; Kana
  HIDOUTPUT(1),       0x02,          //   OUTPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
  REPORT_COUNT(1),    0x01,          //   REPORT_COUNT (1) ; 3 bits (Padding)
  REPORT_SIZE(1),     0x03,          //   REPORT_SIZE (3)
  HIDOUTPUT(1),       0x01,          //   OUTPUT (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
  REPORT_COUNT(1),    0x06,          //   REPORT_COUNT (6) ; 6 bytes (Keys)
  REPORT_SIZE(1),     0x08,          //   REPORT_SIZE(8)
  LOGICAL_MINIMUM(1), 0x00,          //   LOGICAL_MINIMUM(0)
  LOGICAL_MAXIMUM(2), 0xE7, 0x00,    //   LOGICAL_MAXIMUM(231) — full keyboard page 0x00-0xE7
  USAGE_PAGE(1),      0x07,          //   USAGE_PAGE (Kbrd/Keypad)
  USAGE_MINIMUM(1),   0x00,          //   USAGE_MINIMUM (0)
  USAGE_MAXIMUM(2),   0xE7, 0x00,    //   USAGE_MAXIMUM (0xE7) — covers intl keys up to 0x94, modifiers up to 0xE7
  HIDINPUT(1),        0x00,          //   INPUT (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
  END_COLLECTION(0),                 // END_COLLECTION
  // ------------------------------------------------- Media Keys
  USAGE_PAGE(1),      0x0C,          // USAGE_PAGE (Consumer)
  USAGE(1),           0x01,          // USAGE (Consumer Control)
  COLLECTION(1),      0x01,          // COLLECTION (Application)
  REPORT_ID(1),       MEDIA_KEYS_ID, //   REPORT_ID (3)
  USAGE_PAGE(1),      0x0C,          //   USAGE_PAGE (Consumer)
  LOGICAL_MINIMUM(1), 0x00,          //   LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1), 0x01,          //   LOGICAL_MAXIMUM (1)
  REPORT_SIZE(1),     0x01,          //   REPORT_SIZE (1)
  REPORT_COUNT(1),    0x19,          //   REPORT_COUNT (25) — 18 original + 7 new
  USAGE(1),           0xB5,          //   USAGE (Scan Next Track)      ; byte0 bit0: 1
  USAGE(1),           0xB6,          //   USAGE (Scan Previous Track)  ; byte0 bit1: 2
  USAGE(1),           0xB7,          //   USAGE (Stop)                 ; byte0 bit2: 4
  USAGE(1),           0xCD,          //   USAGE (Play/Pause)           ; byte0 bit3: 8
  USAGE(1),           0xE2,          //   USAGE (Mute)                 ; byte0 bit4: 16
  USAGE(1),           0xE9,          //   USAGE (Volume Increment)     ; byte0 bit5: 32
  USAGE(1),           0xEA,          //   USAGE (Volume Decrement)     ; byte0 bit6: 64
  USAGE(2),           0x23, 0x02,    //   Usage (WWW Home)             ; byte0 bit7: 128
  USAGE(2),           0x94, 0x01,    //   Usage (My Computer)  ; byte1 bit0: 1
  USAGE(2),           0x92, 0x01,    //   Usage (Calculator)   ; byte1 bit1: 2
  USAGE(2),           0x2A, 0x02,    //   Usage (WWW fav)      ; byte1 bit2: 4
  USAGE(2),           0x21, 0x02,    //   Usage (WWW search)   ; byte1 bit3: 8
  USAGE(2),           0x26, 0x02,    //   Usage (WWW stop)     ; byte1 bit4: 16
  USAGE(2),           0x24, 0x02,    //   Usage (WWW back)     ; byte1 bit5: 32
  USAGE(2),           0x83, 0x01,    //   Usage (Media sel)    ; byte1 bit6: 64
  USAGE(2),           0x8A, 0x01,    //   Usage (Mail)         ; byte1 bit7: 128
  USAGE(2),           0x25, 0x02,    //   Usage (AC Forward)   ; byte2 bit0: 1
  USAGE(2),           0x27, 0x02,    //   Usage (AC Refresh)   ; byte2 bit1: 2
  USAGE(1),           0x6F,          //   Usage (Brightness++)  ; byte2 bit2: 4
  USAGE(1),           0x70,          //   Usage (Brightness--)  ; byte2 bit3: 8
  USAGE(1),           0x77,          //   Usage (KbdIllumToggle); byte2 bit4: 16
  USAGE(1),           0x78,          //   Usage (KbdIllum--)    ; byte2 bit5: 32
  USAGE(1),           0x79,          //   Usage (KbdIllum++)    ; byte2 bit6: 64
  USAGE(1),           0xB8,          //   Usage (Eject)         ; byte2 bit7: 128
  USAGE(2),           0x9E, 0x01,    //   Usage (ScreenLock)    ; byte3 bit0: 1
  HIDINPUT(1),        0x02,          //   INPUT (Data,Var,Abs) — 25 data bits
  REPORT_COUNT(1),    0x07,          //   REPORT_COUNT (7) — padding to 4-byte boundary
  HIDINPUT(1),        0x03,          //   INPUT (Const,Var,Abs)
  END_COLLECTION(0),                 // END_COLLECTION

  // ------------------------------------------------- Mouse
  USAGE_PAGE(1),       0x01, // USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x02, // USAGE (Mouse)
  COLLECTION(1),       0x01, // COLLECTION (Application)
  USAGE(1),            0x01, //   USAGE (Pointer)
  COLLECTION(1),       0x00, //   COLLECTION (Physical)
  REPORT_ID(1),        MOUSE_ID, //     REPORT_ID (1)
  // ------------------------------------------------- Buttons (Left, Right, Middle, Back, Forward)
  USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
  USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
  USAGE_MAXIMUM(1),    0x05, //     USAGE_MAXIMUM (Button 5)
  LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
  REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
  REPORT_COUNT(1),     0x05, //     REPORT_COUNT (5)
  HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute) ;5 button bits
  // ------------------------------------------------- Padding
  REPORT_SIZE(1),      0x03, //     REPORT_SIZE (3)
  REPORT_COUNT(1),     0x01, //     REPORT_COUNT (1)
  HIDINPUT(1),         0x03, //     INPUT (Constant, Variable, Absolute) ;3 bit padding
  // ------------------------------------------------- X/Y position, Wheel
  USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x30, //     USAGE (X)
  USAGE(1),            0x31, //     USAGE (Y)
  USAGE(1),            0x38, //     USAGE (Wheel)
  LOGICAL_MINIMUM(1),  0x81, //     LOGICAL_MINIMUM (-127)
  LOGICAL_MAXIMUM(1),  0x7f, //     LOGICAL_MAXIMUM (127)
  REPORT_SIZE(1),      0x08, //     REPORT_SIZE (8)
  REPORT_COUNT(1),     0x03, //     REPORT_COUNT (3)
  HIDINPUT(1),         0x06, //     INPUT (Data, Variable, Relative) ;3 bytes (X,Y,Wheel)
  // ------------------------------------------------- Horizontal wheel
  USAGE_PAGE(1),       0x0c, //     USAGE PAGE (Consumer Devices)
  USAGE(2),      0x38, 0x02, //     USAGE (AC Pan)
  LOGICAL_MINIMUM(1),  0x81, //     LOGICAL_MINIMUM (-127)
  LOGICAL_MAXIMUM(1),  0x7f, //     LOGICAL_MAXIMUM (127)
  REPORT_SIZE(1),      0x08, //     REPORT_SIZE (8)
  REPORT_COUNT(1),     0x01, //     REPORT_COUNT (1)
  HIDINPUT(1),         0x06, //     INPUT (Data, Var, Rel)
  END_COLLECTION(0),         //   END_COLLECTION
  END_COLLECTION(0),         // END_COLLECTION

  // ------------------------------------------------- System Keys (Generic Desktop / System Control)
  USAGE_PAGE(1),     0x01,          // USAGE_PAGE (Generic Desktop)
  USAGE(1),          0x80,          // USAGE (System Control)
  COLLECTION(1),     0x01,          // COLLECTION (Application)
  REPORT_ID(1),      SYSTEM_KEYS_ID,
  LOGICAL_MINIMUM(1),0x00,          //   LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1),0x01,          //   LOGICAL_MAXIMUM (1)
  REPORT_SIZE(1),    0x01,          //   REPORT_SIZE (1)
  REPORT_COUNT(1),   0x03,          //   REPORT_COUNT (3)
  USAGE(1),          0x81,          //   USAGE (System Power Down)   ; bit 0
  USAGE(1),          0x82,          //   USAGE (System Sleep)        ; bit 1
  USAGE(1),          0x83,          //   USAGE (System Wake Up)      ; bit 2
  HIDINPUT(1),       0x02,          //   INPUT (Data,Var,Abs)
  REPORT_COUNT(1),   0x01,          //   REPORT_COUNT (1)
  REPORT_SIZE(1),    0x05,          //   REPORT_SIZE (5) — pad to byte
  HIDINPUT(1),       0x03,          //   INPUT (Const,Var,Abs)
  END_COLLECTION(0),                // END_COLLECTION

  // ---- Extended keyboard keys (International / Lang — Report ID 5) ----
  // Keys with HID usages 0x85-0x94 that cannot be encoded as uint8_t (136+usage > 255)
  // 11 data bits + 5 padding bits = 2 bytes
  USAGE_PAGE(1),     0x07,          // USAGE_PAGE (Keyboard/Keypad)
  USAGE(1),          0x01,          // USAGE (Keyboard)
  COLLECTION(1),     0x01,          // COLLECTION (Application)
  REPORT_ID(1),      EXT_KEYS_ID,   //   REPORT_ID (5)
  USAGE_PAGE(1),     0x07,          //   USAGE_PAGE (Keyboard/Keypad)
  LOGICAL_MINIMUM(1),0x00,          //   LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1),0x01,          //   LOGICAL_MAXIMUM (1)
  REPORT_SIZE(1),    0x01,          //   REPORT_SIZE (1)
  REPORT_COUNT(1),   0x0B,          //   REPORT_COUNT (11)
  USAGE(1),          0x85,          //   USAGE (Keypad Comma)       ; bit 0
  USAGE(1),          0x87,          //   USAGE (International1 RO)  ; bit 1
  USAGE(1),          0x88,          //   USAGE (International2 KatakanaHiragana) ; bit 2
  USAGE(1),          0x89,          //   USAGE (International3 Yen) ; bit 3
  USAGE(1),          0x8A,          //   USAGE (International4 Henkan) ; bit 4
  USAGE(1),          0x8B,          //   USAGE (International5 Muhenkan) ; bit 5
  USAGE(1),          0x90,          //   USAGE (Lang1 Hanguel)      ; bit 6
  USAGE(1),          0x91,          //   USAGE (Lang2 Hanja)        ; bit 7
  USAGE(1),          0x92,          //   USAGE (Lang3 Katakana)     ; bit 8
  USAGE(1),          0x93,          //   USAGE (Lang4 Hiragana)     ; bit 9
  USAGE(1),          0x94,          //   USAGE (Lang5 Zenkaku/Hankaku) ; bit 10
  HIDINPUT(1),       0x02,          //   INPUT (Data,Var,Abs)
  REPORT_COUNT(1),   0x05,          //   REPORT_COUNT (5) — padding
  HIDINPUT(1),       0x03,          //   INPUT (Const,Var,Abs)
  END_COLLECTION(0)                 // END_COLLECTION
};

BleComboKeyboard::BleComboKeyboard(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel) : hid(0)
{
  this->deviceName = deviceName;
  this->deviceManufacturer = deviceManufacturer;
  this->batteryLevel = batteryLevel;
  this->connectionStatus = new BleConnectionStatus();
  // Initialize report characteristic pointers to nullptr so null guards are safe
  this->inputKeyboard    = nullptr;
  this->outputKeyboard   = nullptr;
  this->inputMediaKeys   = nullptr;
  this->inputSystemKeys  = nullptr;
  this->inputExtKeys     = nullptr;
  this->inputMouse       = nullptr;
  this->_systemKeyReport = 0;
  this->_begun           = false;
}

void BleComboKeyboard::begin(void)
{
  if (_begun) {
    BLEDevice::startAdvertising();
    return;
  }
  _begun = true;
  xTaskCreate(this->taskServer, "server", 20000, (void *)this, 5, NULL);
}

void BleComboKeyboard::end(void)
{
  if (_begun) {
    BLEDevice::getAdvertising()->stop();
  }
}

bool BleComboKeyboard::isConnected(void) {
  return this->connectionStatus->connected;
}

void BleComboKeyboard::setBatteryLevel(uint8_t level) {
  this->batteryLevel = level;
  if (hid != 0)
    this->hid->setBatteryLevel(this->batteryLevel);
}

void BleComboKeyboard::taskServer(void* pvParameter) {
  BleComboKeyboard* bleKeyboardInstance = (BleComboKeyboard *) pvParameter; //static_cast<BleComboKeyboard *>(pvParameter);
  BLEDevice::init(bleKeyboardInstance->deviceName);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(bleKeyboardInstance->connectionStatus);

  bleKeyboardInstance->hid = new BLEHIDDevice(pServer);
  bleKeyboardInstance->inputKeyboard = bleKeyboardInstance->hid->inputReport(KEYBOARD_ID); // <-- input REPORTID from report map
  bleKeyboardInstance->outputKeyboard = bleKeyboardInstance->hid->outputReport(KEYBOARD_ID);
  bleKeyboardInstance->inputMediaKeys  = bleKeyboardInstance->hid->inputReport(MEDIA_KEYS_ID);
  bleKeyboardInstance->inputSystemKeys = bleKeyboardInstance->hid->inputReport(SYSTEM_KEYS_ID);
  bleKeyboardInstance->inputExtKeys    = bleKeyboardInstance->hid->inputReport(EXT_KEYS_ID);
  bleKeyboardInstance->connectionStatus->inputKeyboard = bleKeyboardInstance->inputKeyboard;
  bleKeyboardInstance->connectionStatus->outputKeyboard = bleKeyboardInstance->outputKeyboard;
  
  bleKeyboardInstance->inputMouse = bleKeyboardInstance->hid->inputReport(MOUSE_ID); // <-- input REPORTID from report map
  bleKeyboardInstance->connectionStatus->inputMouse = bleKeyboardInstance->inputMouse;
 
  bleKeyboardInstance->outputKeyboard->setCallbacks(new KeyboardOutputCallbacks());

  bleKeyboardInstance->hid->manufacturer()->setValue(bleKeyboardInstance->deviceManufacturer);

  bleKeyboardInstance->hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  bleKeyboardInstance->hid->hidInfo(0x00,0x01);

  BLESecurity *pSecurity = new BLESecurity();

  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

  bleKeyboardInstance->hid->reportMap((uint8_t*)_hidReportDescriptor, sizeof(_hidReportDescriptor));
  bleKeyboardInstance->hid->startServices();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(bleKeyboardInstance->hid->hidService()->getUUID());
  pAdvertising->start();
  bleKeyboardInstance->hid->setBatteryLevel(bleKeyboardInstance->batteryLevel);

  ESP_LOGD(LOG_TAG, "Advertising started!");
  vTaskDelay(portMAX_DELAY); //delay(portMAX_DELAY);
}

void BleComboKeyboard::sendReport(BleKeyReport* keys)
{
  if (this->isConnected())
  {
    this->inputKeyboard->setValue((uint8_t*)keys, sizeof(BleKeyReport));
    this->inputKeyboard->notify();
  }
}

void BleComboKeyboard::sendReport(MediaKeyReport* keys)
{
  if (this->isConnected() && this->inputMediaKeys)
  {
    this->inputMediaKeys->setValue((uint8_t*)keys, sizeof(MediaKeyReport));
    this->inputMediaKeys->notify();
  }
}

void BleComboKeyboard::sendSystemReport(SystemKeyReport* keys)
{
  if (this->isConnected() && this->inputSystemKeys)
  {
    this->inputSystemKeys->setValue((uint8_t*)keys, sizeof(SystemKeyReport));
    this->inputSystemKeys->notify();
  }
}

extern
const uint8_t _asciimap[128] PROGMEM;

#define SHIFT 0x80
const uint8_t _asciimap[128] =
{
	0x00,             // NUL
	0x00,             // SOH
	0x00,             // STX
	0x00,             // ETX
	0x00,             // EOT
	0x00,             // ENQ
	0x00,             // ACK
	0x00,             // BEL
	0x2a,			// BS	Backspace
	0x2b,			// TAB	Tab
	0x28,			// LF	Enter
	0x00,             // VT
	0x00,             // FF
	0x00,             // CR
	0x00,             // SO
	0x00,             // SI
	0x00,             // DEL
	0x00,             // DC1
	0x00,             // DC2
	0x00,             // DC3
	0x00,             // DC4
	0x00,             // NAK
	0x00,             // SYN
	0x00,             // ETB
	0x00,             // CAN
	0x00,             // EM
	0x00,             // SUB
	0x00,             // ESC
	0x00,             // FS
	0x00,             // GS
	0x00,             // RS
	0x00,             // US

	0x2c,		   //  ' '
	0x1e|SHIFT,	   // !
	0x34|SHIFT,	   // "
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
	0x27,          // 0
	0x1e,          // 1
	0x1f,          // 2
	0x20,          // 3
	0x21,          // 4
	0x22,          // 5
	0x23,          // 6
	0x24,          // 7
	0x25,          // 8
	0x26,          // 9
	0x33|SHIFT,      // :
	0x33,          // ;
	0x36|SHIFT,      // <
	0x2e,          // =
	0x37|SHIFT,      // >
	0x38|SHIFT,      // ?
	0x1f|SHIFT,      // @
	0x04|SHIFT,      // A
	0x05|SHIFT,      // B
	0x06|SHIFT,      // C
	0x07|SHIFT,      // D
	0x08|SHIFT,      // E
	0x09|SHIFT,      // F
	0x0a|SHIFT,      // G
	0x0b|SHIFT,      // H
	0x0c|SHIFT,      // I
	0x0d|SHIFT,      // J
	0x0e|SHIFT,      // K
	0x0f|SHIFT,      // L
	0x10|SHIFT,      // M
	0x11|SHIFT,      // N
	0x12|SHIFT,      // O
	0x13|SHIFT,      // P
	0x14|SHIFT,      // Q
	0x15|SHIFT,      // R
	0x16|SHIFT,      // S
	0x17|SHIFT,      // T
	0x18|SHIFT,      // U
	0x19|SHIFT,      // V
	0x1a|SHIFT,      // W
	0x1b|SHIFT,      // X
	0x1c|SHIFT,      // Y
	0x1d|SHIFT,      // Z
	0x2f,          // [
	0x31,          // bslash
	0x30,          // ]
	0x23|SHIFT,    // ^
	0x2d|SHIFT,    // _
	0x35,          // `
	0x04,          // a
	0x05,          // b
	0x06,          // c
	0x07,          // d
	0x08,          // e
	0x09,          // f
	0x0a,          // g
	0x0b,          // h
	0x0c,          // i
	0x0d,          // j
	0x0e,          // k
	0x0f,          // l
	0x10,          // m
	0x11,          // n
	0x12,          // o
	0x13,          // p
	0x14,          // q
	0x15,          // r
	0x16,          // s
	0x17,          // t
	0x18,          // u
	0x19,          // v
	0x1a,          // w
	0x1b,          // x
	0x1c,          // y
	0x1d,          // z
	0x2f|SHIFT,    // {
	0x31|SHIFT,    // |
	0x30|SHIFT,    // }
	0x35|SHIFT,    // ~
	0				// DEL
};


uint8_t USBPutChar(uint8_t c);

// press() adds the specified key (printing, non-printing, or modifier)
// to the persistent key report and sends the report.  Because of the way
// USB HID works, the host acts like the key remains pressed until we
// call release(), releaseAll(), or otherwise clear the report and resend.
size_t BleComboKeyboard::press(uint8_t k)
{
	uint8_t i;
	if (k >= 136) {			// it's a non-printing key (not a modifier)
		k = k - 136;
	} else if (k >= 128) {	// it's a modifier key
		_keyReport.modifiers |= (1<<(k-128));
		k = 0;
	} else {				// it's a printing key
		k = pgm_read_byte(_asciimap + k);
		if (!k) {
			setWriteError();
			return 0;
		}
		if (k & 0x80) {						// it's a capital letter or other character reached with shift
			_keyReport.modifiers |= 0x02;	// the left shift modifier
			k &= 0x7F;
		}
	}

	// Add k to the key report only if it's not already present
	// and if there is an empty slot.
	if (_keyReport.keys[0] != k && _keyReport.keys[1] != k &&
		_keyReport.keys[2] != k && _keyReport.keys[3] != k &&
		_keyReport.keys[4] != k && _keyReport.keys[5] != k) {

		for (i=0; i<6; i++) {
			if (_keyReport.keys[i] == 0x00) {
				_keyReport.keys[i] = k;
				break;
			}
		}
		if (i == 6) {
			setWriteError();
			return 0;
		}
	}
	sendReport(&_keyReport);
	return 1;
}

size_t BleComboKeyboard::press(const MediaKeyReport k)
{
    _mediaKeyReport[0] |= k[0];
    _mediaKeyReport[1] |= k[1];
    _mediaKeyReport[2] |= k[2];
    _mediaKeyReport[3] |= k[3];
	sendReport(&_mediaKeyReport);
	return 1;
}

// release() takes the specified key out of the persistent key report and
// sends the report.  This tells the OS the key is no longer pressed and that
// it shouldn't be repeated any more.
size_t BleComboKeyboard::release(uint8_t k)
{
	uint8_t i;
	if (k >= 136) {			// it's a non-printing key (not a modifier)
		k = k - 136;
	} else if (k >= 128) {	// it's a modifier key
		_keyReport.modifiers &= ~(1<<(k-128));
		k = 0;
	} else {				// it's a printing key
		k = pgm_read_byte(_asciimap + k);
		if (!k) {
			return 0;
		}
		if (k & 0x80) {							// it's a capital letter or other character reached with shift
			_keyReport.modifiers &= ~(0x02);	// the left shift modifier
			k &= 0x7F;
		}
	}

	// Test the key report to see if k is present.  Clear it if it exists.
	// Check all positions in case the key is present more than once (which it shouldn't be)
	for (i=0; i<6; i++) {
		if (0 != k && _keyReport.keys[i] == k) {
			_keyReport.keys[i] = 0x00;
		}
	}

	sendReport(&_keyReport);
	return 1;
}

size_t BleComboKeyboard::release(const MediaKeyReport k)
{
    _mediaKeyReport[0] &= ~k[0];
    _mediaKeyReport[1] &= ~k[1];
    _mediaKeyReport[2] &= ~k[2];
    _mediaKeyReport[3] &= ~k[3];
	sendReport(&_mediaKeyReport);
	return 1;
}

void BleComboKeyboard::releaseAll(void)
{
	_keyReport.keys[0] = 0;
	_keyReport.keys[1] = 0;
	_keyReport.keys[2] = 0;
	_keyReport.keys[3] = 0;
	_keyReport.keys[4] = 0;
	_keyReport.keys[5] = 0;
	_keyReport.modifiers = 0;
    _mediaKeyReport[0] = 0;
    _mediaKeyReport[1] = 0;
    _mediaKeyReport[2] = 0;
    _mediaKeyReport[3] = 0;
    _systemKeyReport   = 0;
	sendReport(&_keyReport);
	sendReport(&_mediaKeyReport);
	sendSystemReport(&_systemKeyReport);
}

size_t BleComboKeyboard::write(uint8_t c)
{
	uint8_t p = press(c);  // Keydown
	release(c);            // Keyup
	return p;              // just return the result of press() since release() almost always returns 1
}

size_t BleComboKeyboard::write(const MediaKeyReport c)
{
	uint16_t p = press(c);  // Keydown
	release(c);            // Keyup
	return p;              // just return the result of press() since release() almost always returns 1
}

size_t BleComboKeyboard::pressSystemKey(SystemKeyReport k)
{
    _systemKeyReport |= k;
    sendSystemReport(&_systemKeyReport);
    return 1;
}

size_t BleComboKeyboard::releaseSystemKey(SystemKeyReport k)
{
    _systemKeyReport &= ~k;
    sendSystemReport(&_systemKeyReport);
    return 1;
}

size_t BleComboKeyboard::writeSystemKey(SystemKeyReport c)
{
    pressSystemKey(c);
    releaseSystemKey(c);
    return 1;
}

void BleComboKeyboard::writeExtKey(uint8_t b0, uint8_t b1)
{
    if (!isConnected() || !inputExtKeys) return;
    uint8_t report[2] = {b0, b1};
    inputExtKeys->setValue(report, 2);
    inputExtKeys->notify();
    uint8_t zero[2] = {0, 0};
    inputExtKeys->setValue(zero, 2);
    inputExtKeys->notify();
}

size_t BleComboKeyboard::write(const uint8_t *buffer, size_t size) {
	size_t n = 0;
	while (size--) {
		if (*buffer != '\r') {
			if (write(*buffer)) {
			  n++;
			} else {
			  break;
			}
		}
		buffer++;
	}
	return n;
}

