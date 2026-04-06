#include "KeyboardOutputCallbacks.h"
#include "esp_log.h"

static const char* LOG_TAG = "BLEDevice";

void KeyboardOutputCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    const uint8_t* value = pCharacteristic->getValue().data();
    ESP_LOGI(LOG_TAG, "special keys: %d", *value);
}
