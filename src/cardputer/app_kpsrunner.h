#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include <vector>

namespace Cardputer {

class AppKPSRunner : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override {}
    const char* appName() const override { return "KPScript"; }
    uint16_t iconColor() const override  { return 0xF81F; }
    void requestRedraw() override        { _needsRedraw = true; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    static constexpr int KR_BAR_H = 16;
    static constexpr int KR_BOT_H = 13;
    static constexpr int KR_ROWS  = 7;

    enum State { ST_LIST, ST_CONFIRM_EXEC, ST_RUNNING, ST_NO_SD };

    State               _state      = ST_NO_SD;
    bool                _needsRedraw = true;
    std::vector<String> _scripts;      // full paths
    int                 _sel        = 0;
    int                 _scrollTop  = 0;
    String              _scanPath;     // currently scanned directory
    String              _statusMsg;
    bool                _statusOk  = false;

    void _scan(const String& path);
    void _scanRecursive(const String& path, int depth);
    void _drawTopBar();
    void _drawBottomBar(const char* hint);
    void _drawList();
    void _drawConfirmExec();
    void _handleList(KeyInput ki);
    void _handleConfirmExec(KeyInput ki);
    String _displayName(const String& path) const;
};

} // namespace Cardputer
#endif
