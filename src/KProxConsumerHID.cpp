#include "config.h"
#ifdef BOARD_HAS_USB_HID
#include <Arduino.h>
#include <USB.h>
#include <USBHID.h>
#include "KProxConsumerHID.h"

#ifndef BOARD_M5STACK_CARDPUTER
// atoms3: defined here. Cardputer defines it in main.cpp after
// USBHIDKeyboard USBKeyboard to control TinyUSB HID slot 0 ordering.
KProxConsumerHID KProxConsumer;
#endif

#endif
