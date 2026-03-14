#pragma once
// FIDO2 / CTAP2 USB HID device for ESP32 (TinyUSB / Arduino ESP32 USB stack)
// Presents as HID usage page 0xF1D0 (FIDO Alliance), usage 0x01
// Report size: 64 bytes in, 64 bytes out (standard CTAP HID framing)

#include "../config.h"
#ifdef BOARD_HAS_USB_HID
#include <USB.h>
#include <USBHID.h>

// FIDO2 HID report descriptor — usage page 0xF1D0, usage 0x01
static const uint8_t FIDO2_HID_REPORT_DESCRIPTOR[] = {
    0x06, 0xD0, 0xF1,   // Usage Page (FIDO Alliance)
    0x09, 0x01,         // Usage (Keyboard — reused as CTAP authenticator)
    0xA1, 0x01,         // Collection (Application)
    0x09, 0x20,         //   Usage (Input Report Data)
    0x15, 0x00,         //   Logical Minimum (0)
    0x26, 0xFF, 0x00,   //   Logical Maximum (255)
    0x75, 0x08,         //   Report Size (8)
    0x95, 0x40,         //   Report Count (64)
    0x81, 0x02,         //   Input (Data, Variable, Absolute)
    0x09, 0x21,         //   Usage (Output Report Data)
    0x15, 0x00,         //   Logical Minimum (0)
    0x26, 0xFF, 0x00,   //   Logical Maximum (255)
    0x75, 0x08,         //   Report Size (8)
    0x95, 0x40,         //   Report Count (64)
    0x91, 0x02,         //   Output (Data, Variable, Absolute)
    0xC0                // End Collection
};

class USBHIDFIDO2 : public USBHIDDevice {
public:
    USBHIDFIDO2();
    void begin();
    void end();

    // Send a 64-byte CTAP HID packet to the host
    bool sendPacket(const uint8_t* data, size_t len = 64);

    // Callback — override to handle host->device packets
    virtual void onPacket(const uint8_t* data, size_t len) {}

    // USBHIDDevice interface
    uint16_t _onGetDescriptor(uint8_t* buf) override;
    void _onOutput(uint8_t report_id, const uint8_t* buf, uint16_t len) override;

private:
    USBHID _hid;
    bool   _begun = false;
};

extern USBHIDFIDO2 FIDO2Device;
#endif // BOARD_HAS_USB_HID
