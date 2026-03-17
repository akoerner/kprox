#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include "../totp.h"

namespace Cardputer {

class AppTOTProx : public AppBase {
public:
    void onEnter() override;
    void onExit() override {}
    void onUpdate() override;
    const char* appName() const override { return "TOTProx"; }
    uint16_t iconColor() const override  { return 0xF800; }
    bool handlesGlobalBtnA() const override { return true; } // red

private:
    enum State {
        ST_LIST,
        ST_ADD_NAME,
        ST_ADD_SECRET,
        ST_CONFIRM_DEL,
        ST_GATE_MENU,
        ST_GATE_SECRET,
        ST_GATE_KEY,     // set/replace the CS encryption key
    };

    State  _state      = ST_LIST;
    bool   _needsRedraw = true;

    // Account list
    std::vector<TOTPAccount> _accounts;
    int    _sel         = 0;

    // Add flow
    String _addName;
    String _addSecret;

    // Gate settings
    CSGateMode _pendingGateMode = CSGateMode::NONE;

    // CS key entry state (for gate key setup)
    enum KeyStep { KS_NEW, KS_CONFIRM };
    KeyStep _csKeyStep  = KS_NEW;
    String  _csKeyNew;
    String  _csKeyStatus;

    unsigned long _lastCodeRefresh = 0;
    int32_t       _lastCode        = -1;

    void _reloadAccounts();
    void _drawList();
    void _drawAdd(bool secretStep);
    void _drawConfirmDel();
    void _drawGateMenu();
    void _drawGateSecret();
    void _drawGateKey();

    void _handleList(KeyInput ki);
    void _handleAddName(KeyInput ki);
    void _handleAddSecret(KeyInput ki);
    void _handleConfirmDel(KeyInput ki);
    void _handleGateMenu(KeyInput ki);
    void _handleGateSecret(KeyInput ki);
    void _handleGateKey(KeyInput ki);

    void _drawTopBar(const char* subtitle);
    void _drawBottomBar(const char* hint);

    // Shared input buffer for add/gate flows
    String _inputBuf;
};

} // namespace Cardputer
#endif
