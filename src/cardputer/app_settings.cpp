#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_settings.h"
#include "../storage.h"
#include "../connection.h"
#include <WiFi.h>

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
        "WiFi Settings", "Bluetooth", "USB HID", "API Key", "Device Identity", "Sink Config"
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

    static const char* labels[2] = { "USB Manufacturer", "USB/BT Product Name" };
    String* vals[2] = { &usbManufacturer, &usbProduct };
    int fieldW = disp.width() - 8;

    for (int i = 0; i < 2; i++) {
        int y    = CONTENT_Y + i * 36;
        bool sel  = (i == _idSel);
        bool edit = (sel && _editing);
        if (sel && !edit) disp.fillRect(0, y-1, disp.width(), 12, selBgColor());
        disp.setTextSize(1);
        disp.setTextColor(sel ? TFT_WHITE : labelColor(), sel && !edit ? selBgColor() : (uint16_t)SETTINGS_BG);
        char row[36]; snprintf(row, sizeof(row), "%s%s:", sel ? "> " : "  ", labels[i]);
        disp.drawString(row, 4, y);
        int fieldY = y + 13;
        if (edit) _drawInputField(4, fieldY, fieldW, _editBuf, true);
        else      _drawInputField(4, fieldY, fieldW, *vals[i], false);
    }

    int noteY = CONTENT_Y + 2*36 + 2;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(160,130,40), SETTINGS_BG);
    disp.drawString("* BT name = USB Product; reboot", 4, noteY);
    if (_idSaved) { disp.setTextColor(TFT_GREEN, SETTINGS_BG); disp.drawString("Saved! Reboot.", 4, noteY+12); }

    if (_editing) _drawBottomBar("type  ENTER save  ESC cancel");
    else          _drawBottomBar("up/dn item  </> page  ENT edit  ESC back");
}

void AppSettings::_handlePage4(KeyInput ki) {
    if (_editing) {
        if (ki.esc) { _editing = false; _editBuf = ""; _needsRedraw = true; return; }
        if (ki.enter) {
            if (_editBuf.length() > 0) {
                if (_idSel == 0) usbManufacturer = _editBuf; else usbProduct = _editBuf;
                saveUSBIdentitySettings(); _idSaved = true;
            }
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.del && _editBuf.length()>0) { _editBuf.remove(_editBuf.length()-1); _needsRedraw = true; return; }
        if (ki.ch) { _editBuf += ki.ch; _idSaved = false; _needsRedraw = true; }
        return;
    }
    if (ki.arrowLeft)  { _page = 3; _editing = false; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowRight) { _page = 5; _needsRedraw = true; }  // right of Identity = Sink Config
    else if (ki.arrowUp)   { _idSel = (_idSel-1+2)%2; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowDown) { _idSel = (_idSel+1)%2;    _idSaved = false; _needsRedraw = true; }
    else if (ki.enter) {
        String* vals[2] = { &usbManufacturer, &usbProduct };
        _editBuf = *vals[_idSel]; _editing = true; _idSaved = false; _needsRedraw = true;
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
    else if (ki.arrowRight) { _page = 0; _needsRedraw = true; }  // wrap back to WiFi
    else if (ki.enter) { _editBuf = String(maxSinkSize); _editing = true; _idSaved = false; _needsRedraw = true; }
}

// ---- AppBase overrides ----

void AppSettings::onEnter() {
    _page = 0; _toggleSel = 0; _rebootNote = false;
    _idSel = 0; _editing = false; _idSaved = false; _editBuf = "";
    _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = "";
    _needsRedraw = true;
}

void AppSettings::onExit() {
    _editing = false; _editBuf = "";
}

void AppSettings::onUpdate() {
    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        _page = (_page + 1) % NUM_PAGES;
        _editing = false; _editBuf = ""; _idSaved = false; _toggleSel = 0;
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
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
