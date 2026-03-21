#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_bootprox.h"
#include "ui_manager.h"
#include "../storage.h"
#include "../registers.h"

namespace Cardputer {

static constexpr uint16_t BP_BG = 0x0007;

void AppBootProx::_drawTopBar() {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = disp.color565(0, 60, 120);
    disp.fillRect(0, 0, disp.width(), BP_BAR_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, bc);
    disp.drawString("BootProx", 4, 3);
    const char* st = bootRegEnabled ? "ENABLED" : "OFF";
    uint16_t sc = bootRegEnabled ? disp.color565(80, 255, 80) : disp.color565(160, 160, 160);
    int sw = disp.textWidth(st);
    disp.setTextColor(sc, bc);
    disp.drawString(st, disp.width() - sw - 4, 3);
}

void AppBootProx::_drawBottomBar(const char* hint) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = disp.color565(16, 16, 16);
    disp.fillRect(0, disp.height() - BP_BOT_H, disp.width(), BP_BOT_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(110, 110, 110), bc);
    disp.drawString(hint, 2, disp.height() - BP_BOT_H + 2);
}

void AppBootProx::_draw() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(BP_BG);
    _drawTopBar();

    int y = BP_BAR_H + 4;
    int fw = disp.width() - 8;

    auto row = [&](Field f, const char* label, const String& value, bool editing) {
        bool sel = (_sel == f);
        uint16_t bg = sel ? (editing ? disp.color565(0, 60, 0) : disp.color565(0, 40, 80)) : (uint16_t)BP_BG;
        disp.fillRect(0, y - 1, disp.width(), 14, bg);
        disp.setTextSize(1);
        disp.setTextColor(sel ? TFT_WHITE : disp.color565(160, 160, 160), bg);
        disp.drawString(label, 4, y);
        disp.setTextColor(sel ? TFT_YELLOW : disp.color565(220, 220, 100), bg);
        String display = editing ? (value + "_") : value;
        int vw = disp.textWidth(display);
        disp.drawString(display, disp.width() - vw - 4, y);
        y += 15;
    };

    // Enabled toggle
    row(F_ENABLED,  "Auto-run on boot:", bootRegEnabled ? "Yes" : "No", false);

    // Register selector
    String regLabel = "---";
    if (!registers.empty()) {
        String name = (bootRegIndex < (int)registerNames.size()) ? registerNames[bootRegIndex] : "";
        if (name.isEmpty()) name = "Reg " + String(bootRegIndex + 1);
        if ((int)name.length() > 14) name = name.substring(0, 12) + "..";
        regLabel = "[" + String(bootRegIndex + 1) + "] " + name;
    }
    row(F_REGISTER, "Register:", regLabel, _editing && _sel == F_REGISTER);

    // Boot limit
    String limitLabel = (bootRegLimit == 0) ? "Every boot" : ("x" + String(bootRegLimit));
    row(F_LIMIT,    "Fire limit:", limitLabel, _editing && _sel == F_LIMIT);

    // Fired counter
    row(F_RESET,    "Times fired:", String(bootRegFiredCount), false);

    y += 2;

    // Preview of selected register content
    if (!registers.empty() && bootRegIndex < (int)registers.size()) {
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(80, 80, 80), BP_BG);
        String preview = registers[bootRegIndex];
        if ((int)preview.length() > 34) preview = preview.substring(0, 31) + "...";
        disp.drawString(preview, 4, y);
        y += 12;
    }

    // Status message
    if (!_statusMsg.isEmpty()) {
        disp.setTextColor(_statusOk ? TFT_GREEN : disp.color565(220, 80, 80), BP_BG);
        disp.drawString(_statusMsg, 4, y);
    }

    _drawBottomBar(_editing ? "type  ENTER confirm  ESC cancel"
                            : "up/dn  ENTER edit/toggle  R=reset  ESC back");
}

void AppBootProx::_save() {
    saveBootRegSettings();
    _statusMsg = "Saved"; _statusOk = true;
}

void AppBootProx::_handle(KeyInput ki) {
    if (ki.esc) {
        if (_editing) {
            _editing = false; _editBuf = ""; _statusMsg = ""; _needsRedraw = true;
        } else {
            uiManager.returnToLauncher();
        }
        return;
    }

    if (!_editing) {
        int n = F_COUNT;
        if (ki.arrowUp)   { _sel = (Field)(((int)_sel - 1 + n) % n); _statusMsg = ""; _needsRedraw = true; return; }
        if (ki.arrowDown) { _sel = (Field)(((int)_sel + 1) % n);     _statusMsg = ""; _needsRedraw = true; return; }

        if (ki.ch == 'r' || ki.ch == 'R') {
            bootRegFiredCount = 0;
            if (bootRegLimit > 0) bootRegEnabled = true;
            _save(); _needsRedraw = true; return;
        }

        if (ki.enter) {
            switch (_sel) {
                case F_ENABLED:
                    bootRegEnabled = !bootRegEnabled;
                    _save(); _needsRedraw = true;
                    break;
                case F_REGISTER:
                    _editBuf = String(bootRegIndex + 1);
                    _editing = true; _statusMsg = ""; _needsRedraw = true;
                    break;
                case F_LIMIT:
                    _editBuf = String(bootRegLimit);
                    _editing = true; _statusMsg = ""; _needsRedraw = true;
                    break;
                case F_RESET:
                    bootRegFiredCount = 0;
                    if (bootRegLimit > 0) bootRegEnabled = true;
                    _save(); _needsRedraw = true;
                    break;
                default: break;
            }
        }

        // Arrow left/right to adjust register or limit directly
        if ((ki.arrowLeft || ki.arrowRight) && !registers.empty()) {
            int delta = ki.arrowRight ? 1 : -1;
            if (_sel == F_REGISTER) {
                int n2 = (int)registers.size();
                bootRegIndex = (bootRegIndex + delta + n2) % n2;
                _save(); _needsRedraw = true;
            } else if (_sel == F_LIMIT) {
                bootRegLimit = max(0, bootRegLimit + delta);
                _save(); _needsRedraw = true;
            }
        }
        return;
    }

    // Editing mode
    if (ki.del && _editBuf.length() > 0) { _editBuf.remove(_editBuf.length() - 1); _needsRedraw = true; return; }
    if (ki.ch >= '0' && ki.ch <= '9' && (int)_editBuf.length() < 5) { _editBuf += ki.ch; _needsRedraw = true; return; }

    if (ki.enter) {
        int val = _editBuf.toInt();
        if (_sel == F_REGISTER) {
            if (!registers.empty() && val >= 1 && val <= (int)registers.size()) {
                bootRegIndex = val - 1;
                _save();
            } else {
                _statusMsg = "Invalid register"; _statusOk = false;
            }
        } else if (_sel == F_LIMIT) {
            bootRegLimit = max(0, val);
            _save();
        }
        _editing = false; _editBuf = ""; _needsRedraw = true;
    }
}

void AppBootProx::onEnter() {
    _sel = F_ENABLED; _editing = false; _editBuf = ""; _statusMsg = "";
    _needsRedraw = true;
}

void AppBootProx::onUpdate() {
    if (_needsRedraw) {
        _draw();
        _needsRedraw = false;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();
    _handle(ki);
}

} // namespace Cardputer
#endif
