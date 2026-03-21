#pragma once
#include "globals.h"
#include <map>

// Execute a KPS script string with an existing variable scope.
void kpsExec(const String& script, std::map<String, String>& vars);

// Execute a KPS script with a fresh variable scope.
void kpsExec(const String& script);

// Execute a KPS script file from the SD card.
void kpsExecFile(const String& path, std::map<String, String>& vars);
void kpsExecFile(const String& path);
