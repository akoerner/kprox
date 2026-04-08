// BIOS/GRUB USB HID boot keyboard compatibility.
//
// arduino-esp32 3.x passes the config descriptor to tinyusb_driver_install()
// as a non-NULL pointer in tinyusb_config_t. When non-NULL, ESP-IDF TinyUSB
// serves GET_DESCRIPTOR and drives process_set_config from that static buffer
// directly — tud_descriptor_*_cb() are never called. --wrap on those callbacks
// was therefore ineffective. Fix: call the callbacks ourselves after USB.begin()
// to obtain the RAM buffer pointer, then patch in place.
//
// Patches applied to the live descriptor buffer:
//   Device:     bDeviceClass/Sub/Protocol → 0/0/0   (per-interface, not 0xEF)
//   Interface:  bInterfaceSubClass        → 1        (Boot Interface)
//               bInterfaceProtocol        → 1        (Keyboard)
//   Config:     bmAttributes              → 0xA0     (bus-powered + remote wakeup;
//                                                     0xC0 = self-powered, some BIOS skip)
//               MaxPower                 → 50       (100mA; 500mA refused by strict BIOS)
//   HID IN EP:  bInterval                → 10       (10ms; standard keyboard rate;
//                                                     1ms can fail BIOS scheduler)
//               wMaxPacketSize           → 8        (USB HID boot keyboard mandatory)
//   HID OUT EP: wMaxPacketSize           → 1        (boot LED report: 1 byte)

#ifdef BOARD_M5STACK_CARDPUTER

#include <stdint.h>
#include <string.h>

extern "C" uint8_t const* tud_descriptor_device_cb(void);
extern "C" uint8_t const* tud_descriptor_configuration_cb(uint8_t index);

extern uint16_t usbVidOverride;
extern uint16_t usbPidOverride;

void usbBiosCompatPatchDescriptors(void) {
    uint8_t* dev = (uint8_t*)tud_descriptor_device_cb();
    if (dev) {
        dev[4] = 0;     // bDeviceClass    = 0x00 (per-interface)
        dev[5] = 0;     // bDeviceSubClass
        dev[6] = 0;     // bDeviceProtocol
        // Apply user-configured VID:PID override (Settings → Device Identity).
        // Defaults to Logitech K120 (0x046D:0xC31C) — widely whitelisted in
        // UEFI firmware security policies. Set to 0x0000:0x0000 to disable.
        if (usbVidOverride || usbPidOverride) {
            dev[8]  =  usbVidOverride        & 0xFF;
            dev[9]  = (usbVidOverride >> 8)  & 0xFF;
            dev[10] =  usbPidOverride        & 0xFF;
            dev[11] = (usbPidOverride >> 8)  & 0xFF;
        }
    }

    uint8_t* cfg = (uint8_t*)tud_descriptor_configuration_cb(0);
    if (!cfg) return;

    cfg[7] = 0xA0;  // bmAttributes: bus-powered + remote wakeup (not 0xC0 self-powered)
    cfg[8] = 50;    // bMaxPower: 100mA (250 = 500mA refused by strict BIOS power budgets)

    uint16_t total = (uint16_t)cfg[2] | ((uint16_t)cfg[3] << 8);
    uint8_t* p   = cfg;
    uint8_t* end = cfg + total;

    while (p < end && p[0] > 0) {
        if (p[1] == 0x04 && p[5] == 0x03) {    // Interface descriptor, HID class
            p[6] = 1;   // bInterfaceSubClass = 1 (Boot Interface)
            p[7] = 1;   // bInterfaceProtocol = 1 (Keyboard)

            uint8_t* q = p + p[0];
            while (q < end && q[0] > 0 && q[1] != 0x04) {
                if (q[1] == 0x05) {
                    if (q[2] & 0x80) {
                        q[4] = 8; q[5] = 0; q[6] = 10;
                    } else {
                        q[4] = 1; q[5] = 0;
                    }
                }
                q += q[0];
            }
            break;
        }
        p += p[0];
    }
}

// ── Boot-protocol report handling ─────────────────────────────────────────────

static volatile uint8_t s_hid_protocol = 1;  // matches TinyUSB default (REPORT)

extern "C" void tud_hid_set_protocol_cb(uint8_t itf, uint8_t protocol) {
    (void)itf; s_hid_protocol = protocol;
}
extern "C" void tud_hid_n_set_protocol_cb(uint8_t instance, uint8_t protocol) {
    (void)instance; s_hid_protocol = protocol;
}

extern "C" bool __real_tud_hid_n_report(uint8_t, uint8_t, void const*, uint16_t);

extern "C" bool __wrap_tud_hid_n_report(uint8_t     instance,
                                         uint8_t     report_id,
                                         void const* report,
                                         uint16_t    len) {
    if (s_hid_protocol == 0)
        return __real_tud_hid_n_report(instance, 0, report, 8);
    return __real_tud_hid_n_report(instance, report_id, report, len);
}

#endif
