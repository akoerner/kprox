#pragma once
#include "globals.h"

// Recursively create directories for the given file path (like mkdir -p on the parent).
bool sdMkdirP(const String& filePath);

// Read entire file from SD card. Returns "" if unavailable or missing.
String sdReadFile(const String& path);

// Write (create/overwrite) file on SD card. Creates parent dirs as needed.
bool sdWriteFile(const String& path, const String& content);

// Append to file on SD card. Creates file and parent dirs if they don't exist.
bool sdAppendFile(const String& path, const String& content);

// Delete a file from the SD card.
bool sdDeleteFile(const String& path);

// Create a directory (and parents) on the SD card.
bool sdMkdir(const String& path);

// List directory contents as a JSON array string.
// Each element: {"name":"foo","type":"file","size":1234} or {"name":"bar","type":"dir"}
String sdListDir(const String& path);
