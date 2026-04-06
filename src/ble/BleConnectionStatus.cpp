#include "BleConnectionStatus.h"
#include <NimBLEDevice.h>

// NimBLE handles CCCD (notification subscription) via client writes automatically.
// We just track connection state and restart advertising on disconnect.

void BleConnectionStatus::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    connected = true;
}

void BleConnectionStatus::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    connected = false;
    // Restart advertising — advertising data was already set in begin(), just restart.
    NimBLEDevice::startAdvertising();
}
