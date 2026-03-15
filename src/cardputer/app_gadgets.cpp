#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_gadgets.h"
#include "../registers.h"
#include "ui_manager.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

namespace Cardputer {

static constexpr int  GDG_BG      = 0x0841;
static constexpr char GH_HOST[]   = "api.github.com";
static constexpr char GH_LIST[]   = "/repos/akoerner/kprox/contents/gadgets";
static constexpr char GH_RAW_HOST[] = "raw.githubusercontent.com";

// ---- HTTPS GET helper ----
// Uses HTTP/1.0 to avoid chunked transfer encoding, with a hard read timeout
// and a per-byte watchdog feed so the WDT never trips during large responses.

String AppGadgets::_httpGet(const char* host, const char* path) {
    if (WiFi.status() != WL_CONNECTED) return "";

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15);
    feedWatchdog();

    if (!client.connect(host, 443)) return "";
    feedWatchdog();

    // HTTP/1.0 disables chunked transfer encoding so we can read the body as-is.
    client.printf("GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: KProx/1.0\r\nAccept: application/json\r\nConnection: close\r\n\r\n",
                  path, host);

    // Skip headers
    unsigned long hdrDeadline = millis() + 10000UL;
    while (client.connected() && millis() < hdrDeadline) {
        String line = client.readStringUntil('\n');
        feedWatchdog();
        line.trim();
        if (line.length() == 0) break;
    }

    // Read body with timeout
    String body;
    body.reserve(4096);
    unsigned long bodyDeadline = millis() + 20000UL;
    while ((client.available() || client.connected()) && millis() < bodyDeadline) {
        while (client.available()) {
            body += (char)client.read();
            feedWatchdog();
        }
        if (client.connected()) delay(5);
    }
    client.stop();
    return body;
}

// ---- Phase 1: fetch directory listing, populate _pendingNames ----

bool AppGadgets::_fetchDirectory() {
    feedWatchdog();

    // Up to 3 attempts — GitHub API occasionally returns empty or truncated responses.
    String listing;
    for (int attempt = 0; attempt < 3 && listing.isEmpty(); attempt++) {
        if (attempt > 0) { delay(1500); feedWatchdog(); }
        listing = _httpGet(GH_HOST, GH_LIST);
    }

    if (listing.isEmpty()) {
        _errorMsg = "GitHub unreachable (check WiFi)";
        _state    = ST_ERROR;
        _needsRedraw = true;
        return false;
    }

    // The GitHub API may prepend a rate-limit or redirect body; find the first '['
    int arrayStart = listing.indexOf('[');
    if (arrayStart > 0) listing = listing.substring(arrayStart);

    JsonDocument dir;
    DeserializationError err = deserializeJson(dir, listing);
    if (err || !dir.is<JsonArray>()) {
        _errorMsg = String("Dir parse error: ") + err.c_str();
        _state    = ST_ERROR;
        _needsRedraw = true;
        return false;
    }

    _pendingNames.clear();
    for (JsonVariant entry : dir.as<JsonArray>()) {
        String name = entry["name"] | "";
        String type = entry["type"] | "";
        if (name.endsWith(".json") && type == "file")
            _pendingNames.push_back(name);
    }

    _totalFiles = _pendingNames.size();
    if (_totalFiles == 0) {
        _errorMsg = "No gadgets found in repo";
        _state    = ST_ERROR;
        _needsRedraw = true;
        return false;
    }
    return true;
}

// ---- Phase 2: fetch one file from the front of _pendingNames ----
// Returns true while more files remain, false when done.

bool AppGadgets::_fetchNext() {
    if (_pendingNames.empty()) return false;

    String name = _pendingNames.front();
    _pendingNames.erase(_pendingNames.begin());

    feedWatchdog();
    String rawPath = "/akoerner/kprox/main/gadgets/" + name;
    String raw;
    for (int attempt = 0; attempt < 2 && raw.isEmpty(); attempt++) {
        if (attempt > 0) { delay(800); feedWatchdog(); }
        raw = _httpGet(GH_RAW_HOST, rawPath.c_str());
    }

    if (!raw.isEmpty()) {
        // Trim to the first '{' in case the server prepends stray bytes
        int objStart = raw.indexOf('{');
        if (objStart > 0) raw = raw.substring(objStart);

        JsonDocument gdoc;
        if (!deserializeJson(gdoc, raw)) {
            JsonObject g = gdoc["gadget"];
            if (g) {
                Gadget gad;
                gad.name        = g["name"]        | name;
                gad.description = g["description"] | "";
                gad.content     = g["content"]     | "";
                if (!gad.content.isEmpty()) {
                    _gadgets.push_back(gad);
                    if (_state == ST_LOADING && !_gadgets.empty()) {
                        _state = ST_READY;
                        if (_page < 0) _page = 0;
                    }
                }
            }
        }
    }

    _needsRedraw = true;
    return !_pendingNames.empty();
}

// ---- Install ----

void AppGadgets::_install() {
    if (_page < 0 || _page >= (int)_gadgets.size()) return;
    const Gadget& g = _gadgets[_page];
    addRegister(g.content, g.name);
    _installMsg = "Installed: " + g.name;
    _installOk  = true;
    _needsRedraw = true;
}

// ---- Drawing ----

void AppGadgets::_draw() {
    switch (_state) {
        case ST_LOADING:    _drawLoading(); break;
        case ST_ERROR:      _drawError();   break;
        case ST_READY:
        case ST_INSTALLING: _drawGadget();  break;
        default:            _drawLoading(); break;
    }
    _needsRedraw = false;
}

void AppGadgets::_drawLoading() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(GDG_BG);

    uint16_t barBg = disp.color565(0, 80, 100);
    disp.fillRect(0, 0, disp.width(), BAR_H, barBg);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, barBg);
    disp.drawString("Gadgets", 4, 3);

    int fetched = _totalFiles - (int)_pendingNames.size();
    if (_totalFiles > 0) {
        char prog[24];
        snprintf(prog, sizeof(prog), "%d/%d", fetched, _totalFiles);
        int pw = disp.textWidth(prog);
        disp.drawString(prog, disp.width() - pw - 4, 3);
    }

    int y = BAR_H + 10;
    disp.setTextColor(disp.color565(180, 220, 255), GDG_BG);
    disp.drawString("Fetching from GitHub...", 4, y); y += 14;
    disp.setTextColor(disp.color565(100, 160, 100), GDG_BG);
    disp.drawString("(requires WiFi)", 4, y); y += 18;
    disp.setTextColor(disp.color565(200, 160, 40), GDG_BG);
    disp.drawString("This can take up to", 4, y); y += 13;
    disp.drawString("a minute. Please wait.", 4, y);
}

void AppGadgets::_drawError() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(GDG_BG);

    uint16_t barBg = disp.color565(0, 80, 100);
    disp.fillRect(0, 0, disp.width(), BAR_H, barBg);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, barBg);
    disp.drawString("Gadgets", 4, 3);

    int y = BAR_H + 8;
    disp.setTextColor(disp.color565(220, 80, 80), GDG_BG);
    disp.drawString("Error:", 4, y); y += 14;
    disp.setTextColor(disp.color565(200, 200, 200), GDG_BG);
    String msg = _errorMsg;
    while (msg.length() > 0) {
        int take = min((int)msg.length(), 34);
        disp.drawString(msg.substring(0, take), 4, y);
        msg = msg.substring(take);
        y += 13;
        if (y > disp.height() - BOT_H - 4) break;
    }

    uint16_t botBg = disp.color565(16, 16, 16);
    disp.fillRect(0, disp.height() - BOT_H, disp.width(), BOT_H, botBg);
    disp.setTextColor(disp.color565(100, 100, 100), botBg);
    disp.drawString("ENTER/R retry  ESC back", 2, disp.height() - BOT_H + 2);
}

void AppGadgets::_drawGadget() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(GDG_BG);

    uint16_t barBg = disp.color565(0, 80, 100);
    disp.fillRect(0, 0, disp.width(), BAR_H, barBg);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, barBg);
    disp.drawString("Gadgets", 4, 3);

    // Show fetched/total — continues updating while remaining files load
    int fetched = _totalFiles > 0 ? _totalFiles - (int)_pendingNames.size() : (int)_gadgets.size();
    char pg[20];
    snprintf(pg, sizeof(pg), "%d/%d", _page + 1, (int)_gadgets.size());
    int pw = disp.textWidth(pg);
    disp.drawString(pg, disp.width() - pw - 4, 3);

    // Show a small "loading more" dot in the bar if still fetching
    if (!_pendingNames.empty()) {
        disp.fillCircle(disp.width() - pw - 14, BAR_H / 2,
                        3, disp.color565(200, 160, 40));
    }

    const Gadget& g = _gadgets[_page];
    int y = BAR_H + 5;

    disp.setTextColor(TFT_YELLOW, GDG_BG);
    String nameStr = g.name;
    if (nameStr.length() > 30) nameStr = nameStr.substring(0, 27) + "...";
    disp.drawString(nameStr, 4, y);
    y += 14;

    disp.setTextColor(disp.color565(180, 220, 255), GDG_BG);
    String desc = g.description;
    int linesLeft = 3;
    while (desc.length() > 0 && linesLeft-- > 0) {
        int take = min((int)desc.length(), 34);
        if (take < (int)desc.length()) {
            int sp = desc.lastIndexOf(' ', take);
            if (sp > 20) take = sp + 1;
        }
        disp.drawString(desc.substring(0, take), 4, y);
        desc = desc.substring(take);
        y += 12;
    }
    y += 2;

    disp.setTextColor(disp.color565(130, 130, 130), GDG_BG);
    String preview = g.content;
    if (preview.length() > 56) preview = preview.substring(0, 53) + "...";
    while (preview.length() > 0 && y < disp.height() - BOT_H - 16) {
        int take = min((int)preview.length(), 34);
        disp.drawString(preview.substring(0, take), 4, y);
        preview = preview.substring(take);
        y += 12;
    }

    if (_installMsg.length() > 0) {
        int iy = disp.height() - BOT_H - 14;
        disp.fillRect(0, iy, disp.width(), 13, GDG_BG);
        disp.setTextColor(_installOk ? TFT_GREEN : disp.color565(220, 80, 80), GDG_BG);
        disp.drawString(_installMsg, 4, iy);
    }

    uint16_t botBg = disp.color565(16, 16, 16);
    disp.fillRect(0, disp.height() - BOT_H, disp.width(), BOT_H, botBg);
    disp.setTextColor(disp.color565(100, 100, 100), botBg);
    disp.drawString("</> page  ENTER install  ESC back", 2, disp.height() - BOT_H + 2);
}

// ---- Lifecycle ----

void AppGadgets::onEnter() {
    _needsRedraw = true;
    _installMsg  = "";
    _installOk   = false;
    if (_gadgets.empty() && _state != ST_LOADING) {
        _state       = ST_LOADING;
        _totalFiles  = 0;
        _page        = 0;
        _needsRedraw = true;
        _draw();
        if (!_fetchDirectory()) return;
        _fetchNext();
    } else if (!_gadgets.empty()) {
        _state = ST_READY;
    }
}

void AppGadgets::onUpdate() {
    // Continue background fetching one file per update cycle while still loading
    if (!_pendingNames.empty()) {
        _fetchNext();
        _needsRedraw = true;
    }

    if (_needsRedraw) _draw();

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (_state == ST_ERROR) {
        if (ki.enter || ki.ch == 'r' || ki.ch == 'R') {
            _gadgets.clear();
            _pendingNames.clear();
            _totalFiles  = 0;
            _page        = 0;
            _state       = ST_LOADING;
            _needsRedraw = true;
            _draw();
            if (_fetchDirectory()) _fetchNext();
        } else if (ki.esc) {
            uiManager.returnToLauncher();
        }
        return;
    }

    if (_state == ST_LOADING || _state == ST_INSTALLING) return;

    if (ki.esc) { uiManager.returnToLauncher(); return; }

    int n = (int)_gadgets.size();
    if (ki.arrowLeft || ki.arrowUp) {
        if (_page > 0) { _page--; _installMsg = ""; _needsRedraw = true; }
    } else if (ki.arrowRight || ki.arrowDown) {
        if (_page < n - 1) { _page++; _installMsg = ""; _needsRedraw = true; }
    } else if (ki.enter) {
        _state = ST_INSTALLING;
        _install();
        _state = ST_READY;
    }
}

} // namespace Cardputer
#endif
