#include "debug_hid.h"  // pulls in config.h, which defines BOARD_HAS_USB_HID

#if defined(DEBUG_HID) && defined(BOARD_HAS_USB_HID)

#include <Arduino.h>
#include <USBHIDKeyboard.h>

extern USBHIDKeyboard USBKeyboard;

// ---------------------------------------------------------------------------
// RTC slow memory: survives watchdog reset and soft reset.
// _rtcLog accumulates log lines across reboots, letting us reconstruct
// the full call sequence of a boot that never completed USB init.
// ---------------------------------------------------------------------------
static RTC_DATA_ATTR char     _rtcLog[3072];
static RTC_DATA_ATTR uint16_t _rtcLen;
static RTC_DATA_ATTR uint32_t _bootN;

// Pre-USB in-RAM queue: messages before keyboard is ready
static char  _q[2048];
static int   _qLen   = 0;
static bool  _live   = false;  // true after debugHidFlush()

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void _rtcAppend(const char* s, int n) {
    if ((int)_rtcLen + n >= (int)sizeof(_rtcLog) - 1) {
        uint16_t drop = sizeof(_rtcLog) / 4;
        memmove(_rtcLog, _rtcLog + drop, _rtcLen - drop);
        _rtcLen -= drop;
    }
    memcpy(_rtcLog + _rtcLen, s, n);
    _rtcLen += n;
    _rtcLog[_rtcLen] = '\0';
}

static void _qAppend(const char* s, int n) {
    int space = (int)sizeof(_q) - _qLen - 1;
    if (n > space) n = space;
    if (n <= 0) return;
    memcpy(_q + _qLen, s, n);
    _qLen += n;
    _q[_qLen] = '\0';
}

// Type a string via USB HID keyboard — this is the "UART" output path.
// \n is translated to KEY_RETURN; everything else is sent as-is.
static void _hidType(const char* s) {
    for (; *s; ++s) {
        if (*s == '\n') {
            USBKeyboard.write(KEY_RETURN);
        } else if (*s != '\r') {
            USBKeyboard.write((uint8_t)*s);
        }
        delayMicroseconds(3000);
    }
}

static void _hidBanner(const char* s) {
    _hidType("\n"); _hidType(s); _hidType("\n");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void debugHidInit() {
    ++_bootN;
    _qLen   = 0;
    _q[0]   = '\0';
    _live   = false;

    char hdr[32];
    int n = snprintf(hdr, sizeof(hdr), "=BOOT%lu=\n", (unsigned long)_bootN);
    _rtcAppend(hdr, n);
    _qAppend(hdr, n);
}

void debugHidLog(const char* tag, const char* msg) {
    char line[160];
    int n = snprintf(line, sizeof(line), "%07lums %-10s %s\n",
                     (unsigned long)millis(), tag, msg);
    if (n > (int)sizeof(line) - 1) n = (int)sizeof(line) - 1;

    _rtcAppend(line, n);

    if (_live) {
        _hidType(line);
    } else {
        _qAppend(line, n);
    }
}

void debugHidLogHeap(const char* tag) {
    char msg[64];
    snprintf(msg, sizeof(msg), "free=%u maxBlk=%u",
             (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMaxAllocHeap());
    debugHidLog(tag, msg);
}

// Call immediately after USBKeyboard.begin().
// Waits for USB enumeration, then replays the previous boot's crash log
// followed by this boot's buffered pre-USB messages, then switches to
// live streaming for all subsequent debugHidLog() calls.
void debugHidFlush() {
    _live = true;

    // Allow USB host time to enumerate the HID device
    delay(2500);

    _hidBanner("=== HID DEBUG LOG ===");

    // --- Previous boot crash context ---
    if (_bootN > 1 && _rtcLen > 0) {
        _hidBanner("--- PREV BOOT ---");

        // The RTC buffer contains entries from all previous boots plus the
        // current boot's header.  Find the current boot's marker to split.
        char marker[24];
        snprintf(marker, sizeof(marker), "=BOOT%lu=", (unsigned long)_bootN);
        char* cur = strstr(_rtcLog, marker);
        if (cur && cur > _rtcLog) {
            char saved = *cur;
            *cur = '\0';
            _hidType(_rtcLog);
            *cur = saved;
        } else {
            _hidType(_rtcLog);
        }

        _hidBanner("--- END PREV ---");
    }

    // --- This boot: messages captured before USB was ready ---
    _hidBanner("--- THIS BOOT ---");
    if (_qLen > 0) _hidType(_q);
    _qLen = 0;
    _q[0] = '\0';

    _hidBanner("--- LIVE ---");
}

#endif // DEBUG_HID && BOARD_HAS_USB_HID
