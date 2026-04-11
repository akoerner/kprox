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
//               bcdUSB                   → 1.10     (BIOS expects USB 1.1 boot keyboard;
//                                                     2.00 rejected by most BIOS HID stacks)
//               iSerialNumber            → 0        (suppress serial; extra string fetch
//                                                     stalls some ThinkPad BIOS versions)
//   Interface:  bInterfaceSubClass        → 1        (Boot Interface)
//               bInterfaceProtocol        → 1        (Keyboard)
//               iInterface               → 0        (suppress interface string)
//   Config:     bmAttributes              → 0xA0     (bus-powered + remote wakeup;
//                                                     0xC0 = self-powered, some BIOS skip)
//               MaxPower                 → 50       (100mA; 500mA refused by strict BIOS)
//               iConfiguration           → 0        (suppress config string)
//   HID IN EP:  bInterval                → 10       (10ms; standard keyboard polling rate)
//               wMaxPacketSize           → 8        (USB HID boot keyboard mandatory)
//               (IN moved before OUT in stream; BIOS walks endpoints and uses
//                the first interrupt endpoint as keyboard input — if OUT is
//                first the BIOS polls it and receives no reports)
//   HID OUT EP: wMaxPacketSize           → 1        (boot LED report: 1 byte)

#ifdef BOARD_M5STACK_CARDPUTER

#include <stdint.h>
#include <string.h>
#include <Preferences.h>

extern "C" uint8_t const* tud_descriptor_device_cb(void);
extern "C" uint8_t const* tud_descriptor_configuration_cb(uint8_t index);

extern uint16_t usbVidOverride;
extern uint16_t usbPidOverride;

void usbBiosCompatPatchDescriptors(void) {
    uint8_t* dev = (uint8_t*)tud_descriptor_device_cb();
    if (dev) {
        dev[2] = 0x10;  // bcdUSB = 1.10 (BIOS boot keyboard profile; 2.00 rejected by most BIOS)
        dev[3] = 0x01;
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
        dev[16] = 0;    // iSerialNumber: suppress — extra string GET_DESCRIPTOR stalls some BIOS
    }

    uint8_t* cfg = (uint8_t*)tud_descriptor_configuration_cb(0);
    if (!cfg) return;

    cfg[6] = 0;     // iConfiguration: suppress — matches working boot keyboard profile
    cfg[7] = 0xA0;  // bmAttributes: bus-powered + remote wakeup (not 0xC0 self-powered)
    cfg[8] = 50;    // bMaxPower: 100mA (250 = 500mA refused by strict BIOS power budgets)

    uint16_t total = (uint16_t)cfg[2] | ((uint16_t)cfg[3] << 8);
    uint8_t* p   = cfg;
    uint8_t* end = cfg + total;

    while (p < end && p[0] > 0) {
        if (p[1] == 0x04 && p[5] == 0x03) {    // Interface descriptor, HID class
            p[6] = 1;   // bInterfaceSubClass = 1 (Boot Interface)
            { Preferences _p; _p.begin("kprox", true); p[7] = _p.getUChar("hidProto", 1); _p.end(); }
            p[8] = 0;   // iInterface: suppress

            uint8_t* ep_out = nullptr;
            uint8_t* ep_in  = nullptr;
            uint8_t* q = p + p[0];
            while (q < end && q[0] > 0 && q[1] != 0x04) {
                if (q[1] == 0x05) {
                    if (q[2] & 0x80) { q[4] = 8; q[5] = 0; q[6] = 10; ep_in  = q; }
                    else             { q[4] = 1; q[5] = 0;              ep_out = q; }
                }
                q += q[0];
            }
            // Move IN before OUT so the BIOS finds the keyboard input endpoint first
            if (ep_out && ep_in && ep_out < ep_in) {
                uint8_t tmp[7];
                memcpy(tmp,    ep_out, 7);
                memcpy(ep_out, ep_in,  7);
                memcpy(ep_in,  tmp,    7);
            }
            break;
        }
        p += p[0];
    }
}

// ── Boot-protocol report handling ─────────────────────────────────────────────

static volatile uint8_t s_hid_protocol = 1;  // matches TinyUSB default (REPORT)

// Reset to report protocol on every new SET_CONFIGURATION. Without this,
// s_hid_protocol stays 0 after BIOS boot-protocol use, causing the OS to
// receive raw 8-byte boot reports instead of report-ID-prefixed HID reports.
extern "C" void tud_mount_cb(void) {
    s_hid_protocol = 1;
}

// On disconnect, persist current protocol so the next host gets the right
// bInterfaceProtocol: 0 after BIOS boot-protocol session, 1 after OS.
extern "C" void tud_umount_cb(void) {
    uint8_t* cfg = (uint8_t*)tud_descriptor_configuration_cb(0);
    if (!cfg) return;
    uint16_t total = (uint16_t)cfg[2] | ((uint16_t)cfg[3] << 8);
    uint8_t* p = cfg, *end = cfg + total;
    while (p < end && p[0] > 0) {
        if (p[1] == 0x04 && p[5] == 0x03) {
            p[7] = (s_hid_protocol == 0) ? 0 : 1;
            { Preferences _p; _p.begin("kprox", false); _p.putUChar("hidProto", p[7]); _p.end(); }
            break;
        }
        p += p[0];
    }
}

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
