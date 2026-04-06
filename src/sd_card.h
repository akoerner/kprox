#pragma once
#include <SD.h>
#include <SPI.h>

// SPI is initialised with ss=-1 so the hardware does not manage CS.
// The SD library drives CS_PIN as a plain GPIO, preventing DMA conflicts
// with the WiFi/TCP stack on ESP32-S3.
void   sdInit();
bool   sdMount();
void   sdUnmount();
bool   sdAvailable();

bool   sdMkdirP(const String& filePath);
String sdReadFile(const String& path);
bool   sdWriteFile(const String& path, const String& content);
bool   sdAppendFile(const String& path, const String& content);
bool   sdDeleteFile(const String& path);
bool   sdMkdir(const String& path);
String sdListDir(const String& path);
String sdLsText(const String& path);
