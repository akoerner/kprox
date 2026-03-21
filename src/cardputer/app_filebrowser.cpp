#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_filebrowser.h"
#include "ui_manager.h"
#include "../credential_store.h"
#include "../hid.h"
#include "../kps_parser.h"
#include "../sd_utils.h"
#include <SD.h>
#include <algorithm>

namespace Cardputer {

static constexpr uint16_t FB_BG    = 0x0841;
static constexpr int      ROWS     = 7;

// ---- Helpers ----

String AppFileBrowser::_fullPath(const String& name) const {
    if (_path == "/") return "/" + name;
    return _path + "/" + name;
}

String AppFileBrowser::_parentPath() const {
    if (_path == "/" || _path.isEmpty()) return "/";
    int last = _path.lastIndexOf('/');
    if (last <= 0) return "/";
    return _path.substring(0, last);
}

void AppFileBrowser::_loadDir() {
    _entries.clear();
    _sel       = 0;
    _scrollTop = 0;
    _statusMsg = "";

    if (!sdAvailable()) { _state = ST_NO_SD; return; }

    File dir = SD.open(_path.isEmpty() ? "/" : _path);
    if (!dir || !dir.isDirectory()) {
        _statusMsg = "Cannot open dir";
        _state = ST_BROWSE;
        return;
    }

    // Add ".." entry unless at root
    if (_path != "/" && !_path.isEmpty())
        _entries.push_back({ "..", true, 0 });

    File f = dir.openNextFile();
    while (f) {
        String n = String(f.name());
        int sl = n.lastIndexOf('/');
        if (sl >= 0) n = n.substring(sl + 1);
        if (!n.isEmpty() && n != "." && n != "..")
            _entries.push_back({ n, f.isDirectory(), (size_t)f.size() });
        f.close();
        f = dir.openNextFile();
    }
    dir.close();

    // Dirs first, then files, both alphabetical
    std::sort(_entries.begin(), _entries.end(), [](const Entry& a, const Entry& b) {
        if (a.name == "..") return true;
        if (b.name == "..") return false;
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name < b.name;
    });

    _state = ST_BROWSE;
}

// ---- Drawing ----

void AppFileBrowser::_drawTopBar() {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = disp.color565(0, 80, 60);
    disp.fillRect(0, 0, disp.width(), FB_BAR_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, bc);
    disp.drawString("Files", 4, 3);

    // Show current path truncated
    String p = _path.isEmpty() ? "/" : _path;
    if ((int)p.length() > 22) p = "..." + p.substring(p.length() - 19);
    int pw = disp.textWidth(p);
    disp.setTextColor(disp.color565(180, 255, 220), bc);
    disp.drawString(p, disp.width() - pw - 4, 3);
}

void AppFileBrowser::_drawBottomBar(const char* hint) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = disp.color565(16, 16, 16);
    disp.fillRect(0, disp.height() - FB_BOT_H, disp.width(), FB_BOT_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(110, 110, 110), bc);
    disp.drawString(hint, 2, disp.height() - FB_BOT_H + 2);
}

void AppFileBrowser::_drawBrowse() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(FB_BG);
    _drawTopBar();

    int y = FB_BAR_H + 2;
    int rowH = (disp.height() - FB_BAR_H - FB_BOT_H - 2) / ROWS;

    if (!sdAvailable()) {
        disp.setTextColor(disp.color565(200, 80, 80), FB_BG);
        disp.drawString("SD card not available", 4, y + 10);
        _drawBottomBar("ESC back");
        return;
    }

    if (_entries.empty()) {
        disp.setTextColor(disp.color565(120, 120, 120), FB_BG);
        disp.drawString("Empty directory", 4, y + 10);
    }

    for (int i = 0; i < ROWS && (_scrollTop + i) < (int)_entries.size(); i++) {
        int idx = _scrollTop + i;
        const Entry& e = _entries[idx];
        bool sel = (idx == _sel);

        uint16_t bg = sel ? disp.color565(0, 80, 60) : (uint16_t)FB_BG;
        disp.fillRect(0, y, disp.width(), rowH, bg);

        disp.setTextSize(1);
        // Icon
        disp.setTextColor(e.isDir ? disp.color565(255, 220, 80) : disp.color565(160, 200, 255), bg);
        disp.drawString(e.isDir ? "[D]" : "[F]", 2, y + 2);

        // Name
        disp.setTextColor(sel ? TFT_WHITE : disp.color565(200, 200, 200), bg);
        String display = e.name;
        if ((int)display.length() > 20) display = display.substring(0, 18) + "..";
        disp.drawString(display, 30, y + 2);

        // Size for files
        if (!e.isDir && e.name != "..") {
            String sz;
            if (e.size >= 1024) sz = String(e.size / 1024) + "k";
            else                sz = String(e.size) + "b";
            int sw = disp.textWidth(sz);
            disp.setTextColor(disp.color565(100, 120, 100), bg);
            disp.drawString(sz, disp.width() - sw - 3, y + 2);
        }
        y += rowH;
    }

    // Status message
    if (!_statusMsg.isEmpty()) {
        y = disp.height() - FB_BOT_H - 14;
        disp.setTextColor(_statusOk ? TFT_GREEN : disp.color565(220, 80, 80), FB_BG);
        disp.drawString(_statusMsg, 4, y);
    }

    _drawBottomBar("ENTER view  D dump  DEL delete  ESC up");
}

void AppFileBrowser::_drawView() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(FB_BG);
    _drawTopBar();

    int y = FB_BAR_H + 2;
    int rowH = 12;
    int visRows = (disp.height() - FB_BAR_H - FB_BOT_H - 4) / rowH;

    // Split viewBuf into lines
    std::vector<String> lines;
    int start = 0;
    for (int i = 0; i <= (int)_viewBuf.length(); i++) {
        if (i == (int)_viewBuf.length() || _viewBuf[i] == '\n') {
            lines.push_back(_viewBuf.substring(start, i));
            start = i + 1;
        }
    }

    disp.setTextSize(1);
    for (int i = 0; i < visRows && (_viewLine + i) < (int)lines.size(); i++) {
        String l = lines[_viewLine + i];
        if ((int)l.length() > 37) l = l.substring(0, 35) + "..";
        disp.setTextColor(disp.color565(200, 220, 200), FB_BG);
        disp.drawString(l, 2, y + i * rowH);
    }

    _drawBottomBar("up/dn scroll  ESC back");
}

void AppFileBrowser::_drawConfirmDelete() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(FB_BG);
    _drawTopBar();
    int y = FB_BAR_H + 10;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(220, 80, 80), FB_BG);
    disp.drawString("Delete:", 4, y); y += 14;
    disp.setTextColor(TFT_WHITE, FB_BG);
    if (_sel < (int)_entries.size())
        disp.drawString(_entries[_sel].name, 4, y);
    _drawBottomBar("Y confirm   N/ESC cancel");
}

// ---- Input handlers ----

void AppFileBrowser::_handleBrowse(KeyInput ki) {
    if (ki.esc) {
        if (_path == "/" || _path.isEmpty()) { uiManager.returnToLauncher(); return; }
        _path = _parentPath();
        _loadDir();
        _needsRedraw = true;
        return;
    }

    int n = (int)_entries.size();

    if (ki.arrowUp || ki.arrowLeft) {
        if (n > 0) { _sel = (_sel - 1 + n) % n; if (_sel < _scrollTop) _scrollTop = _sel; }
        _needsRedraw = true; return;
    }
    if (ki.arrowDown || ki.arrowRight) {
        if (n > 0) { _sel = (_sel + 1) % n; if (_sel >= _scrollTop + ROWS) _scrollTop = _sel - ROWS + 1; }
        _needsRedraw = true; return;
    }

    if (ki.enter && n > 0) {
        const Entry& e = _entries[_sel];
        if (e.isDir) {
            _path = (e.name == "..") ? _parentPath() : _fullPath(e.name);
            _loadDir();
        } else {
            _viewBuf  = sdReadFile(_fullPath(e.name));
            _viewLine = 0;
            _state    = ST_VIEW;
        }
        _needsRedraw = true;
        return;
    }

    // D = dump file content to HID as raw text (no token parsing)
    if ((ki.ch == 'd' || ki.ch == 'D') && n > 0 && !_entries[_sel].isDir) {
        String content = sdReadFile(_fullPath(_entries[_sel].name));
        if (!content.isEmpty()) sendPlainText(content);
        _statusMsg = "Dumped"; _statusOk = true;
        _needsRedraw = true;
        return;
    }

    // DEL key = delete
    if (ki.del && n > 0) {
        const Entry& e = _entries[_sel];
        if (e.name != "..") { _state = ST_CONFIRM_DELETE; _needsRedraw = true; }
        return;
    }
}

void AppFileBrowser::_handleView(KeyInput ki) {
    if (ki.esc || ki.enter) { _state = ST_BROWSE; _needsRedraw = true; return; }
    if (ki.arrowDown) { _viewLine++; _needsRedraw = true; }
    if (ki.arrowUp)   { if (_viewLine > 0) _viewLine--; _needsRedraw = true; }
}

void AppFileBrowser::_handleConfirmDelete(KeyInput ki) {
    if (ki.esc || ki.ch == 'n' || ki.ch == 'N') { _state = ST_BROWSE; _needsRedraw = true; return; }
    if (ki.ch == 'y' || ki.ch == 'Y') {
        if (_sel < (int)_entries.size()) {
            String fp = _fullPath(_entries[_sel].name);
            bool ok = sdDeleteFile(fp);
            _statusMsg = ok ? "Deleted" : "Delete failed";
            _statusOk  = ok;
            _loadDir();
        }
        _state = ST_BROWSE; _needsRedraw = true;
    }
}

// ---- Lifecycle ----

void AppFileBrowser::onEnter() {
    _path = "/";
    _loadDir();
    _needsRedraw = true;
}

void AppFileBrowser::onUpdate() {
    if (_needsRedraw) {
        switch (_state) {
            case ST_BROWSE:         _drawBrowse();        break;
            case ST_VIEW:           _drawView();          break;
            case ST_CONFIRM_DELETE: _drawConfirmDelete(); break;
            case ST_NO_SD:          _drawBrowse();        break;
            default:                _drawBrowse();        break;
        }
        _needsRedraw = false;
    }

    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        if (_state == ST_BROWSE) { uiManager.returnToLauncher(); }
        return;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    switch (_state) {
        case ST_BROWSE:         _handleBrowse(ki);        break;
        case ST_VIEW:           _handleView(ki);          break;
        case ST_CONFIRM_DELETE: _handleConfirmDelete(ki); break;
        case ST_NO_SD:          if (ki.esc) uiManager.returnToLauncher(); break;
        default: break;
    }
}

} // namespace Cardputer
#endif
