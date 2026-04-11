#pragma once
#include "config.h"
#ifdef BOARD_HAS_USB_HID

#include <Arduino.h>
#include <USB.h>
#include <USBHID.h>

#define KPROX_CONSUMER_REPORT_ID  3
#define KPROX_SYSTEM_REPORT_ID    4
#define KPROX_EXT_REPORT_ID       5

// Each collection gets its own USB HID interface so Linux creates a separate
// evdev node per collection instead of collapsing them into one.

static const uint8_t KPROX_CONSUMER_ONLY_DESC[] = {
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,
    0x85, KPROX_CONSUMER_REPORT_ID,
    0x05, 0x0C, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x19,
    // byte 0: NextTrack PrevTrack Stop PlayPause Mute VolUp VolDown WWWHome
    0x09, 0xB5, 0x09, 0xB6, 0x09, 0xB7, 0x09, 0xCD,
    0x09, 0xE2, 0x09, 0xE9, 0x09, 0xEA, 0x0A, 0x23, 0x02,
    // byte 1: MyComputer Calculator Bookmarks Search WWWStop WWWBack MediaSel Mail
    0x0A, 0x94, 0x01, 0x0A, 0x92, 0x01,
    0x0A, 0x2A, 0x02, 0x0A, 0x21, 0x02,
    0x0A, 0x26, 0x02, 0x0A, 0x24, 0x02,
    0x0A, 0x83, 0x01, 0x0A, 0x8A, 0x01,
    // byte 2: WWWForward WWWRefresh BrightnessUp BrightnessDown KbdIllumToggle KbdIllumDown KbdIllumUp EjectCD
    0x0A, 0x25, 0x02, 0x0A, 0x27, 0x02,
    0x09, 0x6F, 0x09, 0x70,
    0x09, 0x77, 0x09, 0x78, 0x09, 0x79,
    0x09, 0xB8,
    // byte 3 bit 0: ScreenLock
    0x0A, 0x9E, 0x01,
    0x81, 0x02,
    0x95, 0x07, 0x81, 0x03,
    0xC0
};

static const uint8_t KPROX_SYSTEM_ONLY_DESC[] = {
    0x05, 0x01, 0x09, 0x80, 0xA1, 0x01,
    0x85, KPROX_SYSTEM_REPORT_ID,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x03,
    0x09, 0x81, 0x09, 0x82, 0x09, 0x83,
    0x81, 0x02,
    0x75, 0x05, 0x95, 0x01, 0x81, 0x03,
    0xC0
};

static const uint8_t KPROX_EXTKEY_ONLY_DESC[] = {
    0x05, 0x07, 0x09, 0x01, 0xA1, 0x01,
    0x85, KPROX_EXT_REPORT_ID,
    0x05, 0x07, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0B,
    // byte0: KpComma RO KatakanaHiragana Yen Henkan Muhenkan Hanguel Hanja
    0x09, 0x85, 0x09, 0x87, 0x09, 0x88, 0x09, 0x89,
    0x09, 0x8A, 0x09, 0x8B, 0x09, 0x90, 0x09, 0x91,
    // byte1 bits 0-2: Katakana Hiragana Zenkaku/Hankaku
    0x09, 0x92, 0x09, 0x93, 0x09, 0x94,
    0x81, 0x02,
    0x95, 0x05, 0x81, 0x03,
    0xC0
};

// One USB HID interface per collection → one evdev node per collection.
class KProxHIDSubDev : public USBHIDDevice {
public:
    USBHID         _hid;
    const uint8_t* _desc   = nullptr;
    uint16_t       _len    = 0;
    bool           _active = false;
    bool           _begun  = false;

    void preinit(const uint8_t* desc, uint16_t len) {
        _desc   = desc;
        _len    = len;
        _active = true;
        _hid.addDevice(this, len);
    }

    void begin() {
        if (_active && !_begun) { _hid.begin(); _begun = true; }
    }

    bool send(uint8_t id, const uint8_t* data, uint8_t dlen) {
        return _begun && _hid.SendReport(id, data, dlen);
    }

    uint16_t _onGetDescriptor(uint8_t* buf) override {
        if (_desc) memcpy(buf, _desc, _len);
        return _len ? _len : 0;
    }
};

class KProxConsumerHID {
public:
    KProxHIDSubDev _consumer;
    KProxHIDSubDev _system;
    KProxHIDSubDev _extKey;

    // Call after loading USB settings, BEFORE USB.begin().
    // Only registers the sub-devices that are enabled; each becomes a separate
    // USB HID interface and therefore a separate evdev node under Linux.
    void preinit(bool consumer, bool system, bool extkey) {
        if (consumer) _consumer.preinit(KPROX_CONSUMER_ONLY_DESC, sizeof(KPROX_CONSUMER_ONLY_DESC));
        if (system)   _system.preinit(KPROX_SYSTEM_ONLY_DESC,     sizeof(KPROX_SYSTEM_ONLY_DESC));
        if (extkey)   _extKey.preinit(KPROX_EXTKEY_ONLY_DESC,     sizeof(KPROX_EXTKEY_ONLY_DESC));
    }

    void begin() {
        _consumer.begin();
        _system.begin();
        _extKey.begin();
    }

    bool isReady() const {
        return _consumer._begun || _system._begun || _extKey._begun;
    }

    bool sendConsumer(uint8_t b0, uint8_t b1, uint8_t b2 = 0, uint8_t b3 = 0) {
        uint8_t r[4] = {b0, b1, b2, b3};
        return _consumer.send(KPROX_CONSUMER_REPORT_ID, r, 4);
    }

    bool sendSystem(uint8_t bits) {
        uint8_t r[1] = {bits};
        return _system.send(KPROX_SYSTEM_REPORT_ID, r, 1);
    }

    bool sendExtKey(uint8_t b0, uint8_t b1) {
        uint8_t r[2] = {b0, b1};
        if (!_extKey.send(KPROX_EXT_REPORT_ID, r, 2)) return false;
        uint8_t z[2] = {0, 0};
        return _extKey.send(KPROX_EXT_REPORT_ID, z, 2);
    }
};

extern KProxConsumerHID KProxConsumer;
#endif
