#pragma once

#include "globals.h"
#include <WebSocketsServer.h>

void setupRoutes();

void handleOptions();
void handleNotFound();
void handleFileRead(String path);
void handleNonce();
void handleApiStatus();
void handleRegisters();
void handleSettings();
void handleSendText();
void handleSendMouse();
void handleSendMouseWebSocket(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void handleSink();
void handleFlush();
void handleSinkSize();
void handleSinkDelete();
void handleBluetooth();
void handleUSB();
void handleDevice();
void handleLED();
void handleWiFi();
void handleWipeSettings();
void handleWipeEverything();
void handleRegistersExport();
void handleRegistersImport();
void handleOTAUpload();
void handleOTAComplete();
void handleApiDiscovery();
void handleApiNetwork();
void handleMTLS();
void handleMTLSCerts();
void handleDocs();
void handleKpsRef();
void handleSdApi();
void handleKpsExec();
void handleKeymap();
void handleCredStore();
void handleCredStoreKey();
