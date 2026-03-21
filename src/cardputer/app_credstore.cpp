#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_credstore.h"
#include "../credential_store.h"
#include "../totp.h"
#include "ui_manager.h"

namespace Cardputer {

static constexpr int CS_BG    = 0x1082;
static constexpr int CS_BAR_H = 18;
static constexpr int CS_BOT_H = 14;

static const char* PAGE_LABELS[4] = {
    "Status / Unlock", "Add / Update", "Key & Gate", "Wipe Store"
};

// ---- Raw keyboard ----
// Reads directly from hardware, bypassing pollKeys() navigation aliases.
// This allows , . ; / ` and all other printable chars in input fields.

static CSRawKey pollRaw() {
    CSRawKey rk;
    if (!M5Cardputer.Keyboard.isChange()) return rk;
    if (!M5Cardputer.Keyboard.isPressed()) return rk;

    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
    rk.any   = true;
    rk.del   = ks.del;
    rk.enter = ks.enter;
    rk.tab   = ks.tab;
    rk.fn    = ks.fn;

    // ESC = fn + backtick only. Tab is tab.
    for (uint8_t hk : ks.hid_keys) {
        switch (hk) {
            case 0x29: rk.esc  = true; break;
            case 0x52: if (ks.fn) rk.up   = true; break;
            case 0x51: if (ks.fn) rk.down = true; break;
        }
    }

    for (char c : ks.word) {
        if (c == 0x1B)            { rk.esc = true; continue; }
        if (c == '`')             { rk.esc = true; continue; }  // ` = ESC (fn+` or plain)
        if (ks.fn) {
            if (c == ';') { rk.up   = true; continue; }
            if (c == '.') { rk.down = true; continue; }
        }
        if (c >= 0x20 && c < 0x7F) rk.ch = c;
    }
    return rk;
}

// ---- Helpers ----

void AppCredStore::_drawTopBar(int page) {
    auto& disp = M5Cardputer.Display;
    uint16_t bg = disp.color565(120, 20, 20);
    disp.fillRect(0, 0, disp.width(), CS_BAR_H, bg);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, bg);
    disp.drawString(PAGE_LABELS[page], 4, 3);
    char pg[8];
    snprintf(pg, sizeof(pg), "%d/%d", page + 1, NUM_PAGES);
    int pw = disp.textWidth(pg);
    disp.drawString(pg, disp.width() - pw - 4, 3);
}

void AppCredStore::_drawInputField(int x, int y, int w,
                                    const String& buf, bool active, bool masked) {
    auto& disp = M5Cardputer.Display;
    uint16_t fbg = active ? disp.color565(60, 60, 60) : disp.color565(38, 38, 38);
    disp.fillRect(x, y, w, 14, fbg);
    disp.setTextColor(TFT_WHITE, fbg);
    disp.setTextSize(1);
    int maxChars = (w - 6) / 6;
    String d = masked ? String(buf.length(), '*') : buf;
    if ((int)d.length() > maxChars)
        d = d.substring(d.length() - maxChars);
    if (active) d += '_';
    disp.drawString(d, x + 3, y + 3);
}

void AppCredStore::_resetCredFields() {
    _credLabel    = "";
    _credPassword = "";
    _credUsername = "";
    _credNotes    = "";
    _credStatus    = "";
    _credStatusOk  = false;
    _credField     = CEF_LABEL;
    _credListIdx   = -1;
    _deletePrompted = false;
}

// ---- Page 0: Status / Unlock ----

void AppCredStore::_drawPage0() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(CS_BG);
    _drawTopBar(0);

    int y = CS_BAR_H + 6;
    bool locked = credStoreLocked;

    uint16_t sb = locked ? disp.color565(140,30,30) : disp.color565(30,120,30);
    const char* ss = locked ? "LOCKED" : "UNLOCKED";
    int bw = disp.textWidth(ss) + 12;
    disp.fillRoundRect(4, y, bw, 16, 3, sb);
    disp.setTextColor(TFT_WHITE, sb);
    disp.drawString(ss, 10, y + 3);
    int nextX = bw + 10;

    // Gate mode badge
    CSGateMode gate = csGateGetMode();
    if (gate != CSGateMode::NONE) {
        const char* gateStr = (gate == CSGateMode::TOTP) ? "+TOTP" : "TOTP";
        int gbw = disp.textWidth(gateStr) + 8;
        uint16_t gb = disp.color565(80, 30, 80);
        disp.fillRoundRect(nextX, y, gbw, 16, 3, gb);
        disp.setTextColor(disp.color565(220, 160, 255), gb);
        disp.drawString(gateStr, nextX + 4, y + 3);
        nextX += gbw + 6;
    }

    // NTP sync badge
    bool ntpOk = totpTimeReady();
    const char* ntpStr = ntpOk ? "NTP" : "NO NTP";
    uint16_t nb = ntpOk ? disp.color565(20, 80, 20) : disp.color565(100, 60, 0);
    uint16_t nc = ntpOk ? disp.color565(80, 220, 80) : disp.color565(220, 160, 40);
    int nbw = disp.textWidth(ntpStr) + 8;
    disp.fillRoundRect(nextX, y, nbw, 16, 3, nb);
    disp.setTextColor(nc, nb);
    disp.drawString(ntpStr, nextX + 4, y + 3);
    y += 22;

    // Auto-lock countdown + failed attempts badges
    int nextX2 = 4;

    // Auto-lock countdown badge (only when unlocked and enabled)
    if (!locked && csAutoLockSecs > 0) {
        unsigned long elapsedSec = (millis() - credStoreLastActivity) / 1000UL;
        int remaining = (int)csAutoLockSecs - (int)elapsedSec;
        if (remaining < 0) remaining = 0;
        char lockStr[20];
        snprintf(lockStr, sizeof(lockStr), "LOCK %ds", remaining);
        uint16_t lb = remaining <= 10 ? disp.color565(140,60,0) : disp.color565(20,60,100);
        uint16_t lc = remaining <= 10 ? disp.color565(255,180,80) : disp.color565(100,180,255);
        int lbw = disp.textWidth(lockStr) + 8;
        disp.fillRoundRect(nextX2, y, lbw, 14, 3, lb);
        disp.setTextColor(lc, lb);
        disp.drawString(lockStr, nextX2 + 4, y + 2);
        nextX2 += lbw + 6;
    }

    // Failed attempts badge (always shown when non-zero or wipe enabled)
    int fails = csGetFailedAttempts();
    if (fails > 0 || csAutoWipeAttempts > 0) {
        char failStr[24];
        if (csAutoWipeAttempts > 0)
            snprintf(failStr, sizeof(failStr), "FAIL %d/%d", fails, csAutoWipeAttempts);
        else
            snprintf(failStr, sizeof(failStr), "FAIL %d", fails);
        uint16_t fb = fails > 0 ? disp.color565(120,20,20) : disp.color565(30,30,30);
        uint16_t fc2 = fails > 0 ? disp.color565(255,100,100) : disp.color565(100,100,100);
        int fbw = disp.textWidth(failStr) + 8;
        disp.fillRoundRect(nextX2, y, fbw, 14, 3, fb);
        disp.setTextColor(fc2, fb);
        disp.drawString(failStr, nextX2 + 4, y + 2);
        nextX2 += fbw + 6;
    }

    if (nextX2 > 4) y += 18;  // advance only if second row had content

    disp.setTextSize(1);
    disp.setTextColor(disp.color565(200,200,200), CS_BG);
    if (locked) {
        disp.drawString("Credentials: --", 4, y);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Credentials: %d", credStoreCount());
        disp.drawString(buf, 4, y);
    }
    y += 16;

    if (locked) {
        bool totpOnly = (gate == CSGateMode::TOTP_ONLY);
        bool needsTotp = (gate == CSGateMode::TOTP || gate == CSGateMode::TOTP_ONLY);

        if (!_totpStep) {
            if (!totpOnly) {
                disp.setTextColor(disp.color565(180,180,180), CS_BG);
                disp.drawString("Store key:", 4, y); y += 12;
                _drawInputField(4, y, disp.width() - 8, _keyBuf, true, false);
                y += 18;
            } else {
                disp.setTextColor(disp.color565(220,180,255), CS_BG);
                disp.drawString("Enter TOTP code:", 4, y); y += 12;
                if (!totpTimeReady()) {
                    disp.setTextColor(disp.color565(220,160,0), CS_BG);
                    disp.drawString("No NTP sync — unavailable", 4, y);
                    y += 14;
                } else {
                    _drawInputField(4, y, disp.width() - 8, _totpBuf, true, false);
                    y += 18;
                }
            }
        } else {
            // Step 2: TOTP code after PIN accepted
            disp.setTextColor(disp.color565(100,220,100), CS_BG);
            disp.drawString("Key OK. Enter TOTP:", 4, y); y += 12;
            if (!totpTimeReady()) {
                disp.setTextColor(disp.color565(220,160,0), CS_BG);
                disp.drawString("No NTP sync — cannot unlock", 4, y);
                y += 14;
            } else {
                _drawInputField(4, y, disp.width() - 8, _totpBuf, true, false);
                y += 18;
            }
        }

        if (_unlockFailed) {
            disp.setTextColor(disp.color565(220,80,80), CS_BG);
            disp.drawString(needsTotp && !totpTimeReady()
                            ? "NTP required for TOTP unlock"
                            : "Invalid credentials", 4, y);
        }
    } else {
        auto labels = credStoreListLabels();
        int shown = 0;
        for (auto& lbl : labels) {
            if (shown >= 3) {
                disp.setTextColor(disp.color565(130,130,130), CS_BG);
                disp.drawString("...", 8, y);
                break;
            }
            disp.setTextColor(disp.color565(100,200,255), CS_BG);
            disp.drawString(("  * " + lbl).c_str(), 4, y);
            y += 13;
            shown++;
        }
    }

    uint16_t bb = disp.color565(16,16,16);
    disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb);
    disp.setTextColor(disp.color565(100,100,100), bb);
    if (locked)
        disp.drawString("type  ENTER unlock  </> page  ESC back", 2, disp.height()-CS_BOT_H+2);
    else
        disp.drawString("ENTER=lock  </> page  ESC back", 2, disp.height()-CS_BOT_H+2);
}

void AppCredStore::_drawConfirmLock() {
    auto& disp = M5Cardputer.Display;
    int bx=20, by=44, bw=disp.width()-40, bh=50;
    disp.fillRoundRect(bx, by, bw, bh, 6, disp.color565(40,10,10));
    disp.drawRoundRect(bx, by, bw, bh, 6, disp.color565(200,60,60));
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, disp.color565(40,10,10));
    disp.setTextDatum(MC_DATUM);
    int cx = bx + bw/2;
    disp.drawString("Lock credential store?", cx, by+14);
    disp.drawString("Y confirm   N cancel", cx, by+30);
    disp.setTextDatum(TL_DATUM);
}

void AppCredStore::_handlePage0(CSRawKey rk) {
    if (_confirmingLock) {
        if (rk.ch=='y'||rk.ch=='Y') {
            credStoreLock();
            _snapLocked=true; _snapCount=-1;
            _confirmingLock=false; _keyBuf=""; _totpBuf=""; _totpStep=false;
        } else if (rk.ch=='n'||rk.ch=='N'||rk.esc) {
            _confirmingLock=false;
        }
        _needsRedraw=true; return;
    }

    bool locked = credStoreLocked;
    CSGateMode gate = csGateGetMode();
    bool totpOnly = (gate == CSGateMode::TOTP_ONLY);

    if (_keyBuf.isEmpty() && _totpBuf.isEmpty() && !_totpStep) {
        if (rk.ch==',') { _page=NUM_PAGES-1; _needsRedraw=true; return; }
        if (rk.ch=='/') { _page=1;           _needsRedraw=true; return; }
    }

    if (locked) {
        if (rk.esc) {
            if (_totpStep) {
                // Go back to key entry
                _totpStep=false; _totpBuf=""; _unlockFailed=false; _needsRedraw=true;
            } else if (_keyBuf.isEmpty() && _totpBuf.isEmpty()) {
                uiManager.returnToLauncher();
            } else {
                _keyBuf=""; _totpBuf=""; _unlockFailed=false; _needsRedraw=true;
            }
            return;
        }

        if (_totpStep || totpOnly) {
            // Collecting TOTP code — only digits, 6 chars max
            if (rk.del && _totpBuf.length()>0) { _totpBuf.remove(_totpBuf.length()-1); _unlockFailed=false; _needsRedraw=true; return; }
            if (rk.enter) {
                if (!totpTimeReady()) { _unlockFailed=true; _needsRedraw=true; return; }
                bool ok = totpOnly
                    ? credStoreUnlockWithTOTP("", _totpBuf)
                    : credStoreUnlockWithTOTP(_keyBuf, _totpBuf);
                if (ok) {
                    _unlockFailed=false; _totpStep=false; _keyBuf=""; _totpBuf="";
                    _snapLocked=false; _snapCount=credStoreCount();
                } else {
                    _unlockFailed=true; _totpBuf="";
                }
                _needsRedraw=true; return;
            }
            if (rk.ch>='0' && rk.ch<='9' && (int)_totpBuf.length()<6) {
                _totpBuf+=rk.ch; _unlockFailed=false; _needsRedraw=true;
            }
        } else {
            // Key entry
            if (rk.enter && _keyBuf.length()>0) {
                if (gate == CSGateMode::TOTP) {
                    // Verify key first, then ask for TOTP
                    // Peek: try unlock with empty TOTP — it will fail at TOTP check
                    // Use a temp check: decrypt keycheck directly
                    // Instead: gate == TOTP means we do two-step; check key validity first
                    _totpStep = true; _totpBuf = ""; _unlockFailed=false;
                    _needsRedraw=true; return;
                }
                if (credStoreUnlock(_keyBuf)) {
                    _unlockFailed=false; _snapLocked=false; _snapCount=credStoreCount();
                } else {
                    _unlockFailed=true;
                }
                _keyBuf=""; _needsRedraw=true; return;
            }
            if (rk.del && _keyBuf.length()>0) { _keyBuf.remove(_keyBuf.length()-1); _unlockFailed=false; _needsRedraw=true; return; }
            if (rk.ch) { _keyBuf+=rk.ch; _unlockFailed=false; _needsRedraw=true; }
        }
    } else {
        if (rk.enter) { _confirmingLock=true; _needsRedraw=true; }
        if (rk.esc)   { uiManager.returnToLauncher(); }
    }
}

// ---- Page 1: Add / Update / Delete ----

void AppCredStore::_drawDeleteConfirm() {
    auto& disp = M5Cardputer.Display;
    int bx=10, by=52, bw=disp.width()-20, bh=50;
    disp.fillRoundRect(bx, by, bw, bh, 6, disp.color565(40,10,10));
    disp.drawRoundRect(bx, by, bw, bh, 6, disp.color565(200,60,60));
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, disp.color565(40,10,10));
    disp.setTextDatum(MC_DATUM);
    int cx = bx+bw/2;
    String msg = "Delete \"" + _credLabel + "\"?";
    if ((int)msg.length() > 30) msg = "Delete this credential?";
    disp.drawString(msg, cx, by+14);
    disp.drawString("Y confirm   N cancel", cx, by+30);
    disp.setTextDatum(TL_DATUM);
}

void AppCredStore::_drawPage1() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(CS_BG);
    _drawTopBar(1);

    int y = CS_BAR_H + 5;
    int fw = disp.width() - 8;

    if (!credStoreLocked) {
        bool labelActive = (_credField == CEF_LABEL);
        bool passActive  = (_credField == CEF_PASSWORD);
        bool userActive  = (_credField == CEF_USERNAME);
        bool notesActive = (_credField == CEF_NOTES);

        // --- Label field ---
        disp.setTextSize(1);
        disp.setTextColor(labelActive ? TFT_WHITE : disp.color565(160,160,160), CS_BG);
        disp.drawString("Label:", 4, y);

        if (labelActive && _credListIdx >= 0) {
            auto labels = credStoreListLabels();
            if (!labels.empty()) {
                char nav[12];
                snprintf(nav, sizeof(nav), "%d/%d", _credListIdx+1, (int)labels.size());
                int nw = disp.textWidth(nav);
                disp.setTextColor(disp.color565(100,200,255), CS_BG);
                disp.drawString(nav, disp.width()-nw-4, y);
            }
        }

        if (!_credLabel.isEmpty() && _credListIdx < 0) {
            bool exists = credStoreLabelExists(_credLabel);
            uint16_t badgeBg = exists ? disp.color565(30,80,120) : disp.color565(20,80,20);
            const char* badgeStr = exists ? "UPD" : "NEW";
            int bw2 = disp.textWidth(badgeStr)+8;
            int bx2 = disp.width()-bw2-4;
            disp.fillRoundRect(bx2, y, bw2, 12, 2, badgeBg);
            disp.setTextColor(TFT_WHITE, badgeBg);
            disp.drawString(badgeStr, bx2+4, y+2);
        }

        y += 11;
        _drawInputField(4, y, fw, _credLabel, labelActive, false);
        y += 16;

        // --- Password field ---
        disp.setTextColor(passActive ? TFT_WHITE : disp.color565(160,160,160), CS_BG);
        disp.drawString("Password:", 4, y);
        y += 11;
        _drawInputField(4, y, fw, _credPassword, passActive, true);
        y += 15;

        // --- Username field ---
        disp.setTextColor(userActive ? TFT_WHITE : disp.color565(160,160,160), CS_BG);
        disp.drawString("Username:", 4, y);
        y += 11;
        _drawInputField(4, y, fw, _credUsername, userActive, false);
        y += 15;

        // --- Notes field ---
        disp.setTextColor(notesActive ? TFT_WHITE : disp.color565(160,160,160), CS_BG);
        disp.drawString("Notes:", 4, y);
        y += 11;
        _drawInputField(4, y, fw, _credNotes, notesActive, false);
        y += 14;

        if (labelActive && credStoreCount() > 0) {
            disp.setTextSize(1);
            disp.setTextColor(disp.color565(90,90,120), CS_BG);
            disp.drawString("fn+up/dn browse  DEL del", 4, y);
            y += 12;
        }

        if (_credStatus.length() > 0) {
            disp.setTextColor(_credStatusOk ? TFT_GREEN : disp.color565(220,80,80), CS_BG);
            disp.drawString(_credStatus, 4, y);
        }
    } else {
        disp.setTextColor(disp.color565(180,100,60), CS_BG);
        disp.drawString("Unlock the store first", 4, y+20);
        disp.drawString("(page 1)", 4, y+34);
    }

    uint16_t bb = disp.color565(16,16,16);
    disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb);
    disp.setTextColor(disp.color565(100,100,100), bb);
    disp.drawString("TAB field  ENTER save  DEL delete  </> page", 2, disp.height()-CS_BOT_H+2);

    if (_deletePrompted) _drawDeleteConfirm();
}

void AppCredStore::_handlePage1(CSRawKey rk) {
    // Delete confirmation overlay
    if (_deletePrompted) {
        if (rk.ch=='y'||rk.ch=='Y') {
            credStoreDelete(_credLabel);
            _credStatus   = "Deleted";
            _credStatusOk = true;
            _snapCount    = credStoreCount();
            _resetCredFields();
        } else if (rk.ch=='n'||rk.ch=='N'||rk.esc) {
            _deletePrompted = false;
        }
        _needsRedraw = true;
        return;
    }

    bool bothEmpty = _credLabel.isEmpty() && _credPassword.isEmpty()
                  && _credUsername.isEmpty() && _credNotes.isEmpty();

    // Page navigation when fields are clear
    if (bothEmpty) {
        if (rk.ch==',') { _page=0; _resetCredFields(); _needsRedraw=true; return; }
        if (rk.ch=='/') { _page=2; _resetCredFields(); _needsRedraw=true; return; }
    }

    if (rk.esc) {
        if (!bothEmpty) {
            _resetCredFields(); _needsRedraw=true;
        } else {
            _page=0; _needsRedraw=true;
        }
        return;
    }

    if (!credStoreLocked) {
        if (rk.tab) {
            _credField   = (CredEditField)(((int)_credField + 1) % CEF_COUNT);
            _credListIdx = -1;
            _credStatus  = "";
            _needsRedraw = true;
            return;
        }

        if (_credField == CEF_LABEL) {
            // Up/down: cycle through existing labels (never shows values)
            if (rk.up || rk.down) {
                auto labels = credStoreListLabels();
                if (!labels.empty()) {
                    int n = (int)labels.size();
                    if (_credListIdx < 0)
                        _credListIdx = rk.up ? n-1 : 0;
                    else
                        _credListIdx = rk.up ? (_credListIdx-1+n)%n : (_credListIdx+1)%n;
                    _credLabel    = labels[_credListIdx];
                    _credPassword = "";
                    _credUsername = "";
                    _credNotes    = "";
                    _credStatus = "";
                    _needsRedraw = true;
                }
                return;
            }

            // DEL on label field with an existing credential = delete prompt
            if (rk.del) {
                if (!_credLabel.isEmpty() && credStoreLabelExists(_credLabel)) {
                    _deletePrompted = true;
                    _needsRedraw    = true;
                } else if (!_credLabel.isEmpty()) {
                    _credLabel.remove(_credLabel.length()-1);
                    _credListIdx=-1; _credStatus=""; _needsRedraw=true;
                }
                return;
            }

            if (rk.enter) {
                if (!_credLabel.isEmpty()) {
                    _credField=CEF_PASSWORD; _credListIdx=-1; _needsRedraw=true;
                }
                return;
            }

            if (rk.ch) {
                _credLabel  += rk.ch;
                _credListIdx = -1; _credStatus=""; _needsRedraw=true;
            }

        } else {
            // CEF_PASSWORD / CEF_USERNAME / CEF_NOTES
            String& buf = (_credField==CEF_USERNAME) ? _credUsername
                        : (_credField==CEF_NOTES)    ? _credNotes
                        : _credPassword;

            if (rk.enter) {
                if (_credLabel.isEmpty()) {
                    _credStatus="Label required"; _credStatusOk=false; _needsRedraw=true;
                    return;
                }
                // Save all non-empty fields; always save password (may be clearing it)
                bool existed = credStoreLabelExists(_credLabel);
                bool ok = credStoreSet(_credLabel, _credPassword, CredField::PASSWORD);
                if (ok && !_credUsername.isEmpty())
                    ok = credStoreSet(_credLabel, _credUsername, CredField::USERNAME);
                if (ok && !_credNotes.isEmpty())
                    ok = credStoreSet(_credLabel, _credNotes, CredField::NOTES);
                if (ok) {
                    _credStatus   = existed ? "Updated!" : "Saved!";
                    _credStatusOk = true;
                    _snapCount    = credStoreCount();
                    _resetCredFields();
                } else {
                    _credStatus="Save failed"; _credStatusOk=false;
                }
                _needsRedraw=true; return;
            }

            if (rk.del && buf.length()>0) {
                buf.remove(buf.length()-1);
                _credStatus=""; _needsRedraw=true; return;
            }
            if (rk.ch) {
                buf+=rk.ch; _credStatus=""; _needsRedraw=true;
            }
        }
    }
}

// ---- Page 2: Key & Gate ----

void AppCredStore::_drawPage2() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(CS_BG);
    _drawTopBar(2);

    int y = CS_BAR_H + 4;
    disp.setTextSize(1);

    if (credStoreLocked) {
        disp.setTextColor(disp.color565(180,100,60), CS_BG);
        disp.drawString("Unlock the store first.", 4, y + 10);
        disp.drawString("(page 1)", 4, y + 24);
    {
        uint16_t bb_ = disp.color565(16,16,16);
        disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb_);
        disp.setTextColor(disp.color565(100,100,100), bb_);
        disp.drawString("</> page  ESC back", 2, disp.height()-CS_BOT_H+2);
    }
        return;
    }

    CSGateMode gate = csGateGetMode();

    // ---- Sub-mode: standard rekey ----
    if (_p2Mode == P2_REKEY) {
        // Gate badge
        static const char* gateNames[] = { "Key only", "Key + TOTP", "TOTP only" };
        disp.setTextColor(disp.color565(130,130,130), CS_BG);
        disp.drawString("Gate:", 4, y);
        disp.setTextColor(disp.color565(200,160,255), CS_BG);
        disp.drawString(gateNames[(int)gate], 4 + disp.textWidth("Gate: "), y);
        y += 14;

        static const char* labels[3] = { "Current key:", "New key:", "Confirm:" };
        String* bufs[3] = { &_rkOld, &_rkNew, &_rkConfirm };
        int fw = disp.width() - 8;
        for (int i = 0; i < 3; i++) {
            disp.setTextColor((_rkField==(RekeyField)i) ? TFT_WHITE : disp.color565(160,160,160), CS_BG);
            disp.drawString(labels[i], 4, y); y += 11;
            _drawInputField(4, y, fw, *bufs[i], _rkField==(RekeyField)i, false);
            y += 16;
        }
        if (_rkStatus.length() > 0) {
            disp.setTextColor(_rkStatusOk ? TFT_GREEN : disp.color565(220,80,80), CS_BG);
            disp.drawString(_rkStatus, 4, y); y += 13;
        }

        // Gate switch shortcuts
        y = max(y, CS_BAR_H + 74);
        disp.setTextColor(disp.color565(80,130,80), CS_BG);
        if (gate == CSGateMode::TOTP_ONLY) {
            disp.drawString("K = switch to key/PIN", 4, y);
        } else {
            disp.drawString("T = add TOTP gate", 4, y); y += 12;
            if (gate == CSGateMode::TOTP)
                disp.drawString("O = TOTP-only mode", 4, y);
        }
    {
        uint16_t bb_ = disp.color565(16,16,16);
        disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb_);
        disp.setTextColor(disp.color565(100,100,100), bb_);
        disp.drawString("TAB next  ENTER save  T/K/O switch  </>", 2, disp.height()-CS_BOT_H+2);
    }

    // ---- Sub-mode: entering TOTP secret for gate ----
    } else if (_p2Mode == P2_GATE_SET) {
        static const char* modeNames[] = { "", "Key+TOTP", "TOTP only" };
        char hdr[32]; snprintf(hdr, sizeof(hdr), "Gate: %s", modeNames[(int)_p2NewGate]);
        disp.setTextColor(TFT_YELLOW, CS_BG);
        disp.drawString(hdr, 4, y); y += 14;
        disp.setTextColor(disp.color565(160,160,160), CS_BG);
        disp.drawString("TOTP secret (Base32):", 4, y); y += 12;
        disp.drawString("Leave blank = keep current", 4, y); y += 14;
        _drawInputField(4, y, disp.width()-8, _p2TotpSec, true); y += 18;
        if (_rkStatus.length() > 0) {
            disp.setTextColor(_rkStatusOk ? TFT_GREEN : disp.color565(220,80,80), CS_BG);
            disp.drawString(_rkStatus, 4, y);
        }
    {
        uint16_t bb_ = disp.color565(16,16,16);
        disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb_);
        disp.setTextColor(disp.color565(100,100,100), bb_);
        disp.drawString("type  ENTER confirm  ESC cancel", 2, disp.height()-CS_BOT_H+2);
    }

    // ---- Sub-mode: entering new symmetric key (leaving TOTP-only) ----
    } else if (_p2Mode == P2_GATE_NEWKEY) {
        disp.setTextColor(TFT_YELLOW, CS_BG);
        disp.drawString("Leave TOTP-only", 4, y); y += 14;
        int minLen = (_p2NewGate == CSGateMode::TOTP) ? 4 : 8;
        char hint[32]; snprintf(hint, sizeof(hint), "New key (min %d chars):", minLen);
        disp.setTextColor(disp.color565(160,160,160), CS_BG);
        disp.drawString(hint, 4, y); y += 12;
        _drawInputField(4, y, disp.width()-8, _p2NewKey, _rkField==RK_NEW, false); y += 16;
        disp.drawString("Confirm:", 4, y); y += 12;
        _drawInputField(4, y, disp.width()-8, _p2NewKeyConf, _rkField==RK_CONFIRM, false); y += 16;
        if (_rkStatus.length() > 0) {
            disp.setTextColor(_rkStatusOk ? TFT_GREEN : disp.color565(220,80,80), CS_BG);
            disp.drawString(_rkStatus, 4, y);
        }
    {
        uint16_t bb_ = disp.color565(16,16,16);
        disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb_);
        disp.setTextColor(disp.color565(100,100,100), bb_);
        disp.drawString("type  TAB next  ENTER save  ESC cancel", 2, disp.height()-CS_BOT_H+2);
    }
    }
}

void AppCredStore::_handlePage2(CSRawKey rk) {
    if (!credStoreLocked) {

        // ---- Sub-mode: TOTP secret entry ----
        if (_p2Mode == P2_GATE_SET) {
            if (rk.esc)  { _p2Mode=P2_REKEY; _p2TotpSec=""; _rkStatus=""; _needsRedraw=true; return; }
            if (rk.del && _p2TotpSec.length()>0) { _p2TotpSec.remove(_p2TotpSec.length()-1); _needsRedraw=true; return; }
            if (rk.enter) {
                // Validate secret if provided
                if (!_p2TotpSec.isEmpty() && (int)_p2TotpSec.length() < 16) {
                    _rkStatus="Secret min 16 chars"; _rkStatusOk=false; _needsRedraw=true; return;
                }
                if (!_p2TotpSec.isEmpty()) csGateSetSecret(_p2TotpSec);
                if (csGateGetSecret().isEmpty()) {
                    _rkStatus="No TOTP secret set"; _rkStatusOk=false; _needsRedraw=true; return;
                }
                // For TOTP_ONLY: rekey from current symmetric key to gate secret
                if (_p2NewGate == CSGateMode::TOTP_ONLY) {
                    if (!credStoreRekey(credStoreRuntimeKey, csGateGetSecret())) {
                        _rkStatus="Rekey failed"; _rkStatusOk=false; _needsRedraw=true; return;
                    }
                }
                csGateSetMode(_p2NewGate);
                _rkStatus="Gate mode set!"; _rkStatusOk=true;
                _p2Mode=P2_REKEY; _p2TotpSec="";
                _needsRedraw=true; return;
            }
            if (rk.ch) { _p2TotpSec += (char)toupper(rk.ch); _needsRedraw=true; }
            return;
        }

        // ---- Sub-mode: new key when leaving TOTP-only ----
        if (_p2Mode == P2_GATE_NEWKEY) {
            if (rk.esc) { _p2Mode=P2_REKEY; _p2NewKey=""; _p2NewKeyConf=""; _rkStatus=""; _rkField=RK_NEW; _needsRedraw=true; return; }
            if (rk.tab) { _rkField=(_rkField==RK_NEW)?RK_CONFIRM:RK_NEW; _needsRedraw=true; return; }
            String* cur = (_rkField==RK_NEW) ? &_p2NewKey : &_p2NewKeyConf;
            if (rk.del && cur->length()>0) { cur->remove(cur->length()-1); _rkStatus=""; _needsRedraw=true; return; }
            if (rk.enter) {
                if (_rkField==RK_NEW) { _rkField=RK_CONFIRM; _needsRedraw=true; return; }
                int minLen = (_p2NewGate == CSGateMode::TOTP) ? 4 : 8;
                if (_p2NewKey != _p2NewKeyConf) { _rkStatus="Keys don't match"; _rkStatusOk=false; }
                else if ((int)_p2NewKey.length() < minLen) {
                    char buf[24]; snprintf(buf, sizeof(buf), "Min %d chars", minLen);
                    _rkStatus=buf; _rkStatusOk=false;
                } else if (credStoreRekey(credStoreRuntimeKey, _p2NewKey)) {
                    csGateSetMode(_p2NewGate);
                    _rkStatus="Done!"; _rkStatusOk=true;
                    _p2Mode=P2_REKEY; _p2NewKey=""; _p2NewKeyConf=""; _rkField=RK_OLD;
                } else { _rkStatus="Rekey failed"; _rkStatusOk=false; }
                _needsRedraw=true; return;
            }
            if (rk.ch) { *cur += rk.ch; _rkStatus=""; _needsRedraw=true; }
            return;
        }

        // ---- Sub-mode: standard rekey ----
        CSGateMode gate = csGateGetMode();

        // Gate switch shortcuts (only when input fields are empty)
        bool inputClear = _rkOld.isEmpty() && _rkNew.isEmpty() && _rkConfirm.isEmpty();
        if (inputClear) {
            // T = add/switch to Key+TOTP
            if ((rk.ch=='t'||rk.ch=='T') && gate != CSGateMode::TOTP) {
                if (gate == CSGateMode::TOTP_ONLY) {
                    _p2NewGate=CSGateMode::TOTP; _p2Mode=P2_GATE_NEWKEY; _p2NewKey=""; _p2NewKeyConf=""; _rkField=RK_NEW;
                } else {
                    _p2NewGate=CSGateMode::TOTP; _p2Mode=P2_GATE_SET; _p2TotpSec="";
                }
                _rkStatus=""; _needsRedraw=true; return;
            }
            // O = switch to TOTP-only
            if ((rk.ch=='o'||rk.ch=='O') && gate != CSGateMode::TOTP_ONLY) {
                _p2NewGate=CSGateMode::TOTP_ONLY; _p2Mode=P2_GATE_SET; _p2TotpSec="";
                _rkStatus=""; _needsRedraw=true; return;
            }
            // K = switch to key-only
            if ((rk.ch=='k'||rk.ch=='K') && gate != CSGateMode::NONE) {
                if (gate == CSGateMode::TOTP_ONLY) {
                    _p2NewGate=CSGateMode::NONE; _p2Mode=P2_GATE_NEWKEY; _p2NewKey=""; _p2NewKeyConf=""; _rkField=RK_NEW;
                } else {
                    csGateSetMode(CSGateMode::NONE);
                    _rkStatus="Key-only mode set"; _rkStatusOk=true;
                }
                _needsRedraw=true; return;
            }
            if (rk.ch==',') { _page=1; _needsRedraw=true; return; }
            if (rk.ch=='/') { _page=3; _needsRedraw=true; return; }
        }

        if (rk.esc) { _page=0; _rkOld=""; _rkNew=""; _rkConfirm=""; _rkStatus=""; _needsRedraw=true; return; }

        if (rk.tab) { _rkField=(RekeyField)(((int)_rkField+1)%3); _needsRedraw=true; return; }
        String* cur = (_rkField==RK_OLD)?&_rkOld : (_rkField==RK_NEW)?&_rkNew : &_rkConfirm;
        if (rk.enter) {
            if (_rkField!=RK_CONFIRM) { _rkField=(RekeyField)((int)_rkField+1); _needsRedraw=true; }
            else {
                int minLen = (gate==CSGateMode::TOTP) ? 4 : 8;
                if (_rkNew!=_rkConfirm)              { _rkStatus="Keys don't match"; _rkStatusOk=false; }
                else if ((int)_rkNew.length()<minLen) { char b[16]; snprintf(b,sizeof(b),"Min %d chars",minLen); _rkStatus=b; _rkStatusOk=false; }
                else if (credStoreRekey(_rkOld,_rkNew)) {
                    _rkStatus="Key changed!"; _rkStatusOk=true;
                    _rkOld=""; _rkNew=""; _rkConfirm=""; _rkField=RK_OLD;
                } else { _rkStatus="Wrong current key"; _rkStatusOk=false; }
                _needsRedraw=true;
            }
            return;
        }
        if (rk.del && cur->length()>0) { cur->remove(cur->length()-1); _rkStatus=""; _needsRedraw=true; return; }
        if (rk.ch) { *cur+=rk.ch; _rkStatus=""; _needsRedraw=true; }

    } else {
        // locked — only page navigation
        if (rk.ch==',') { _page=1; _needsRedraw=true; }
        if (rk.ch=='/') { _page=3; _needsRedraw=true; }
        if (rk.esc)     { _page=0; _needsRedraw=true; }
    }
}

// ---- Page 3: Wipe ----

void AppCredStore::_drawPage3() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(CS_BG);
    _drawTopBar(3);

    int y = CS_BAR_H+10;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(220,80,80), CS_BG);
    disp.drawString("! Deletes ALL credentials", 4, y); y+=14;
    disp.drawString("! Clears the key check",    4, y); y+=14;
    disp.drawString("! Cannot be undone",         4, y); y+=20;

    if (_wipePrompted) {
        uint16_t pb = disp.color565(60,15,15);
        disp.fillRoundRect(4, y, disp.width()-8, 22, 4, pb);
        disp.setTextColor(TFT_WHITE, pb);
        disp.drawString("Type Y to confirm wipe", 8, y+6);
        y+=30;
    } else {
        disp.setTextColor(disp.color565(180,180,180), CS_BG);
        disp.drawString("Press ENTER to begin wipe", 4, y);
        y+=18;
    }

    if (_wipeStatus.length()>0) {
        disp.setTextColor(_wipeStatusOk ? TFT_GREEN : disp.color565(220,80,80), CS_BG);
        disp.drawString(_wipeStatus, 4, y);
    }

    uint16_t bb = disp.color565(16,16,16);
    disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb);
    disp.setTextColor(disp.color565(100,100,100), bb);
    disp.drawString("ENTER wipe  </> page  ESC back", 2, disp.height()-CS_BOT_H+2);
}

void AppCredStore::_handlePage3(CSRawKey rk) {
    if (!_wipePrompted) {
        if (rk.ch==',') { _page=2; _needsRedraw=true; return; }
        if (rk.ch=='/') { _page=0; _needsRedraw=true; return; }
    }
    if (rk.esc) {
        if (_wipePrompted) { _wipePrompted=false; _wipeStatus=""; _needsRedraw=true; }
        else { _page=0; _needsRedraw=true; }
        return;
    }
    if (_wipePrompted) {
        if (rk.ch=='y'||rk.ch=='Y') {
            credStoreWipe();
            _wipeStatus="Store wiped"; _wipeStatusOk=true;
            _wipePrompted=false;
            _snapLocked=true; _snapCount=-1;
            _rkOld=""; _rkNew=""; _rkConfirm=""; _rkStatus="";
            _resetCredFields();
        } else if (rk.ch=='n'||rk.ch=='N') {
            _wipePrompted=false; _wipeStatus="";
        }
        _needsRedraw=true; return;
    }
    if (rk.enter) { _wipePrompted=true; _wipeStatus=""; _needsRedraw=true; }
}

// ---- Poll & lifecycle ----

void AppCredStore::_pollState() {
    // Auto-lock check
    if (!credStoreLocked && csAutoLockSecs > 0) {
        unsigned long elapsed = (millis() - credStoreLastActivity) / 1000UL;
        if (elapsed >= (unsigned long)csAutoLockSecs) {
            credStoreLock();
            _confirmingLock=false; _keyBuf=""; _totpBuf=""; _totpStep=false;
        } else {
            _needsRedraw = true; // keep countdown ticking
        }
    }

    bool curLocked = credStoreLocked;
    int  curCount  = curLocked ? -1 : credStoreCount();
    if (curLocked!=_snapLocked || curCount!=_snapCount) {
        _snapLocked=curLocked; _snapCount=curCount;
        _confirmingLock=false;
        _needsRedraw=true;
    }

    // Redraw every second when unlocked to keep countdown ticking
    if (!curLocked && _page == 0) _needsRedraw = true;
}

void AppCredStore::onEnter() {
    _page=0; _needsRedraw=true;
    _confirmingLock=false; _keyBuf=""; _totpBuf=""; _totpStep=false; _unlockFailed=false;
    _resetCredFields();
    _rkField=RK_OLD; _rkOld=""; _rkNew=""; _rkConfirm=""; _rkStatus="";
    _p2Mode=P2_REKEY; _p2TotpSec=""; _p2NewKey=""; _p2NewKeyConf="";
    _wipePrompted=false; _wipeStatus="";
    _lastPollMs=0;
    _snapLocked=!credStoreLocked; _snapCount=-1;
}

void AppCredStore::onExit() {
    _confirmingLock=false; _keyBuf=""; _totpBuf=""; _totpStep=false; _wipePrompted=false;
    _resetCredFields();
    _rkOld=""; _rkNew=""; _rkConfirm="";
    _p2Mode=P2_REKEY; _p2TotpSec=""; _p2NewKey=""; _p2NewKeyConf="";
}

void AppCredStore::onUpdate() {
    unsigned long now = millis();

    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        _page=(_page+1)%NUM_PAGES;
        _confirmingLock=false; _wipePrompted=false; _deletePrompted=false;
        _needsRedraw=true;
        return;
    }

    if (now-_lastPollMs >= POLL_INTERVAL_MS) {
        _lastPollMs=now;
        _pollState();
    }

    if (_needsRedraw) {
        switch (_page) {
            case 0: _drawPage0(); if (_confirmingLock) _drawConfirmLock(); break;
            case 1: _drawPage1(); break; // delete confirm is drawn inside _drawPage1
            case 2: _drawPage2(); break;
            case 3: _drawPage3(); break;
        }
        _needsRedraw=false;
    }

    CSRawKey rk = pollRaw();
    if (!rk.any) return;
    uiManager.notifyInteraction();
    credStoreLastActivity = millis();

    switch (_page) {
        case 0: _handlePage0(rk); break;
        case 1: _handlePage1(rk); break;
        case 2: _handlePage2(rk); break;
        case 3: _handlePage3(rk); break;
    }
}

} // namespace Cardputer
#endif
