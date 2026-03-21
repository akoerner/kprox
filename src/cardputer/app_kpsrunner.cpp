#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_kpsrunner.h"
#include "ui_manager.h"
#include "../credential_store.h"
#include "../kps_parser.h"
#include "../sd_utils.h"
#include <SD.h>
#include <algorithm>

namespace Cardputer {

static constexpr uint16_t KR_BG = 0x200F;

// ---- Script scanner ----

void AppKPSRunner::_scanRecursive(const String& path, int depth) {
    if (depth > 4) return;
    if (!sdMount()) return;

    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) return;

    File f = dir.openNextFile();
    while (f) {
        String name = String(f.name());
        int sl = name.lastIndexOf('/');
        if (sl >= 0) name = name.substring(sl + 1);

        if (f.isDirectory()) {
            String subPath = (path == "/") ? ("/" + name) : (path + "/" + name);
            _scanRecursive(subPath, depth + 1);
        } else {
            String lower = name;
            lower.toLowerCase();
            if (lower.endsWith(".kps")) {
                String full = (path == "/") ? ("/" + name) : (path + "/" + name);
                _scripts.push_back(full);
            }
        }
        f.close();
        f = dir.openNextFile();
    }
    dir.close();
}

void AppKPSRunner::_scan(const String& path) {
    _scripts.clear();
    _sel       = 0;
    _scrollTop = 0;

    if (!sdAvailable()) { _state = ST_NO_SD; return; }

    _scanPath = path;
    _scanRecursive(path, 0);
    std::sort(_scripts.begin(), _scripts.end());
    _state = ST_LIST;
}

String AppKPSRunner::_displayName(const String& path) const {
    // Show path relative to /scripts/ if possible, otherwise just basename
    String p = path;
    if (p.startsWith("/scripts/")) p = p.substring(9);
    return p;
}

// ---- Drawing ----

void AppKPSRunner::_drawTopBar() {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = disp.color565(100, 0, 100);
    disp.fillRect(0, 0, disp.width(), KR_BAR_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, bc);
    disp.drawString("KPScript", 4, 3);

    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d scripts", (int)_scripts.size());
    int cw = disp.textWidth(cnt);
    disp.setTextColor(disp.color565(220, 180, 255), bc);
    disp.drawString(cnt, disp.width() - cw - 4, 3);
}

void AppKPSRunner::_drawBottomBar(const char* hint) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = disp.color565(16, 16, 16);
    disp.fillRect(0, disp.height() - KR_BOT_H, disp.width(), KR_BOT_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(110, 110, 110), bc);
    disp.drawString(hint, 2, disp.height() - KR_BOT_H + 2);
}

void AppKPSRunner::_drawList() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(KR_BG);
    _drawTopBar();

    int y    = KR_BAR_H + 2;
    int rowH = (disp.height() - KR_BAR_H - KR_BOT_H - 2) / KR_ROWS;

    if (_state == ST_NO_SD || !sdAvailable()) {
        disp.setTextColor(disp.color565(200, 80, 80), KR_BG);
        disp.drawString("SD card not available", 4, y + 10);
        _drawBottomBar("ESC back");
        return;
    }

    if (_scripts.empty()) {
        disp.setTextColor(disp.color565(130, 130, 130), KR_BG);
        disp.drawString("No .kps scripts found", 4, y + 6);
        disp.drawString("Place .kps files on SD", 4, y + 20);
        disp.drawString("Scanned: " + _scanPath, 4, y + 34);
        _drawBottomBar("R=rescan  ESC back");
        return;
    }

    for (int i = 0; i < KR_ROWS && (_scrollTop + i) < (int)_scripts.size(); i++) {
        int idx = _scrollTop + i;
        bool sel = (idx == _sel);
        uint16_t bg = sel ? disp.color565(100, 0, 100) : (uint16_t)KR_BG;
        disp.fillRect(0, y, disp.width(), rowH, bg);

        String name = _displayName(_scripts[idx]);
        if ((int)name.length() > 28) name = "..." + name.substring(name.length() - 25);

        disp.setTextSize(1);
        disp.setTextColor(sel ? TFT_WHITE : disp.color565(200, 160, 255), bg);
        disp.drawString(name, 4, y + 2);
        y += rowH;
    }

    if (!_statusMsg.isEmpty()) {
        y = disp.height() - KR_BOT_H - 14;
        disp.setTextColor(_statusOk ? TFT_GREEN : disp.color565(220, 80, 80), KR_BG);
        disp.drawString(_statusMsg, 4, y);
    }

    _drawBottomBar("ENTER run  R rescan  ESC back");
}

void AppKPSRunner::_drawConfirmExec() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(KR_BG);
    _drawTopBar();
    int y = KR_BAR_H + 8;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(180, 255, 180), KR_BG);
    disp.drawString("Run script?", 4, y); y += 14;
    disp.setTextColor(TFT_WHITE, KR_BG);
    if (_sel < (int)_scripts.size()) {
        String dn = _displayName(_scripts[_sel]);
        if ((int)dn.length() > 28) dn = "..." + dn.substring(dn.length() - 25);
        disp.drawString(dn, 4, y);
    }
    _drawBottomBar("Y confirm   N/ESC cancel");
}

// ---- Input handlers ----

void AppKPSRunner::_handleList(KeyInput ki) {
    if (ki.esc) { uiManager.returnToLauncher(); return; }

    int n = (int)_scripts.size();

    if (ki.ch == 'r' || ki.ch == 'R') {
        _scan("/");
        _needsRedraw = true;
        return;
    }

    if (n == 0) return;

    if (ki.arrowUp || ki.arrowLeft) {
        _sel = (_sel - 1 + n) % n;
        if (_sel < _scrollTop) _scrollTop = _sel;
        _needsRedraw = true; return;
    }
    if (ki.arrowDown || ki.arrowRight) {
        _sel = (_sel + 1) % n;
        if (_sel >= _scrollTop + KR_ROWS) _scrollTop = _sel - KR_ROWS + 1;
        _needsRedraw = true; return;
    }
    if (ki.enter && n > 0) {
        _state = ST_CONFIRM_EXEC;
        _needsRedraw = true;
        return;
    }
}

void AppKPSRunner::_handleConfirmExec(KeyInput ki) {
    if (ki.esc || ki.ch == 'n' || ki.ch == 'N') {
        _state = ST_LIST; _needsRedraw = true; return;
    }
    if ((ki.ch == 'y' || ki.ch == 'Y') && _sel < (int)_scripts.size()) {
        _statusMsg = "Running..."; _statusOk = true;
        _state = ST_LIST; _needsRedraw = true;
        kpsExecFile(_scripts[_sel]);
        _statusMsg = "Done"; _statusOk = true;
        _needsRedraw = true;
    }
}

// ---- Lifecycle ----

void AppKPSRunner::onEnter() {
    _scan("/");
    _needsRedraw = true;
}

void AppKPSRunner::onUpdate() {
    if (_needsRedraw) {
        switch (_state) {
            case ST_LIST:         _drawList();        break;
            case ST_CONFIRM_EXEC: _drawConfirmExec(); break;
            case ST_NO_SD:        _drawList();        break;
            default:              _drawList();        break;
        }
        _needsRedraw = false;
    }

    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        uiManager.returnToLauncher();
        return;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    switch (_state) {
        case ST_LIST:
        case ST_NO_SD:        _handleList(ki);        break;
        case ST_CONFIRM_EXEC: _handleConfirmExec(ki); break;
        default: break;
    }
}

} // namespace Cardputer
#endif
