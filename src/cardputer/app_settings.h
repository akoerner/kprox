#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

class AppSettings : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "Settings"; }
    uint16_t iconColor() const override { return 0x07E0; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    // 0=WiFi, 1=BT, 2=USB, 3=API Key, 4=Device Identity, 5=Sink Config, 6=Timing 1/2, 7=Timing 2/2, 8=Startup App, 9=App Layout
    static constexpr int NUM_PAGES = 11;
    int  _page        = 0;
    bool _needsRedraw = true;

    int  _toggleSel  = 0;
    bool _rebootNote = false;

    enum WifiInputState { WS_SSID, WS_PASS, WS_CONNECTING, WS_DONE };
    WifiInputState _wifiState = WS_SSID;
    String         _wifiInputBuf;
    String         _newSSID;
    String         _wifiStatusMsg;
    bool           _wifiSuccess = false;

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
    void _connectWifi();
    void _drawInputField(int x, int y, int w, const String& text, bool active, bool masked = false);
    void _drawToggleRow(int y, bool selected, const char* label, bool enabled,
                        const char* connStatus, uint16_t connColor);
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
