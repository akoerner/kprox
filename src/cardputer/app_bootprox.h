#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

class AppBootProx : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override {}
    const char* appName() const override { return "BootProx"; }
    uint16_t iconColor() const override  { return 0x07FF; }
    void requestRedraw() override        { _needsRedraw = true; }

private:
    static constexpr int BP_BAR_H = 16;
    static constexpr int BP_BOT_H = 13;

    enum Field { F_ENABLED = 0, F_REGISTER, F_LIMIT, F_RESET, F_COUNT };

    bool          _needsRedraw = true;
    Field         _sel         = F_ENABLED;
    bool          _editing     = false;
    String        _editBuf;
    String        _statusMsg;
    bool          _statusOk    = false;

    void _draw();
    void _drawTopBar();
    void _drawBottomBar(const char* hint);
    void _handle(KeyInput ki);
    void _save();
};

} // namespace Cardputer
#endif
