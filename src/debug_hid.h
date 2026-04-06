#pragma once
#include "config.h"   // defines BOARD_HAS_USB_HID before the guard below is evaluated

// USB-HID debug logger.
// Enable by adding -DDEBUG_HID=1 to build_flags in platformio.ini.
//
// Output is sent as USB HID keyboard keystrokes so any text editor open
// on the host PC receives the log as typed text — i.e. HID as UART.
//
// RTC_DATA_ATTR is used for the log buffer so it survives soft reset and
// watchdog reboots.  On the next successful boot the previous boot's full
// log is replayed first, which captures exactly what happened before a crash.

#if defined(DEBUG_HID) && defined(BOARD_HAS_USB_HID)

void debugHidInit();       // call as the very first statement in setup()
void debugHidFlush();      // call right after USBKeyboard.begin()
void debugHidLog(const char* tag, const char* msg);
void debugHidLogHeap(const char* tag);

#define DHID(t, m)    debugHidLog(t, m)
#define DHID_HEAP(t)  debugHidLogHeap(t)

#else

#define debugHidInit()        ((void)0)
#define debugHidFlush()       ((void)0)
#define debugHidLog(t, m)     ((void)0)
#define debugHidLogHeap(t)    ((void)0)
#define DHID(t, m)            ((void)0)
#define DHID_HEAP(t)          ((void)0)

#endif
