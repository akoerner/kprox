#pragma once
#include <WebSocketsServer.h>

extern WebSocketsServer webSocket;

void handleSendMouseWebSocket(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
