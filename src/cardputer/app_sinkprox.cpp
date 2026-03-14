#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_sinkprox.h"
#include "../hid.h"
#include "ui_manager.h"
#include <SPIFFS.h>
#include <WiFi.h>

namespace Cardputer {

static constexpr uint16_t SK_BG    = 0x0410;
static constexpr int      SK_BAR_H = 18;
static constexpr int      SK_BOT_H = 14;

void AppSinkProx::_pollSize() {
    if (SPIFFS.exists(SINK_FILE)) {
        File f = SPIFFS.open(SINK_FILE, "r");
        if (f) { _sinkSize = f.size(); f.close(); }
        else     _sinkSize = 0;
    } else {
        _sinkSize = 0;
    }
}

void AppSinkProx::_doFlush() {
    if (!SPIFFS.exists(SINK_FILE)) {
        _statusMsg = "Sink is empty"; _statusOk = false; _needsRedraw = true; return;
    }
    File f = SPIFFS.open(SINK_FILE, "r");
    if (!f) { _statusMsg = "Open failed"; _statusOk = false; _needsRedraw = true; return; }
    String content = f.readString();
    f.close();
    SPIFFS.remove(SINK_FILE);
    _sinkSize = 0;
    if (!content.isEmpty()) {
        pendingTokenStrings.push_back(content);
        _statusMsg = "Flushed " + String(content.length()) + " bytes";
        _statusOk  = true;
    } else {
        _statusMsg = "Sink was empty"; _statusOk = false;
    }
    _needsRedraw = true;
}

void AppSinkProx::_doDelete() {
    if (SPIFFS.exists(SINK_FILE)) SPIFFS.remove(SINK_FILE);
    _sinkSize  = 0;
    _statusMsg = "Sink deleted";
    _statusOk  = false;
    _confirmDelete = false;
    _needsRedraw   = true;
}

void AppSinkProx::_draw() {
    auto& disp = M5Cardputer.Display;
    const int W = disp.width();
    const int H = disp.height();

    disp.fillScreen(SK_BG);

    // Header
    uint16_t barBg = disp.color565(0, 80, 80);
    disp.fillRect(0, 0, W, SK_BAR_H, barBg);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, barBg);
    disp.drawString("SinkProx", 4, 3);

    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        int iw = disp.textWidth(ip);
        disp.setTextColor(disp.color565(160, 255, 200), barBg);
        disp.drawString(ip, W - iw - 4, 3);
    } else {
        disp.setTextColor(disp.color565(200, 80, 80), barBg);
        disp.drawString("No WiFi", W - disp.textWidth("No WiFi") - 4, 3);
    }

    int y = SK_BAR_H + 6;

    // Confirm delete overlay
    if (_confirmDelete) {
        disp.setTextColor(disp.color565(255, 80, 80), SK_BG);
        disp.drawString("Delete sink buffer?", 4, y); y += 16;
        disp.setTextColor(TFT_WHITE, SK_BG);
        disp.drawString("Y = confirm  N / ESC = cancel", 4, y);
        uint16_t botBg = disp.color565(16, 16, 16);
        disp.fillRect(0, H - SK_BOT_H, W, SK_BOT_H, botBg);
        disp.setTextColor(disp.color565(180, 80, 80), botBg);
        disp.drawString("Y delete  N/ESC cancel", 2, H - SK_BOT_H + 2);
        _needsRedraw = false;
        return;
    }

    // Sink size badge
    uint16_t sizeBg = (_sinkSize > 0) ? disp.color565(0,80,60) : disp.color565(30,30,30);
    char sizeBuf[32];
    snprintf(sizeBuf, sizeof(sizeBuf), "SINK: %u bytes", (unsigned)_sinkSize);
    int bw = disp.textWidth(sizeBuf) + 10;
    disp.fillRoundRect(4, y, bw, 14, 3, sizeBg);
    disp.setTextColor(_sinkSize > 0 ? disp.color565(100,255,180) : disp.color565(120,120,120), sizeBg);
    disp.drawString(sizeBuf, 9, y + 2); y += 20;

    // Endpoints
    String baseIP   = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "<ip>";
    String baseMdns = String(hostname) + ".local";

    disp.setTextColor(disp.color565(100, 200, 200), SK_BG);
    disp.drawString("Endpoints:", 4, y); y += 13;
    disp.setTextColor(disp.color565(180, 220, 180), SK_BG);
    disp.drawString("POST http://" + baseIP + "/api/sink", 4, y); y += 12;
    disp.drawString("POST http://" + baseMdns + "/api/sink", 4, y); y += 15;

    // curl tip — small font
    disp.setTextColor(disp.color565(100, 160, 120), SK_BG);
    disp.setTextSize(1);
    disp.drawString("curl example:", 4, y); y += 12;
    disp.setTextSize(0);
    disp.setTextColor(disp.color565(140, 190, 140), SK_BG);
    disp.drawString(("curl -X POST http://" + baseIP + "/api/sink").c_str(), 4, y); y += 9;
    disp.drawString("-H \"X-Auth:<hmac>\" -d '{\"text\":\"hello\"}'", 4, y); y += 11;
    disp.setTextSize(1);

    // Status message
    if (_statusMsg.length() > 0) {
        disp.fillRect(0, H - SK_BOT_H - 14, W, 13, SK_BG);
        disp.setTextColor(_statusOk ? TFT_GREEN : disp.color565(220,80,80), SK_BG);
        disp.drawString(_statusMsg, 4, H - SK_BOT_H - 13);
    }

    // Bottom bar
    uint16_t botBg = disp.color565(16, 16, 16);
    disp.fillRect(0, H - SK_BOT_H, W, SK_BOT_H, botBg);
    disp.setTextColor(disp.color565(100, 140, 100), botBg);
    disp.drawString("ENT/G0 flush  D delete  ESC back", 2, H - SK_BOT_H + 2);

    _needsRedraw = false;
}

void AppSinkProx::_checkBtnA() {
    unsigned long now = millis();
    auto& btn = M5Cardputer.BtnA;

    if (btn.wasPressed()) { _lastBtnPress = now; _haltTriggered = false; uiManager.notifyInteraction(); }
    if (g_btnAHaltedPlayback) { _skipRelease = true; g_btnAHaltedPlayback = false; }
    if (btn.wasReleased()) {
        if (_skipRelease) { _skipRelease = false; }
        else { _lastBtnRelease = now; if (!_haltTriggered) _btnCount++; }
    }
    if (btn.isPressed() && !_haltTriggered && (now - _lastBtnPress >= 2000)) {
        _haltTriggered = true; _btnCount = 0;
        isHalted ? resumeOperations() : haltAllOperations();
        _needsRedraw = true;
    }
    if (_btnCount > 0 && (now - _lastBtnRelease > DBL_MS)) {
        if (_btnCount == 1 && !_confirmDelete) _doFlush();
        _btnCount = 0;
    }
}

void AppSinkProx::onEnter() {
    _statusMsg     = "";
    _confirmDelete = false;
    _needsRedraw   = true;
    _lastPoll      = 0;
    _pollSize();
}

void AppSinkProx::onUpdate() {
    _checkBtnA();

    unsigned long now = millis();
    if (now - _lastPoll >= POLL_MS) {
        _lastPoll = now;
        size_t prev = _sinkSize;
        _pollSize();
        if (_sinkSize != prev) _needsRedraw = true;
    }

    if (_needsRedraw) _draw();

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    // Confirmation dialog
    if (_confirmDelete) {
        if (ki.ch == 'y' || ki.ch == 'Y') { _doDelete(); }
        else { _confirmDelete = false; _needsRedraw = true; }
        return;
    }

    if (ki.esc) { uiManager.returnToLauncher(); return; }
    if (ki.enter) { _doFlush(); return; }
    if (ki.ch == 'd' || ki.ch == 'D') {
        _confirmDelete = true; _needsRedraw = true; return;
    }
}

} // namespace Cardputer
#endif
