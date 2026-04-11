#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_settings.h"
#include "../storage.h"
#include "../credential_store.h"
#include "../connection.h"
#include "../hid.h"
#include "../registers.h"
#include <WiFi.h>
#include <algorithm>
#include "../sd_card.h"
#include "../keymap.h"
#include "../keymap_data.h"
#include <SD.h>

namespace Cardputer {

static const char* s_builtinKeymapNames[] = {
    "English (US)", "German (QWERTZ)", "French (AZERTY)", "Italian",
    "Spanish", "Swedish", "Swiss German", "Portuguese",
    "British (UK)", "Norwegian", "Finnish", "Danish",
    "Dutch", "Polish", "Hungarian", "Dvorak", "Colemak"
};
static const char* s_builtinKeymapIds[] = {
    "en", "de", "fr", "it", "es", "se", "ch", "pt",
    "gb", "no", "fi", "da", "nl", "pl", "hu", "dvorak", "colemak"
};
static constexpr int S_BUILTIN_COUNT = 17;


static constexpr int  SETTINGS_BG      = 0x1863;
static constexpr int  BAR_TOP_H        = 18;
static constexpr int  BAR_BOT_H        = 14;
static constexpr int  CONTENT_Y        = BAR_TOP_H + 4;

static uint16_t barColor()  { return M5Cardputer.Display.color565(20, 110, 60); }
static uint16_t botColor()  { return M5Cardputer.Display.color565(16, 16, 16); }
static uint16_t labelColor(){ return M5Cardputer.Display.color565(170, 170, 170); }
static uint16_t selBgColor(){ return M5Cardputer.Display.color565(20, 60, 20); }

void AppSettings::_drawTopBar(int pageNum) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = barColor();
    disp.fillRect(0, 0, disp.width(), BAR_TOP_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, bc);

    static const char* pageLabels[NUM_PAGES] = {
        "WiFi Settings", "Bluetooth", "USB HID", "API Key",
        "Device Identity", "Sink Config", "HID Timing 1/2", "HID Timing 2/2",
        "Startup App", "App Layout", "CS Security", "SD Storage", "Backups",
        "Keymap", "Display"
    };
    disp.drawString(pageLabels[pageNum], 4, 3);

    char pageStr[8];
    snprintf(pageStr, sizeof(pageStr), "%d/%d", pageNum + 1, NUM_PAGES);
    int pw = disp.textWidth(pageStr);
    disp.drawString(pageStr, disp.width() - pw - 4, 3);
    drawTabHint(4 + disp.textWidth(pageLabels[pageNum]) + 3);
}

void AppSettings::_drawBottomBar(const char* hint) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = botColor();
    disp.fillRect(0, disp.height() - BAR_BOT_H, disp.width(), BAR_BOT_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(110, 110, 110), bc);
    disp.drawString(hint, 2, disp.height() - BAR_BOT_H + 2);
}

void AppSettings::_drawInputField(int x, int y, int w, const String& text, bool active, bool masked) {
    auto& disp = M5Cardputer.Display;
    uint16_t fbg = disp.color565(50, 50, 50);
    disp.fillRect(x, y, w, 14, fbg);
    disp.setTextColor(TFT_WHITE, fbg);
    disp.setTextSize(1);

    int maxChars = (w - 4) / 6;
    String display = text;
    if (masked && display.length() > 0) {
        display = String(display.length(), '*');
    }
    if ((int)display.length() > maxChars) {
        display = display.substring(display.length() - maxChars);
    }
    if (active) display += "_";
    disp.drawString(display, x + 2, y + 3);
}

// ---- Shared toggle row helper ----

void AppSettings::_drawToggleRow(int y, bool selected, const char* label,
                                  bool enabled, const char* connStatus, uint16_t connColor) {
    auto& disp = M5Cardputer.Display;
    uint16_t rowBg = selected ? selBgColor() : (uint16_t)SETTINGS_BG;
    if (selected) disp.fillRect(0, y - 1, disp.width(), 14, rowBg);

    disp.setTextSize(1);
    disp.setTextColor(selected ? TFT_WHITE : labelColor(), rowBg);
    char lbl[28];
    snprintf(lbl, sizeof(lbl), "%s%s", selected ? "> " : "  ", label);
    disp.drawString(lbl, 4, y + 1);

    // ON/OFF badge
    const char* onoff = enabled ? "ON" : "OFF";
    uint16_t bb = enabled ? disp.color565(20,70,20) : disp.color565(70,20,20);
    uint16_t bc = enabled ? disp.color565(80,220,80) : disp.color565(200,60,60);
    int rx = disp.width() - 4;

    // optional connection status
    if (connStatus && *connStatus) {
        int bw = disp.textWidth(connStatus) + 6;
        rx -= bw;
        disp.fillRoundRect(rx, y, bw, 11, 2, disp.color565(25,25,25));
        disp.setTextColor(connColor, disp.color565(25,25,25));
        disp.drawString(connStatus, rx + 3, y + 1);
        rx -= 4;
    }
    int bw = disp.textWidth(onoff) + 8;
    rx -= bw;
    disp.fillRoundRect(rx, y - 1, bw, 12, 3, bb);
    disp.setTextColor(bc, bb);
    disp.drawString(onoff, rx + 4, y);
}

// ---- Page 1: Bluetooth ----

void AppSettings::_drawPage1() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(1);

    // 6 rows: BT enable, BT Keyboard, BT Mouse, BT Intl Keys, BT Consumer, BT System
    bool btConn = bluetoothEnabled && BLE_KEYBOARD_VALID && BLE_KEYBOARD.isConnected();
    const char* connStr = btConn ? "connected" : (bluetoothEnabled ? "no peer" : "");
    uint16_t connCol = btConn ? disp.color565(80,220,80) : disp.color565(200,160,0);

    struct { const char* label; bool* flag; } rows[6] = {
        { "Bluetooth",       &bluetoothEnabled       },
        { "  BT Keyboard",   &bleKeyboardEnabled     },
        { "  BT Mouse",      &bleMouseEnabled        },
        { "  BT Intl Keys",  &bleIntlKeyboardEnabled },
        { "  BT Consumer",   &bleConsumerEnabled     },
        { "  BT System",     &bleSystemEnabled       },
    };

    for (int i = 0; i < 6; i++) {
        int ry = CONTENT_Y + i * 16;
        _drawToggleRow(ry, i == _toggleSel, rows[i].label, *rows[i].flag,
                       i == 0 ? connStr : nullptr, connCol);
    }

    _drawBottomBar("up/dn  ENTER toggle  C reconnect  </> page  ESC");
}

void AppSettings::_handlePage1(KeyInput ki) {
    static constexpr int N = 6;
    if (ki.arrowLeft)  { _page = 0; _needsRedraw = true; return; }  // left of BT = WiFi
    if (ki.arrowRight) { _page = 2; _toggleSel = 0; _needsRedraw = true; return; }  // right of BT = USB
    if (ki.arrowUp)    { _toggleSel = (_toggleSel - 1 + N) % N; _needsRedraw = true; return; }
    if (ki.arrowDown)  { _toggleSel = (_toggleSel + 1) % N;      _needsRedraw = true; return; }

    if (ki.enter) {
        switch (_toggleSel) {
            case 0:
                if (bluetoothEnabled) disableBluetooth(); else enableBluetooth();
                break;
            case 1:
                bleKeyboardEnabled = !bleKeyboardEnabled;
                saveBtSettings(); break;
            case 2:
                bleMouseEnabled = !bleMouseEnabled;
                saveBtSettings(); break;
            case 3:
                bleIntlKeyboardEnabled = !bleIntlKeyboardEnabled;
                saveBtSettings(); break;
            case 4:
                bleConsumerEnabled = !bleConsumerEnabled;
                saveBtSettings(); break;
            case 5:
                bleSystemEnabled = !bleSystemEnabled;
                saveBtSettings(); break;
        }
        _needsRedraw = true; return;
    }

    if (ki.ch == 'c' || ki.ch == 'C') {
        if (BLE_KEYBOARD_VALID) {
            if (!bluetoothEnabled) {
                enableBluetooth();
            } else {
                BLE_KEYBOARD.end(); delay(300); BLE_KEYBOARD.begin();
            }
        }
        _needsRedraw = true; return;
    }
}

// ---- Page 1: USB HID ----

// ---- Page 2: USB HID ----

void AppSettings::_drawPage2() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(2);

    // 7 rows: USB enable, USB Keyboard, USB Mouse, USB Intl Keys, USB Consumer, USB System, FIDO2
    struct { const char* label; bool* flag; } rows[7] = {
        { "USB HID",          &usbEnabled             },
        { "  USB Keyboard",   &usbKeyboardEnabled     },
        { "  USB Mouse",      &usbMouseEnabled        },
        { "  USB Intl Keys",  &usbIntlKeyboardEnabled },
        { "  USB Consumer",   &usbConsumerEnabled     },
        { "  USB System",     &usbSystemEnabled       },
        { "  FIDO2/CTAP2",    &fido2Enabled           },
    };

    for (int i = 0; i < 7; i++) {
        int ry = CONTENT_Y + i * 13;
        _drawToggleRow(ry, i == _toggleSel, rows[i].label, *rows[i].flag, nullptr, 0);
    }

    if (_rebootNote) {
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(200,160,0), SETTINGS_BG);
        disp.drawString("Reboot to apply USB changes", 4, CONTENT_Y + 7*13 + 2);
    }

    _drawBottomBar("up/dn  ENTER toggle  </> page  ESC back");
}

void AppSettings::_handlePage2(KeyInput ki) {
    static constexpr int N = 7;
    if (ki.arrowLeft)  { _page = 1; _toggleSel = 0; _needsRedraw = true; return; }  // left of USB = BT
    if (ki.arrowRight) {
        _page = 3; _idSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; return;  // right of USB = API
    }
    if (false) { _page = 2; _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = "";
        _needsRedraw = true; return;
    }
    if (ki.arrowUp)   { _toggleSel = (_toggleSel - 1 + N) % N; _needsRedraw = true; return; }
    if (ki.arrowDown) { _toggleSel = (_toggleSel + 1) % N;      _needsRedraw = true; return; }

    if (ki.enter) {
        switch (_toggleSel) {
            case 0: usbEnabled             = !usbEnabled;             saveUSBSettings(); _rebootNote = true; break;
            case 1: usbKeyboardEnabled     = !usbKeyboardEnabled;     saveUSBSettings(); _rebootNote = true; break;
            case 2: usbMouseEnabled        = !usbMouseEnabled;        saveUSBSettings(); _rebootNote = true; break;
            case 3: usbIntlKeyboardEnabled = !usbIntlKeyboardEnabled; saveUSBSettings(); break;
            case 4: usbConsumerEnabled     = !usbConsumerEnabled;     saveUSBSettings(); _rebootNote = true; break;
            case 5: usbSystemEnabled       = !usbSystemEnabled;       saveUSBSettings(); _rebootNote = true; break;
            case 6: fido2Enabled           = !fido2Enabled;           saveUSBSettings(); _rebootNote = true; break;
        }
        _needsRedraw = true; return;
    }
}

void AppSettings::_connectWifi() {
    WiFi.disconnect(true);
    delay(200);
    WiFi.setHostname(hostname);
    WiFi.mode(WIFI_STA);
    WiFi.begin(_newSSID.c_str(), wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        feedWatchdog();
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiSSID     = _newSSID;
        wifiEnabled  = true;
        saveWiFiSettings();
        saveWifiEnabledSettings();
        _wifiStatusMsg = "Connected!";
        _wifiSuccess   = true;
    } else {
        _wifiStatusMsg = "Connection failed.";
        _wifiSuccess   = false;
    }
    _wifiState = WS_DONE;
}

// ---- Page 0: WiFi Settings ----

void AppSettings::_drawPage0() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(0);

    int y = CONTENT_Y;
    bool wconn = (WiFi.status() == WL_CONNECTED);

    if (_wifiState == WS_SSID || _wifiState == WS_PASS) {
        // Row 0: Enable/disable toggle
        bool enSel  = (_wifiSel == 0);
        uint16_t enBg = enSel ? disp.color565(20, 50, 90) : SETTINGS_BG;
        if (enSel) disp.fillRect(0, y, disp.width(), 14, enBg);
        disp.setTextColor(enSel ? TFT_WHITE : labelColor(), enBg);
        disp.drawString(enSel ? "> WiFi:" : "  WiFi:", 4, y);
        const char* enLabel = wifiEnabled ? "Enabled" : "Disabled";
        uint16_t enCol = wifiEnabled ? disp.color565(80,220,80) : disp.color565(180,80,80);
        if (wconn) { enLabel = "Connected"; enCol = disp.color565(80,220,80); }
        disp.setTextColor(enCol, enBg);
        disp.drawString(enLabel, 60, y);
        y += 18;

        // Row 1: SSID field
        bool ssidSel  = (_wifiSel == 1);
        bool ssidEdit = (ssidSel && _wifiEditing);
        disp.setTextColor(ssidSel ? TFT_WHITE : labelColor(), SETTINGS_BG);
        disp.drawString(ssidSel ? "> SSID:" : "  SSID:", 4, y);
        String ssidVal = ssidEdit ? _wifiInputBuf : (_newSSID.isEmpty() ? wifiSSID : _newSSID);
        if (ssidVal.isEmpty()) ssidVal = "(none)";
        _drawInputField(60, y, disp.width() - 64, ssidVal, ssidEdit);
        y += 18;

        // Row 2: Password field
        bool passSel  = (_wifiSel == 2);
        bool passEdit = (passSel && _wifiEditing);
        disp.setTextColor(passSel ? TFT_WHITE : labelColor(), SETTINGS_BG);
        disp.drawString(passSel ? "> Pass:" : "  Pass:", 4, y);
        String passVal = passEdit ? _wifiInputBuf : "********";
        _drawInputField(60, y, disp.width() - 64, passVal, passEdit);
        y += 18;

        // Row 3: Connect button
        bool connSel = (_wifiSel == 3);
        uint16_t cbg = connSel ? disp.color565(20,80,20) : disp.color565(30,30,30);
        disp.fillRoundRect(4, y, 90, 13, 2, cbg);
        disp.setTextColor(connSel ? TFT_WHITE : disp.color565(100,180,100), cbg);
        disp.drawString(connSel ? "> CONNECT" : "  connect", 8, y + 2);

        if (_wifiEditing)
            _drawBottomBar("type  DEL=del  ENTER=save  fn+`=cancel");
        else
            _drawBottomBar("up/dn=row  ENTER=toggle/edit/connect  </>=page");
    } else if (_wifiState == WS_CONNECTING) {
        disp.setTextColor(disp.color565(200,160,0), SETTINGS_BG);
        disp.drawString("Connecting...", 4, y + 10);
        _drawBottomBar("");
    } else {
        uint16_t statusColor = _wifiSuccess ? disp.color565(80,220,80) : disp.color565(220,80,80);
        disp.setTextColor(statusColor, SETTINGS_BG);
        disp.drawString(_wifiStatusMsg, 4, y + 6);
        if (_wifiSuccess) {
            disp.setTextColor(labelColor(), SETTINGS_BG);
            disp.drawString(WiFi.localIP().toString().c_str(), 4, y + 20);
        }
        _drawBottomBar("any key to continue");
    }
}

void AppSettings::_handlePage0(KeyInput ki) {
    if (_wifiState == WS_DONE) {
        if (ki.anyKey) {
            _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = "";
            _wifiStatusMsg = ""; _wifiEditing = false; _wifiSel = 0;
        }
        _needsRedraw = true; return;
    }
    if (_wifiState == WS_CONNECTING) return;

    // Page navigation and row selection — only when not editing
    if (!_wifiEditing) {
        if (ki.arrowLeft)  { _page = NUM_PAGES-1; _idSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; return; }
        if (ki.arrowRight) { _page = 1; _toggleSel = 0; _needsRedraw = true; return; }
        if (ki.arrowUp)   { _wifiSel = (_wifiSel - 1 + 4) % 4; _needsRedraw = true; return; }
        if (ki.arrowDown) { _wifiSel = (_wifiSel + 1) % 4;     _needsRedraw = true; return; }
        if (ki.enter) {
            if (_wifiSel == 0) {
                // Toggle WiFi enable/disable
                wifiEnabled = !wifiEnabled;
                saveWifiEnabledSettings();
                if (wifiEnabled)  { WiFi.mode(WIFI_STA); WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str()); }
                else              { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); }
                _needsRedraw = true;
            } else if (_wifiSel == 1) {
                // Edit SSID
                _wifiInputBuf = _newSSID.isEmpty() ? wifiSSID : _newSSID;
                _wifiEditing = true; _needsRedraw = true;
            } else if (_wifiSel == 2) {
                // Edit password
                _wifiInputBuf = "";
                _wifiEditing = true; _needsRedraw = true;
            } else {
                // Connect
                if (_newSSID.isEmpty()) _newSSID = wifiSSID;
                if (!wifiEnabled) { wifiEnabled = true; saveWifiEnabledSettings(); }
                _wifiState = WS_CONNECTING; _needsRedraw = true; _drawPage0(); _connectWifi();
            }
            return;
        }
        return;
    }

    // Editing mode
    if (ki.enter) {
        if (_wifiSel == 1) {
            if (_wifiInputBuf.length() > 0) _newSSID = _wifiInputBuf;
        } else if (_wifiSel == 2) {
            if (_wifiInputBuf.length() > 0) wifiPassword = _wifiInputBuf;
        }
        _wifiEditing = false; _wifiInputBuf = "";
        _needsRedraw = true; return;
    }
    if (ki.del && _wifiInputBuf.length() > 0) { _wifiInputBuf.remove(_wifiInputBuf.length()-1); _needsRedraw = true; return; }
    if (ki.ch) { _wifiInputBuf += ki.ch; _needsRedraw = true; }
}

// ---- Page 3: API Key ----

void AppSettings::_drawPage3() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(3);

    int y = CONTENT_Y;
    int fieldW = disp.width() - 8;
    disp.setTextSize(1);
    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("Current key:", 4, y); y += 12;

    String masked;
    if (apiKey.length() <= 4) { masked = apiKey; }
    else {
        masked = apiKey.substring(0,2);
        for (size_t i = 2; i < apiKey.length()-2; i++) masked += '*';
        masked += apiKey.substring(apiKey.length()-2);
    }
    _drawInputField(4, y, fieldW, masked, false); y += 20;

    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("New API Key:", 4, y); y += 12;
    _drawInputField(4, y, fieldW, _editBuf, _editing);

    if (_idSaved) { disp.setTextColor(TFT_GREEN, SETTINGS_BG); disp.drawString("Saved!", 4, y+22); }

    if (_editing) _drawBottomBar("type  ENTER save  ESC cancel");
    else          _drawBottomBar("ENTER edit  </> page  ESC back");
}

void AppSettings::_handlePage3(KeyInput ki) {
    if (_editing) {
        if (ki.esc) { _editing = false; _editBuf = ""; _needsRedraw = true; return; }
        if (ki.enter) {
            if (_editBuf.length() > 0) { apiKey = _editBuf; saveApiKeySettings(); _idSaved = true; }
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.del && _editBuf.length()>0) { _editBuf.remove(_editBuf.length()-1); _needsRedraw = true; return; }
        if (ki.ch) { _editBuf += ki.ch; _idSaved = false; _needsRedraw = true; }
        return;
    }
    if (ki.arrowLeft)  { _page = 2; _toggleSel = 0; _needsRedraw = true; }  // left of API = USB
    else if (ki.arrowRight) { _page = 4; _idSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; }  // right of API = Identity
    else if (ki.enter) { _editBuf = ""; _editing = true; _idSaved = false; _needsRedraw = true; }
}

// ---- Page 4: Device Identity ----
// 6 fields, 16px row height → 6×16=96px ≤ 99px available (135 - 22 top - 14 bottom)

void AppSettings::_drawPage4() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(4);

    String vidStr = String(usbVidOverride, HEX); vidStr.toUpperCase();
    while (vidStr.length() < 4) vidStr = "0" + vidStr;
    String pidStr = String(usbPidOverride, HEX); pidStr.toUpperCase();
    while (pidStr.length() < 4) pidStr = "0" + pidStr;

    const char* labels[6] = {
        "USB Mfr", "USB/BT Product", "Hostname", "USB Serial",
        "USB VID (hex)", "USB PID (hex)"
    };
    const char* descs[6] = {
        "USB mfr descriptor", "BT name + USB product",
        "mDNS name (reboot)", "USB serial (reboot)",
        "Vendor ID (reboot)", "Product ID (reboot)"
    };
    String strs[6] = { usbManufacturer, usbProduct, hostnameStr, usbSerialNumber, vidStr, pidStr };

    static constexpr int ROW_H  = 16;
    static constexpr int N_ROWS = 6;
    int fieldW = disp.width() - 8;

    for (int i = 0; i < N_ROWS; i++) {
        int y    = CONTENT_Y + i * ROW_H;
        bool sel  = (i == _idSel);
        bool edit = (sel && _editing);
        if (sel && !edit) disp.fillRect(0, y - 1, disp.width(), ROW_H - 1, selBgColor());
        disp.setTextSize(1);
        uint16_t rowBg = (sel && !edit) ? selBgColor() : (uint16_t)SETTINGS_BG;
        disp.setTextColor(sel ? TFT_WHITE : labelColor(), rowBg);
        char row[32]; snprintf(row, sizeof(row), "%s%s:", sel ? "> " : "  ", labels[i]);
        disp.drawString(row, 4, y);

        if (edit) {
            _drawInputField(4, y + 1, fieldW, _editBuf, true);
        } else {
            int lw = disp.textWidth(row) + 4;
            int avail = disp.width() - lw - 6;
            String preview = strs[i];
            if (preview.isEmpty()) preview = descs[i];
            while (preview.length() > 0 && disp.textWidth(preview) > avail)
                preview = preview.substring(0, preview.length() - 1);
            disp.setTextColor(sel ? TFT_YELLOW : disp.color565(130, 180, 130), rowBg);
            disp.drawString(preview, lw, y);
        }
    }

    if (_idSaved) {
        disp.setTextColor(TFT_GREEN, SETTINGS_BG);
        disp.drawString("Saved!", 4, CONTENT_Y + N_ROWS * ROW_H + 1);
    }

    if (_editing) _drawBottomBar("type  ENTER save  ESC cancel");
    else          _drawBottomBar("up/dn item  ENTER edit  </> page");
}

void AppSettings::_handlePage4(KeyInput ki) {
    static constexpr int N = 6;
    if (_editing) {
        if (ki.esc)   { _editing = false; _editBuf = ""; _needsRedraw = true; return; }
        if (ki.enter) {
            if (_editBuf.length() > 0) {
                if (_idSel == 0) { usbManufacturer = _editBuf; saveUSBIdentitySettings(); }
                else if (_idSel == 1) { usbProduct = _editBuf; saveUSBIdentitySettings(); }
                else if (_idSel == 2) { hostnameStr = _editBuf; hostname = hostnameStr.c_str(); saveHostnameSettings(); }
                else if (_idSel == 3) { usbSerialNumber = _editBuf; saveHostnameSettings(); }
                else {
                    uint16_t val = (uint16_t)strtol(_editBuf.c_str(), nullptr, 16);
                    if (_idSel == 4) usbVidOverride = val;
                    else             usbPidOverride = val;
                    saveUSBIdentitySettings();
                }
                _idSaved = true;
            }
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.del && _editBuf.length() > 0) { _editBuf.remove(_editBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.ch) { _editBuf += ki.ch; _idSaved = false; _needsRedraw = true; }
        return;
    }
    if (ki.arrowLeft)       { _page = 3; _editing = false; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowRight) { _page = 5; _needsRedraw = true; }
    else if (ki.arrowUp)    { _idSel = (_idSel - 1 + N) % N; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowDown)  { _idSel = (_idSel + 1) % N;      _idSaved = false; _needsRedraw = true; }
    else if (ki.enter) {
        // pre-populate edit buffer with current value
        if      (_idSel == 0) _editBuf = usbManufacturer;
        else if (_idSel == 1) _editBuf = usbProduct;
        else if (_idSel == 2) _editBuf = hostnameStr;
        else if (_idSel == 3) _editBuf = usbSerialNumber;
        else {
            uint16_t v = (_idSel == 4) ? usbVidOverride : usbPidOverride;
            _editBuf = String(v, HEX); _editBuf.toUpperCase();
            while (_editBuf.length() < 4) _editBuf = "0" + _editBuf;
        }
        _editing = true; _idSaved = false; _needsRedraw = true;
    }
}

// ---- Page 5: Sink Config ----

void AppSettings::_drawPage5() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(5);

    int y = CONTENT_Y;
    disp.setTextSize(1);

    // Current sink file size
    size_t curSize = 0;
    if (SPIFFS.exists("/sink.txt")) {
        File f = SPIFFS.open("/sink.txt", "r");
        if (f) { curSize = f.size(); f.close(); }
    }
    char sizeBuf[40];
    snprintf(sizeBuf, sizeof(sizeBuf), "Current: %u bytes", (unsigned)curSize);
    disp.setTextColor(curSize > 0 ? disp.color565(80,200,120) : disp.color565(100,100,100), SETTINGS_BG);
    disp.drawString(sizeBuf, 4, y); y += 16;

    // Max size config
    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("Max sink size (bytes):", 4, y); y += 13;
    disp.drawString("0 = unlimited", 6, y); y += 16;

    String maxStr = String(maxSinkSize);
    _drawInputField(4, y, disp.width() - 8, _editing ? _editBuf : maxStr, _editing);
    if (_idSaved) {
        disp.setTextColor(TFT_GREEN, SETTINGS_BG);
        disp.drawString("Saved!", 4, y + 22);
    }

    if (_editing) _drawBottomBar("type size  ENTER save  ESC cancel");
    else          _drawBottomBar("ENTER edit  </> page  ESC back");
}

void AppSettings::_handlePage5(KeyInput ki) {
    if (_editing) {
        if (ki.esc)   { _editing = false; _editBuf = ""; _needsRedraw = true; return; }
        if (ki.enter) {
            maxSinkSize = _editBuf.toInt();
            if (maxSinkSize < 0) maxSinkSize = 0;
            saveSinkSettings();
            _idSaved = true;
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.del && _editBuf.length() > 0) { _editBuf.remove(_editBuf.length()-1); _needsRedraw = true; return; }
        // only allow digits
        if (ki.ch >= '0' && ki.ch <= '9') { _editBuf += ki.ch; _idSaved = false; _needsRedraw = true; }
        return;
    }
    if (ki.arrowLeft)  { _page = 4; _editing = false; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowRight) { _page = 6; _timingSel = 0; _idSaved = false; _needsRedraw = true; }
    else if (ki.enter) { _editBuf = String(maxSinkSize); _editing = true; _idSaved = false; _needsRedraw = true; }
}

// ---- Shared timing page renderer ----

static void drawTimingRows(int offset, int count, int sel, bool editing,
                            const char* labels[], const char* descs[], int* vals[],
                            const String& editBuf, bool idSaved) {
    auto& disp = M5Cardputer.Display;
    static constexpr int  SETTINGS_BG  = 0x1863;
    auto selBg  = disp.color565(20, 60, 20);
    auto lc     = disp.color565(170, 170, 170);

    int y = CONTENT_Y;
    for (int i = 0; i < count; i++) {
        bool sel_  = (i == sel);
        bool edit_ = (sel_ && editing);
        uint16_t rowBg = sel_ ? selBg : (uint16_t)SETTINGS_BG;
        if (sel_) disp.fillRect(0, y - 1, disp.width(), 22, rowBg);

        disp.setTextSize(1);
        disp.setTextColor(sel_ ? TFT_WHITE : lc, rowBg);
        char lbl[28]; snprintf(lbl, sizeof(lbl), "%s%s:", sel_ ? "> " : "  ", labels[offset + i]);
        disp.drawString(lbl, 4, y);

        disp.setTextColor(disp.color565(120, 160, 120), rowBg);
        disp.drawString(descs[offset + i], 8, y + 10);

        String valStr = edit_ ? (editBuf + "_") : String(*vals[offset + i]);
        int vw = disp.textWidth(valStr) + 4;
        disp.setTextColor(sel_ ? TFT_YELLOW : TFT_WHITE, rowBg);
        disp.drawString(valStr, disp.width() - vw - 2, y);
        y += 20;
    }

    if (idSaved) {
        disp.setTextColor(TFT_GREEN, (uint16_t)SETTINGS_BG);
        disp.drawString("Saved!", 4, y + 2);
    }
}

// ---- Page 6: HID Timing 1/2 (press, release, between keys) ----

void AppSettings::_drawPage6() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(6);

    static const char* labels[6] = {
        "Press Delay",   "Release Delay", "Between Keys",
        "Send Text",     "Special Key",   "Token Delay"
    };
    static const char* descs[6] = {
        "ms key held after press",    "ms pause after release",
        "ms gap between keystrokes",  "ms after full text send",
        "ms after special/media key", "ms between token execution"
    };
    int* vals[6] = {
        &g_keyPressDelay, &g_keyReleaseDelay, &g_betweenKeysDelay,
        &g_betweenSendTextDelay, &g_specialKeyDelay, &g_tokenDelay
    };

    drawTimingRows(0, 3, _timingSel, _editing, labels, descs, vals, _editBuf, _idSaved);

    if (_editing) _drawBottomBar("digits  ENTER save  ESC cancel");
    else          _drawBottomBar("up/dn  ENTER edit  BtnG0=test  </>");
}

void AppSettings::_handlePage6(KeyInput ki) {
    static constexpr int N = 3;
    int* vals[N] = { &g_keyPressDelay, &g_keyReleaseDelay, &g_betweenKeysDelay };

    if (_editing) {
        if (ki.esc)   { _editing = false; _editBuf = ""; _needsRedraw = true; return; }
        if (ki.enter) {
            int v = _editBuf.toInt();
            if (v >= 0) { *vals[_timingSel] = v; saveTimingSettings(); _idSaved = true; }
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.del && _editBuf.length() > 0) { _editBuf.remove(_editBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.ch >= '0' && ki.ch <= '9') { _editBuf += ki.ch; _idSaved = false; _needsRedraw = true; }
        return;
    }
    if (ki.arrowLeft)  { _page = 5; _timingSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowRight) { _page = 7; _timingSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowUp)   { _timingSel = (_timingSel - 1 + N) % N; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowDown) { _timingSel = (_timingSel + 1) % N;     _idSaved = false; _needsRedraw = true; }
    else if (ki.enter) { _editBuf = String(*vals[_timingSel]); _editing = true; _idSaved = false; _needsRedraw = true; }
}

// ---- Page 7: HID Timing 2/2 (send text, special key, token) ----

void AppSettings::_drawPage7() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(7);

    static const char* labels[6] = {
        "Press Delay",   "Release Delay", "Between Keys",
        "Send Text",     "Special Key",   "Token Delay"
    };
    static const char* descs[6] = {
        "ms key held after press",    "ms pause after release",
        "ms gap between keystrokes",  "ms after full text send",
        "ms after special/media key", "ms between token execution"
    };
    int* vals[6] = {
        &g_keyPressDelay, &g_keyReleaseDelay, &g_betweenKeysDelay,
        &g_betweenSendTextDelay, &g_specialKeyDelay, &g_tokenDelay
    };

    drawTimingRows(3, 3, _timingSel, _editing, labels, descs, vals, _editBuf, _idSaved);

    if (_editing) _drawBottomBar("digits  ENTER save  ESC cancel");
    else          _drawBottomBar("up/dn  ENTER edit  BtnG0=test  </>");
}

void AppSettings::_handlePage7(KeyInput ki) {
    static constexpr int N = 3;
    int* vals[N] = { &g_betweenSendTextDelay, &g_specialKeyDelay, &g_tokenDelay };

    if (_editing) {
        if (ki.esc)   { _editing = false; _editBuf = ""; _needsRedraw = true; return; }
        if (ki.enter) {
            int v = _editBuf.toInt();
            if (v >= 0) { *vals[_timingSel] = v; saveTimingSettings(); _idSaved = true; }
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.del && _editBuf.length() > 0) { _editBuf.remove(_editBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.ch >= '0' && ki.ch <= '9') { _editBuf += ki.ch; _idSaved = false; _needsRedraw = true; }
        return;
    }
    if (ki.arrowLeft)  { _page = 6; _timingSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowRight) { _page = 8; _idSel = 0; _needsRedraw = true; }  // -> Startup App
    else if (ki.arrowUp)   { _timingSel = (_timingSel - 1 + N) % N; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowDown) { _timingSel = (_timingSel + 1) % N;     _idSaved = false; _needsRedraw = true; }
    else if (ki.enter) { _editBuf = String(*vals[_timingSel]); _editing = true; _idSaved = false; _needsRedraw = true; }
}

// ---- Page 8: Startup App ----

void AppSettings::_drawPage8() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(8);

    const auto& apps = uiManager.apps();
    int N_APPS = (int)apps.size() - 1; // exclude launcher at 0

    int y = CONTENT_Y;
    disp.setTextSize(1);
    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("Default app on boot:", 4, y); y += 16;

    int visible = (disp.height() - y - BAR_BOT_H - 4) / 15;
    int scroll  = max(0, _idSel - visible + 1);

    for (int i = 0; i < N_APPS && (i - scroll) < visible; i++) {
        if (i < scroll) continue;
        int appIdx = i + 1;
        bool sel = (_idSel == i);
        bool cur = (defaultAppIndex == appIdx);
        uint16_t rowBg = sel ? selBgColor() : (uint16_t)SETTINGS_BG;
        if (sel) disp.fillRect(0, y - 1, disp.width(), 14, rowBg);

        disp.setTextColor(sel ? TFT_WHITE : labelColor(), rowBg);
        char row[36];
        const char* name = (appIdx < (int)apps.size()) ? apps[appIdx]->appName() : "?";
        snprintf(row, sizeof(row), "%s%s", sel ? "> " : "  ", name);
        disp.drawString(row, 4, y);

        if (cur) {
            const char* badge = "DEFAULT";
            int bw = disp.textWidth(badge) + 6;
            int bx = disp.width() - bw - 2;
            disp.fillRoundRect(bx, y - 1, bw, 12, 3, disp.color565(20, 80, 20));
            disp.setTextColor(disp.color565(80, 220, 80), disp.color565(20, 80, 20));
            disp.drawString(badge, bx + 3, y);
        }
        y += 15;
    }

    if (_idSaved) {
        disp.setTextColor(TFT_GREEN, SETTINGS_BG);
        disp.drawString("Saved! Takes effect on reboot.", 4, disp.height() - BAR_BOT_H - 14);
    }

    _drawBottomBar("up/dn select  ENTER set default  </> page");
}

void AppSettings::_handlePage8(KeyInput ki) {
    int N = (int)uiManager.apps().size() - 1;
    if (N < 1) N = 1;
    if (ki.arrowLeft)  { _page = 7; _timingSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowRight) { _page = 9; _idSel = 0; _needsRedraw = true; }
    else if (ki.arrowUp)   { _idSel = (_idSel - 1 + N) % N; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowDown) { _idSel = (_idSel + 1) % N;      _idSaved = false; _needsRedraw = true; }
    else if (ki.enter) {
        defaultAppIndex = _idSel + 1;
        saveDefaultAppSettings();
        _idSaved = true;
        _needsRedraw = true;
    }
}

// ---- Page 9: App Layout ----

void AppSettings::_drawPage9() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(9);

    const auto& apps = uiManager.apps();
    int N_LAYOUT_APPS = (int)apps.size() - 1; // exclude launcher
    if (N_LAYOUT_APPS < 1) N_LAYOUT_APPS = 1;

    int y = CONTENT_Y;
    disp.setTextSize(1);
    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("App order & visibility:", 4, y); y += 14;

    if ((int)appOrder.size() < N_LAYOUT_APPS) {
        for (int i = (int)appOrder.size() + 1; i <= N_LAYOUT_APPS; i++) appOrder.push_back(i);
    }
    while ((int)appHidden.size() < (int)appOrder.size()) appHidden.push_back(false);

    int visible = (disp.height() - y - BAR_BOT_H - 4) / 15;
    int scroll  = 0;
    if (_idSel >= visible) scroll = _idSel - visible + 1;

    for (int i = 0; i < N_LAYOUT_APPS && (i - scroll) < visible; i++) {
        if (i < scroll) continue;
        int slot   = i;
        int appIdx = appOrder[slot] - 1;  // 0-based into APP_LAYOUT_NAMES
        bool isSettings = (appOrder[slot] == N_LAYOUT_APPS);
        bool hidden = appHidden[slot];
        bool sel    = (_idSel == slot);

        uint16_t rowBg = sel ? (_editing ? disp.color565(80,40,0) : selBgColor()) : (uint16_t)SETTINGS_BG;
        if (sel) disp.fillRect(0, y - 1, disp.width(), 14, rowBg);

        // Position indicator
        char pos[4]; snprintf(pos, sizeof(pos), "%2d.", slot + 1);
        disp.setTextColor(disp.color565(80, 80, 80), rowBg);
        disp.drawString(pos, 4, y);

        // App name
        disp.setTextColor(sel ? TFT_WHITE : (hidden ? disp.color565(80, 80, 80) : labelColor()), rowBg);
        const char* name = (appIdx >= 0 && (appIdx + 1) < (int)apps.size()) ? apps[appIdx + 1]->appName() : "?";
        char lbl[24]; snprintf(lbl, sizeof(lbl), "%s%s", sel ? "> " : "  ", name);
        disp.drawString(lbl, 24, y);

        // Badges
        int rx = disp.width() - 4;
        if (isSettings) {
            const char* badge = "FIXED";
            int bw = disp.textWidth(badge) + 6;
            rx -= bw;
            disp.fillRoundRect(rx, y - 1, bw, 12, 2, disp.color565(30, 30, 80));
            disp.setTextColor(disp.color565(100, 100, 200), disp.color565(30, 30, 80));
            disp.drawString(badge, rx + 3, y);
        } else {
            const char* vis = hidden ? "HIDDEN" : "SHOW";
            uint16_t bb = hidden ? disp.color565(70, 20, 20) : disp.color565(20, 60, 20);
            uint16_t bc = hidden ? disp.color565(200, 60, 60) : disp.color565(80, 220, 80);
            int bw = disp.textWidth(vis) + 8;
            rx -= bw;
            disp.fillRoundRect(rx, y - 1, bw, 12, 3, bb);
            disp.setTextColor(bc, bb);
            disp.drawString(vis, rx + 4, y);
        }

        y += 15;
    }

    if (_idSaved) {
        disp.setTextColor(TFT_GREEN, SETTINGS_BG);
        disp.drawString("Saved!", 4, disp.height() - BAR_BOT_H - 14);
    }

    if (_editing)
        _drawBottomBar("up/dn MOVE  ENTER/ESC done  H toggle");
    else
        _drawBottomBar("up/dn cursor  ENTER reorder  H hide  </> page");
}

void AppSettings::_handlePage9(KeyInput ki) {
    int N_LAYOUT_APPS = (int)uiManager.apps().size() - 1;
    if (N_LAYOUT_APPS < 1) N_LAYOUT_APPS = 1;
    if ((int)appOrder.size() < N_LAYOUT_APPS) {
        for (int i = (int)appOrder.size() + 1; i <= N_LAYOUT_APPS; i++) appOrder.push_back(i);
    }
    while ((int)appHidden.size() < (int)appOrder.size()) appHidden.push_back(false);

    // Left/right = page navigation (always)
    if (ki.arrowLeft && !_editing) {
        _page = 8; _idSel = 0; _idSaved = false; _editing = false; _needsRedraw = true; return;
    }
    if (ki.arrowRight && !_editing) {
        _page = 10; _idSel = 0; _editing = false; _needsRedraw = true; return;
    }

    if (!_editing) {
        // Up/down = move cursor
        if (ki.arrowUp)   { _idSel = (_idSel - 1 + N_LAYOUT_APPS) % N_LAYOUT_APPS; _idSaved = false; _needsRedraw = true; return; }
        if (ki.arrowDown) { _idSel = (_idSel + 1) % N_LAYOUT_APPS;                  _idSaved = false; _needsRedraw = true; return; }

        // ENTER = enter reorder mode for selected item (Settings always fixed)
        if (ki.enter && appOrder[_idSel] != N_LAYOUT_APPS) {
            _editing = true; _needsRedraw = true; return;
        }

        // H = toggle hidden without entering reorder mode
        if ((ki.ch == 'h' || ki.ch == 'H') && appOrder[_idSel] != N_LAYOUT_APPS) {
            appHidden[_idSel] = !appHidden[_idSel];
            saveAppLayout(); _idSaved = true; _needsRedraw = true;
        }
    } else {
        // Reorder mode: up/down moves the item, ENTER/ESC exits
        if (ki.arrowUp && _idSel > 0) {
            std::swap(appOrder[_idSel], appOrder[_idSel - 1]);
            std::swap(appHidden[_idSel], appHidden[_idSel - 1]);
            _idSel--;
            saveAppLayout(); _idSaved = true; _needsRedraw = true; return;
        }
        if (ki.arrowDown && _idSel < N_LAYOUT_APPS - 1) {
            std::swap(appOrder[_idSel], appOrder[_idSel + 1]);
            std::swap(appHidden[_idSel], appHidden[_idSel + 1]);
            _idSel++;
            saveAppLayout(); _idSaved = true; _needsRedraw = true; return;
        }
        if (ki.enter || ki.esc) {
            _editing = false; _needsRedraw = true; return;
        }
    }
}

// ---- Page 10: CS Security (auto-lock, auto-wipe) ----

void AppSettings::_drawPage10() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(10);

    int y = CONTENT_Y;
    disp.setTextSize(1);

    if (credStoreLocked) {
        disp.setTextColor(disp.color565(200, 100, 50), SETTINGS_BG);
        disp.drawString("Unlock CredStore first", 4, y + 10);
        disp.drawString("to change these settings.", 4, y + 24);
        // Still show current values and failed counter read-only
        y += 46;
        disp.setTextColor(disp.color565(130, 130, 130), SETTINGS_BG);
        char buf[32];
        snprintf(buf, sizeof(buf), "Auto-lock: %ds", csAutoLockSecs);
        disp.drawString(buf, 4, y); y += 14;
        snprintf(buf, sizeof(buf), "Auto-wipe: %d fails", csAutoWipeAttempts);
        disp.drawString(buf, 4, y); y += 14;
        int fails = csGetFailedAttempts();
        char failStr[32];
        if (csAutoWipeAttempts > 0)
            snprintf(failStr, sizeof(failStr), "Fails: %d/%d", fails, csAutoWipeAttempts);
        else
            snprintf(failStr, sizeof(failStr), "Fails: %d", fails);
        disp.setTextColor(fails > 0 ? disp.color565(220, 80, 80) : disp.color565(80, 200, 80), SETTINGS_BG);
        disp.drawString(failStr, 4, y);
        _drawBottomBar("Unlock CredStore to edit  </>");
        return;
    }

    // Auto-lock row
    bool lockSel = (_toggleSel == 0);
    uint16_t lockBg = lockSel ? selBgColor() : (uint16_t)SETTINGS_BG;
    if (lockSel) disp.fillRect(0, y-1, disp.width(), 14, lockBg);
    disp.setTextColor(labelColor(), lockBg);
    disp.drawString("Auto-lock (secs, 0=off):", 4, y);
    char lockVal[12]; snprintf(lockVal, sizeof(lockVal), "%d", csAutoLockSecs);
    if (_editing && lockSel) {
        String buf = _editBuf + "_";
        int bx = disp.width() - disp.textWidth(buf.c_str()) - 8;
        disp.setTextColor(TFT_YELLOW, lockBg);
        disp.drawString(buf.c_str(), bx, y);
    } else {
        int vw = disp.textWidth(lockVal);
        disp.setTextColor(lockSel ? TFT_WHITE : labelColor(), lockBg);
        disp.drawString(lockVal, disp.width() - vw - 8, y);
    }
    y += 18;

    // Auto-wipe row
    bool wipeSel = (_toggleSel == 1);
    uint16_t wipeBg = wipeSel ? selBgColor() : (uint16_t)SETTINGS_BG;
    if (wipeSel) disp.fillRect(0, y-1, disp.width(), 14, wipeBg);
    disp.setTextColor(labelColor(), wipeBg);
    disp.drawString("Auto-wipe after N fails:", 4, y);
    char wipeVal[12]; snprintf(wipeVal, sizeof(wipeVal), "%d", csAutoWipeAttempts);
    if (_editing && wipeSel) {
        String buf = _editBuf + "_";
        int bx = disp.width() - disp.textWidth(buf.c_str()) - 8;
        disp.setTextColor(TFT_YELLOW, wipeBg);
        disp.drawString(buf.c_str(), bx, y);
    } else {
        int vw = disp.textWidth(wipeVal);
        disp.setTextColor(wipeSel ? TFT_WHITE : labelColor(), wipeBg);
        disp.drawString(wipeVal, disp.width() - vw - 8, y);
    }
    y += 18;

    // Failed attempts counter
    int fails = csGetFailedAttempts();
    disp.setTextColor(disp.color565(130, 130, 130), SETTINGS_BG);
    disp.drawString("Failed unlock attempts:", 4, y);
    char failVal[12]; snprintf(failVal, sizeof(failVal), "%d", fails);
    uint16_t fc = fails > 0 ? disp.color565(220, 80, 80) : disp.color565(80, 200, 80);
    disp.setTextColor(fc, SETTINGS_BG);
    disp.drawString(failVal, disp.width() - disp.textWidth(failVal) - 8, y);
    y += 18;

    // Reset fails shortcut
    disp.setTextColor(disp.color565(80, 130, 80), SETTINGS_BG);
    disp.drawString("R = reset failed counter", 4, y);

    if (_idSaved) {
        disp.setTextColor(TFT_GREEN, SETTINGS_BG);
        disp.drawString("Saved!", 4, disp.height() - BAR_BOT_H - 14);
    }

    _drawBottomBar("up/dn select  ENTER edit  R=reset fails  </>");
}

void AppSettings::_handlePage10(KeyInput ki) {
    if (ki.arrowLeft && !_editing) {
        _page = 9; _toggleSel = 0; _editing = false; _editBuf = ""; _needsRedraw = true; return;
    }
    if (ki.arrowRight && !_editing) {
        _page = 11; _toggleSel = 0; _editing = false; _editBuf = ""; _needsRedraw = true; return;
    }

    // Navigation works even when locked; editing does not
    if (credStoreLocked) { _needsRedraw = true; return; }

    if (!_editing) {
        if (ki.arrowUp)   { _toggleSel = (_toggleSel - 1 + 2) % 2; _needsRedraw = true; return; }
        if (ki.arrowDown) { _toggleSel = (_toggleSel + 1) % 2;     _needsRedraw = true; return; }

        if (ki.ch == 'r' || ki.ch == 'R') {
            csResetFailedAttempts();
            _idSaved = true; _needsRedraw = true; return;
        }

        if (ki.enter) {
            _editing = true;
            _editBuf = (_toggleSel == 0) ? String(csAutoLockSecs) : String(csAutoWipeAttempts);
            _needsRedraw = true; return;
        }
        return;
    }

    // Editing
    if (ki.esc || ki.enter) {
        if (ki.enter && _editBuf.length() > 0) {
            int val = _editBuf.toInt();
            if (val < 0) val = 0;
            if (_toggleSel == 0) csAutoLockSecs     = val;
            else                  csAutoWipeAttempts = val;
            saveCsSecuritySettings();
            _idSaved = true;
        }
        _editing = false; _editBuf = ""; _needsRedraw = true; return;
    }
    if (ki.del && _editBuf.length() > 0) { _editBuf.remove(_editBuf.length()-1); _needsRedraw = true; return; }
    if (ki.ch >= '0' && ki.ch <= '9' && (int)_editBuf.length() < 6) { _editBuf += ki.ch; _needsRedraw = true; }
}

// ---- Page 11: SD Storage ----

void AppSettings::_drawPage11() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(11);

    int y = CONTENT_Y;
    disp.setTextSize(1);

    bool onSd = (csStorageLocation == "sd");
    bool sdOk = sdAvailable();
    bool nvsHasDb = ([]{ preferences.begin("kprox_db", true);
                          bool h = preferences.getInt("cs_db_n", 0) > 0;
                          preferences.end(); return h; })();
    bool sdHasDb = sdExists();

    // Current location header
    disp.setTextColor(labelColor(), SETTINGS_BG);
    String locLine = "Active: ";
    locLine += onSd ? "SD card" : "NVS (built-in flash)";
    disp.drawString(locLine, 4, y); y += 12;

    // DB presence indicators
    disp.setTextColor(disp.color565(120, 120, 120), SETTINGS_BG);
    String nvsInfo = "NVS: "; nvsInfo += nvsHasDb ? "has DB" : "empty";
    String sdInfo  = "SD:  "; sdInfo  += !sdOk ? "not found" : (sdHasDb ? "has DB" : "empty");
    disp.drawString(nvsInfo, 4, y); disp.drawString(sdInfo, 80, y); y += 16;

    // --- Transfer row (sel 0) ---
    // Copies DB to other location and wipes source. Requires unlock.
    bool sel0 = (_toggleSel == 0);
    uint16_t bg0 = sel0 ? selBgColor() : (uint16_t)SETTINGS_BG;
    if (sel0) disp.fillRect(0, y-1, disp.width(), 26, bg0);
    disp.setTextColor(sel0 ? TFT_WHITE : disp.color565(100, 200, 255), bg0);
    String transferDest = onSd ? "NVS" : "SD";
    disp.drawString(sel0 ? ">" : " ", 4, y);
    disp.drawString("Transfer to " + transferDest, 14, y); y += 12;
    disp.setTextColor(sel0 ? disp.color565(200, 200, 200) : disp.color565(90, 90, 90), bg0);
    disp.drawString(" Moves DB + wipes source (unlock req.)", 14, y); y += 16;

    // --- Switch row (sel 1) ---
    // Changes active location with no data movement.
    bool sel1 = (_toggleSel == 1);
    uint16_t bg1 = sel1 ? selBgColor() : (uint16_t)SETTINGS_BG;
    if (sel1) disp.fillRect(0, y-1, disp.width(), 26, bg1);
    String switchDest = onSd ? "NVS" : "SD";
    bool switchOk = onSd ? true : sdOk;
    uint16_t sw1Color = switchOk ? (sel1 ? TFT_WHITE : disp.color565(100, 220, 130))
                                  : disp.color565(80, 80, 80);
    disp.setTextColor(sw1Color, bg1);
    disp.drawString(sel1 ? ">" : " ", 4, y);
    disp.drawString("Switch to " + switchDest, 14, y); y += 12;
    disp.setTextColor(sel1 ? disp.color565(200, 200, 200) : disp.color565(90, 90, 90), bg1);
    disp.drawString(" No data moved, just changes active ptr", 14, y); y += 16;

    // --- Format SD row (sel 2) ---
    bool sel2 = (_toggleSel == 2);
    uint16_t bg2 = sel2 ? disp.color565(80, 20, 20) : (uint16_t)SETTINGS_BG;
    if (sel2) disp.fillRect(0, y-1, disp.width(), 14, bg2);
    uint16_t fmtColor = sdOk ? (sel2 ? TFT_WHITE : disp.color565(220, 80, 80))
                              : disp.color565(80, 80, 80);
    disp.setTextColor(fmtColor, bg2);
    disp.drawString(sel2 ? ">" : " ", 4, y);
    disp.drawString("Format SD card", 14, y); y += 18;

    if (_idSaved) {
        disp.setTextColor(disp.color565(80, 220, 80), SETTINGS_BG);
        disp.drawString(_editBuf.isEmpty() ? "Done." : _editBuf, 4, y);
    } else if (!_editBuf.isEmpty()) {
        disp.setTextColor(disp.color565(220, 80, 80), SETTINGS_BG);
        disp.drawString(_editBuf, 4, y);
    }

    // Confirmation overlay
    if (_storageConfirmOp > 0) {
        int ox = 4, oy = disp.height() - BAR_BOT_H - 70;
        int ow = disp.width() - 8, oh = 66;
        uint16_t obg = disp.color565(60, 35, 10);
        disp.fillRoundRect(ox, oy, ow, oh, 4, obg);
        disp.drawRoundRect(ox, oy, ow, oh, 4, disp.color565(220, 160, 40));
        disp.setTextSize(1);
        disp.setTextColor(TFT_WHITE, obg);
        if (_storageConfirmOp == 1) {
            // Transfer confirmation
            String srcName = (csStorageLocation == "sd") ? "SD" : "NVS";
            String dstName = _storageConfirmDest == "sd" ? "SD" : "NVS";
            disp.drawString("Transfer " + srcName + " -> " + dstName + "?", ox+4, oy+4);
            disp.setTextColor(disp.color565(255, 200, 100), obg);
            disp.drawString("DB copied to " + dstName + ", then " + srcName, ox+4, oy+16);
            disp.drawString("is wiped. Store must be unlocked.", ox+4, oy+28);
            if (credStoreLocked) {
                disp.setTextColor(disp.color565(255, 100, 80), obg);
                disp.drawString("! Unlock the store first !", ox+4, oy+42);
                _drawBottomBar("N = cancel");
            } else {
                disp.setTextColor(disp.color565(180, 255, 180), obg);
                disp.drawString("Store is unlocked - ready.", ox+4, oy+42);
                _drawBottomBar("Y = transfer + wipe source  N = cancel");
            }
        } else {
            // Switch confirmation
            String dstName = _storageConfirmDest == "sd" ? "SD" : "NVS";
            bool destEmpty = (_storageConfirmDest == "sd") ? !sdHasDb : !nvsHasDb;
            disp.drawString("Switch active DB to " + dstName + "?", ox+4, oy+4);
            disp.setTextColor(disp.color565(255, 200, 100), obg);
            disp.drawString("No data is moved. Store will lock.", ox+4, oy+16);
            if (destEmpty) {
                disp.setTextColor(disp.color565(255, 150, 80), obg);
                disp.drawString(dstName + " has no DB - device may not unlock", ox+4, oy+28);
                disp.drawString("until a DB is transferred there.", ox+4, oy+40);
                _drawBottomBar("Y = switch anyway  N = cancel");
            } else {
                disp.setTextColor(disp.color565(180, 255, 180), obg);
                disp.drawString(dstName + " has a DB - ready to switch.", ox+4, oy+28);
                _drawBottomBar("Y = switch  N = cancel");
            }
        }
    } else {
        _drawBottomBar("up/dn=select  ENTER  fn+</>= page");
    }
}

void AppSettings::_handlePage11(KeyInput ki) {
    if (ki.arrowLeft && !_editing && _storageConfirmOp == 0) {
        _page = 10; _toggleSel = 0; _editing = false; _editBuf = ""; _idSaved = false;
        _sdConfirmPending = false; _storageConfirmOp = 0; _needsRedraw = true; return;
    }
    if (ki.arrowRight && !_editing && _storageConfirmOp == 0) {
        _page = 12; _backupSel = 0; _backupScroll = 0; _backupStatus = ""; _backupRefresh();
        _sdConfirmPending = false; _storageConfirmOp = 0; _needsRedraw = true; return;
    }

    // Confirmation overlay active
    if (_storageConfirmOp > 0) {
        bool confirmed = (ki.ch == 'y' || ki.ch == 'Y');
        bool cancelled = (ki.ch == 'n' || ki.ch == 'N' || ki.esc);
        if (!confirmed && !cancelled) return;

        if (cancelled) {
            _storageConfirmOp = 0;
            _editBuf = "Cancelled";
            _needsRedraw = true; return;
        }

        // confirmed
        if (_storageConfirmOp == 1) {
            // Transfer: copy to dest, wipe source
            if (credStoreLocked) {
                _storageConfirmOp = 0;
                _editBuf = "Unlock store first";
                _needsRedraw = true; return;
            }
            String oldLoc = csStorageLocation;
            csStorageLocation = _storageConfirmDest;
            if (writeKDBX(credStoreRuntimeKey)) {
                if (oldLoc == "sd") {
                    sdRemove();
                } else {
                    preferences.begin("kprox_db", false); preferences.clear(); preferences.end();
                }
                saveCsStorageLocation();
                _editBuf = "Transferred. Source wiped.";
                _idSaved = true;
            } else {
                csStorageLocation = oldLoc;
                _editBuf = "Write failed - source unchanged";
            }
        } else {
            // Switch: pointer only, lock store
            csStorageLocation = _storageConfirmDest;
            saveCsStorageLocation();
            if (!credStoreLocked) credStoreLock();
            _editBuf = "Switched. Re-unlock to use.";
            _idSaved = true;
        }
        _storageConfirmOp = 0;
        _needsRedraw = true; return;
    }

    if (ki.arrowUp)   { _toggleSel = (_toggleSel - 1 + 3) % 3; _idSaved = false; _editBuf = ""; _needsRedraw = true; return; }
    if (ki.arrowDown) { _toggleSel = (_toggleSel + 1) % 3;     _idSaved = false; _editBuf = ""; _needsRedraw = true; return; }

    if (!ki.enter) return;
    _idSaved = false; _editBuf = "";

    String dest = (csStorageLocation == "sd") ? "nvs" : "sd";

    if (_toggleSel == 0) {
        // Transfer
        if (dest == "sd" && !sdAvailable()) { _editBuf = "SD not found"; _needsRedraw = true; return; }
        _storageConfirmDest = dest;
        _storageConfirmOp = 1;

    } else if (_toggleSel == 1) {
        // Switch
        if (dest == "sd" && !sdAvailable()) { _editBuf = "SD not found"; _needsRedraw = true; return; }
        _storageConfirmDest = dest;
        _storageConfirmOp = 2;

    } else if (_toggleSel == 2) {
        // Format SD
        if (!sdAvailable()) { _editBuf = "SD not found"; _needsRedraw = true; return; }
        if (sdFormat()) {
            if (csStorageLocation == "sd") credStoreLock();
            _editBuf = "SD formatted";
            _idSaved = true;
        } else {
            _editBuf = "Format failed";
        }
    }
    _needsRedraw = true;
}

// ---- AppBase overrides ----

void AppSettings::onEnter() {
    _page = 0; _toggleSel = 0; _rebootNote = false;
    _idSel = 0; _editing = false; _idSaved = false; _editBuf = "";
    _timingSel = 0;
    _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = "";
    _keymapSaved = false; _dispSel = 0; _dispEditing = false;
    _keymapSel = 0;
    _sdConfirmPending = false;
    _storageConfirmOp = 0; _storageConfirmDest = "";
    for (int i = 0; i < S_BUILTIN_COUNT; i++) { if (activeKeymap == s_builtinKeymapIds[i]) { _keymapSel = i; break; } }
    _needsRedraw = true;
}

void AppSettings::onExit() {
    _editing = false; _editBuf = "";
}

void AppSettings::onUpdate() {
    // BtnA on timing pages triggers the test action — check before the keyboard anyKey gate
    // so a bare button press (no keyboard input) still fires.
    if ((_page == 6 || _page == 7) && !_editing && M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        playRegister(activeRegister);
        _needsRedraw = true;
        return;
    }

    // BtnA cycles pages on all other pages
    if (M5Cardputer.BtnA.wasPressed() && _page != 6 && _page != 7) {
        uiManager.notifyInteraction();
        _page = (_page + 1) % NUM_PAGES;
        _editing = false; _editBuf = ""; _idSaved = false; _toggleSel = 0;
        _timingSel = 0;
        if (_page == 0) { _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = ""; }
        _needsRedraw = true;
        return;
    }

    if (_needsRedraw) {
        switch (_page) {
            case 0: _drawPage0(); break;  // WiFi Settings
            case 1: _drawPage1(); break;  // Bluetooth
            case 2: _drawPage2(); break;  // USB HID
            case 3: _drawPage3(); break;  // API Key
            case 4: _drawPage4(); break;  // Device Identity
            case 5: _drawPage5(); break;  // Sink Config
            case 6: _drawPage6(); break;  // HID Timing 1/2
            case 7: _drawPage7(); break;  // HID Timing 2/2
            case 8: _drawPage8(); break;  // Startup App
            case 9: _drawPage9(); break;  // App Layout
            case 10: _drawPage10(); break; // CS Security
            case 11: _drawPage11(); break; // SD Storage
            case 12: _drawPage12(); break; // Backups
            case 13: _drawPage13(); break; // Keymap
            case 14: _drawPage14(); break; // Display
        }
        _needsRedraw = false;
    }

    bool isEditing = _wifiEditing || _editing;
    KeyInput ki = pollKeys(isEditing);
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (ki.esc) {
        if (_sdConfirmPending) { _sdConfirmPending = false; _editBuf = "Cancelled"; _needsRedraw = true; return; }
        if (_wifiEditing) { _wifiEditing = false; _wifiInputBuf = ""; _needsRedraw = true; return; }
        if (_editing)     { _editing = false; _editBuf = ""; _needsRedraw = true; return; }
        uiManager.returnToLauncher();
        return;
    }

    switch (_page) {
        case 0: _handlePage0(ki); break;  // WiFi Settings
        case 1: _handlePage1(ki); break;  // Bluetooth
        case 2: _handlePage2(ki); break;  // USB HID
        case 3: _handlePage3(ki); break;  // API Key
        case 4: _handlePage4(ki); break;  // Device Identity
        case 5: _handlePage5(ki); break;  // Sink Config
        case 6: _handlePage6(ki); break;  // HID Timing 1/2
        case 7: _handlePage7(ki); break;  // HID Timing 2/2
        case 8: _handlePage8(ki); break;  // Startup App
        case 9: _handlePage9(ki); break;  // App Layout
        case 10: _handlePage10(ki); break; // CS Security
        case 11: _handlePage11(ki); break; // SD Storage
        case 12: _handlePage12(ki); break; // Backups
        case 13: _handlePage13(ki); break; // Keymap
        case 14: _handlePage14(ki); break; // Display
    }
}


// ============================================================
// Page 12: Backups
// ============================================================
// Backup format — SD card, path: /backups/kprox_YYYYMMDD_HHMMSS.json
// JSON: { "version":1, "registers":[...], "registerNames":[...],
//          "activeRegister":N, "settings":{...} }
// "settings" is optional; "registers" is optional.
// Either or both can be present depending on what was backed up.

static const char* BACKUP_DIR = "/backups";

void AppSettings::_backupRefresh() {
    _backupFiles.clear();
    if (!sdAvailable()) return;
    // Unmount then remount to flush the FAT directory cache
    sdUnmount();
    if (!sdMount()) return;
    File dir = SD.open(BACKUP_DIR);
    if (!dir || !dir.isDirectory()) return;
    File f = dir.openNextFile();
    while (f) {
        String name = String(f.name());
        int sl = name.lastIndexOf('/');
        if (sl >= 0) name = name.substring(sl + 1);
        if (name.endsWith(".json")) _backupFiles.push_back(name);
        f.close();
        f = dir.openNextFile();
    }
    dir.close();
    // Sort newest first (names are timestamps)
    std::sort(_backupFiles.begin(), _backupFiles.end(),
              [](const String& a, const String& b){ return a > b; });
}

bool AppSettings::_backupCreate(bool includeRegs, bool includeSettings) {
    if (!sdAvailable()) { _backupStatus = "SD not available"; _backupStatusOk = false; return false; }
    sdMkdir(BACKUP_DIR);

    time_t t = time(nullptr);
    struct tm* tm = localtime(&t);
    char fname[48];
    snprintf(fname, sizeof(fname), "%s/kprox_%04d%02d%02d_%02d%02d%02d.json",
             BACKUP_DIR,
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    JsonDocument doc;
    doc["version"] = 1;

    if (includeRegs) {
        doc["activeRegister"] = activeRegister;
        JsonArray arr = doc["registers"].to<JsonArray>();
        for (int i = 0; i < (int)registers.size(); i++) {
            JsonObject o = arr.add<JsonObject>();
            o["number"]  = i;
            o["content"] = registers[i];
            o["name"]    = (i < (int)registerNames.size()) ? registerNames[i] : "";
        }
    }

    if (includeSettings) {
        JsonObject s = doc["settings"].to<JsonObject>();
        serializeAllSettings(s);
    }

    String json;
    serializeJson(doc, json);
    if (!sdWriteFile(String(fname), json)) {
        _backupStatus = "Write failed"; _backupStatusOk = false; return false;
    }
    _backupStatus = "Saved"; _backupStatusOk = true;
    _backupRefresh();
    return true;
}

bool AppSettings::_backupRestore(const String& filename) {
    String path = String(BACKUP_DIR) + "/" + filename;
    String json = sdReadFile(path);
    if (json.isEmpty()) { _backupStatus = "Read failed"; _backupStatusOk = false; return false; }

    JsonDocument doc;
    if (deserializeJson(doc, json)) { _backupStatus = "Parse error"; _backupStatusOk = false; return false; }

    if (doc["registers"].is<JsonArray>()) {
        registers.clear(); registerNames.clear();
        for (JsonVariant v : doc["registers"].as<JsonArray>()) {
            registers.push_back(v["content"] | "");
            registerNames.push_back(v["name"]    | "");
        }
        if (!doc["activeRegister"].isNull()) {
            int n = doc["activeRegister"].as<int>();
            if (n >= 0 && (size_t)n < registers.size()) activeRegister = n;
        }
        saveRegisters();
    }

    if (doc["settings"].is<JsonObject>()) {
        JsonObject s = doc["settings"].as<JsonObject>();
        deserializeAllSettings(s);
        saveAllSettings();
    }

    _backupStatus = "Restored"; _backupStatusOk = true;
    return true;
}

// ---- Draw page 12 ----

void AppSettings::_drawPage12() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(12);

    int y = CONTENT_Y;
    disp.setTextSize(1);

    // Action buttons: 0=regs only, 1=settings only, 2=both, then 3..N = restore entries
    struct { const char* lbl; int sel; } actions[] = {
        { "Backup Registers",         0 },
        { "Backup Settings",          1 },
        { "Backup Both",              2 },
        { "Refresh list",             3 },
    };
    int nActions = 4;

    for (int i = 0; i < nActions; i++) {
        bool sel = (_backupSel == i);
        uint16_t bg = sel ? selBgColor() : (uint16_t)SETTINGS_BG;
        if (sel) disp.fillRect(0, y - 1, disp.width(), 14, bg);
        disp.setTextColor(sel ? TFT_WHITE : labelColor(), bg);
        char row[40]; snprintf(row, sizeof(row), "%s%s", sel ? "> " : "  ", actions[i].lbl);
        disp.drawString(row, 4, y);
        y += 15;
    }

    // Divider
    disp.drawFastHLine(4, y, disp.width() - 8, disp.color565(50, 50, 50));
    y += 4;

    // SD backup file list
    if (!sdAvailable()) {
        disp.setTextColor(disp.color565(180, 80, 80), SETTINGS_BG);
        disp.drawString("SD not available", 4, y);
    } else if (_backupFiles.empty()) {
        disp.setTextColor(disp.color565(100, 100, 100), SETTINGS_BG);
        disp.drawString("No backups on SD", 4, y);
    } else {
        int visRows = (disp.height() - y - BAR_BOT_H - 14) / 14;
        if (_backupSel >= nActions) {
            int fileIdx = _backupSel - nActions;
            if (fileIdx >= _backupScroll + visRows) _backupScroll = fileIdx - visRows + 1;
            if (fileIdx < _backupScroll) _backupScroll = fileIdx;
            if (_backupScroll < 0) _backupScroll = 0;
        }
        for (int i = 0; i < visRows && (_backupScroll + i) < (int)_backupFiles.size(); i++) {
            int idx = _backupScroll + i;
            bool sel = (_backupSel == nActions + idx);
            uint16_t bg = sel ? selBgColor() : (uint16_t)SETTINGS_BG;
            if (sel) disp.fillRect(0, y - 1, disp.width(), 13, bg);
            disp.setTextColor(sel ? TFT_WHITE : disp.color565(140, 200, 140), bg);
            String name = _backupFiles[idx];
            // Trim to fit: "kprox_20240315_143022.json" -> "20240315 14:30"
            String display = name;
            if (name.startsWith("kprox_") && name.length() >= 20) {
                String d2 = name.substring(6, 14);  // YYYYMMDD
                String t2 = name.substring(15, 21); // HHMMSS
                display = d2.substring(0,4)+"-"+d2.substring(4,6)+"-"+d2.substring(6)
                          +" "+t2.substring(0,2)+":"+t2.substring(2,4);
            }
            if ((int)display.length() > 22) display = display.substring(0, 20) + "..";
            disp.drawString((sel ? "> " : "  ") + display, 4, y);
            if (sel) {
                disp.setTextColor(disp.color565(100, 200, 100), bg);
                disp.drawString("ENTER=restore", disp.width() - 82, y);
            }
            y += 14;
        }
    }

    // Status message
    if (!_backupStatus.isEmpty()) {
        int sy = disp.height() - BAR_BOT_H - 14;
        disp.setTextColor(_backupStatusOk ? TFT_GREEN : disp.color565(220, 80, 80), SETTINGS_BG);
        disp.drawString(_backupStatus, 4, sy);
    }

    _drawBottomBar("up/dn  ENTER=action  D=delete  </> page");
}

// ---- Handle page 12 ----

void AppSettings::_handlePage12(KeyInput ki) {
    int nActions = 4;
    int total = nActions + (int)_backupFiles.size();

    if (ki.arrowLeft)  { _page = 11; _toggleSel = 0; _needsRedraw = true; return; }  // SD Storage
    if (ki.arrowRight) {
        _page = 13; _keymapSaved = false;
        for (int i = 0; i < S_BUILTIN_COUNT; i++) { if (activeKeymap == s_builtinKeymapIds[i]) { _keymapSel = i; break; } }
        _needsRedraw = true; return;
    }  // Keymap



    if (ki.arrowUp)   { _backupSel = (_backupSel - 1 + total) % total; _backupStatus = ""; _needsRedraw = true; return; }
    if (ki.arrowDown) { _backupSel = (_backupSel + 1) % total;         _backupStatus = ""; _needsRedraw = true; return; }

    if ((ki.ch == 'd' || ki.ch == 'D') && _backupSel >= nActions) {
        int idx = _backupSel - nActions;
        if (idx < (int)_backupFiles.size()) {
            String path = String(BACKUP_DIR) + "/" + _backupFiles[idx];
            bool deleted = sdDeleteFile(path);
            _backupRefresh();  // remounts to flush cache
            int maxSel = nActions + (int)_backupFiles.size() - 1;
            if (_backupSel > maxSel) _backupSel = max(nActions - 1, maxSel);
            _backupStatus = deleted ? "Deleted" : "Delete failed";
            _backupStatusOk = deleted;
        }
        _needsRedraw = true; return;
    }

    if (!ki.enter) return;
    _backupStatus = "";

    switch (_backupSel) {
        case 0: _backupCreate(true,  false); break;
        case 1: _backupCreate(false, true);  break;
        case 2: _backupCreate(true,  true);  break;
        case 3: _backupRefresh(); _backupStatus = "Refreshed"; _backupStatusOk = true; break;
        default: {
            int idx = _backupSel - nActions;
            if (idx >= 0 && idx < (int)_backupFiles.size())
                _backupRestore(_backupFiles[idx]);
            break;
        }
    }
    _needsRedraw = true;
}

// ============================================================
// Page 13: Keymap Selection
// ============================================================

void AppSettings::_drawPage13() {
    auto& d = M5Cardputer.Display;
    _drawTopBar(13);
    d.fillRect(0, BAR_TOP_H, d.width(), d.height() - BAR_TOP_H - BAR_BOT_H, 0x0000);

    d.setTextSize(1);
    int y = BAR_TOP_H + 4;
    int rowH = 11;
    int visRows = (d.height() - BAR_TOP_H - BAR_BOT_H - 4) / rowH;
    int scroll = max(0, _keymapSel - visRows / 2);

    for (int i = scroll; i < S_BUILTIN_COUNT && (i - scroll) < visRows; i++) {
        bool sel     = (i == _keymapSel);
        bool active  = (activeKeymap == String(s_builtinKeymapIds[i]));
        uint16_t bg  = sel ? d.color565(20, 60, 120) : (uint16_t)0x0000;
        uint16_t tc  = active ? (uint16_t)TFT_GREEN : sel ? (uint16_t)TFT_WHITE : d.color565(180,180,180);
        if (sel) d.fillRect(0, y, d.width(), rowH, bg);
        d.setTextColor(tc, bg);
        String label = String(active ? "* " : "  ") + s_builtinKeymapNames[i];
        d.drawString(label, 4, y + 1);
        y += rowH;
    }

    if (_keymapSaved) {
        d.setTextColor(TFT_GREEN, 0x0000);
        d.drawString("Saved!", d.width() - 44, BAR_TOP_H + 4);
    }

    _drawBottomBar("up/dn=select  ENTER=apply  fn+</>= page");
}

void AppSettings::_handlePage13(KeyInput ki) {
    if (ki.arrowLeft)  { _page = 12; _backupSel = 0; _backupScroll = 0; _backupStatus = ""; _backupRefresh(); _needsRedraw = true; return; }
    if (ki.arrowRight) { _page = 14; _dispSel = 0; _needsRedraw = true; return; }

    if (ki.arrowUp)   { if (_keymapSel > 0) _keymapSel--; _keymapSaved = false; _needsRedraw = true; return; }
    if (ki.arrowDown) { if (_keymapSel < S_BUILTIN_COUNT - 1) _keymapSel++; _keymapSaved = false; _needsRedraw = true; return; }

    if (ki.enter) {
        keymapLoad(String(s_builtinKeymapIds[_keymapSel]));
        keymapSaveActive();
        _keymapSaved = true;
        _needsRedraw = true;
    }
}

// ============================================================
// Page 14: Display Settings
// ============================================================

void AppSettings::_drawPage14() {
    auto& d = M5Cardputer.Display;
    _drawTopBar(14);
    d.fillRect(0, BAR_TOP_H, d.width(), d.height() - BAR_TOP_H - BAR_BOT_H, 0x0000);
    d.setTextSize(1);

    int y = BAR_TOP_H + 8;

    // Brightness row
    {
        bool sel     = (_dispSel == 0);
        bool editing = (sel && _dispEditing);
        uint16_t bg  = editing ? d.color565(20, 80, 20)
                     : sel    ? d.color565(20, 60, 120)
                     :          (uint16_t)0x0000;
        if (sel) d.fillRect(0, y - 2, d.width(), 14, bg);
        d.setTextColor(sel ? TFT_WHITE : d.color565(160, 160, 160), bg);
        d.drawString(editing ? "> Brightness:" : "  Brightness:", 4, y);
        int barX = 100, barW = d.width() - barX - 28;
        d.drawRect(barX, y, barW, 10, d.color565(60, 60, 60));
        int fill = (int)((long)barW * g_displayBrightness / 255);
        if (fill > 0) d.fillRect(barX + 1, y + 1, fill,
                                  8, editing ? d.color565(80, 220, 80) : d.color565(80, 160, 255));
        char vbuf[8]; snprintf(vbuf, sizeof(vbuf), "%d", g_displayBrightness);
        d.setTextColor(editing ? TFT_GREEN : sel ? TFT_WHITE : d.color565(140, 140, 140), 0x0000);
        d.drawString(vbuf, barX + barW + 3, y);
        y += 22;
    }

    // Timeout row
    {
        bool sel     = (_dispSel == 1);
        bool editing = (sel && _dispEditing);
        uint16_t bg  = editing ? d.color565(20, 80, 20)
                     : sel    ? d.color565(20, 60, 120)
                     :          (uint16_t)0x0000;
        if (sel) d.fillRect(0, y - 2, d.width(), 14, bg);
        d.setTextColor(sel ? TFT_WHITE : d.color565(160, 160, 160), bg);
        d.drawString(editing ? "> Timeout:" : "  Timeout:", 4, y);

        static const unsigned long timeoutOpts[]  = {5000, 10000, 30000, 60000};
        static const char*         timeoutLabels[] = {"5s", "10s", "30s", "1min"};
        static constexpr int       TIMEOUT_COUNT   = 4;

        int tx = 80;
        for (int i = 0; i < TIMEOUT_COUNT; i++) {
            bool active = (g_screenTimeoutMs == timeoutOpts[i]);
            uint16_t tbg = active ? (editing ? d.color565(20, 100, 20) : d.color565(30, 90, 30))
                                  : d.color565(35, 35, 35);
            uint16_t ttc = active ? TFT_GREEN : d.color565(150, 150, 150);
            int tw = d.textWidth(timeoutLabels[i]) + 6;
            d.fillRoundRect(tx, y, tw, 12, 2, tbg);
            d.setTextColor(ttc, tbg);
            d.drawString(timeoutLabels[i], tx + 3, y + 2);
            tx += tw + 3;
        }
        y += 22;
    }

    if (_dispEditing)
        _drawBottomBar("</> adjust  ENTER save  ESC cancel");
    else
        _drawBottomBar("up/dn=row  ENTER=edit  fn+</>= page");
}

void AppSettings::_handlePage14(KeyInput ki) {
    if (_dispEditing) {
        // ESC or ENTER exits edit mode (changes are saved on each adjustment)
        if (ki.esc || ki.enter) {
            _dispEditing = false;
            _needsRedraw = true;
            return;
        }
        if (_dispSel == 0) {
            if (ki.arrowLeft)  g_displayBrightness = max(16,  g_displayBrightness - 16);
            if (ki.arrowRight) g_displayBrightness = min(255, g_displayBrightness + 16);
            if (ki.arrowLeft || ki.arrowRight) {
                M5Cardputer.Display.setBrightness((uint8_t)g_displayBrightness);
                saveDisplaySettings();
                _needsRedraw = true;
            }
        } else {
            static const unsigned long opts[] = {5000, 10000, 30000, 60000};
            constexpr int N = 4;
            int cur = 0;
            for (int i = 0; i < N; i++) if (g_screenTimeoutMs == opts[i]) { cur = i; break; }
            if (ki.arrowLeft)  cur = (cur - 1 + N) % N;
            if (ki.arrowRight) cur = (cur + 1) % N;
            if (ki.arrowLeft || ki.arrowRight) {
                g_screenTimeoutMs = opts[cur];
                saveDisplaySettings();
                _needsRedraw = true;
            }
        }
        return;
    }

    // Not editing
    if (ki.arrowUp || ki.arrowDown) {
        _dispSel = (_dispSel + 1) % 2;
        _needsRedraw = true;
        return;
    }
    if (ki.enter) {
        _dispEditing = true;
        _needsRedraw = true;
        return;
    }
    if (ki.arrowLeft)  {
        _page = 13;
        for (int i = 0; i < S_BUILTIN_COUNT; i++) { if (activeKeymap == s_builtinKeymapIds[i]) { _keymapSel = i; break; } }
        _keymapSaved = false; _needsRedraw = true; return;
    }
    if (ki.arrowRight) {
        _page = 0; _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = "";
        _wifiStatusMsg = ""; _wifiEditing = false; _wifiSel = 0; _needsRedraw = true; return;
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
