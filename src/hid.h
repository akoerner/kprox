#pragma once

#include "globals.h"
#include <vector>

bool isBLEConnected();
bool isUSBConnected();
bool hasAnyConnection();

void hidReleaseAll();
void hidPressRaw(uint8_t hidUsage, uint8_t modifiers);
void hidReleaseRaw();

void sendPlainText(const String& text);
void sendSpecialKey(uint8_t keycode);
void sendSpecialKeyTimed(uint8_t keycode, int holdMs);
void pressKey(uint8_t keycode);
void releaseKey(uint8_t keycode);
void sendKeyChord(const std::vector<uint8_t>& keycodes, uint8_t modifiers = 0);
void processChord(const String& chordStr);

void sendConsumerKey(const MediaKeyReport key);
void sendSystemKey(SystemKeyReport key);

void setMousePosition(int x, int y);
void sendMouseMovement(int deltaX, int deltaY);
void sendMouseClick(int button);
void sendMousePress(int button);
void sendMouseRelease(int button);
void sendMouseDoubleClick(int button);

void sendBatchedMouseMovement();
void accumulateMouseMovement(int16_t deltaX, int16_t deltaY);

void haltAllOperations();
void resumeOperations();
