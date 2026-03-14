#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_fuzzyprox.h"
#include "../registers.h"
#include "../credential_store.h"
#include "../hid.h"
#include "ui_manager.h"
#include <algorithm>

namespace Cardputer {

static constexpr uint16_t FZ_BG      = 0x080F;
static constexpr int      FZ_BAR_H   = 18;
static constexpr int      FZ_BOT_H   = 14;
static constexpr int      FZ_ROW_H   = 14;

// ---- Fuzzy scoring ----
// Returns score >= 0 if all needle chars appear in order in hay (case-insensitive).
// Consecutive matches score higher. Returns -1 on no match.

int AppFuzzyProx::_fuzzyScore(const String& hay, const String& needle) const {
    if (needle.isEmpty()) return 0;

    String h = hay;    h.toLowerCase();
    String n = needle; n.toLowerCase();

    int hi = 0, ni = 0, score = 0, consec = 0;
    while (hi < (int)h.length() && ni < (int)n.length()) {
        if (h[hi] == n[ni]) {
            score += 1 + consec * 2;
            consec++;
            ni++;
        } else {
            consec = 0;
        }
        hi++;
    }
    return (ni == (int)n.length()) ? score : -1;
}

void AppFuzzyProx::_rebuildMatches() {
    struct Sc { int idx; int score; };
    std::vector<Sc> scored;

    for (int i = 0; i < (int)registers.size(); i++) {
        String hay = "";
        if (i < (int)registerNames.size() && !registerNames[i].isEmpty())
            hay = registerNames[i] + " ";
        hay += registers[i];

        int s = _fuzzyScore(hay, _query);
        if (_query.isEmpty() || s >= 0)
            scored.push_back({i, _query.isEmpty() ? i : s});
    }

    if (!_query.isEmpty()) {
        std::sort(scored.begin(), scored.end(),
                  [](const Sc& a, const Sc& b){ return a.score > b.score; });
    }

    _matches.clear();
    for (auto& sc : scored) _matches.push_back(sc.idx);

    if (_matches.empty()) _sel = 0;
    else _sel = constrain(_sel, 0, (int)_matches.size() - 1);
}

// ---- Drawing ----

void AppFuzzyProx::_draw() {
    auto& disp = M5Cardputer.Display;
    const int W = disp.width();
    const int H = disp.height();

    disp.fillScreen(FZ_BG);

    // ---- Top bar ----
    uint16_t barBg = disp.color565(100, 20, 100);
    disp.fillRect(0, 0, W, FZ_BAR_H, barBg);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, barBg);
    disp.drawString("FuzzyProx", 4, 3);

    // CS badge
    {
        bool locked = credStoreLocked;
        const char* cs = locked ? "CS:LOCKED" : "CS:UNLOCKED";
        uint16_t csBg = locked ? disp.color565(140,30,30) : disp.color565(20,100,20);
        int csW = disp.textWidth(cs) + 8;
        disp.fillRoundRect(W - csW - 2, 2, csW, 14, 3, csBg);
        disp.setTextColor(TFT_WHITE, csBg);
        disp.drawString(cs, W - csW + 2, 4);
    }

    // ---- Query input field ----
    int qy = FZ_BAR_H + 2;
    uint16_t qBg = disp.color565(40, 10, 40);
    disp.fillRect(0, qy, W, 16, qBg);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(220, 180, 255), qBg);
    disp.drawString("> ", 2, qy + 3);
    int qx = 2 + disp.textWidth("> ");
    // Truncate query to fit
    int maxQChars = (W - qx - 8) / 6;
    String qDisp = _query;
    if ((int)qDisp.length() > maxQChars)
        qDisp = qDisp.substring(qDisp.length() - maxQChars);
    disp.drawString(qDisp + "_", qx, qy + 3);

    // Match count
    char countBuf[12];
    snprintf(countBuf, sizeof(countBuf), "[%d]", (int)_matches.size());
    int cw = disp.textWidth(countBuf);
    disp.setTextColor(disp.color565(150, 120, 150), qBg);
    disp.drawString(countBuf, W - cw - 2, qy + 3);

    // ---- Results list ----
    int listY    = qy + 18;
    int listH    = H - FZ_BOT_H - listY;
    int maxRows  = listH / FZ_ROW_H;

    if (_matches.empty()) {
        disp.setTextColor(disp.color565(100, 80, 100), FZ_BG);
        disp.drawString(_query.isEmpty() ? "No registers" : "No matches", 6, listY + 4);
    } else {
        // Scroll window: keep _sel visible
        int scrollStart = 0;
        if (_sel >= maxRows) scrollStart = _sel - maxRows + 1;

        for (int vi = 0; vi < maxRows && vi + scrollStart < (int)_matches.size(); vi++) {
            int ri  = _matches[vi + scrollStart];
            bool sel = (vi + scrollStart == _sel);
            int ry  = listY + vi * FZ_ROW_H;

            uint16_t rowBg = sel ? disp.color565(80, 0, 80) : FZ_BG;
            if (sel) disp.fillRect(0, ry, W, FZ_ROW_H, rowBg);

            // Index badge
            char idx[6];
            snprintf(idx, sizeof(idx), "%3d", ri);
            disp.setTextColor(sel ? disp.color565(220,180,255) : disp.color565(120,80,120), rowBg);
            disp.drawString(idx, 2, ry + 2);

            // Active register indicator
            if (ri == activeRegister) {
                disp.setTextColor(disp.color565(80,220,80), rowBg);
                disp.drawString("*", 24, ry + 2);
            }

            // Name or content preview
            String name = (ri < (int)registerNames.size()) ? registerNames[ri] : "";
            String content = (ri < (int)registers.size()) ? registers[ri] : "";
            String label = name.isEmpty() ? content : name;
            int labelMaxChars = (W - 34) / 6;
            if ((int)label.length() > labelMaxChars)
                label = label.substring(0, labelMaxChars - 2) + "..";
            disp.setTextColor(sel ? TFT_WHITE : disp.color565(180,160,180), rowBg);
            disp.drawString(label, 32, ry + 2);
        }
    }

    // ---- Status / feedback ----
    if (_justConfirmed) {
        int sy = H - FZ_BOT_H - 14;
        disp.fillRect(0, sy, W, 13, FZ_BG);
        disp.setTextColor(TFT_GREEN, FZ_BG);
        String msg = "Active: " + String(activeRegister);
        if (activeRegister < (int)registerNames.size() && !registerNames[activeRegister].isEmpty())
            msg += "  " + registerNames[activeRegister];
        disp.drawString(msg, 4, sy);
    }

    // ---- Bottom bar ----
    uint16_t botBg = disp.color565(16,16,16);
    disp.fillRect(0, H - FZ_BOT_H, W, FZ_BOT_H, botBg);
    disp.setTextColor(disp.color565(110,80,110), botBg);
    disp.drawString("type filter  up/dn sel  ENTER set active  BtnA play", 2, H - FZ_BOT_H + 2);

    _needsRedraw = false;
}

// ---- BtnA: single press = play active, double press = cycle register ----

void AppFuzzyProx::_checkBtnA() {
    unsigned long now = millis();
    auto& btn = M5Cardputer.BtnA;

    if (btn.wasPressed()) {
        _lastBtnPress = now;
        _haltTriggered = false;
        uiManager.notifyInteraction();
    }

    if (g_btnAHaltedPlayback) {
        _skipRelease = true;
        g_btnAHaltedPlayback = false;
    }

    if (btn.wasReleased()) {
        if (_skipRelease) { _skipRelease = false; }
        else { _lastBtnRelease = now; if (!_haltTriggered) _btnCount++; }
    }

    if (btn.isPressed() && !_haltTriggered && (now - _lastBtnPress >= 2000)) {
        _haltTriggered = true;
        _btnCount = 0;
        isHalted ? resumeOperations() : haltAllOperations();
        _needsRedraw = true;
    }

    if (_btnCount > 0 && (now - _lastBtnRelease > DBL_MS)) {
        if (_btnCount == 1) {
            if (!registers.empty() && !registers[activeRegister].isEmpty())
                pendingTokenStrings.push_back(registers[activeRegister]);
        }
        _btnCount = 0;
        _needsRedraw = true;
    }
}

// ---- Lifecycle ----

void AppFuzzyProx::onEnter() {
    _query = "";
    _sel   = 0;
    _justConfirmed = false;
    _snapLocked    = credStoreLocked;
    _rebuildMatches();
    _needsRedraw = true;
}

void AppFuzzyProx::onExit() {
    _query = "";
}

void AppFuzzyProx::onUpdate() {
    _checkBtnA();

    // Redraw if CS lock state changed
    bool nowLocked = credStoreLocked;
    if (nowLocked != _snapLocked) {
        _snapLocked  = nowLocked;
        _needsRedraw = true;
    }

    if (_needsRedraw) _draw();

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (ki.esc) { uiManager.returnToLauncher(); return; }

    // Up/down navigate matches
    if (ki.arrowUp) {
        if (!_matches.empty()) _sel = constrain(_sel - 1, 0, (int)_matches.size() - 1);
        _justConfirmed = false;
        _needsRedraw   = true;
        return;
    }
    if (ki.arrowDown) {
        if (!_matches.empty()) _sel = constrain(_sel + 1, 0, (int)_matches.size() - 1);
        _justConfirmed = false;
        _needsRedraw   = true;
        return;
    }

    // ENTER: if selection matches active register, play it; otherwise set it active
    if (ki.enter) {
        if (!_matches.empty()) {
            int target = _matches[_sel];
            if (target == activeRegister) {
                // Already active — play it
                if (!registers[target].isEmpty())
                    pendingTokenStrings.push_back(registers[target]);
            } else {
                activeRegister = target;
                saveActiveRegister();
                _justConfirmed = true;
            }
            _needsRedraw = true;
        }
        return;
    }

    // DEL — backspace query
    if (ki.del) {
        if (_query.length() > 0) {
            _query.remove(_query.length() - 1);
            _sel = 0;
            _justConfirmed = false;
            _rebuildMatches();
            _needsRedraw = true;
        }
        return;
    }

    // Any printable character — append to query
    if (ki.ch) {
        _query += ki.ch;
        _sel = 0;
        _justConfirmed = false;
        _rebuildMatches();
        _needsRedraw = true;
    }
}

} // namespace Cardputer
#endif
