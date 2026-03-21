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

namespace Cardputer {

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
        "Startup App", "App Layout", "CS Security", "SD Storage"
    };
    disp.drawString(pageLabels[pageNum], 4, 3);

    char pageStr[8];
    snprintf(pageStr, sizeof(pageStr), "%d/%d", pageNum + 1, NUM_PAGES);
    int pw = disp.textWidth(pageStr);
    disp.drawString(pageStr, disp.width() - pw - 4, 3);
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

// ---- Page 0: Bluetooth ----

void AppSettings::_drawPage0() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(0);

    // 3 rows: BT enable, BT Keyboard, BT Mouse
    bool btConn = bluetoothInitialized && BLE_KEYBOARD_VALID && BLE_KEYBOARD.isConnected();
    const char* connStr = btConn ? "connected" : (bluetoothEnabled ? "no peer" : "");
    uint16_t connCol = btConn ? disp.color565(80,220,80) : disp.color565(200,160,0);

    struct { const char* label; bool* flag; } rows[3] = {
        { "Bluetooth",    &bluetoothEnabled   },
        { "  BT Keyboard", &bleKeyboardEnabled },
        { "  BT Mouse",    &bleMouseEnabled    },
    };

    for (int i = 0; i < 3; i++) {
        int ry = CONTENT_Y + i * 22;
        _drawToggleRow(ry, i == _toggleSel, rows[i].label, *rows[i].flag,
                       i == 0 ? connStr : nullptr, connCol);
    }

    if (_rebootNote) {
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(200,160,0), SETTINGS_BG);
        disp.drawString("Reboot to apply BT changes", 4, CONTENT_Y + 3*22 + 4);
    }

    _drawBottomBar("up/dn  ENTER toggle  C reconnect  </> page  ESC");
}

void AppSettings::_handlePage0(KeyInput ki) {
    static constexpr int N = 3;
    if (ki.arrowLeft)  { _page = 0; _needsRedraw = true; return; }  // left of BT = WiFi
    if (ki.arrowRight) { _page = 2; _toggleSel = 0; _needsRedraw = true; return; }  // right of BT = USB
    if (ki.arrowUp)    { _toggleSel = (_toggleSel - 1 + N) % N; _needsRedraw = true; return; }
    if (ki.arrowDown)  { _toggleSel = (_toggleSel + 1) % N;      _needsRedraw = true; return; }

    if (ki.enter) {
        switch (_toggleSel) {
            case 0:
                if (bluetoothEnabled) disableBluetooth(); else enableBluetooth();
                _rebootNote = true; break;
            case 1:
                bleKeyboardEnabled = !bleKeyboardEnabled;
                saveBtSettings(); break;
            case 2:
                bleMouseEnabled = !bleMouseEnabled;
                saveBtSettings(); break;
        }
        _needsRedraw = true; return;
    }

    if (ki.ch == 'c' || ki.ch == 'C') {
        if (bluetoothInitialized && BLE_KEYBOARD_VALID) {
            BLE_KEYBOARD.end(); delay(300); BLE_KEYBOARD.begin();
        } else if (!bluetoothEnabled) {
            enableBluetooth();
        }
        _needsRedraw = true; return;
    }
}

// ---- Page 1: USB HID ----

void AppSettings::_drawPage1() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(1);

    // 4 rows: USB enable, USB Keyboard, USB Mouse, FIDO2
    struct { const char* label; bool* flag; } rows[4] = {
        { "USB HID",        &usbEnabled         },
        { "  USB Keyboard", &usbKeyboardEnabled  },
        { "  USB Mouse",    &usbMouseEnabled     },
        { "  FIDO2/CTAP2",  &fido2Enabled        },
    };

    for (int i = 0; i < 4; i++) {
        int ry = CONTENT_Y + i * 20;
        _drawToggleRow(ry, i == _toggleSel, rows[i].label, *rows[i].flag, nullptr, 0);
    }

    if (_rebootNote) {
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(200,160,0), SETTINGS_BG);
        disp.drawString("Reboot to apply USB changes", 4, CONTENT_Y + 4*20 + 2);
    }

    _drawBottomBar("up/dn  ENTER toggle  </> page  ESC back");
}

void AppSettings::_handlePage1(KeyInput ki) {
    static constexpr int N = 4;
    if (ki.arrowLeft)  { _page = 1; _toggleSel = 0; _needsRedraw = true; return; }  // left of USB = BT
    if (ki.arrowRight) {
        _page = 3; _idSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; return;  // right of USB = API
    }
    // dead block (was WiFi nav) - remove
    if (false) { _page = 2; _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = "";
        _needsRedraw = true; return;
    }
    if (ki.arrowUp)   { _toggleSel = (_toggleSel - 1 + N) % N; _needsRedraw = true; return; }
    if (ki.arrowDown) { _toggleSel = (_toggleSel + 1) % N;      _needsRedraw = true; return; }

    if (ki.enter) {
        switch (_toggleSel) {
            case 0: usbEnabled         = !usbEnabled;         saveUSBSettings(); _rebootNote = true; break;
            case 1: usbKeyboardEnabled = !usbKeyboardEnabled; saveUSBSettings(); _rebootNote = true; break;
            case 2: usbMouseEnabled    = !usbMouseEnabled;    saveUSBSettings(); _rebootNote = true; break;
            case 3: fido2Enabled       = !fido2Enabled;       saveUSBSettings(); _rebootNote = true; break;
        }
        _needsRedraw = true; return;
    }
}

void AppSettings::_connectWifi() {
    WiFi.disconnect(true);
    delay(200);
    WiFi.setHostname(hostname);
    WiFi.mode(WIFI_STA);
    WiFi.begin(_newSSID.c_str(), _wifiInputBuf.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        feedWatchdog();
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiSSID     = _newSSID;
        wifiPassword = _wifiInputBuf;
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

// ---- Page 2: Set WiFi ----

void AppSettings::_drawPage2() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(2);

    int y = CONTENT_Y;

    // WiFi master toggle + connection status
    bool wconn = (WiFi.status() == WL_CONNECTED);
    const char* wconn_str = wconn ? "connected" : (wifiEnabled ? "disconnected" : "");
    uint16_t wconn_col = wconn ? disp.color565(80,220,80) : disp.color565(200,60,60);
    _drawToggleRow(y, true, "WiFi", wifiEnabled, wconn_str, wconn_col);
    y += 16;

    // Action hint
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(120,180,120), SETTINGS_BG);
    if (!wifiEnabled)    disp.drawString("ENTER enable+connect  C connect", 6, y);
    else if (wconn)      disp.drawString("ENTER disable  C disconnect", 6, y);
    else                 disp.drawString("ENTER disable  C reconnect", 6, y);
    y += 18;

    // SSID info
    disp.setTextSize(1);
    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("SSID:", 4, y);
    String maskedSSID = wifiSSID.isEmpty() ? "(none)" : wifiSSID;
    disp.setTextColor(TFT_YELLOW, SETTINGS_BG);
    disp.drawString(maskedSSID, 4 + disp.textWidth("SSID: "), y);
    y += 16;

    if (_wifiState == WS_SSID) {
        disp.setTextColor(labelColor(), SETTINGS_BG);
        disp.drawString("New SSID:", 4, y); y += 12;
        _drawInputField(4, y, disp.width() - 8, _wifiInputBuf, true);
        _drawBottomBar("ENTER toggle  C conn/disc  type=new SSID  </>");
    } else if (_wifiState == WS_PASS) {
        disp.setTextColor(disp.color565(100,200,100), SETTINGS_BG);
        disp.drawString("SSID: " + _newSSID, 4, y); y += 18;
        disp.setTextColor(labelColor(), SETTINGS_BG);
        disp.drawString("Password:", 4, y); y += 12;
        _drawInputField(4, y, disp.width() - 8, _wifiInputBuf, true, true);
        _drawBottomBar("type pass  ENTER connect  ESC back");
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

void AppSettings::_handlePage2(KeyInput ki) {
    if (_wifiState == WS_DONE) {
        if (ki.anyKey) { _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = ""; }
        _needsRedraw = true; return;
    }
    if (_wifiState == WS_CONNECTING) return;

    if (ki.arrowLeft && _wifiState == WS_SSID && _wifiInputBuf.length() == 0) {
        _page = NUM_PAGES-1; _idSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; return;  // left of WiFi = Identity
    }
    if (ki.arrowRight && _wifiState == WS_SSID && _wifiInputBuf.length() == 0) {
        _page = 1; _toggleSel = 0; _needsRedraw = true; return;  // right of WiFi = BT
    }

    // ENTER — toggle WiFi enabled (only when input buffer is empty)
    if (ki.enter && _wifiState == WS_SSID && _wifiInputBuf.length() == 0) {
        wifiEnabled = !wifiEnabled;
        saveWifiEnabledSettings();
        if (wifiEnabled) { WiFi.mode(WIFI_STA); WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str()); }
        else             { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); }
        _needsRedraw = true; return;
    }

    // C — connect/disconnect
    if ((ki.ch == 'c' || ki.ch == 'C') && _wifiState == WS_SSID) {
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.disconnect();
        } else {
            if (!wifiEnabled) { wifiEnabled = true; saveWifiEnabledSettings(); }
            WiFi.mode(WIFI_STA); WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        }
        _needsRedraw = true; return;
    }

    if (ki.enter) {
        if (_wifiState == WS_SSID) {
            if (_wifiInputBuf.length() > 0) { _newSSID = _wifiInputBuf; _wifiInputBuf = ""; _wifiState = WS_PASS; }
        } else if (_wifiState == WS_PASS) {
            _wifiState = WS_CONNECTING; _needsRedraw = true; _drawPage2(); _connectWifi();
        }
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

void AppSettings::_drawPage4() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(4);

    const char* labels[4] = { "USB Manufacturer", "USB/BT Product", "Hostname", "USB Serial" };
    const char* descs[4]  = { "USB mfr descriptor", "BT name + USB product", "mDNS name (reboot)", "USB serial (reboot)" };
    String* vals[4] = { &usbManufacturer, &usbProduct, &hostnameStr, &usbSerialNumber };
    int fieldW = disp.width() - 8;

    for (int i = 0; i < 4; i++) {
        int y    = CONTENT_Y + i * 26;
        bool sel  = (i == _idSel);
        bool edit = (sel && _editing);
        if (sel && !edit) disp.fillRect(0, y - 1, disp.width(), 14, selBgColor());
        disp.setTextSize(1);
        uint16_t rowBg = (sel && !edit) ? selBgColor() : (uint16_t)SETTINGS_BG;
        disp.setTextColor(sel ? TFT_WHITE : labelColor(), rowBg);
        char row[36]; snprintf(row, sizeof(row), "%s%s:", sel ? "> " : "  ", labels[i]);
        disp.drawString(row, 4, y);

        if (edit) {
            _drawInputField(4, y + 13, fieldW, _editBuf, true);
        } else {
            // value preview on right of label
            int lw = disp.textWidth(row) + 6;
            int avail = disp.width() - lw - 8;
            String preview = *vals[i];
            if (preview.isEmpty()) preview = descs[i];
            while (preview.length() > 0 && disp.textWidth(preview) > avail)
                preview = preview.substring(0, preview.length() - 1);
            disp.setTextColor(sel ? TFT_YELLOW : disp.color565(130, 180, 130), rowBg);
            disp.drawString(preview, lw, y);
        }
    }

    if (_idSaved) {
        disp.setTextColor(TFT_GREEN, SETTINGS_BG);
        disp.drawString("Saved! Reboot for USB/host.", 4, CONTENT_Y + 4 * 26 + 2);
    }

    if (_editing) _drawBottomBar("type  ENTER save  ESC cancel");
    else          _drawBottomBar("up/dn item  ENTER edit  </> page");
}

void AppSettings::_handlePage4(KeyInput ki) {
    static constexpr int N = 4;
    String* vals[N] = { &usbManufacturer, &usbProduct, &hostnameStr, &usbSerialNumber };
    if (_editing) {
        if (ki.esc) { _editing = false; _editBuf = ""; _needsRedraw = true; return; }
        if (ki.enter) {
            if (_editBuf.length() > 0) {
                *vals[_idSel] = _editBuf;
                if (_idSel <= 1) saveUSBIdentitySettings();
                else             saveHostnameSettings();
                if (_idSel == 2) hostname = hostnameStr.c_str();
                _idSaved = true;
            }
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.del && _editBuf.length() > 0) { _editBuf.remove(_editBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.ch) { _editBuf += ki.ch; _idSaved = false; _needsRedraw = true; }
        return;
    }
    if (ki.arrowLeft)  { _page = 3; _editing = false; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowRight) { _page = 5; _needsRedraw = true; }
    else if (ki.arrowUp)   { _idSel = (_idSel - 1 + N) % N; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowDown) { _idSel = (_idSel + 1) % N;      _idSaved = false; _needsRedraw = true; }
    else if (ki.enter) { _editBuf = *vals[_idSel]; _editing = true; _idSaved = false; _needsRedraw = true; }
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

    // App name list — must match registration order in main.cpp
    static const char* APP_NAMES[] = {
        "KProx", "FuzzyProx", "RegEdit", "CredStore",
        "Gadgets", "SinkProx", "Keyboard", "Clock",
        "QRProx", "SchedProx", "TOTProx", "Settings"
    };
    static constexpr int N_APPS = 12;

    int y = CONTENT_Y;
    disp.setTextSize(1);
    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("Default app on boot:", 4, y); y += 16;

    int visible = (disp.height() - y - BAR_BOT_H - 4) / 15;
    int scroll  = max(0, _idSel - visible + 1);
    if (_idSel - (defaultAppIndex - 1) < 0) scroll = 0;

    for (int i = 0; i < N_APPS && i < visible + scroll; i++) {
        if (i < scroll) continue;
        int appIdx = i + 1; // launcher is 0, user apps start at 1
        bool sel = (_idSel == i);
        bool cur = (defaultAppIndex == appIdx);
        uint16_t rowBg = sel ? selBgColor() : (uint16_t)SETTINGS_BG;
        if (sel) disp.fillRect(0, y - 1, disp.width(), 14, rowBg);

        disp.setTextColor(sel ? TFT_WHITE : labelColor(), rowBg);
        char row[36];
        snprintf(row, sizeof(row), "%s%s", sel ? "> " : "  ", APP_NAMES[i]);
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
    static constexpr int N = 12;
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

static const char* APP_LAYOUT_NAMES[] = {
    "KProx", "FuzzyProx", "RegEdit", "CredStore",
    "Gadgets", "SinkProx", "Keyboard", "Clock",
    "QRProx", "SchedProx", "TOTProx", "Settings"
};
static constexpr int N_LAYOUT_APPS = 12;

void AppSettings::_drawPage9() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(9);

    int y = CONTENT_Y;
    disp.setTextSize(1);
    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("App order & visibility:", 4, y); y += 14;

    // Ensure appOrder is populated
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
        bool isSettings = (appOrder[slot] == N_LAYOUT_APPS);  // Settings is always last registered
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
        const char* name = (appIdx >= 0 && appIdx < N_LAYOUT_APPS) ? APP_LAYOUT_NAMES[appIdx] : "?";
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

    bool onSd   = (csStorageLocation == "sd");
    bool sdOk   = sdAvailable();

    // Current location
    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("CS database location:", 4, y); y += 14;

    // NVS row
    bool nvsSelected = (_toggleSel == 0);
    uint16_t nvsBg = nvsSelected ? selBgColor() : (uint16_t)SETTINGS_BG;
    if (nvsSelected) disp.fillRect(0, y-1, disp.width(), 14, nvsBg);
    disp.setTextColor(nvsSelected ? TFT_WHITE : labelColor(), nvsBg);
    String nvsLabel = (!onSd ? "> " : "  ");
    nvsLabel += "NVS (built-in flash)";
    disp.drawString(nvsLabel, 4, y); y += 14;

    // SD row
    bool sdSelected = (_toggleSel == 1);
    uint16_t sdBg = sdSelected ? selBgColor() : (uint16_t)SETTINGS_BG;
    if (sdSelected) disp.fillRect(0, y-1, disp.width(), 14, sdBg);
    uint16_t sdColor = sdOk ? (sdSelected ? (uint16_t)TFT_WHITE : labelColor())
                             : disp.color565(100, 100, 100);
    disp.setTextColor(sdColor, sdBg);
    String sdLabel = (onSd ? "> " : "  ");
    sdLabel += "SD card";
    sdLabel += sdOk ? "" : " (not found)";
    disp.drawString(sdLabel, 4, y); y += 18;

    // Format SD row
    bool fmtSelected = (_toggleSel == 2);
    uint16_t fmtBg = fmtSelected ? disp.color565(80, 20, 20) : (uint16_t)SETTINGS_BG;
    if (fmtSelected) disp.fillRect(0, y-1, disp.width(), 14, fmtBg);
    uint16_t fmtColor = sdOk ? (fmtSelected ? (uint16_t)TFT_WHITE : disp.color565(220, 80, 80))
                              : disp.color565(80, 80, 80);
    disp.setTextColor(fmtColor, fmtBg);
    disp.drawString("  Format SD card", 4, y); y += 18;

    if (_idSaved) {
        disp.setTextColor(disp.color565(80, 220, 80), SETTINGS_BG);
        disp.drawString("Saved.", 4, y);
    }
    if (!_editBuf.isEmpty()) {
        disp.setTextColor(disp.color565(220, 80, 80), SETTINGS_BG);
        disp.drawString(_editBuf, 4, y);
    }

    _drawBottomBar("up/dn select  ENTER apply  </> page");
}

void AppSettings::_handlePage11(KeyInput ki) {
    if (ki.arrowLeft && !_editing) {
        _page = 10; _toggleSel = 0; _editing = false; _editBuf = ""; _idSaved = false; _needsRedraw = true; return;
    }
    if (ki.arrowRight && !_editing) {
        _page = 0; _toggleSel = 0; _editing = false; _editBuf = ""; _idSaved = false; _needsRedraw = true; return;
    }

    if (ki.arrowUp)   { _toggleSel = (_toggleSel - 1 + 3) % 3; _idSaved = false; _editBuf = ""; _needsRedraw = true; return; }
    if (ki.arrowDown) { _toggleSel = (_toggleSel + 1) % 3;     _idSaved = false; _editBuf = ""; _needsRedraw = true; return; }

    if (!ki.enter) return;
    _idSaved = false; _editBuf = "";

    if (_toggleSel == 0) {
        // Set NVS
        if (csStorageLocation != "nvs") {
            String oldLoc = csStorageLocation;
            csStorageLocation = "nvs";
            if (!credStoreLocked) {
                if (writeKDBX(credStoreRuntimeKey)) {
                    if (oldLoc == "sd") sdRemove();
                    saveCsStorageLocation();
                    _idSaved = true;
                } else {
                    csStorageLocation = oldLoc;
                    _editBuf = "Write failed";
                }
            } else {
                saveCsStorageLocation();
                _idSaved = true;
            }
        } else {
            _idSaved = true;
        }
    } else if (_toggleSel == 1) {
        // Set SD
        if (!sdAvailable()) { _editBuf = "SD not found"; _needsRedraw = true; return; }
        if (csStorageLocation != "sd") {
            String oldLoc = csStorageLocation;
            csStorageLocation = "sd";
            if (!credStoreLocked) {
                if (writeKDBX(credStoreRuntimeKey)) {
                    if (oldLoc == "nvs") { preferences.begin("kprox_db", false); preferences.clear(); preferences.end(); }
                    saveCsStorageLocation();
                    _idSaved = true;
                } else {
                    csStorageLocation = oldLoc;
                    _editBuf = "Write failed";
                }
            } else {
                saveCsStorageLocation();
                _idSaved = true;
            }
        } else {
            _idSaved = true;
        }
    } else if (_toggleSel == 2) {
        // Format SD
        if (!sdAvailable()) { _editBuf = "SD not found"; _needsRedraw = true; return; }
        if (sdFormat()) {
            if (csStorageLocation == "sd") credStoreLock();
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
            case 0: _drawPage2(); break;  // WiFi Settings
            case 1: _drawPage0(); break;  // Bluetooth
            case 2: _drawPage1(); break;  // USB HID
            case 3: _drawPage3(); break;  // API Key
            case 4: _drawPage4(); break;  // Device Identity
            case 5: _drawPage5(); break;  // Sink Config
            case 6: _drawPage6(); break;  // HID Timing 1/2
            case 7: _drawPage7(); break;  // HID Timing 2/2
            case 8: _drawPage8(); break;  // Startup App
            case 9: _drawPage9(); break;  // App Layout
            case 10: _drawPage10(); break; // CS Security
            case 11: _drawPage11(); break; // SD Storage
        }
        _needsRedraw = false;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (ki.esc) {
        if (_editing) { _editing = false; _editBuf = ""; _needsRedraw = true; }
        else if (_page == 0) { uiManager.returnToLauncher(); }
        else { _page--; _toggleSel = 0; _needsRedraw = true; }
        return;
    }

    switch (_page) {
        case 0: _handlePage2(ki); break;  // WiFi Settings
        case 1: _handlePage0(ki); break;  // Bluetooth
        case 2: _handlePage1(ki); break;  // USB HID
        case 3: _handlePage3(ki); break;  // API Key
        case 4: _handlePage4(ki); break;  // Device Identity
        case 5: _handlePage5(ki); break;  // Sink Config
        case 6: _handlePage6(ki); break;  // HID Timing 1/2
        case 7: _handlePage7(ki); break;  // HID Timing 2/2
        case 8: _handlePage8(ki); break;  // Startup App
        case 9: _handlePage9(ki); break;  // App Layout
        case 10: _handlePage10(ki); break; // CS Security
        case 11: _handlePage11(ki); break; // SD Storage
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
