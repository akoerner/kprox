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
    if (isUSBConnected()) USBKeyboard.print(text);
#endif
}

static void hidPress(uint8_t key) {
    if (bleKbActive()) BLE_KEYBOARD.press(key);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected()) USBKeyboard.press(key);
#endif
}

static void hidRelease(uint8_t key) {
    if (bleKbActive()) BLE_KEYBOARD.release(key);
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected()) USBKeyboard.release(key);
#endif
}

void hidReleaseAll() {
    if (bleKbActive()) BLE_KEYBOARD.releaseAll();
#ifdef BOARD_HAS_USB_HID
    if (isUSBConnected()) USBKeyboard.releaseAll();
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
    if (isUSBConnected()) {
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
    if (isUSBConnected()) {
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

void processChord(const String& chordStr) {
    String str = chordStr;
    std::vector<uint8_t> chordKeys;
    uint8_t modifiers = 0;

    if (str.indexOf("CTRL+")  >= 0) { modifiers |= KEY_LEFT_CTRL;  str.replace("CTRL+",  ""); }
    if (str.indexOf("ALT+")   >= 0) { modifiers |= KEY_LEFT_ALT;   str.replace("ALT+",   ""); }
    if (str.indexOf("SHIFT+") >= 0) { modifiers |= KEY_LEFT_SHIFT; str.replace("SHIFT+", ""); }
    if (str.indexOf("GUI+")   >= 0 || str.indexOf("CMD+") >= 0 || str.indexOf("WIN+") >= 0) {
        modifiers |= KEY_LEFT_GUI;
        str.replace("GUI+", ""); str.replace("CMD+", ""); str.replace("WIN+", "");
    }
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
        if      (str == "ENTER")       chordKeys.push_back(KEY_RETURN);
        else if (str == "SPACE")       chordKeys.push_back(KEY_SPACE);
        else if (str == "TAB")         chordKeys.push_back(KEY_TAB);
        else if (str == "ESC")         chordKeys.push_back(KEY_ESC);
        else if (str == "DELETE")      chordKeys.push_back(KEY_DELETE);
        else if (str == "BACKSPACE")   chordKeys.push_back(KEY_BACKSPACE);
        else if (str == "LEFT")        chordKeys.push_back(KEY_LEFT_ARROW);
        else if (str == "RIGHT")       chordKeys.push_back(KEY_RIGHT_ARROW);
        else if (str == "UP")          chordKeys.push_back(KEY_UP_ARROW);
        else if (str == "DOWN")        chordKeys.push_back(KEY_DOWN_ARROW);
        else if (str == "INSERT")      chordKeys.push_back(KEY_INSERT);
        else if (str == "HOME")        chordKeys.push_back(KEY_HOME);
        else if (str == "END")         chordKeys.push_back(KEY_END);
        else if (str == "PAGEUP")      chordKeys.push_back(KEY_PAGE_UP);
        else if (str == "PAGEDOWN")    chordKeys.push_back(KEY_PAGE_DOWN);
        else if (str == "PRINTSCREEN" || str == "SYSRQ") chordKeys.push_back(KEY_PRINTSCREEN);
        else if (str.startsWith("F") && str.length() <= 3) {
            int n = str.substring(1).toInt();
            if (n >= 1 && n <= 12) chordKeys.push_back(KEY_F1 + (n - 1));
        }
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
