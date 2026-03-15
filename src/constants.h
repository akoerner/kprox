#pragma once

#include <Arduino.h>

// ---- LED color type ----
typedef struct { uint8_t r, g, b; } LEDColor;

// ---- LED duty cycles ----
#define LED_DEFAULT_DUTY_CYCLE       200
#define LED_SAVE_DUTY_CYCLE          20
#define LED_MEMORY_WDT_DUTY_CYCLE    1000
#define LED_COLOR_WIFI_CONNECTED_DUTY_CYCLE 20
#define LED_COLOR_USB_CONNECTED_DUTY_CYCLE  20

// ---- LED colors — connection status ----
#define LED_COLOR_WIFI_CONNECTED     (LEDColor){0,   255, 0}
#define LED_COLOR_WIFI_DISCONNECTED  (LEDColor){255, 165, 0}
#define LED_COLOR_WIFI_ERROR         (LEDColor){255, 0,   0}
#define LED_COLOR_BLE_CONNECTED      (LEDColor){0,   0,   255}
#define LED_COLOR_BLE_DISCONNECTED   (LEDColor){0,   0,   128}
#define LED_COLOR_USB_CONNECTED      (LEDColor){255, 255, 0}
#define LED_COLOR_USB_DISCONNECTED   (LEDColor){128, 128, 0}

// ---- LED colors — TX indicators ----
#define LED_COLOR_TX_BLE             (LEDColor){0,   0,   255}
#define LED_COLOR_TX_USB             (LEDColor){255, 255, 0}
#define LED_COLOR_TX_BOTH            (LEDColor){255, 0,   255}
#define LED_COLOR_TX_NO_CONNECTION   (LEDColor){255, 0,   0}

// ---- LED colors — system status ----
#define LED_COLOR_BOOT               (LEDColor){255, 0,   255}
#define LED_COLOR_HALT               (LEDColor){255, 255, 0}
#define LED_COLOR_RESUME             (LEDColor){0,   255, 0}
#define LED_COLOR_ACTIVITY           (LEDColor){255, 255, 255}
#define LED_COLOR_SAVE               (LEDColor){255, 153, 0}

// ---- LED colors — BT/USB control ----
#define LED_COLOR_BT_ENABLE          (LEDColor){0,   0,   255}
#define LED_COLOR_BT_DISABLE         (LEDColor){255, 165, 0}
#define LED_COLOR_USB_ENABLE         (LEDColor){255, 255, 0}
#define LED_COLOR_USB_DISABLE        (LEDColor){255, 128, 0}

// ---- LED colors — register operations ----
#define LED_COLOR_REG_PLAY           (LEDColor){255, 255, 0}
#define LED_COLOR_REG_CHANGE         (LEDColor){52,  235, 161}
#define LED_COLOR_REG_DELETE         (LEDColor){255, 0,   0}
#define LED_COLOR_REG_DELETE_ALL     (LEDColor){255, 0,   0}
#define LED_COLOR_BLINK              (LEDColor){0,   0,   255}
#define LED_COLOR_MEMORY_WDT         (LEDColor){0,   0,   255}

// ---- LED colors — WiFi ops ----
#define LED_COLOR_WIFI_CONNECTING    (LEDColor){255, 165, 0}
#define LED_COLOR_WIFI_SUCCESS       (LEDColor){0,   255, 0}
#define LED_COLOR_WIFI_FAILED        (LEDColor){255, 0,   0}

// ---- LED colors — mDNS ----
#define LED_COLOR_MDNS_SUCCESS       (LEDColor){0,   255, 255}
#define LED_COLOR_MDNS_FAILED        (LEDColor){255, 0,   255}

// ---- HID keycodes ----
#ifndef KEY_RETURN
#  define KEY_RETURN        0xB0
#endif
#ifndef KEY_ESCAPE
#  define KEY_ESCAPE        0xB1
#endif
#ifndef KEY_BACKSPACE
#  define KEY_BACKSPACE     0xB2
#endif
#ifndef KEY_TAB
#  define KEY_TAB           0xB3
#endif
#ifndef KEY_SPACE
#  define KEY_SPACE         0x20
#endif
#ifndef KEY_DELETE
#  define KEY_DELETE        0xD4
#endif
#ifndef KEY_RIGHT_ARROW
#  define KEY_RIGHT_ARROW   0xD7
#endif
#ifndef KEY_LEFT_ARROW
#  define KEY_LEFT_ARROW    0xD8
#endif
#ifndef KEY_DOWN_ARROW
#  define KEY_DOWN_ARROW    0xD9
#endif
#ifndef KEY_UP_ARROW
#  define KEY_UP_ARROW      0xDA
#endif
#ifndef KEY_LEFT_CTRL
#  define KEY_LEFT_CTRL     0x80
#endif
#ifndef KEY_LEFT_SHIFT
#  define KEY_LEFT_SHIFT    0x81
#endif
#ifndef KEY_LEFT_ALT
#  define KEY_LEFT_ALT      0x82
#endif
#ifndef KEY_LEFT_GUI
#  define KEY_LEFT_GUI      0x83
#endif
#ifndef KEY_RIGHT_CTRL
#  define KEY_RIGHT_CTRL    0x84
#endif
#ifndef KEY_RIGHT_SHIFT
#  define KEY_RIGHT_SHIFT   0x85
#endif
#ifndef KEY_RIGHT_ALT
#  define KEY_RIGHT_ALT     0x86
#endif
#ifndef KEY_RIGHT_GUI
#  define KEY_RIGHT_GUI     0x87
#endif
#ifndef KEY_INSERT
#  define KEY_INSERT        0x49
#endif
#ifndef KEY_HOME
#  define KEY_HOME          0x4A
#endif
#ifndef KEY_PAGE_UP
#  define KEY_PAGE_UP       0x4B
#endif
#ifndef KEY_PAGE_DOWN
#  define KEY_PAGE_DOWN     0x4E
#endif
#ifndef KEY_END
#  define KEY_END           0x4D
#endif
#ifndef KEY_PRINTSCREEN
#  define KEY_PRINTSCREEN   0xCE
#endif
#ifndef KEY_SYSRQ
#  define KEY_SYSRQ         0xCE
#endif
#ifndef KEY_F1
#  define KEY_F1  0x3A
#  define KEY_F2  0x3B
#  define KEY_F3  0x3C
#  define KEY_F4  0x3D
#  define KEY_F5  0x3E
#  define KEY_F6  0x3F
#  define KEY_F7  0x40
#  define KEY_F8  0x41
#  define KEY_F9  0x42
#  define KEY_F10 0x43
#  define KEY_F11 0x44
#  define KEY_F12 0x45
#endif
#ifdef KEY_ESC
#  undef KEY_ESC
#endif
#define KEY_ESC KEY_ESCAPE

// ---- Extended key constants (Arduino HID library encoding: 136 + HID_usage) ----
// BleComboKeyboard.h already defines KEY_CAPS_LOCK=0xC1 and KEY_F1-F24=0xC2-0xFB as
// const uint8_t values using this same encoding. We add the keys it omits. These values
// work identically for both BLE (BleComboKeyboard::press) and USB (USBHIDKeyboard::press).
//
// Encoding: press(k) with k>=136 sends HID usage (k-136). With k<136 the value is
// treated as ASCII and run through _asciimap — which is why raw HID usages (0x39 etc.)
// produced wrong or no output.

// KEY_CAPS_LOCK (0xC1) is already defined in BleComboKeyboard.h — do not redefine.

#ifndef KEY_SCROLL_LOCK
#  define KEY_SCROLL_LOCK   0xCF   // HID usage 0x47
#endif
#ifndef KEY_PAUSE
#  define KEY_PAUSE         0xD0   // HID usage 0x48
#endif
#ifndef KEY_NUM_LOCK
#  define KEY_NUM_LOCK      0xDB   // HID usage 0x53
#endif
#ifndef KEY_APPLICATION
#  define KEY_APPLICATION   0xED   // HID usage 0x65
#endif

// Numpad (all encoded as 136 + HID usage)
#ifndef KEY_KP_DIVIDE
#  define KEY_KP_DIVIDE     0xDC   // HID 0x54
#  define KEY_KP_MULTIPLY   0xDD   // HID 0x55
#  define KEY_KP_MINUS      0xDE   // HID 0x56
#  define KEY_KP_PLUS       0xDF   // HID 0x57
#  define KEY_KP_ENTER      0xE0   // HID 0x58
#  define KEY_KP1           0xE1   // HID 0x59
#  define KEY_KP2           0xE2   // HID 0x5A
#  define KEY_KP3           0xE3   // HID 0x5B
#  define KEY_KP4           0xE4   // HID 0x5C
#  define KEY_KP5           0xE5   // HID 0x5D
#  define KEY_KP6           0xE6   // HID 0x5E
#  define KEY_KP7           0xE7   // HID 0x5F
#  define KEY_KP8           0xE8   // HID 0x60
#  define KEY_KP9           0xE9   // HID 0x61
#  define KEY_KP0           0xEA   // HID 0x62
#  define KEY_KP_DOT        0xEB   // HID 0x63
#endif

// F13-F24 are already defined in BleComboKeyboard.h as 0xF0-0xFB. Do not redefine.

// ---- Timing constants (ms) ----
#define KEY_PRESS_DELAY          25
#define KEY_RELEASE_DELAY        15
#define BETWEEN_KEYS_DELAY       30
#define BETWEEN_SEND_TEXT_DELAY  100
#define SPECIAL_KEY_DELAY        50
#define TOKEN_DELAY              100
