#include "ws_mouse.h"
#include "hid.h"

WebSocketsServer webSocket(81);

void handleSendMouseWebSocket(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (type != WStype_TEXT) return;

    JsonDocument doc;
    deserializeJson(doc, payload, length);

    if (!doc["x"].isNull() || !doc["y"].isNull()) {
        int dx = doc["x"] | 0, dy = doc["y"] | 0;
        if (dx || dy) sendMouseMovement(dx, dy);
    }

    if (!doc["action"].isNull()) {
        String action = doc["action"].as<String>();
        int button    = doc["button"] | MOUSE_LEFT;
        if      (action == "click")   sendMouseClick(button);
        else if (action == "double")  sendMouseDoubleClick(button);
        else if (action == "press")   sendMousePress(button);
        else if (action == "release") sendMouseRelease(button);
    }
}
