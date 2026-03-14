#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include <vector>

namespace Cardputer {

struct Gadget {
    String name;
    String description;
    String content;
};

class AppGadgets : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override {}
    const char* appName() const override { return "Gadgets"; }
    uint16_t iconColor() const override  { return 0x07FF; }

private:
    enum State { ST_IDLE, ST_LOADING, ST_READY, ST_ERROR, ST_INSTALLING };

    State               _state       = ST_IDLE;
    std::vector<Gadget> _gadgets;
    std::vector<String> _pendingNames; // file names left to fetch
    int                 _totalFiles   = 0;
    int                 _page         = 0;
    bool                _needsRedraw  = true;
    String              _errorMsg;
    String              _installMsg;
    bool                _installOk    = false;

    static constexpr int BAR_H = 18;
    static constexpr int BOT_H = 14;

    // Fetch one pending gadget file per onUpdate call while ST_LOADING
    bool _fetchDirectory();
    bool _fetchNext();    // returns false when done
    void _draw();
    void _drawLoading();
    void _drawError();
    void _drawGadget();
    void _install();

    static String _httpGet(const char* host, const char* path);
};

} // namespace Cardputer
#endif
