#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_gadgets.h"
#include "../registers.h"
#include "ui_manager.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

namespace Cardputer {

static constexpr int  GDG_BG        = 0x0841;
static constexpr char GH_HOST[]     = "api.github.com";
static constexpr char GH_LIST[]     = "/repos/akoerner/kprox/contents/gadgets";

// ---- HTTPS GET ----

String AppGadgets::_httpGet(const char* host, const char* path) {
    if (WiFi.status() != WL_CONNECTED) return "";

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(12);
    feedWatchdog();

    if (!client.connect(host, 443)) return "";
    feedWatchdog();

    client.printf(
        "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: KProx/1.0\r\n"
        "Accept: application/json\r\nConnection: close\r\n\r\n",
        path, host);

    // Skip HTTP headers
    unsigned long deadline = millis() + 8000UL;
    while (client.connected() && millis() < deadline) {
        String line = client.readStringUntil('\n');
        feedWatchdog();
        line.trim();
        if (line.length() == 0) break;
    }

    String body;
    body.reserve(2048);
    deadline = millis() + 15000UL;
    while ((client.available() || client.connected()) && millis() < deadline) {
        while (client.available()) {
            body += (char)client.read();
            feedWatchdog();
        }
        if (client.connected()) delay(5);
    }
    client.stop();
    return body;
}

// ---- Phase 1: directory listing — only extract name + type fields ----

bool AppGadgets::_fetchDirectory() {
    auto& disp = M5Cardputer.Display;

    if (WiFi.status() != WL_CONNECTED) {
        _errorMsg = "No WiFi"; _state = ST_ERROR; return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(12);
    feedWatchdog();

    bool connected = false;
    for (int attempt = 0; attempt < 3 && !connected; attempt++) {
        if (attempt > 0) {
            disp.setTextColor(disp.color565(200, 160, 40), GDG_BG);
            char buf[24]; snprintf(buf, sizeof(buf), "Retry %d/3...", attempt + 1);
            disp.drawString(buf, 4, BAR_H + 38);
            delay(1500); feedWatchdog();
        }
        connected = client.connect(GH_HOST, 443);
    }

    if (!connected) {
        _errorMsg = "GitHub unreachable (check WiFi)";
        _state = ST_ERROR; return false;
    }

    client.printf(
        "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: KProx/1.0\r\n"
        "Accept: application/json\r\nConnection: close\r\n\r\n",
        GH_LIST, GH_HOST);
    feedWatchdog();

    // Skip headers
    unsigned long deadline = millis() + 8000UL;
    while (client.connected() && millis() < deadline) {
        String line = client.readStringUntil('\n');
        feedWatchdog();
        line.trim();
        if (line.length() == 0) break;
    }

    // Stream-scan the body for "download_url":"<url>" without buffering it.
    // We keep only a small sliding window — large enough to hold the key + URL.
    const char* KEY    = "\"download_url\":\"";
    const int   KEYLEN = 16;
    const int   WINLEN = 256;   // key (16) + max URL (~230) + closing quote

    String window;
    window.reserve(WINLEN * 2);

    _slots.clear();
    deadline = millis() + 20000UL;

    while ((client.available() || client.connected()) && millis() < deadline) {
        while (client.available()) {
            window += (char)client.read();
            feedWatchdog();

            // Once we have a useful amount, scan and trim
            if ((int)window.length() >= WINLEN) {
                int keyPos = window.indexOf(KEY);
                if (keyPos >= 0) {
                    // Extract the URL value
                    int valStart = keyPos + KEYLEN;
                    int valEnd   = window.indexOf('"', valStart);
                    if (valEnd >= 0) {
                        String url = window.substring(valStart, valEnd);
                        if (url.endsWith(".json")) {
                            int slash = url.lastIndexOf('/');
                            String filename = (slash >= 0) ? url.substring(slash + 1) : url;

                            GadgetSlot slot;
                            slot.filename    = filename;
                            slot.downloadUrl = url;
                            String display = filename;
                            display.replace(".json", "");
                            display.replace("_", " ");
                            if (display.length() > 0)
                                display[0] = toupper((unsigned char)display[0]);
                            slot.gadget.name = display;
                            _slots.push_back(slot);
                        }
                        // Trim consumed portion, keep tail in case key straddles boundary
                        window = window.substring(valEnd + 1);
                    } else {
                        // URL value not yet fully received — keep the window
                    }
                } else {
                    // Key not found — keep only the last (KEYLEN-1) chars in case
                    // the key straddles a window boundary
                    if ((int)window.length() > KEYLEN - 1)
                        window = window.substring(window.length() - (KEYLEN - 1));
                }
            }
        }
        if (client.connected()) delay(5);
    }

    // Scan any remainder in the window
    {
        int keyPos = window.indexOf(KEY);
        while (keyPos >= 0) {
            int valStart = keyPos + KEYLEN;
            int valEnd   = window.indexOf('"', valStart);
            if (valEnd < 0) break;
            String url = window.substring(valStart, valEnd);
            if (url.endsWith(".json")) {
                int slash = url.lastIndexOf('/');
                String filename = (slash >= 0) ? url.substring(slash + 1) : url;
                GadgetSlot slot;
                slot.filename    = filename;
                slot.downloadUrl = url;
                String display = filename;
                display.replace(".json", "");
                display.replace("_", " ");
                if (display.length() > 0)
                    display[0] = toupper((unsigned char)display[0]);
                slot.gadget.name = display;
                _slots.push_back(slot);
            }
            window = window.substring(valEnd + 1);
            keyPos = window.indexOf(KEY);
        }
    }

    client.stop();

    if (_slots.empty()) {
        _errorMsg = "No gadgets found in listing";
        _state = ST_ERROR; return false;
    }
    return true;
}

// ---- Phase 2: fetch one gadget using its stored download_url ----

bool AppGadgets::_fetchSlot(int idx) {
    if (idx < 0 || idx >= (int)_slots.size()) return false;
    GadgetSlot& slot = _slots[idx];
    if (slot.fetched || slot.fetchErr) return true;

    if (slot.downloadUrl.isEmpty()) {
        slot.fetchErr = true;
        return false;
    }

    // Parse host and path from the stored download_url
    // Expected format: https://raw.githubusercontent.com/path/to/file.json
    String url = slot.downloadUrl;
    if (url.startsWith("https://")) url = url.substring(8);
    int firstSlash = url.indexOf('/');
    if (firstSlash < 0) { slot.fetchErr = true; return false; }
    String host = url.substring(0, firstSlash);
    String path = url.substring(firstSlash);

    String raw;
    for (int attempt = 0; attempt < 2 && raw.isEmpty(); attempt++) {
        if (attempt > 0) { delay(600); feedWatchdog(); }
        raw = _httpGet(host.c_str(), path.c_str());
    }

    if (raw.isEmpty()) {
        slot.fetchErr = true;
        return false;
    }

    int objStart = raw.indexOf('{');
    if (objStart > 0) raw = raw.substring(objStart);

    JsonDocument gdoc;
    if (deserializeJson(gdoc, raw)) {
        slot.fetchErr = true;
        return false;
    }
    JsonObject g = gdoc["gadget"];
    if (!g) {
        slot.fetchErr = true;
        return false;
    }

    slot.gadget.name        = g["name"]        | slot.gadget.name;
    slot.gadget.description = g["description"] | "";
    slot.gadget.content     = g["content"]     | "";
    slot.fetched = true;
    return true;
}

// ---- Phase 2: fetch one gadget's content by slot index ----

// ---- Install ----

void AppGadgets::_install() {
    if (_page < 0 || _page >= (int)_slots.size()) return;
    const Gadget& g = _slots[_page].gadget;
    if (g.content.isEmpty()) return;
    addRegister(g.content, g.name);
    _statusMsg = "Installed: " + g.name;
    _statusOk  = true;
}

// ---- Drawing ----

void AppGadgets::_draw() {
    switch (_state) {
        case ST_DIR_LOADING:    _drawDirLoading();    break;
        case ST_GADGET_LOADING: _drawGadgetLoading(); break;
        case ST_ERROR:          _drawError();         break;
        default:                _drawGadget();        break;
    }
    _needsRedraw = false;
}

static void _gdgTopBar(const char* right = nullptr) {
    auto& disp = M5Cardputer.Display;
    uint16_t barBg = disp.color565(0, 80, 100);
    disp.fillRect(0, 0, disp.width(), 18, barBg);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, barBg);
    disp.drawString("Gadgets", 4, 3);
    if (right) {
        int rw = disp.textWidth(right);
        disp.drawString(right, disp.width() - rw - 4, 3);
    }
}

static void _gdgBotBar(const char* hint) {
    auto& disp = M5Cardputer.Display;
    uint16_t botBg = disp.color565(16, 16, 16);
    disp.fillRect(0, disp.height() - 14, disp.width(), 14, botBg);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(100, 100, 100), botBg);
    disp.drawString(hint, 2, disp.height() - 13);
}

void AppGadgets::_drawDirLoading() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(GDG_BG);
    _gdgTopBar();
    int y = BAR_H + 10;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(180, 220, 255), GDG_BG);
    disp.drawString("Fetching gadget list...", 4, y); y += 14;
    disp.setTextColor(disp.color565(100, 160, 100), GDG_BG);
    disp.drawString("(requires WiFi)", 4, y);
    _gdgBotBar("ESC back");
}

void AppGadgets::_drawGadgetLoading() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(GDG_BG);

    char pg[16];
    snprintf(pg, sizeof(pg), "%d/%d", _page + 1, (int)_slots.size());
    _gdgTopBar(pg);

    int y = BAR_H + 6;
    disp.setTextSize(1);

    // Show derived name while loading
    disp.setTextColor(TFT_YELLOW, GDG_BG);
    disp.drawString(_slots[_page].gadget.name, 4, y); y += 16;

    disp.setTextColor(disp.color565(180, 220, 255), GDG_BG);
    disp.drawString("Loading...", 4, y);

    _gdgBotBar("ESC back");
}

void AppGadgets::_drawGadget() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(GDG_BG);

    if (_slots.empty()) {
        _gdgTopBar();
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(180, 180, 180), GDG_BG);
        disp.drawString("No gadgets loaded.", 4, BAR_H + 20);
        _gdgBotBar("R=refresh  ESC back");
        return;
    }

    char pg[16];
    snprintf(pg, sizeof(pg), "%d/%d", _page + 1, (int)_slots.size());
    _gdgTopBar(pg);

    const GadgetSlot& slot = _slots[_page];
    int y = BAR_H + 5;
    disp.setTextSize(1);

    if (slot.fetchErr) {
        disp.setTextColor(disp.color565(220, 80, 80), GDG_BG);
        disp.drawString("Failed to load gadget.", 4, y); y += 14;
        disp.setTextColor(disp.color565(160, 160, 160), GDG_BG);
        disp.drawString(slot.filename, 4, y);
        _gdgBotBar("</> page  R retry  ESC back");
        return;
    }

    const Gadget& g = slot.gadget;

    disp.setTextColor(TFT_YELLOW, GDG_BG);
    String nameStr = g.name;
    if ((int)nameStr.length() > 30) nameStr = nameStr.substring(0, 27) + "...";
    disp.drawString(nameStr, 4, y); y += 14;

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

    disp.setTextColor(disp.color565(120, 120, 120), GDG_BG);
    String preview = g.content;
    if ((int)preview.length() > 56) preview = preview.substring(0, 53) + "...";
    while (preview.length() > 0 && y < disp.height() - BOT_H - 16) {
        int take = min((int)preview.length(), 34);
        disp.drawString(preview.substring(0, take), 4, y);
        preview = preview.substring(take);
        y += 12;
    }

    if (_statusMsg.length() > 0) {
        int iy = disp.height() - BOT_H - 14;
        disp.fillRect(0, iy, disp.width(), 13, GDG_BG);
        disp.setTextColor(_statusOk ? TFT_GREEN : disp.color565(220, 80, 80), GDG_BG);
        disp.drawString(_statusMsg, 4, iy);
    }

    _gdgBotBar("</> page  ENTER install  P play  R refresh  ESC");
}

void AppGadgets::_drawError() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(GDG_BG);
    _gdgTopBar();
    int y = BAR_H + 8;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(220, 80, 80), GDG_BG);
    disp.drawString("Error:", 4, y); y += 14;
    disp.setTextColor(disp.color565(200, 200, 200), GDG_BG);
    String msg = _errorMsg;
    while (msg.length() > 0 && y < disp.height() - BOT_H - 4) {
        int take = min((int)msg.length(), 34);
        disp.drawString(msg.substring(0, take), 4, y);
        msg = msg.substring(take);
        y += 13;
    }
    _gdgBotBar("ENTER/R retry  ESC back");
}

// ---- Lifecycle ----

void AppGadgets::onEnter() {
    _statusMsg   = "";
    _statusOk    = false;
    _needsRedraw = true;

    if (!_slots.empty()) {
        _state = ST_READY;
        return;
    }

    _state = ST_DIR_LOADING;
    _draw();

    if (!_fetchDirectory()) {
        _needsRedraw = true;
        return;
    }

    // Immediately fetch the first gadget so there's something to show
    _page  = 0;
    _state = ST_GADGET_LOADING;
    _needsRedraw = true;
    _draw();
    _fetchSlot(0);
    _state       = ST_READY;
    _needsRedraw = true;
}

void AppGadgets::onUpdate() {
    // ESC is always checked first — never blocked by loading state
    KeyInput ki = pollKeys();
    if (ki.esc) {
        uiManager.returnToLauncher();
        return;
    }

    // If we need to fetch the current slot, do it now (one at a time, on demand)
    if (_state == ST_READY && !_slots.empty()) {
        GadgetSlot& slot = _slots[_page];
        if (!slot.fetched && !slot.fetchErr) {
            _state       = ST_GADGET_LOADING;
            _needsRedraw = true;
            _draw();
            _fetchSlot(_page);
            _state       = ST_READY;
            _needsRedraw = true;
        }
    }

    if (_needsRedraw) _draw();

    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (_state == ST_ERROR) {
        if (ki.enter || ki.ch == 'r' || ki.ch == 'R') {
            _slots.clear();
            _page        = 0;
            _state       = ST_DIR_LOADING;
            _needsRedraw = true;
            _draw();
            if (_fetchDirectory()) {
                _state       = ST_GADGET_LOADING;
                _needsRedraw = true;
                _draw();
                _fetchSlot(0);
                _state = ST_READY;
            }
            _needsRedraw = true;
        }
        return;
    }

    if (_state != ST_READY) return;

    int n = (int)_slots.size();
    if (n == 0) return;

    if (ki.arrowLeft || ki.arrowUp) {
        if (_page > 0) { _page--; _statusMsg = ""; _needsRedraw = true; }
    } else if (ki.arrowRight || ki.arrowDown) {
        if (_page < n - 1) { _page++; _statusMsg = ""; _needsRedraw = true; }
    } else if (ki.enter) {
        _install();
        _needsRedraw = true;
    } else if (ki.ch == 'p' || ki.ch == 'P') {
        const GadgetSlot& slot = _slots[_page];
        if (slot.fetched && !slot.gadget.content.isEmpty())
            pendingTokenStrings.push_back(slot.gadget.content);
    } else if (ki.ch == 'r' || ki.ch == 'R') {
        // Force re-fetch current slot
        _slots[_page].fetched   = false;
        _slots[_page].fetchErr  = false;
        _statusMsg = "";
        _needsRedraw = true;
    }
}

} // namespace Cardputer
#endif
