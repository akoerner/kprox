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

struct GadgetSlot {
    String  filename;
    String  downloadUrl;    // raw URL from directory listing — correct branch guaranteed
    bool    fetched  = false;
    bool    fetchErr = false;
    Gadget  gadget;
};

class AppGadgets : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override {}
    const char* appName() const override { return "Gadgets"; }
    uint16_t iconColor() const override  { return 0x07FF; }

private:
    enum State { ST_IDLE, ST_DIR_LOADING, ST_GADGET_LOADING, ST_READY, ST_ERROR };

    State                  _state      = ST_IDLE;
    std::vector<GadgetSlot> _slots;
    int                    _page       = 0;
    bool                   _needsRedraw = true;
    String                 _errorMsg;
    String                 _statusMsg;
    bool                   _statusOk   = false;

    static constexpr int BAR_H = 18;
    static constexpr int BOT_H = 14;

    bool _fetchDirectory();
    bool _fetchSlot(int idx);

    void _draw();
    void _drawDirLoading();
    void _drawGadgetLoading();
    void _drawGadget();
    void _drawError();
    void _install();

    static String _httpGet(const char* host, const char* path);
};

} // namespace Cardputer
#endif
