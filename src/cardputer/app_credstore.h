#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include "../totp.h"

namespace Cardputer {

struct CSRawKey {
    char ch    = 0;
    bool del   = false;
    bool enter = false;
    bool esc   = false;
    bool tab   = false;
    bool up    = false;
    bool down  = false;
    bool fn    = false;
    bool any   = false;
};

class AppCredStore : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "CredStore"; }
    uint16_t iconColor() const override  { return 0xF800; }
    void requestRedraw() override { _needsRedraw = true; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    static constexpr int NUM_PAGES = 4;
    static constexpr unsigned long POLL_INTERVAL_MS = 500;

    int           _page        = 0;
    bool          _needsRedraw = false;
    unsigned long _lastPollMs  = 0;
    bool          _snapLocked  = true;
    int           _snapCount   = -1;

    // Page 0 — status / unlock
    String _keyBuf;
    String _totpBuf;           // 6-digit TOTP input buffer
    bool   _totpStep    = false; // true = waiting for TOTP code after PIN
    bool   _unlockFailed   = false;
    bool   _confirmingLock = false;

    // Page 1 — add / update / delete
    // When _credListIdx >= 0 the user is browsing existing labels.
    // Value is NEVER pre-filled from the store; user must retype it.
    // Delete mode: _deletePrompted=true shows Y/N confirmation.
    enum CredField { CF_LABEL = 0, CF_VALUE = 1 };
    CredField _credField      = CF_LABEL;
    String    _credLabel;
    String    _credValue;
    String    _credStatus;
    bool      _credStatusOk  = false;
    int       _credListIdx   = -1;
    bool      _deletePrompted = false;

    // Page 2 — change key
    // Page 2 — change key / gate mode
    // Sub-modes:
    //   P2_REKEY     — standard symmetric→symmetric rekey
    //   P2_GATE_SET  — set gate mode + TOTP secret (entering secret)
    //   P2_GATE_NEWKEY — enter new symmetric key when leaving TOTP-only
    enum Page2Mode { P2_REKEY = 0, P2_GATE_SET, P2_GATE_NEWKEY };
    Page2Mode  _p2Mode     = P2_REKEY;
    CSGateMode _p2NewGate  = CSGateMode::NONE;
    String     _p2TotpSec;   // gate TOTP secret being entered
    String     _p2NewKey;    // new symmetric key (leaving TOTP-only)
    String     _p2NewKeyConf;
    enum RekeyField { RK_OLD = 0, RK_NEW = 1, RK_CONFIRM = 2 };
    RekeyField _rkField    = RK_OLD;
    String     _rkOld;
    String     _rkNew;
    String     _rkConfirm;
    String     _rkStatus;
    bool       _rkStatusOk = false;

    // Page 3 — wipe
    bool   _wipePrompted = false;
    String _wipeStatus;
    bool   _wipeStatusOk = false;

    void _drawTopBar(int page);
    void _drawPage0();
    void _drawPage1();
    void _drawPage2();
    void _drawPage3();
    void _drawConfirmLock();
    void _drawDeleteConfirm();
    void _drawInputField(int x, int y, int w, const String& buf,
                         bool active, bool masked = false);
    void _handlePage0(CSRawKey rk);
    void _handlePage1(CSRawKey rk);
    void _handlePage2(CSRawKey rk);
    void _handlePage3(CSRawKey rk);
    void _pollState();
    void _resetCredFields();
};

} // namespace Cardputer
#endif
