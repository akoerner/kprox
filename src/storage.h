#pragma once

#include "globals.h"

void saveWiFiSettings();
void loadWiFiSettings();

void saveWifiEnabledSettings();
void loadWifiEnabledSettings();

void saveBtSettings();
void loadBtSettings();

void saveUSBSettings();
void loadUSBSettings();

void saveUSBIdentitySettings();
void loadUSBIdentitySettings();

void saveApiKeySettings();
void loadApiKeySettings();

void saveUtcOffsetSettings();
void loadUtcOffsetSettings();

void saveLEDSettings();
void loadLEDSettings();

void saveKeymapSettings();
void loadKeymapSettings();

void saveSinkSettings();
void loadSinkSettings();

void saveTimingSettings();
void loadTimingSettings();

void saveHostnameSettings();
void loadHostnameSettings();

void saveDefaultAppSettings();
void loadDefaultAppSettings();

void saveAppLayout();
void saveCsSecuritySettings();
void loadCsSecuritySettings();
void saveCsStorageLocation();
void loadCsStorageLocation();
void loadAppLayout(int numApps);

void wipeAllSettings();
