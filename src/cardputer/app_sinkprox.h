#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

class AppSinkProx : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override {}
    void requestRedraw() override { _needsRedraw = true; }
    const char* appName() const override { return "SinkProx"; }
    uint16_t iconColor() const override  { return 0x04FF; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    bool          _needsRedraw   = true;
    bool          _confirmDelete = false;
    String        _statusMsg;
    bool          _statusOk      = false;
    size_t        _sinkSize      = 0;
    unsigned long _lastPoll      = 0;
    int           _page          = 0;   // 0 = main, 1 = help

    static constexpr unsigned long POLL_MS   = 2000;
    static constexpr const char*   SINK_FILE = "/sink.txt";

    unsigned long _lastBtnPress   = 0;
    unsigned long _lastBtnRelease = 0;
    int           _btnCount       = 0;
    bool          _haltTriggered  = false;
    bool          _skipRelease    = false;
    static constexpr unsigned long DBL_MS = 350;

    void _draw();
    void _drawHelp();
    void _doFlush();
    void _doDelete();
    void _pollSize();
    void _checkBtnA();
};

} // namespace Cardputer
#endif
