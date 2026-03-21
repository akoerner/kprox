#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_totprox.h"
#include "../hid.h"
#include "../credential_store.h"
#include <time.h>
#include <inttypes.h>

namespace Cardputer {

static constexpr uint16_t TP_BG    = 0x180E;
static constexpr int      TP_BAR_H = 16;
static constexpr int      TP_BOT_H = 14;
static constexpr int      TP_Y     = TP_BAR_H + 2;

void AppTOTProx::_drawTopBar(const char* subtitle) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = disp.color565(160, 30, 30);
    disp.fillRect(0, 0, disp.width(), TP_BAR_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, bc);
    disp.drawString("TOTProx", 4, 3);

    if (credStoreLocked) {
        uint16_t lb = disp.color565(200, 60, 0);
        const char* lockStr = "LOCKED";
        int lw = disp.textWidth(lockStr) + 8;
        int lx = disp.width() - lw - 2;
        disp.fillRoundRect(lx, 2, lw, TP_BAR_H - 4, 2, lb);
        disp.setTextColor(TFT_WHITE, lb);
        disp.drawString(lockStr, lx + 4, 4);
    } else if (subtitle && *subtitle) {
        int sw = disp.textWidth(subtitle);
        disp.setTextColor(disp.color565(255, 200, 200), bc);
        disp.drawString(subtitle, disp.width() - sw - 4, 3);
    }
}

void AppTOTProx::_drawBottomBar(const char* hint) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = disp.color565(16, 16, 16);
    disp.fillRect(0, disp.height() - TP_BOT_H, disp.width(), TP_BOT_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(110, 110, 110), bc);
    disp.drawString(hint, 2, disp.height() - TP_BOT_H + 2);
}

void AppTOTProx::_reloadAccounts() {
    _accounts = totpListAccounts();
    if (_sel >= (int)_accounts.size()) _sel = max(0, (int)_accounts.size() - 1);
}

// ---- List view ----

void AppTOTProx::_drawList() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TP_BG);

    char subtitle[12];
    snprintf(subtitle, sizeof(subtitle), "%d accounts", (int)_accounts.size());
    _drawTopBar(subtitle);

    int y = TP_Y + 2;

    if (_accounts.empty()) {
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(130, 130, 130), TP_BG);
        if (credStoreLocked) {
            disp.drawString("Unlock CredStore first.", 4, y + 12);
            disp.drawString("TOTP secrets are encrypted", 4, y + 26);
            disp.drawString("with the credential store.", 4, y + 40);
        } else {
            disp.drawString("No accounts.", 4, y + 12);
            disp.drawString("Press N to add one.", 4, y + 28);
        }
        _drawBottomBar("N=add  G=gate settings  ESC back");
        return;
    }

    // Current account
    const TOTPAccount& acct = _accounts[_sel];

    // Time sync status
    bool timeOk = totpTimeReady();
    time_t now  = timeOk ? time(nullptr) : 0;

    // Compute code
    int32_t code  = timeOk ? (int32_t)totpCompute(acct.secret, now, acct.period, acct.digits) : -1;
    int     secsLeft = timeOk ? totpSecondsRemaining(now, acct.period) : 0;

    // Account name
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(255, 180, 180), TP_BG);
    String nameDisp = acct.name;
    if ((int)nameDisp.length() > 28) nameDisp = nameDisp.substring(0, 25) + "...";
    disp.drawString(nameDisp, 4, y); y += 14;

    // Large 6-digit code
    if (!credStoreLocked && timeOk && code >= 0) {
        char codeBuf[10];
        snprintf(codeBuf, sizeof(codeBuf), "%06" PRId32, code);

        char spaced[12];
        snprintf(spaced, sizeof(spaced), "%c%c%c %c%c%c",
                 codeBuf[0], codeBuf[1], codeBuf[2],
                 codeBuf[3], codeBuf[4], codeBuf[5]);

        disp.setTextSize(3);
        int tw = disp.textWidth(spaced);
        uint16_t codeColor = (secsLeft <= 5)
            ? disp.color565(220, 80, 80)
            : disp.color565(100, 255, 100);
        disp.setTextColor(codeColor, TP_BG);
        disp.drawString(spaced, (disp.width() - tw) / 2, y); y += 32;
    } else {
        disp.setTextSize(1);
        const char* msg = credStoreLocked ? "Unlock CredStore to view"
                        : (timeOk        ? "Invalid secret"
                                         : "No NTP sync");
        uint16_t msgColor = credStoreLocked
            ? disp.color565(220, 100, 0)
            : disp.color565(220, 160, 0);
        disp.setTextColor(msgColor, TP_BG);
        disp.drawString(msg, 4, y + 8); y += 28;
    }

    // Countdown bar
    if (!credStoreLocked && timeOk && secsLeft > 0) {
        int barW = disp.width() - 8;
        int fillW = (barW * secsLeft) / acct.period;
        uint16_t barColor = (secsLeft <= 5)
            ? disp.color565(220, 80, 80)
            : disp.color565(40, 160, 40);
        disp.fillRect(4, y, barW, 6, disp.color565(40, 40, 40));
        disp.fillRect(4, y, fillW, 6, barColor);

        char secBuf[8]; snprintf(secBuf, sizeof(secBuf), "%ds", secsLeft);
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(140, 140, 140), TP_BG);
        disp.drawString(secBuf, disp.width() - disp.textWidth(secBuf) - 4, y - 1);
        y += 10;
    }

    // Navigator dots
    if ((int)_accounts.size() > 1) {
        int dotSpacing = 10;
        int totalDots  = min((int)_accounts.size(), 10);
        int dotX       = (disp.width() - totalDots * dotSpacing) / 2;
        for (int i = 0; i < totalDots; i++) {
            uint16_t dc = (i == _sel) ? disp.color565(220, 80, 80) : disp.color565(60, 60, 60);
            disp.fillCircle(dotX + i * dotSpacing, y + 4, 2, dc);
        }
        y += 12;
    }

    // Gate mode badge
    CSGateMode gate = csGateGetMode();
    if (gate != CSGateMode::NONE) {
        const char* gateStr = (gate == CSGateMode::TOTP) ? "CS: +TOTP" : "CS: TOTP only";
        disp.setTextSize(1);
        uint16_t gb = disp.color565(80, 30, 80);
        int gw = disp.textWidth(gateStr) + 8;
        disp.fillRoundRect(4, y, gw, 12, 2, gb);
        disp.setTextColor(disp.color565(220, 160, 255), gb);
        disp.drawString(gateStr, 8, y + 2);
    }

    _drawBottomBar(credStoreLocked
        ? "</> acct  N add  D del  (unlock CredStore)  ESC"
        : "</> acct  ENTER type  N add  D del  G gate  ESC");
}

void AppTOTProx::_handleList(KeyInput ki) {
    if (ki.esc)  { uiManager.returnToLauncher(); return; }

    int n = (int)_accounts.size();

    if (n > 0) {
        if (ki.arrowLeft || ki.arrowUp) {
            _sel = (_sel - 1 + n) % n;
            _lastCode = -1; _needsRedraw = true; return;
        }
        if (ki.arrowRight || ki.arrowDown) {
            _sel = (_sel + 1) % n;
            _lastCode = -1; _needsRedraw = true; return;
        }

        if (ki.enter) {
            if (!credStoreLocked && totpTimeReady()) {
                const TOTPAccount& a = _accounts[_sel];
                int32_t code = (int32_t)totpCompute(a.secret, time(nullptr), a.period, a.digits);
                char buf[10]; snprintf(buf, sizeof(buf), "%06" PRId32, code);
                sendPlainText(buf);
            }
            return;
        }

        if (ki.ch == 'd' || ki.ch == 'D') {
            if (credStoreLocked) return;
            _state = ST_CONFIRM_DEL; _needsRedraw = true; return;
        }
    }

    if (ki.ch == 'n' || ki.ch == 'N') {
        if (credStoreLocked) return; // can't add when locked — secrets can't be encrypted
        _addName = ""; _addSecret = ""; _inputBuf = "";
        _state = ST_ADD_NAME; _needsRedraw = true; return;
    }
    if (ki.ch == 'g' || ki.ch == 'G') {
        _pendingGateMode = csGateGetMode();
        _state = ST_GATE_MENU; _needsRedraw = true; return;
    }
}

// ---- Add name ----

void AppTOTProx::_drawAdd(bool secretStep) {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TP_BG);
    _drawTopBar(secretStep ? "Secret" : "Name");

    int y = TP_Y + 6;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(255, 180, 180), TP_BG);

    if (!secretStep) {
        disp.drawString("Account name:", 4, y); y += 14;
        disp.setTextColor(disp.color565(180, 180, 180), TP_BG);
        disp.drawString("e.g. github, work-vpn", 4, y); y += 18;
    } else {
        disp.drawString("Base32 secret:", 4, y); y += 14;
        if (!_addName.isEmpty()) {
            disp.setTextColor(disp.color565(130, 130, 130), TP_BG);
            disp.drawString("Account: " + _addName, 4, y); y += 14;
        }
        disp.setTextColor(disp.color565(180, 180, 180), TP_BG);
        disp.drawString("From TOTP QR code setup", 4, y); y += 18;
    }

    // Input field
    uint16_t fbg = disp.color565(40, 40, 40);
    disp.fillRect(4, y, disp.width() - 8, 14, fbg);
    disp.setTextColor(TFT_WHITE, fbg);
    int maxCh = (disp.width() - 14) / 6;
    String disp_ = _inputBuf;
    if ((int)disp_.length() > maxCh) disp_ = disp_.substring(disp_.length() - maxCh);
    disp.drawString(disp_ + "_", 6, y + 3);

    _drawBottomBar("type  ENTER next  ESC cancel");
}

void AppTOTProx::_handleAddName(KeyInput ki) {
    if (ki.esc)  { _state = ST_LIST; _needsRedraw = true; return; }
    if (ki.del && _inputBuf.length() > 0) { _inputBuf.remove(_inputBuf.length()-1); _needsRedraw = true; return; }
    if (ki.enter && _inputBuf.length() > 0) {
        _addName   = _inputBuf;
        _inputBuf  = "";
        _state     = ST_ADD_SECRET;
        _needsRedraw = true;
        return;
    }
    if (ki.ch) { _inputBuf += ki.ch; _needsRedraw = true; }
}

void AppTOTProx::_handleAddSecret(KeyInput ki) {
    if (ki.esc)  { _state = ST_LIST; _needsRedraw = true; return; }
    if (ki.del && _inputBuf.length() > 0) { _inputBuf.remove(_inputBuf.length()-1); _needsRedraw = true; return; }
    if (ki.enter && _inputBuf.length() >= 16) {
        TOTPAccount a;
        a.name   = _addName;
        a.secret = _inputBuf;
        a.digits = 6;
        a.period = 30;
        if (totpAddAccount(a)) {
            _reloadAccounts();
            // Select the newly added account
            for (int i = 0; i < (int)_accounts.size(); i++) {
                if (_accounts[i].name.equalsIgnoreCase(_addName)) { _sel = i; break; }
            }
        }
        _state = ST_LIST; _needsRedraw = true;
        return;
    }
    if (ki.ch) { _inputBuf += (char)toupper(ki.ch); _needsRedraw = true; }
}

// ---- Confirm delete ----

void AppTOTProx::_drawConfirmDel() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TP_BG);
    _drawTopBar("Delete?");
    int y = TP_Y + 6;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(220, 80, 80), TP_BG);
    disp.drawString("Delete account:", 4, y); y += 14;
    disp.setTextColor(TFT_WHITE, TP_BG);
    if (_sel < (int)_accounts.size())
        disp.drawString(_accounts[_sel].name, 4, y);
    _drawBottomBar("Y=confirm  N/ESC=cancel");
}

void AppTOTProx::_handleConfirmDel(KeyInput ki) {
    if (ki.esc || ki.ch == 'n' || ki.ch == 'N') { _state = ST_LIST; _needsRedraw = true; return; }
    if (ki.ch == 'y' || ki.ch == 'Y') {
        if (_sel < (int)_accounts.size())
            totpDeleteAccount(_accounts[_sel].name);
        _reloadAccounts();
        _state = ST_LIST; _needsRedraw = true;
    }
}

// ---- Gate settings ----

void AppTOTProx::_drawGateMenu() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TP_BG);
    _drawTopBar("Gate");

    int y = TP_Y + 2;
    disp.setTextSize(1);

    // Current mode header
    CSGateMode cur = csGateGetMode();
    bool hasSecret = !csGateGetSecret().isEmpty();
    static const char* modeNames[] = { "Off (key only)", "Key + TOTP", "TOTP only" };
    disp.setTextColor(disp.color565(140, 140, 140), TP_BG);
    disp.drawString("Current:", 4, y);
    disp.setTextColor(disp.color565(220, 180, 255), TP_BG);
    disp.drawString(modeNames[(int)cur], 4 + disp.textWidth("Current:") + 4, y);
    y += 14;

    disp.setTextColor(disp.color565(100, 100, 100), TP_BG);
    disp.drawString(hasSecret ? "TOTP secret: set" : "TOTP secret: not set", 4, y);
    y += 14;

    const struct { CSGateMode mode; const char* label; const char* desc; } opts[] = {
        { CSGateMode::NONE,      "Off",        "Key only (no TOTP)"},
        { CSGateMode::TOTP,      "Key + TOTP", "PIN then TOTP code"},
        { CSGateMode::TOTP_ONLY, "TOTP only",  "No PIN — code alone"},
    };

    for (auto& o : opts) {
        bool sel = (_pendingGateMode == o.mode);
        uint16_t rb = sel ? disp.color565(80, 20, 20) : (uint16_t)TP_BG;
        if (sel) disp.fillRect(0, y-1, disp.width(), 22, rb);
        disp.setTextColor(sel ? TFT_WHITE : disp.color565(160, 160, 160), rb);
        char lbl[32]; snprintf(lbl, sizeof(lbl), "%s %s", sel ? ">" : " ", o.label);
        disp.drawString(lbl, 4, y);
        disp.setTextColor(disp.color565(110, 110, 110), rb);
        disp.drawString(o.desc, 16, y + 11);
        y += 24;
    }

    // Action hints based on selected mode
    disp.setTextColor(disp.color565(80, 140, 80), TP_BG);
    if (_pendingGateMode == CSGateMode::NONE)
        disp.drawString("S=set CS key  ENTER save", 4, y);
    else if (_pendingGateMode == CSGateMode::TOTP)
        disp.drawString("S=set CS key  T=set TOTP  ENTER save", 4, y);
    else
        disp.drawString("T=set TOTP secret  ENTER save", 4, y);

    _drawBottomBar("up/dn mode  S=key  T=TOTP  ENTER save  ESC");
}

void AppTOTProx::_handleGateMenu(KeyInput ki) {
    if (ki.esc) { _state = ST_LIST; _needsRedraw = true; return; }

    if (ki.arrowUp || ki.arrowLeft) {
        _pendingGateMode = (CSGateMode)(((int)_pendingGateMode - 1 + 3) % 3);
        _needsRedraw = true; return;
    }
    if (ki.arrowDown || ki.arrowRight) {
        _pendingGateMode = (CSGateMode)(((int)_pendingGateMode + 1) % 3);
        _needsRedraw = true; return;
    }

    // S — set/replace CS encryption key
    if (ki.ch == 's' || ki.ch == 'S') {
        _csKeyNew = ""; _csKeyStatus = ""; _csKeyStep = KS_NEW; _inputBuf = "";
        _state = ST_GATE_KEY; _needsRedraw = true; return;
    }

    // T — set/replace TOTP gate secret
    if (ki.ch == 't' || ki.ch == 'T') {
        if (_pendingGateMode != CSGateMode::NONE) {
            _inputBuf = "";
            _state = ST_GATE_SECRET; _needsRedraw = true; return;
        }
    }

    if (ki.enter) {
        if (_pendingGateMode != CSGateMode::NONE && csGateGetSecret().isEmpty()) {
            _inputBuf = "";
            _state = ST_GATE_SECRET; _needsRedraw = true; return;
        }
        if (_pendingGateMode == CSGateMode::TOTP_ONLY) {
            preferences.begin("kprox_cs", false);
            preferences.putString("cs_kc", "");
            preferences.end();
            credStoreLock();
        }
        csGateSetMode(_pendingGateMode);
        _state = ST_LIST; _needsRedraw = true;
    }
}

void AppTOTProx::_drawGateKey() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TP_BG);

    bool isConfirm = (_csKeyStep == KS_CONFIRM);
    _drawTopBar(isConfirm ? "Confirm Key" : "Set CS Key");

    int y = TP_Y + 6;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(255, 200, 180), TP_BG);

    if (!isConfirm) {
        disp.drawString("New CS encryption key:", 4, y); y += 14;
        disp.setTextColor(disp.color565(160, 160, 160), TP_BG);
        disp.drawString("Min 8 chars.", 4, y); y += 12;
        disp.drawString("This encrypts all stored", 4, y); y += 11;
        disp.drawString("credentials. Don't lose it.", 4, y); y += 18;
    } else {
        disp.drawString("Re-enter to confirm:", 4, y); y += 26;
    }

    uint16_t fbg = disp.color565(40, 40, 40);
    disp.fillRect(4, y, disp.width() - 8, 14, fbg);
    disp.setTextColor(TFT_WHITE, fbg);
    int maxCh = (disp.width() - 14) / 6;
    // Show masked input
    String masked = String(_inputBuf.length(), '*');
    if ((int)masked.length() > maxCh) masked = masked.substring(masked.length() - maxCh);
    disp.drawString(masked + "_", 6, y + 3);
    y += 20;

    if (_csKeyStatus.length() > 0) {
        bool ok = _csKeyStatus.startsWith("OK");
        disp.setTextColor(ok ? TFT_GREEN : disp.color565(220, 80, 80), TP_BG);
        disp.drawString(_csKeyStatus.c_str() + (ok ? 3 : 0), 4, y);
    }

    _drawBottomBar(isConfirm ? "re-enter  ENTER confirm  ESC back" : "type key  ENTER next  ESC cancel");
}

void AppTOTProx::_handleGateKey(KeyInput ki) {
    if (ki.esc) {
        if (_csKeyStep == KS_CONFIRM) {
            _csKeyStep = KS_NEW; _inputBuf = ""; _csKeyStatus = ""; _needsRedraw = true;
        } else {
            _state = ST_GATE_MENU; _inputBuf = ""; _csKeyStatus = ""; _needsRedraw = true;
        }
        return;
    }

    if (ki.del && _inputBuf.length() > 0) { _inputBuf.remove(_inputBuf.length()-1); _csKeyStatus = ""; _needsRedraw = true; return; }

    if (ki.enter) {
        if (_csKeyStep == KS_NEW) {
            if (_inputBuf.length() < 8) { _csKeyStatus = "Min 8 characters"; _needsRedraw = true; return; }
            _csKeyNew = _inputBuf;
            _inputBuf = "";
            _csKeyStep = KS_CONFIRM;
            _csKeyStatus = "";
            _needsRedraw = true; return;
        } else {
            // Confirm step
            if (_inputBuf != _csKeyNew) {
                _csKeyStatus = "Keys don't match";
                _csKeyStep = KS_NEW; _inputBuf = ""; _csKeyNew = "";
                _needsRedraw = true; return;
            }
            // Apply: if store has existing data, rekey it; otherwise just set the key
            // The store will be re-keyed from whatever was the old runtime key.
            // If the store is currently unlocked, rekey properly.
            // If locked/uninitialised, the new key becomes the initial key on next unlock.
            if (!credStoreLocked && !credStoreRuntimeKey.isEmpty()) {
                if (!credStoreRekey(credStoreRuntimeKey, _csKeyNew)) {
                    _csKeyStatus = "Rekey failed";
                    _csKeyStep = KS_NEW; _inputBuf = ""; _csKeyNew = "";
                    _needsRedraw = true; return;
                }
            } else {
                // Store locked — wipe keycheck so next unlock initialises with new key
                // (user has acknowledged this by setting a key without being unlocked)
                preferences.begin("kprox_cs", false);
                preferences.putString("cs_kc", "");
                preferences.end();
            }
            _csKeyStatus = "OK Key set!";
            _inputBuf = ""; _csKeyNew = "";
            _csKeyStep = KS_NEW;
            _needsRedraw = true;
            // Brief display, then return to gate menu
            return;
        }
    }

    if (ki.ch) { _inputBuf += ki.ch; _csKeyStatus = ""; _needsRedraw = true; }
}

void AppTOTProx::_drawGateSecret() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TP_BG);
    _drawTopBar("Gate Secret");

    int y = TP_Y + 6;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(255, 180, 180), TP_BG);
    disp.drawString("Gate TOTP secret:", 4, y); y += 14;
    disp.setTextColor(disp.color565(160, 160, 160), TP_BG);
    disp.drawString("Base32 key for CS lock.", 4, y); y += 12;
    disp.drawString("Store separately!", 4, y); y += 18;

    uint16_t fbg = disp.color565(40, 40, 40);
    disp.fillRect(4, y, disp.width() - 8, 14, fbg);
    disp.setTextColor(TFT_WHITE, fbg);
    int maxCh = (disp.width() - 14) / 6;
    String d = _inputBuf;
    if ((int)d.length() > maxCh) d = d.substring(d.length() - maxCh);
    disp.drawString(d + "_", 6, y + 3);

    _drawBottomBar("type secret  ENTER save  ESC cancel");
}

void AppTOTProx::_handleGateSecret(KeyInput ki) {
    if (ki.esc) { _state = ST_GATE_MENU; _needsRedraw = true; return; }
    if (ki.del && _inputBuf.length() > 0) { _inputBuf.remove(_inputBuf.length()-1); _needsRedraw = true; return; }
    if (ki.enter && _inputBuf.length() >= 16) {
        csGateSetSecret(_inputBuf);
        // When activating TOTP_ONLY the gate secret becomes the CS encryption key.
        // Clear the keycheck so the store re-initialises with it on first unlock.
        if (_pendingGateMode == CSGateMode::TOTP_ONLY) {
            preferences.begin("kprox_cs", false);
            preferences.putString("cs_kc", "");
            preferences.end();
            credStoreLock();
        }
        csGateSetMode(_pendingGateMode);
        _state = ST_LIST; _needsRedraw = true;
        return;
    }
    if (ki.ch) { _inputBuf += (char)toupper(ki.ch); _needsRedraw = true; }
}

// ---- Lifecycle ----

void AppTOTProx::onEnter() {
    _reloadAccounts();
    _state       = ST_LIST;
    _lastCode    = -1;
    _needsRedraw = true;
}

void AppTOTProx::onUpdate() {
    // Refresh code display every second
    unsigned long now = millis();
    if (_state == ST_LIST && now - _lastCodeRefresh > 1000) {
        _lastCodeRefresh = now;
        _needsRedraw = true;
    }

    if (_needsRedraw) {
        switch (_state) {
            case ST_LIST:         _drawList();          break;
            case ST_ADD_NAME:     _drawAdd(false);      break;
            case ST_ADD_SECRET:   _drawAdd(true);       break;
            case ST_CONFIRM_DEL:  _drawConfirmDel();    break;
            case ST_GATE_MENU:    _drawGateMenu();      break;
            case ST_GATE_SECRET:  _drawGateSecret();    break;
            case ST_GATE_KEY:     _drawGateKey();       break;
        }
        _needsRedraw = false;
    }

    // BtnA — type current code (requires credstore unlocked)
    if (_state == ST_LIST && M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        if (!credStoreLocked && !_accounts.empty() && totpTimeReady()) {
            const TOTPAccount& a = _accounts[_sel];
            int32_t code = (int32_t)totpCompute(a.secret, time(nullptr), a.period, a.digits);
            char buf[10]; snprintf(buf, sizeof(buf), "%06" PRId32, code);
            sendPlainText(buf);
        }
        return;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    switch (_state) {
        case ST_LIST:        _handleList(ki);       break;
        case ST_ADD_NAME:    _handleAddName(ki);    break;
        case ST_ADD_SECRET:  _handleAddSecret(ki);  break;
        case ST_CONFIRM_DEL: _handleConfirmDel(ki); break;
        case ST_GATE_MENU:   _handleGateMenu(ki);   break;
        case ST_GATE_SECRET: _handleGateSecret(ki); break;
        case ST_GATE_KEY:    _handleGateKey(ki);    break;
    }
}

} // namespace Cardputer
#endif
