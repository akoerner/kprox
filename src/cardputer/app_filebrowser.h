#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"
#include <SD.h>
#include <vector>

namespace Cardputer {

class AppFileBrowser : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override { _closeView(); }
    const char* appName() const override { return "Files"; }
    const char* appHelp()  const override { return "Browse and manage SD card files.\nENTER: view file or enter directory\nD: dump to HID  DEL: delete file"; }
    const uint16_t* appIcon() const override { return fa_folder_open_48; }
    void requestRedraw() override        { _needsRedraw = true; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    static constexpr int FB_BAR_H = 16;
    static constexpr int FB_BOT_H = 13;

    enum State {
        ST_BROWSE,
        ST_CONFIRM_DELETE,
        ST_VIEW,
        ST_NO_SD,
    };

    struct Entry {
        String name;
        bool   isDir;
        size_t size;
    };

    State               _state       = ST_NO_SD;
    bool                _needsRedraw = true;
    String              _path;
    std::vector<Entry>  _entries;
    int                 _sel         = 0;
    int                 _scrollTop   = 0;

    String              _viewPath;
    File                _viewFile;
    std::vector<uint32_t> _lineStarts;
    bool                _viewScanned = false;
    int                 _viewLine    = 0;

    String              _statusMsg;
    bool                _statusOk    = false;

    void _loadDir();
    void _openView(const String& path);
    void _closeView();
    String _readViewLine(int lineNum);
    void _drawTopBar();
    void _drawBottomBar(const char* hint);
    void _drawBrowse();
    void _drawView();
    void _drawConfirmDelete();
    void _handleBrowse(KeyInput ki);
    void _handleView(KeyInput ki);
    void _handleConfirmDelete(KeyInput ki);

    String _fullPath(const String& name) const;
    String _parentPath() const;
};

} // namespace Cardputer
#endif
