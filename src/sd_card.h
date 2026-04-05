#pragma once
#include <SD.h>
#include <SPI.h>

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
