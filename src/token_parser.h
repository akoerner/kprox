#pragma once

#include "globals.h"
#include <map>

void putTokenString(const String& text);
void parseAndSendText(const String& text);
void parseAndSendText(const String& text, std::map<String, String>& vars);

// Resolve inline tokens ({CREDSTORE}, {MATH}, {TOTP}, variables, etc.) in a string.
// Called by kps_parser to evaluate expressions.
String evaluateAllTokens(const String& text, std::map<String, String>& vars);

// Check for user-initiated abort (ESC / BtnA). Called by kps_parser sleep loops.
void checkParseInterrupt();

// Resolve a register name or 1-based index to a 0-based index (-1 = no match).
int resolveRegisterArg(const String& arg);
