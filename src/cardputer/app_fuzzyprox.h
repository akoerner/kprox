#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include <vector>

namespace Cardputer {

class AppFuzzyProx : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    void requestRedraw() override { _needsRedraw = true; }
    const char* appName() const override { return "FuzzyProx"; }
    uint16_t iconColor() const override  { return 0xF81F; } // magenta
    bool handlesGlobalBtnA() const override { return true; }

private:
    String           _query;
    std::vector<int> _matches;  // register indices sorted by score
    int              _sel       = 0;
    bool             _needsRedraw   = true;
    bool             _snapLocked    = true;
    bool             _justConfirmed = false; // show "Active!" feedback

    static constexpr int BAR_H = 18;
    static constexpr int BOT_H = 14;
    static constexpr int ROW_H = 14;

    // BtnA state for play-on-press behaviour
    unsigned long _lastBtnPress   = 0;
    unsigned long _lastBtnRelease = 0;
    int           _btnCount       = 0;
    bool          _haltTriggered  = false;
    bool          _skipRelease    = false;
    static constexpr unsigned long DBL_MS = 350;

    void _rebuildMatches();
    int  _fuzzyScore(const String& hay, const String& needle) const;
    void _draw();
    void _checkBtnA();
};

} // namespace Cardputer
#endif
