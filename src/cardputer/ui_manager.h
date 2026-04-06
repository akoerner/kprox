#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include <vector>

namespace Cardputer {

struct KeyInput {
    bool enter     = false;
    bool del       = false;
    bool esc       = false;
    bool tab       = false;
    bool arrowUp   = false;
    bool arrowDown = false;
    bool arrowLeft = false;
    bool arrowRight= false;
    bool fn        = false;
    bool nextPage  = false;
    char ch        = 0;
    bool anyKey    = false;
    bool isRepeat  = false;
};

KeyInput pollKeys(bool editMode = false);
void     drawTabHint(int afterX); // draw small ? badge after title text

class UIManager {
public:
    static constexpr unsigned long SCREEN_TIMEOUT_MS = 60000; // kept for ABI; runtime uses g_screenTimeoutMs

    UIManager();

    void addApp(AppBase* app);
    void reserveApps(size_t n) { _apps.reserve(n); }
    void launchApp(int index);
    void returnToLauncher();
    void update();
    void wakeScreen();
    void notifyInteraction();

    int currentAppIndex() const { return _currentApp; }
    AppBase* currentApp() { return _apps[_currentApp]; }
    const std::vector<AppBase*>& apps() const { return _apps; }
    std::vector<int> visibleApps() const;
    bool isScreenOn() const { return _screenOn; }

private:
    std::vector<AppBase*> _apps;
    int           _currentApp      = 0;
    bool          _showHelp        = false;
    int           _helpScroll      = 0;
    bool          _helpNeedsRedraw = false;

    void _drawHelpOverlay(AppBase* app);
    bool          _screenOn        = true;
    bool          _needsFullRedraw = false;
    unsigned long _lastInteraction = 0;
};

extern UIManager uiManager;

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
