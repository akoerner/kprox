#pragma once
#include "config.h"
#ifdef BOARD_HAS_USB_HID

#include <Arduino.h>
#include <USB.h>
#include <USBHID.h>

#define KPROX_CONSUMER_REPORT_ID  3
#define KPROX_SYSTEM_REPORT_ID    4

static const uint8_t KPROX_CONSUMER_DESC[] = {
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,
    0x85, KPROX_CONSUMER_REPORT_ID,
    0x05, 0x0C, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10,
    0x09, 0xB5, 0x09, 0xB6, 0x09, 0xB7, 0x09, 0xCD,
    0x09, 0xE2, 0x09, 0xE9, 0x09, 0xEA,
    0x0A, 0x23, 0x02, 0x0A, 0x94, 0x01, 0x0A, 0x92, 0x01,
    0x0A, 0x2A, 0x02, 0x0A, 0x21, 0x02,
    0x0A, 0x26, 0x02, 0x0A, 0x24, 0x02,
    0x0A, 0x83, 0x01, 0x0A, 0x8A, 0x01,
    0x81, 0x02, 0xC0,
    0x05, 0x01, 0x09, 0x80, 0xA1, 0x01,
    0x85, KPROX_SYSTEM_REPORT_ID,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x03,
    0x09, 0x81, 0x09, 0x82, 0x09, 0x83,
    0x81, 0x02,
    0x75, 0x05, 0x95, 0x01, 0x81, 0x03,
    0xC0
};

class KProxConsumerHID : public USBHIDDevice {
public:
    // addDevice() called in constructor so it runs at static init time —
    // BEFORE USB.begin() builds the config descriptor. This ensures the
    // consumer+system descriptor bytes are included in tinyusb_hid_device_descriptor_len
    // when the host requests the USB configuration descriptor.
    KProxConsumerHID() : _begun(false) {
        _hid.addDevice(this, sizeof(KPROX_CONSUMER_DESC));
    }

    // begin() creates the send semaphore (needed by SendReport)
    void begin() {
        if (_begun) return;
        _hid.begin();
        _begun = true;
    }

    bool sendConsumer(uint8_t b0, uint8_t b1) {
        if (!_begun) return false;
        uint8_t r[2] = {b0, b1};
        return _hid.SendReport(KPROX_CONSUMER_REPORT_ID, r, 2);
    }

    bool sendSystem(uint8_t bits) {
        if (!_begun) return false;
        uint8_t r[1] = {bits};
        return _hid.SendReport(KPROX_SYSTEM_REPORT_ID, r, 1);
    }

    bool isReady() const { return _begun; }

    uint16_t _onGetDescriptor(uint8_t* buf) override {
        memcpy(buf, KPROX_CONSUMER_DESC, sizeof(KPROX_CONSUMER_DESC));
        return sizeof(KPROX_CONSUMER_DESC);
    }

private:
    USBHID _hid;
    bool   _begun;
};

extern KProxConsumerHID KProxConsumer;
#endif
