#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"

namespace Cardputer {

class AppSettings : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "Settings"; }
    const char* appHelp()  const override { return "Device settings across 15 pages.\nLeft/right arrows to change page.\nSettings are saved immediately."; }
    const uint16_t* appIcon() const override { return fa_gear_48; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    // 0=WiFi, 1=BT, 2=USB, 3=API Key, 4=Device Identity, 5=Sink Config, 6=Timing 1/2, 7=Timing 2/2, 8=Startup App, 9=App Layout
    static constexpr int NUM_PAGES = 15;
    int  _page        = 0;
    bool _needsRedraw = true;

    int  _toggleSel  = 0;
    bool _rebootNote = false;

    enum WifiInputState { WS_SSID, WS_PASS, WS_CONNECTING, WS_DONE };
    WifiInputState _wifiState = WS_SSID;
    String         _wifiInputBuf;
    String         _newSSID;
    String         _wifiStatusMsg;
    bool           _wifiSuccess  = false;
    int            _wifiSel      = 0;   // 0=SSID 1=Pass
    bool           _wifiEditing  = false;

    int    _idSel   = 0;
    bool   _editing = false;
    String _editBuf;
    bool   _idSaved = false;

    // Timing page
    int    _timingSel = 0;

    void _drawTopBar(int pageNum);
    void _drawBottomBar(const char* hint);
    void _drawPage0();
    void _drawPage1();
    void _drawPage2();
    void _drawPage3();
    void _drawPage4();
    void _drawPage5();
    void _drawPage6();
    void _drawPage7();
    void _drawPage8();
    void _drawPage9();
    void _drawPage10();
    void _handlePage0(KeyInput ki);
    void _handlePage1(KeyInput ki);
    void _handlePage2(KeyInput ki);
    void _handlePage3(KeyInput ki);
    void _handlePage4(KeyInput ki);
    void _handlePage5(KeyInput ki);
    void _handlePage6(KeyInput ki);
    void _handlePage7(KeyInput ki);
    void _handlePage8(KeyInput ki);
    void _handlePage9(KeyInput ki);
    void _handlePage10(KeyInput ki);
    void _drawPage11();
    void _handlePage11(KeyInput ki);
    void _drawPage12();
    void _handlePage12(KeyInput ki);
    void _drawPage13();
    void _handlePage13(KeyInput ki);
    void _drawPage14();
    void _handlePage14(KeyInput ki);

    // Keymap page state
    int  _keymapSel   = 0;
    bool _keymapSaved = false;

    // Display page state
    int  _dispSel     = 0;
    bool _dispEditing = false;

    // Backup page state
    int             _backupSel   = 0;  // 0=create 1..N=restore entries
    int             _backupScroll = 0;
    std::vector<String> _backupFiles;   // listed .json files on SD
    String          _backupStatus;
    bool            _backupStatusOk = false;
    void            _backupRefresh();
    bool            _backupCreate(bool includeRegs, bool includeSettings);
    bool            _backupRestore(const String& path);

    // SD storage confirmation state
    bool            _sdConfirmPending = false;
    // 0=none, 1=transfer pending, 2=switch pending
    int             _storageConfirmOp = 0;
    String          _storageConfirmDest; // "sd" or "nvs" 
    void _connectWifi();
    void _drawInputField(int x, int y, int w, const String& text, bool active, bool masked = false);
    void _drawToggleRow(int y, bool selected, const char* label, bool enabled,
                        const char* connStatus, uint16_t connColor);
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
