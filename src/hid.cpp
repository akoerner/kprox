#include "hid.h"
#include "led.h"
#include "keymap.h"

// ---- Internal HID dispatch ----

static bool anyHIDConnected() { return isBLEConnected() || isUSBConnected(); }

static bool bleKbActive()    { return isBLEConnected() && bleKeyboardEnabled; }
static bool bleMouseActive() { return isBLEConnected() && bleMouseEnabled; }

static void hidPrint(const String& text) {
    if (bleKbActive()) BLE_KEYBOARD.print(text);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && usbKeyboardEnabled) USBKeyboard.print(text);
#endif
}

static void hidPress(uint8_t key) {
    if (bleKbActive()) BLE_KEYBOARD.press(key);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && usbKeyboardEnabled) USBKeyboard.press(key);
#endif
}

static void hidRelease(uint8_t key) {
    if (bleKbActive()) BLE_KEYBOARD.release(key);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && usbKeyboardEnabled) USBKeyboard.release(key);
#endif
}

void hidReleaseAll() {
    if (bleKbActive()) BLE_KEYBOARD.releaseAll();
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected()) {
        if (usbKeyboardEnabled) USBKeyboard.releaseAll();
        if (KProxConsumer.isReady()) { KProxConsumer.sendConsumer(0,0); KProxConsumer.sendSystem(0); }
    }
#endif
}

static void hidMouseMoveStep(int8_t x, int8_t y) {
    if (bleMouseActive()) BLE_MOUSE.move(x, y);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && usbMouseReady) USBMouse.move(x, y);
#endif
}

static void hidMouseClick(int button) {
    if (bleMouseActive()) BLE_MOUSE.click(button);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && usbMouseReady) USBMouse.click(button);
#endif
}

static void hidMousePress(int button) {
    if (bleMouseActive()) BLE_MOUSE.press(button);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && usbMouseReady) USBMouse.press(button);
#endif
}

static void hidMouseRelease(int button) {
    if (bleMouseActive()) BLE_MOUSE.release(button);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && usbMouseReady) USBMouse.release(button);
#endif
}

// ---- Connection status ----

bool isBLEConnected() {
    return bluetoothEnabled && bluetoothInitialized && BLE_KEYBOARD_VALID && BLE_KEYBOARD.isConnected();
}

bool isUSBConnected() {
#ifdef BOARD_HAS_USB_HID
    return usbEnabled && usbInitialized && usbKeyboardReady;
#else
    return false;
#endif
}

bool hasAnyConnection() {
    return isBLEConnected() || isUSBConnected();
}

// ---- Raw HID report (bypasses Arduino translation table) ----
// modifiers is the standard HID modifier bitmask: 0x01=LCtrl, 0x02=LShift, 0x04=LAlt, 0x08=LGUI, 0x40=RAlt(AltGr)

void hidPressRaw(uint8_t hidUsage, uint8_t modifiers) {
    BleKeyReport report = {};
    report.modifiers = modifiers;
    report.keys[0]   = hidUsage;
    if (bleKbActive()) BLE_KEYBOARD.sendReport(&report);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && usbKeyboardEnabled) {
        KeyReport kr = {};
        kr.modifiers = modifiers;
        kr.keys[0]   = hidUsage;
        USBKeyboard.sendReport(&kr);
    }
#endif
}

void hidReleaseRaw() {
    BleKeyReport report = {};
    if (bleKbActive()) BLE_KEYBOARD.sendReport(&report);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && usbKeyboardEnabled) {
        KeyReport kr = {};
        USBKeyboard.sendReport(&kr);
    }
#endif
}

// ---- High-level send ----

static uint16_t nextCodepoint(const String& str, int& pos) {
    uint8_t b = (uint8_t)str[pos];
    if (b < 0x80) { pos += 1; return b; }
    if ((b & 0xE0) == 0xC0) {
        uint16_t cp = ((b & 0x1F) << 6) | ((uint8_t)str[pos + 1] & 0x3F);
        pos += 2; return cp;
    }
    if ((b & 0xF0) == 0xE0) {
        uint16_t cp = ((b & 0x0F) << 12) | (((uint8_t)str[pos + 1] & 0x3F) << 6) | ((uint8_t)str[pos + 2] & 0x3F);
        pos += 3; return cp;
    }
    pos++; return 0;
}

void sendPlainText(const String& text) {
    if (isHalted || !anyHIDConnected()) return;

    if (activeKeymap == "en") {
        hidPrint(text);
    } else {
        int pos = 0;
        while (pos < (int)text.length()) {
            uint16_t cp = nextCodepoint(text, pos);
            if (cp == 0) continue;

            KeyEntry ke;
            if (keymapLookup(cp, ke)) {
                hidPressRaw(ke.hidUsage, ke.modifiers);
                delay(KEY_PRESS_DELAY);
                hidReleaseRaw();
                delay(KEY_RELEASE_DELAY);
            } else if (cp < 0x80) {
                char buf[2] = { (char)cp, 0 };
                hidPrint(String(buf));
            }
            // non-ASCII codepoints with no table entry are silently skipped
        }
    }

    flashTxIndicator();
    delay(BETWEEN_SEND_TEXT_DELAY);
}

void sendSpecialKey(uint8_t keycode) {
    if (isHalted || !anyHIDConnected()) return;
    hidReleaseAll();
    delay(KEY_RELEASE_DELAY);
    hidPress(keycode);
    delay(KEY_PRESS_DELAY);
    hidRelease(keycode);
    delay(KEY_RELEASE_DELAY);
    flashTxIndicator();
    delay(SPECIAL_KEY_DELAY);
}

void sendSpecialKeyTimed(uint8_t keycode, int holdMs) {
    if (isHalted || !anyHIDConnected()) return;
    hidReleaseAll();
    delay(KEY_RELEASE_DELAY);
    hidPress(keycode);
    unsigned long t0 = millis();
    while ((int)(millis() - t0) < holdMs) {
        server.handleClient();
        delay(10);
    }
    hidRelease(keycode);
    delay(KEY_RELEASE_DELAY);
    flashTxIndicator();
    delay(SPECIAL_KEY_DELAY);
}

void pressKey(uint8_t keycode) {
    if (isHalted || !anyHIDConnected()) return;
    hidPress(keycode);
    flashTxIndicator();
}

void releaseKey(uint8_t keycode) {
    if (isHalted) return;
    hidRelease(keycode);
}

void sendConsumerKey(const MediaKeyReport key) {
    if (isHalted || !anyHIDConnected()) return;
    if (bleKbActive()) {
        BLE_KEYBOARD.write(key);
        delay(KEY_PRESS_DELAY + KEY_RELEASE_DELAY);
    }
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && KProxConsumer.isReady()) {
        KProxConsumer.sendConsumer(key[0], key[1]);
        delay(KEY_PRESS_DELAY);
        KProxConsumer.sendConsumer(0, 0);
        delay(KEY_RELEASE_DELAY);
    }
#endif
    flashTxIndicator();
    delay(SPECIAL_KEY_DELAY);
}

void sendSystemKey(SystemKeyReport key) {
    if (isHalted || !anyHIDConnected()) return;
    if (bleKbActive()) {
        BLE_KEYBOARD.writeSystemKey(key);
        delay(KEY_PRESS_DELAY + KEY_RELEASE_DELAY);
    }
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected() && KProxConsumer.isReady()) {
        KProxConsumer.sendSystem(key);
        delay(KEY_PRESS_DELAY);
        KProxConsumer.sendSystem(0);
        delay(KEY_RELEASE_DELAY);
    }
#endif
    flashTxIndicator();
    delay(SPECIAL_KEY_DELAY);
}

void sendKeyChord(const std::vector<uint8_t>& keycodes, uint8_t modifiers) {
    if (isHalted || !anyHIDConnected()) return;
    hidReleaseAll();
    delay(KEY_RELEASE_DELAY);
    if (modifiers) { hidPress(modifiers); delay(KEY_PRESS_DELAY); }
    for (uint8_t key : keycodes) { if (key) { hidPress(key); delay(5); } }
    delay(KEY_PRESS_DELAY);
    for (auto it = keycodes.rbegin(); it != keycodes.rend(); ++it) {
        if (*it) { hidRelease(*it); delay(5); }
    }
    if (modifiers) { hidRelease(modifiers); delay(KEY_RELEASE_DELAY); }
    hidReleaseAll();
    flashTxIndicator();
    delay(SPECIAL_KEY_DELAY);
}

static uint8_t resolveChordKey(const String& s) {
    if (s == "ENTER" || s == "RETURN")   return KEY_RETURN;
    if (s == "SPACE")                    return KEY_SPACE;
    if (s == "TAB")                      return KEY_TAB;
    if (s == "ESC" || s == "ESCAPE")     return KEY_ESC;
    if (s == "DELETE" || s == "DEL")     return KEY_DELETE;
    if (s == "BACKSPACE" || s == "BKSP") return KEY_BACKSPACE;
    if (s == "LEFT")                     return KEY_LEFT_ARROW;
    if (s == "RIGHT")                    return KEY_RIGHT_ARROW;
    if (s == "UP")                       return KEY_UP_ARROW;
    if (s == "DOWN")                     return KEY_DOWN_ARROW;
    if (s == "INSERT")                   return KEY_INSERT;
    if (s == "HOME")                     return KEY_HOME;
    if (s == "END")                      return KEY_END;
    if (s == "PAGEUP")                   return KEY_PAGE_UP;
    if (s == "PAGEDOWN")                 return KEY_PAGE_DOWN;
    if (s == "PRINTSCREEN" || s == "PRTSC" || s == "SYSRQ") return KEY_PRINTSCREEN;
    if (s == "CAPSLOCK" || s == "CAPS")  return KEY_CAPS_LOCK;
    if (s == "NUMLOCK")                  return KEY_NUM_LOCK;
    if (s == "SCROLLLOCK" || s == "SCRLK") return KEY_SCROLL_LOCK;
    if (s == "PAUSE" || s == "PAUSEBREAK" || s == "BREAK") return KEY_PAUSE;
    if (s == "APPLICATION" || s == "MENU" || s == "APP")   return KEY_APPLICATION;
    if (s == "KPENTER")                  return KEY_KP_ENTER;
    if (s == "KPPLUS")                   return KEY_KP_PLUS;
    if (s == "KPMINUS")                  return KEY_KP_MINUS;
    if (s == "KPMULTIPLY" || s == "KPSTAR") return KEY_KP_MULTIPLY;
    if (s == "KPDIVIDE" || s == "KPSLASH") return KEY_KP_DIVIDE;
    if (s == "KPDOT" || s == "KPDECIMAL") return KEY_KP_DOT;
    if (s.startsWith("KP") && s.length() == 3) {
        char d = s[2];
        if (d >= '0' && d <= '9') return (d == '0') ? KEY_KP0 : (KEY_KP1 + (d - '1'));
    }
    if (s.startsWith("F")) {
        int n = s.substring(1).toInt();
        if (n >= 1  && n <= 12) return KEY_F1  + (n - 1);
        if (n >= 13 && n <= 24) return KEY_F13 + (n - 13);
    }
    return 0;
}

void processChord(const String& chordStr) {
    String str = chordStr;
    std::vector<uint8_t> chordKeys;
    uint8_t modifiers = 0;

    if (str.indexOf("LCTRL+")  >= 0) { modifiers |= KEY_LEFT_CTRL;  str.replace("LCTRL+",  ""); }
    if (str.indexOf("RCTRL+")  >= 0) { modifiers |= KEY_RIGHT_CTRL; str.replace("RCTRL+",  ""); }
    if (str.indexOf("CTRL+")   >= 0) { modifiers |= KEY_LEFT_CTRL;  str.replace("CTRL+",   ""); }
    if (str.indexOf("LSHIFT+") >= 0) { modifiers |= KEY_LEFT_SHIFT; str.replace("LSHIFT+", ""); }
    if (str.indexOf("RSHIFT+") >= 0) { modifiers |= KEY_RIGHT_SHIFT; str.replace("RSHIFT+",""); }
    if (str.indexOf("SHIFT+")  >= 0) { modifiers |= KEY_LEFT_SHIFT; str.replace("SHIFT+",  ""); }
    if (str.indexOf("LALT+")   >= 0) { modifiers |= KEY_LEFT_ALT;   str.replace("LALT+",   ""); }
    if (str.indexOf("ALTGR+")  >= 0) { modifiers |= KEY_RIGHT_ALT;  str.replace("ALTGR+",  ""); }
    if (str.indexOf("RALT+")   >= 0) { modifiers |= KEY_RIGHT_ALT;  str.replace("RALT+",   ""); }
    if (str.indexOf("ALT+")    >= 0) { modifiers |= KEY_LEFT_ALT;   str.replace("ALT+",    ""); }
    if (str.indexOf("WINDOWS+")>= 0) { modifiers |= KEY_LEFT_GUI;   str.replace("WINDOWS+",""); }
    if (str.indexOf("SUPER+")  >= 0) { modifiers |= KEY_LEFT_GUI;   str.replace("SUPER+",  ""); }
    if (str.indexOf("MOD+")    >= 0) { modifiers |= KEY_LEFT_GUI;   str.replace("MOD+",    ""); }
    if (str.indexOf("CMD+")    >= 0) { modifiers |= KEY_LEFT_GUI;   str.replace("CMD+",    ""); }
    if (str.indexOf("WIN+")    >= 0) { modifiers |= KEY_LEFT_GUI;   str.replace("WIN+",    ""); }
    if (str.indexOf("GUI+")    >= 0) { modifiers |= KEY_LEFT_GUI;   str.replace("GUI+",    ""); }
    str.trim();

    if (str.indexOf("SYSRQ+") >= 0) {
        chordKeys.push_back(KEY_PRINTSCREEN);
        String rem = str.substring(str.indexOf("SYSRQ+") + 6);
        rem.trim();
        if (rem.length() == 1) {
            char c = rem.charAt(0);
            chordKeys.push_back((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
        }
    } else if (str.indexOf("PRINTSCREEN+") >= 0) {
        chordKeys.push_back(KEY_PRINTSCREEN);
        String rem = str.substring(str.indexOf("PRINTSCREEN+") + 12);
        rem.trim();
        if (rem.length() == 1) {
            char c = rem.charAt(0);
            chordKeys.push_back((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
        }
    } else if (str.length() == 1) {
        char c = str.charAt(0);
        chordKeys.push_back((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    } else {
        uint8_t resolved = resolveChordKey(str);
        if (resolved) chordKeys.push_back(resolved);
    }

    if (!chordKeys.empty() || modifiers) {
        sendKeyChord(chordKeys, modifiers);
    }
}

// ---- Mouse ----

void setMousePosition(int x, int y) {
    if (isHalted) return;
    int deltaX = x - currentMouseX;
    int deltaY = y - currentMouseY;
    while ((deltaX != 0 || deltaY != 0) && anyHIDConnected()) {
        int stepX = constrain(deltaX, -127, 127);
        int stepY = constrain(deltaY, -127, 127);
        hidMouseMoveStep(stepX, stepY);
        flashTxIndicator();
        currentMouseX += stepX;
        currentMouseY += stepY;
        deltaX -= stepX;
        deltaY -= stepY;
        if (abs(deltaX) > 127 || abs(deltaY) > 127) delay(10);
    }
}

void sendMouseMovement(int deltaX, int deltaY) {
    if (isHalted || !anyHIDConnected()) return;
    hidMouseMoveStep(constrain(deltaX, -127, 127), constrain(deltaY, -127, 127));
    flashTxIndicator();
}

void sendMouseClick(int button) {
    if (isHalted || !anyHIDConnected()) return;
    hidMouseClick(button);
    flashTxIndicator();
}

void sendMousePress(int button) {
    if (isHalted || !anyHIDConnected()) return;
    hidMousePress(button);
    flashTxIndicator();
}

void sendMouseRelease(int button) {
    if (isHalted || !anyHIDConnected()) return;
    hidMouseRelease(button);
    flashTxIndicator();
}

void sendMouseDoubleClick(int button) {
    if (isHalted || !anyHIDConnected()) return;
    hidMouseClick(button);
    delay(50);
    hidMouseClick(button);
    flashTxIndicator();
}

void sendBatchedMouseMovement() {
    if (!mouseBatch.hasMovement || !anyHIDConnected() || isHalted) return;
    hidMouseMoveStep(
        constrain(mouseBatch.accumulatedX, -127, 127),
        constrain(mouseBatch.accumulatedY, -127, 127)
    );
    flashTxIndicator();
    mouseBatch.accumulatedX = 0;
    mouseBatch.accumulatedY = 0;
    mouseBatch.hasMovement  = false;
    mouseBatch.lastUpdate   = millis();
}

void accumulateMouseMovement(int16_t deltaX, int16_t deltaY) {
    if (isHalted) return;
    mouseBatch.accumulatedX += deltaX;
    mouseBatch.accumulatedY += deltaY;
    mouseBatch.hasMovement   = true;
    mouseBatch.lastUpdate    = millis();
}

// ---- Halt / resume ----

bool g_parserAbort        = false;
bool g_btnAHaltedPlayback = false;

void haltAllOperations() {
    g_parserAbort    = true;
    isLooping        = false;
    loopingRegister  = -1;
    isHalted         = true;
    pendingTokenStrings.clear();
    hidReleaseAll();
    requestInProgress        = false;
    mouseBatch.accumulatedX  = 0;
    mouseBatch.accumulatedY  = 0;
    mouseBatch.hasMovement   = false;

    if (ledEnabled) {
        for (int i = 0; i < 3; i++) { setLED(LED_COLOR_HALT, 100); delay(100); }
    }
}

void resumeOperations() {
    isHalted      = false;
    g_parserAbort = false;
    if (ledEnabled) setLED(LED_COLOR_RESUME, 200);
}
