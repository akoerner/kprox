#include "../config.h"
#ifdef BOARD_HAS_USB_HID
#include "USBHIDFIDO2.h"
#include <Arduino.h>

USBHIDFIDO2 FIDO2Device;

USBHIDFIDO2::USBHIDFIDO2() {}

void USBHIDFIDO2::begin() {
    if (_begun) return;
    _hid.addDevice(this, sizeof(FIDO2_HID_REPORT_DESCRIPTOR));
    _hid.begin();
    _begun = true;
}

void USBHIDFIDO2::end() {
    _begun = false;
}

uint16_t USBHIDFIDO2::_onGetDescriptor(uint8_t* buf) {
    memcpy(buf, FIDO2_HID_REPORT_DESCRIPTOR, sizeof(FIDO2_HID_REPORT_DESCRIPTOR));
    return sizeof(FIDO2_HID_REPORT_DESCRIPTOR);
}

bool USBHIDFIDO2::sendPacket(const uint8_t* data, size_t len) {
    if (!_begun) return false;
    uint8_t pkt[64] = {};
    memcpy(pkt, data, len < 64 ? len : 64);
    return _hid.SendReport(0, pkt, 64);
}

void USBHIDFIDO2::_onOutput(uint8_t report_id, const uint8_t* buf, uint16_t len) {
    onPacket(buf, len);
}
#endif // BOARD_HAS_USB_HID
