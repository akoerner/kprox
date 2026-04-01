let textLoopInterval = null;
let isTextLooping = false;
let isConnected = false;
let requestInProgress = false;
let currentActiveRegister = -1;
let registers = [];
let numRegisters = 0;

let bluetoothEnabled = true;
let bluetoothInitialized = false;
let bluetoothConnected = false;

let deviceLoopActive = false;
let deviceLoopingRegister = -1;

let ledEnabled = true;
let ledColorR = 0;
let ledColorG = 255;
let ledColorB = 0;

let trackpadElement = null;
let isTracking = false;
let lastTrackpadPosition = {x: 0, y: 0};
let trackpadPendingDx = 0;
let trackpadPendingDy = 0;
let ipAddress = null;
let deviceHostname = null;
let csGateMode = 0; // 0=key-only, 1=key+TOTP, 2=TOTP-only

// Mouse movement pipeline -- separate from the main API queue to avoid latency from
// nonce serialisation competing with keyboard/register requests.
let _mouseNonce = null;
let _mouseNonceFetching = false;
let _mouseSendInFlight = false;
let _mouseIntervalId = null;
let MOUSE_HZ = 15; // 15 Hz provides usable movement without noticeable lag.

let apiKey = 'kprox1337';

const keyReference = [
    // Control Characters
    {char: '', desc: 'Null', ascii: 0, hid: 0x00, shift: false, token: ''},
    {char: '', desc: 'Tab', ascii: 9, hid: 0x2B, shift: false, token: '{TAB}'},
    {char: '', desc: 'Enter', ascii: 13, hid: 0x28, shift: false, token: '{ENTER}'},
    {char: '', desc: 'Escape', ascii: 27, hid: 0x29, shift: false, token: '{ESC}'},
    
    // Printable Characters
    {char: ' ', desc: 'Space', ascii: 32, hid: 0x2C, shift: false, token: '{SPACE}'},
    {char: '!', desc: 'Exclamation Mark', ascii: 33, hid: 0x1E, shift: true, token: ''},
    {char: '"', desc: 'Quotation Mark', ascii: 34, hid: 0x34, shift: true, token: ''},
    {char: '#', desc: 'Hash/Pound', ascii: 35, hid: 0x20, shift: true, token: ''},
    {char: '$', desc: 'Dollar Sign', ascii: 36, hid: 0x21, shift: true, token: ''},
    {char: '%', desc: 'Percent Sign', ascii: 37, hid: 0x22, shift: true, token: ''},
    {char: '&', desc: 'Ampersand', ascii: 38, hid: 0x24, shift: true, token: ''},
    {char: "'", desc: 'Apostrophe', ascii: 39, hid: 0x34, shift: false, token: ''},
    {char: '(', desc: 'Left Parenthesis', ascii: 40, hid: 0x26, shift: true, token: ''},
    {char: ')', desc: 'Right Parenthesis', ascii: 41, hid: 0x27, shift: true, token: ''},
    {char: '*', desc: 'Asterisk', ascii: 42, hid: 0x25, shift: true, token: ''},
    {char: '+', desc: 'Plus Sign', ascii: 43, hid: 0x2E, shift: true, token: ''},
    {char: ',', desc: 'Comma', ascii: 44, hid: 0x36, shift: false, token: ''},
    {char: '-', desc: 'Hyphen/Minus', ascii: 45, hid: 0x2D, shift: false, token: ''},
    {char: '.', desc: 'Period', ascii: 46, hid: 0x37, shift: false, token: ''},
    {char: '/', desc: 'Forward Slash', ascii: 47, hid: 0x38, shift: false, token: ''},
    
    // Numbers
    {char: '0', desc: 'Zero', ascii: 48, hid: 0x27, shift: false, token: ''},
    {char: '1', desc: 'One', ascii: 49, hid: 0x1E, shift: false, token: ''},
    {char: '2', desc: 'Two', ascii: 50, hid: 0x1F, shift: false, token: ''},
    {char: '3', desc: 'Three', ascii: 51, hid: 0x20, shift: false, token: ''},
    {char: '4', desc: 'Four', ascii: 52, hid: 0x21, shift: false, token: ''},
    {char: '5', desc: 'Five', ascii: 53, hid: 0x22, shift: false, token: ''},
    {char: '6', desc: 'Six', ascii: 54, hid: 0x23, shift: false, token: ''},
    {char: '7', desc: 'Seven', ascii: 55, hid: 0x24, shift: false, token: ''},
    {char: '8', desc: 'Eight', ascii: 56, hid: 0x25, shift: false, token: ''},
    {char: '9', desc: 'Nine', ascii: 57, hid: 0x26, shift: false, token: ''},
    
    // Symbols continued
    {char: ':', desc: 'Colon', ascii: 58, hid: 0x33, shift: true, token: ''},
    {char: ';', desc: 'Semicolon', ascii: 59, hid: 0x33, shift: false, token: ''},
    {char: '<', desc: 'Less Than', ascii: 60, hid: 0x36, shift: true, token: ''},
    {char: '=', desc: 'Equal Sign', ascii: 61, hid: 0x2E, shift: false, token: ''},
    {char: '>', desc: 'Greater Than', ascii: 62, hid: 0x37, shift: true, token: ''},
    {char: '?', desc: 'Question Mark', ascii: 63, hid: 0x38, shift: true, token: ''},
    {char: '@', desc: 'At Sign', ascii: 64, hid: 0x1F, shift: true, token: ''},
    
    // Uppercase Letters
    {char: 'A', desc: 'Letter A (uppercase)', ascii: 65, hid: 0x04, shift: true, token: ''},
    {char: 'B', desc: 'Letter B (uppercase)', ascii: 66, hid: 0x05, shift: true, token: ''},
    {char: 'C', desc: 'Letter C (uppercase)', ascii: 67, hid: 0x06, shift: true, token: ''},
    {char: 'D', desc: 'Letter D (uppercase)', ascii: 68, hid: 0x07, shift: true, token: ''},
    {char: 'E', desc: 'Letter E (uppercase)', ascii: 69, hid: 0x08, shift: true, token: ''},
    {char: 'F', desc: 'Letter F (uppercase)', ascii: 70, hid: 0x09, shift: true, token: ''},
    {char: 'G', desc: 'Letter G (uppercase)', ascii: 71, hid: 0x0A, shift: true, token: ''},
    {char: 'H', desc: 'Letter H (uppercase)', ascii: 72, hid: 0x0B, shift: true, token: ''},
    {char: 'I', desc: 'Letter I (uppercase)', ascii: 73, hid: 0x0C, shift: true, token: ''},
    {char: 'J', desc: 'Letter J (uppercase)', ascii: 74, hid: 0x0D, shift: true, token: ''},
    {char: 'K', desc: 'Letter K (uppercase)', ascii: 75, hid: 0x0E, shift: true, token: ''},
    {char: 'L', desc: 'Letter L (uppercase)', ascii: 76, hid: 0x0F, shift: true, token: ''},
    {char: 'M', desc: 'Letter M (uppercase)', ascii: 77, hid: 0x10, shift: true, token: ''},
    {char: 'N', desc: 'Letter N (uppercase)', ascii: 78, hid: 0x11, shift: true, token: ''},
    {char: 'O', desc: 'Letter O (uppercase)', ascii: 79, hid: 0x12, shift: true, token: ''},
    {char: 'P', desc: 'Letter P (uppercase)', ascii: 80, hid: 0x13, shift: true, token: ''},
    {char: 'Q', desc: 'Letter Q (uppercase)', ascii: 81, hid: 0x14, shift: true, token: ''},
    {char: 'R', desc: 'Letter R (uppercase)', ascii: 82, hid: 0x15, shift: true, token: ''},
    {char: 'S', desc: 'Letter S (uppercase)', ascii: 83, hid: 0x16, shift: true, token: ''},
    {char: 'T', desc: 'Letter T (uppercase)', ascii: 84, hid: 0x17, shift: true, token: ''},
    {char: 'U', desc: 'Letter U (uppercase)', ascii: 85, hid: 0x18, shift: true, token: ''},
    {char: 'V', desc: 'Letter V (uppercase)', ascii: 86, hid: 0x19, shift: true, token: ''},
    {char: 'W', desc: 'Letter W (uppercase)', ascii: 87, hid: 0x1A, shift: true, token: ''},
    {char: 'X', desc: 'Letter X (uppercase)', ascii: 88, hid: 0x1B, shift: true, token: ''},
    {char: 'Y', desc: 'Letter Y (uppercase)', ascii: 89, hid: 0x1C, shift: true, token: ''},
    {char: 'Z', desc: 'Letter Z (uppercase)', ascii: 90, hid: 0x1D, shift: true, token: ''},
    
    // More symbols
    {char: '[', desc: 'Left Bracket', ascii: 91, hid: 0x2F, shift: false, token: ''},
    {char: '\\', desc: 'Backslash', ascii: 92, hid: 0x31, shift: false, token: ''},
    {char: ']', desc: 'Right Bracket', ascii: 93, hid: 0x30, shift: false, token: ''},
    {char: '^', desc: 'Caret', ascii: 94, hid: 0x23, shift: true, token: ''},
    {char: '_', desc: 'Underscore', ascii: 95, hid: 0x2D, shift: true, token: ''},
    {char: '`', desc: 'Grave Accent', ascii: 96, hid: 0x35, shift: false, token: ''},
    
    // Lowercase Letters
    {char: 'a', desc: 'Letter A (lowercase)', ascii: 97, hid: 0x04, shift: false, token: ''},
    {char: 'b', desc: 'Letter B (lowercase)', ascii: 98, hid: 0x05, shift: false, token: ''},
    {char: 'c', desc: 'Letter C (lowercase)', ascii: 99, hid: 0x06, shift: false, token: ''},
    {char: 'd', desc: 'Letter D (lowercase)', ascii: 100, hid: 0x07, shift: false, token: ''},
    {char: 'e', desc: 'Letter E (lowercase)', ascii: 101, hid: 0x08, shift: false, token: ''},
    {char: 'f', desc: 'Letter F (lowercase)', ascii: 102, hid: 0x09, shift: false, token: ''},
    {char: 'g', desc: 'Letter G (lowercase)', ascii: 103, hid: 0x0A, shift: false, token: ''},
    {char: 'h', desc: 'Letter H (lowercase)', ascii: 104, hid: 0x0B, shift: false, token: ''},
    {char: 'i', desc: 'Letter I (lowercase)', ascii: 105, hid: 0x0C, shift: false, token: ''},
    {char: 'j', desc: 'Letter J (lowercase)', ascii: 106, hid: 0x0D, shift: false, token: ''},
    {char: 'k', desc: 'Letter K (lowercase)', ascii: 107, hid: 0x0E, shift: false, token: ''},
    {char: 'l', desc: 'Letter L (lowercase)', ascii: 108, hid: 0x0F, shift: false, token: ''},
    {char: 'm', desc: 'Letter M (lowercase)', ascii: 109, hid: 0x10, shift: false, token: ''},
    {char: 'n', desc: 'Letter N (lowercase)', ascii: 110, hid: 0x11, shift: false, token: ''},
    {char: 'o', desc: 'Letter O (lowercase)', ascii: 111, hid: 0x12, shift: false, token: ''},
    {char: 'p', desc: 'Letter P (lowercase)', ascii: 112, hid: 0x13, shift: false, token: ''},
    {char: 'q', desc: 'Letter Q (lowercase)', ascii: 113, hid: 0x14, shift: false, token: ''},
    {char: 'r', desc: 'Letter R (lowercase)', ascii: 114, hid: 0x15, shift: false, token: ''},
    {char: 's', desc: 'Letter S (lowercase)', ascii: 115, hid: 0x16, shift: false, token: ''},
    {char: 't', desc: 'Letter T (lowercase)', ascii: 116, hid: 0x17, shift: false, token: ''},
    {char: 'u', desc: 'Letter U (lowercase)', ascii: 117, hid: 0x18, shift: false, token: ''},
    {char: 'v', desc: 'Letter V (lowercase)', ascii: 118, hid: 0x19, shift: false, token: ''},
    {char: 'w', desc: 'Letter W (lowercase)', ascii: 119, hid: 0x1A, shift: false, token: ''},
    {char: 'x', desc: 'Letter X (lowercase)', ascii: 120, hid: 0x1B, shift: false, token: ''},
    {char: 'y', desc: 'Letter Y (lowercase)', ascii: 121, hid: 0x1C, shift: false, token: ''},
    {char: 'z', desc: 'Letter Z (lowercase)', ascii: 122, hid: 0x1D, shift: false, token: ''},
    
    // Final symbols
    {char: '{', desc: 'Left Brace', ascii: 123, hid: 0x2F, shift: true, token: ''},
    {char: '|', desc: 'Pipe', ascii: 124, hid: 0x31, shift: true, token: ''},
    {char: '}', desc: 'Right Brace', ascii: 125, hid: 0x30, shift: true, token: ''},
    {char: '~', desc: 'Tilde', ascii: 126, hid: 0x35, shift: true, token: ''},
    
    // Special Keys with tokens
    {char: '', desc: 'Backspace', ascii: 8, hid: 0x2A, shift: false, token: '{BACKSPACE}'},
    {char: '', desc: 'Delete', ascii: 127, hid: 0x4C, shift: false, token: '{DELETE}'},
    {char: '', desc: 'Insert', ascii: 0, hid: 0x49, shift: false, token: '{INSERT}'},
    {char: '', desc: 'Home', ascii: 0, hid: 0x4A, shift: false, token: '{HOME}'},
    {char: '', desc: 'End', ascii: 0, hid: 0x4D, shift: false, token: '{END}'},
    {char: '', desc: 'Page Up', ascii: 0, hid: 0x4B, shift: false, token: '{PAGEUP}'},
    {char: '', desc: 'Page Down', ascii: 0, hid: 0x4E, shift: false, token: '{PAGEDOWN}'},
    
    // Arrow Keys
    {char: '', desc: 'Right Arrow', ascii: 0, hid: 0x4F, shift: false, token: '{RIGHT}'},
    {char: '', desc: 'Left Arrow', ascii: 0, hid: 0x50, shift: false, token: '{LEFT}'},
    {char: '', desc: 'Down Arrow', ascii: 0, hid: 0x51, shift: false, token: '{DOWN}'},
    {char: '', desc: 'Up Arrow', ascii: 0, hid: 0x52, shift: false, token: '{UP}'},
    
    // Function Keys
    {char: '', desc: 'F1', ascii: 0, hid: 0x3A, shift: false, token: '{F1}'},
    {char: '', desc: 'F2', ascii: 0, hid: 0x3B, shift: false, token: '{F2}'},
    {char: '', desc: 'F3', ascii: 0, hid: 0x3C, shift: false, token: '{F3}'},
    {char: '', desc: 'F4', ascii: 0, hid: 0x3D, shift: false, token: '{F4}'},
    {char: '', desc: 'F5', ascii: 0, hid: 0x3E, shift: false, token: '{F5}'},
    {char: '', desc: 'F6', ascii: 0, hid: 0x3F, shift: false, token: '{F6}'},
    {char: '', desc: 'F7', ascii: 0, hid: 0x40, shift: false, token: '{F7}'},
    {char: '', desc: 'F8', ascii: 0, hid: 0x41, shift: false, token: '{F8}'},
    {char: '', desc: 'F9', ascii: 0, hid: 0x42, shift: false, token: '{F9}'},
    {char: '', desc: 'F10', ascii: 0, hid: 0x43, shift: false, token: '{F10}'},
    {char: '', desc: 'F11', ascii: 0, hid: 0x44, shift: false, token: '{F11}'},
    {char: '', desc: 'F12', ascii: 0, hid: 0x45, shift: false, token: '{F12}'},
    
    // System Keys
    {char: '', desc: 'Print Screen', ascii: 0, hid: 0x46, shift: false, token: '{PRINTSCREEN}'},
    {char: '', desc: 'System Request', ascii: 0, hid: 0x46, shift: false, token: '{SYSRQ}'},
    {char: '', desc: 'Scroll Lock', ascii: 0, hid: 0x47, shift: false, token: '{SCROLLLOCK}'},
    {char: '', desc: 'Pause', ascii: 0, hid: 0x48, shift: false, token: '{PAUSE}'},
    
    // Keypad Numbers
    {char: '', desc: 'Keypad 0', ascii: 0, hid: 0x62, shift: false, token: '{KP0}'},
    {char: '', desc: 'Keypad 1', ascii: 0, hid: 0x59, shift: false, token: '{KP1}'},
    {char: '', desc: 'Keypad 2', ascii: 0, hid: 0x5A, shift: false, token: '{KP2}'},
    {char: '', desc: 'Keypad 3', ascii: 0, hid: 0x5B, shift: false, token: '{KP3}'},
    {char: '', desc: 'Keypad 4', ascii: 0, hid: 0x5C, shift: false, token: '{KP4}'},
    {char: '', desc: 'Keypad 5', ascii: 0, hid: 0x5D, shift: false, token: '{KP5}'},
    {char: '', desc: 'Keypad 6', ascii: 0, hid: 0x5E, shift: false, token: '{KP6}'},
    {char: '', desc: 'Keypad 7', ascii: 0, hid: 0x5F, shift: false, token: '{KP7}'},
    {char: '', desc: 'Keypad 8', ascii: 0, hid: 0x60, shift: false, token: '{KP8}'},
    {char: '', desc: 'Keypad 9', ascii: 0, hid: 0x61, shift: false, token: '{KP9}'},
    
    // Keypad Operations
    {char: '', desc: 'Keypad Enter', ascii: 0, hid: 0x58, shift: false, token: '{KPENTER}'},
    {char: '', desc: 'Keypad Plus', ascii: 0, hid: 0x57, shift: false, token: '{KPPLUS}'},
    {char: '', desc: 'Keypad Minus', ascii: 0, hid: 0x56, shift: false, token: '{KPMINUS}'},
    {char: '', desc: 'Keypad Multiply', ascii: 0, hid: 0x55, shift: false, token: '{KPMULTIPLY}'},
    {char: '', desc: 'Keypad Divide', ascii: 0, hid: 0x54, shift: false, token: '{KPDIVIDE}'},
    {char: '', desc: 'Keypad Decimal', ascii: 0, hid: 0x63, shift: false, token: '{KPDOT}'},
    {char: '', desc: 'Num Lock', ascii: 0, hid: 0x53, shift: false, token: '{NUMLOCK}'},
    
    // Caps Lock
    {char: '', desc: 'Caps Lock', ascii: 0, hid: 0x39, shift: false, token: '{CAPSLOCK}'},
    
    // Application/Menu Key
    {char: '', desc: 'Application/Menu', ascii: 0, hid: 0x65, shift: false, token: '{APPLICATION}'},
    
    // ==== SYSTEM CONTROL TOKENS ====
    {char: '', desc: 'Enable Bluetooth - Activates BLE HID connectivity', ascii: 0, hid: 0, shift: false, token: '{BLUETOOTH_ENABLE}'},
    {char: '', desc: 'Disable Bluetooth - Deactivates BLE HID connectivity', ascii: 0, hid: 0, shift: false, token: '{BLUETOOTH_DISABLE}'},
    {char: '', desc: 'Enable USB HID - Activates USB HID connectivity', ascii: 0, hid: 0, shift: false, token: '{USB_ENABLE}'},
    {char: '', desc: 'Disable USB HID - Deactivates USB HID connectivity', ascii: 0, hid: 0, shift: false, token: '{USB_DISABLE}'},
    {char: '', desc: 'Halt Operations - Stops all script execution immediately', ascii: 0, hid: 0, shift: false, token: '{HALT}'},
    {char: '', desc: 'Resume Operations - Resumes halted script execution', ascii: 0, hid: 0, shift: false, token: '{RESUME}'},
    {char: '', desc: 'SinkProx Flush - Flushes the sink buffer (/sink.txt) to HID output and clears it', ascii: 0, hid: 0, shift: false, token: '{SINKPROX}'},
    
    // ==== TIMING TOKENS ====
    {char: '', desc: 'Sleep/Delay - Pauses execution for specified milliseconds. Ex: {SLEEP 1000}', ascii: 0, hid: 0, shift: false, token: '{SLEEP ms}'},
    {char: '', desc: 'Variable Sleep - Dynamic delay using expressions. Ex: {SLEEP {RAND 500 2000}}', ascii: 0, hid: 0, shift: false, token: '{SLEEP {expression}}'},
    {char: '', desc: 'Schedule - Wait until wall-clock time (NTP). Ex: {SCHEDULE 14:30} waits until 14:30:00', ascii: 0, hid: 0, shift: false, token: '{SCHEDULE HH:MM}'},
    {char: '', desc: 'Schedule with seconds - Wait until exact time. Ex: {SCHEDULE 09:00:00}', ascii: 0, hid: 0, shift: false, token: '{SCHEDULE HH:MM:SS}'},
    
    // ==== LOOP CONTROL TOKENS ====
    {char: '', desc: 'Infinite Loop Start - Begins endless loop until halted. Ex: {LOOP}text{ENDLOOP}', ascii: 0, hid: 0, shift: false, token: '{LOOP}'},
    {char: '', desc: 'Timed Loop Start - Loop for specified duration in ms. Ex: {LOOP 5000}Hello{ENDLOOP}', ascii: 0, hid: 0, shift: false, token: '{LOOP duration}'},
    {char: '', desc: 'Variable Loop Start (LOOP) - Loop with counter. Ex: {LOOP i 1 1 10}{i}{ENDLOOP}', ascii: 0, hid: 0, shift: false, token: '{LOOP var start increment end}'},
    {char: '', desc: 'Loop End - Marks end of LOOP block', ascii: 0, hid: 0, shift: false, token: '{ENDLOOP}'},
    {char: '', desc: 'FOR Loop - Counted loop with named variable. Ex: {FOR i 1 1 10}{i}{ENTER}{ENDFOR}', ascii: 0, hid: 0, shift: false, token: '{FOR var start increment end}'},
    {char: '', desc: 'FOR Loop End - Marks end of FOR block', ascii: 0, hid: 0, shift: false, token: '{ENDFOR}'},
    {char: '', desc: 'WHILE Loop - Repeats while condition is true. Ex: {WHILE {i} < 10}...{ENDWHILE}', ascii: 0, hid: 0, shift: false, token: '{WHILE left op right}'},
    {char: '', desc: 'WHILE Loop End - Marks end of WHILE block', ascii: 0, hid: 0, shift: false, token: '{ENDWHILE}'},
    {char: '', desc: 'Break - Unconditionally exit the current loop. Ex: {IF {i} == 5}{BREAK}{ENDIF}', ascii: 0, hid: 0, shift: false, token: '{BREAK}'},
    {char: '', desc: 'Conditional Break - Exit loop when variable equals value. Ex: {BREAK i 5}', ascii: 0, hid: 0, shift: false, token: '{BREAK var value}'},

    // ==== VARIABLE TOKENS ====
    {char: '', desc: 'Set Variable - Assign a value to a named variable. Value is evaluated (supports math, rand, other vars). Ex: {SET total 0}', ascii: 0, hid: 0, shift: false, token: '{SET varname expr}'},
    {char: '', desc: 'Set Variable from Math - Ex: {SET total {MATH {total} + {i}}}', ascii: 0, hid: 0, shift: false, token: '{SET varname {MATH expr}}'},
    {char: '', desc: 'Set Variable from Rand - Ex: {SET r {RAND 1 100}}', ascii: 0, hid: 0, shift: false, token: '{SET varname {RAND min max}}'},

    // ==== CONDITIONAL TOKENS ====
    {char: '', desc: 'If Condition - Execute body if condition is true. Operators: ==, !=, <, >, <=, >=. Ex: {IF {i} == 5}...{ENDIF}', ascii: 0, hid: 0, shift: false, token: '{IF left op right}'},
    {char: '', desc: 'If-Else - Execute true or false branch. Ex: {IF {i} < 10}single{ELSE}double{ENDIF}', ascii: 0, hid: 0, shift: false, token: '{IF left op right}...{ELSE}...{ENDIF}'},
    {char: '', desc: 'Else Branch - False branch of an IF block', ascii: 0, hid: 0, shift: false, token: '{ELSE}'},
    {char: '', desc: 'End If - Closes an IF or IF/ELSE block', ascii: 0, hid: 0, shift: false, token: '{ENDIF}'},
    {char: '', desc: 'Nested If - Conditionals can be nested inside loop or if bodies', ascii: 0, hid: 0, shift: false, token: '{IF {i} > 0}{IF {i} < 5}...{ENDIF}{ENDIF}'},
    
    // ==== INPUT CONTROL TOKENS ====
    {char: '', desc: 'Key Chord - Multiple key combination. Ex: {CHORD CTRL+C}', ascii: 0, hid: 0, shift: false, token: '{CHORD combination}'},
    {char: '', desc: 'Ctrl Combinations - Control key combos. Ex: {CHORD CTRL+ALT+DELETE}', ascii: 0, hid: 0, shift: false, token: '{CHORD CTRL+key}'},
    {char: '', desc: 'Alt Combinations - Alt key combos. Ex: {CHORD ALT+TAB}', ascii: 0, hid: 0, shift: false, token: '{CHORD ALT+key}'},
    {char: '', desc: 'Shift Combinations - Shift key combos. Ex: {CHORD SHIFT+F10}', ascii: 0, hid: 0, shift: false, token: '{CHORD SHIFT+key}'},
    {char: '', desc: 'GUI/Windows Combinations - Windows/Cmd key combos. Ex: {CHORD GUI+R}', ascii: 0, hid: 0, shift: false, token: '{CHORD GUI+key}'},
    {char: '', desc: 'Multiple Modifiers - Complex combinations. Ex: {CHORD CTRL+SHIFT+ESC}', ascii: 0, hid: 0, shift: false, token: '{CHORD mod1+mod2+key}'},
    
    // ==== RAW HID CONTROL TOKENS ====
    {char: '', desc: 'Raw HID Keycode - Send direct HID keycode. Ex: {HID 0x04}', ascii: 0, hid: 0, shift: false, token: '{HID keycode}'},
    {char: '', desc: 'HID with Modifiers - Modifiers + keycode. Ex: {HID 0x02 0x04}', ascii: 0, hid: 0, shift: false, token: '{HID modifier keycode}'},
    {char: '', desc: 'HID Multiple Keys - Multiple keycodes with modifier. Ex: {HID 0x02 0x16 0x17 0x18}', ascii: 0, hid: 0, shift: false, token: '{HID mod key1 key2 key3}'},
    {char: '', desc: 'Consumer Controls - Media keys. Ex: {HID 0xE9} (Volume Up)', ascii: 0, hid: 0, shift: false, token: '{HID consumer_code}'},
    
    // ==== MOUSE CONTROL TOKENS ====
    {char: '', desc: 'Set Mouse Position - Absolute positioning. Ex: {SETMOUSE 400 300}', ascii: 0, hid: 0, shift: false, token: '{SETMOUSE x y}'},
    {char: '', desc: 'Move Mouse Relative - Relative movement. Ex: {MOVEMOUSE 10 -5}', ascii: 0, hid: 0, shift: false, token: '{MOVEMOUSE dx dy}'},
    {char: '', desc: 'Mouse Click - Click mouse button. Ex: {MOUSECLICK 1} (left=1, right=2, middle=3)', ascii: 0, hid: 0, shift: false, token: '{MOUSECLICK button}'},
    {char: '', desc: 'Mouse Double Click - Double click. Default left button', ascii: 0, hid: 0, shift: false, token: '{MOUSEDOUBLECLICK}'},
    {char: '', desc: 'Mouse Press - Press and hold button. Ex: {MOUSEPRESS 1}', ascii: 0, hid: 0, shift: false, token: '{MOUSEPRESS button}'},
    {char: '', desc: 'Mouse Release - Release held button. Ex: {MOUSERELEASE 1}', ascii: 0, hid: 0, shift: false, token: '{MOUSERELEASE button}'},
    {char: '', desc: 'Dynamic Mouse Position - Using expressions. Ex: {SETMOUSE {MATH 200 + {i} * 10} 300}', ascii: 0, hid: 0, shift: false, token: '{SETMOUSE {expr} {expr}}'},
    {char: '', desc: 'Random Mouse Movement - Random relative movement. Ex: {MOVEMOUSE {RAND -50 50} {RAND -50 50}}', ascii: 0, hid: 0, shift: false, token: '{MOVEMOUSE {expr} {expr}}'},
    
    // ==== MATHEMATICAL TOKENS ====
    {char: '', desc: 'Math Expression - Basic arithmetic. Ex: {MATH 10 + 5 * 2}', ascii: 0, hid: 0, shift: false, token: '{MATH expression}'},
    {char: '', desc: 'Math with Variables - Using loop variables. Ex: {MATH {i} * 2 + 1}', ascii: 0, hid: 0, shift: false, token: '{MATH {var} operation}'},
    {char: '', desc: 'Sine Function - Trigonometric sine. Ex: {MATH sin(1.57)}', ascii: 0, hid: 0, shift: false, token: '{MATH sin(angle)}'},
    {char: '', desc: 'Cosine Function - Trigonometric cosine. Ex: {MATH cos(0)}', ascii: 0, hid: 0, shift: false, token: '{MATH cos(angle)}'},
    {char: '', desc: 'Nested Math - Complex expressions. Ex: {MATH cos({MATH {i} * 3.14159 / 180})}', ascii: 0, hid: 0, shift: false, token: '{MATH func({MATH expr})}'},
    {char: '', desc: 'Math Constants - PI and E constants. Ex: {MATH PI * 2}', ascii: 0, hid: 0, shift: false, token: '{MATH PI} or {MATH E}'},
    
    // ==== DATA GENERATION TOKENS ====
    {char: '', desc: 'ASCII Character - Send character by ASCII value. Ex: {ASCII 65}', ascii: 0, hid: 0, shift: false, token: '{ASCII decimal}'},
    {char: '', desc: 'ASCII Hex - Send character by hex value. Ex: {ASCII 0x41}', ascii: 0, hid: 0, shift: false, token: '{ASCII 0xhex}'},
    {char: '', desc: 'ASCII with Expression - Dynamic ASCII. Ex: {ASCII {RAND 65 90}}', ascii: 0, hid: 0, shift: false, token: '{ASCII {expression}}'},
    {char: '', desc: 'Random Number - Generate random integer. Ex: {RAND 1 100}', ascii: 0, hid: 0, shift: false, token: '{RAND min max}'},
    {char: '', desc: 'Random with Variables - Dynamic ranges. Ex: {RAND {i} {i} + 10}', ascii: 0, hid: 0, shift: false, token: '{RAND {expr} {expr}}'},
    
    // ==== VARIABLE TOKENS ====
    {char: '', desc: 'Loop Variable i - Access loop counter i. Ex: Item {i}', ascii: 0, hid: 0, shift: false, token: '{i}'},
    {char: '', desc: 'Loop Variable j - Access loop counter j. Ex: Row {j}', ascii: 0, hid: 0, shift: false, token: '{j}'},
    {char: '', desc: 'Custom Loop Variable - Any variable name defined in LOOP. Ex: {counter}', ascii: 0, hid: 0, shift: false, token: '{variable_name}'},
    
    // ==== NETWORK CONTROL TOKENS ====
    {char: '', desc: 'WiFi Connection - Connect to wireless network. Ex: {WIFI MyNetwork MyPassword}', ascii: 0, hid: 0, shift: false, token: '{WIFI ssid password}'},

    // ==== KEYMAP TOKENS ====
    {char: '', desc: 'Switch Keymap - Load a named keyboard layout. Ex: {KEYMAP de} for German', ascii: 0, hid: 0, shift: false, token: '{KEYMAP id}'},

    // ==== CREDENTIAL STORE TOKENS ====
    {char: '', desc: 'Credential Lookup - Substitutes the decrypted value for the named credential. Store must be unlocked; outputs empty string when locked. Ex: {CREDSTORE wifi_pass}', ascii: 0, hid: 0, shift: false, token: '{CREDSTORE label}'},
    
    // ==== COMPLEX EXAMPLE TOKENS ====
    {char: '', desc: 'Circle Drawing - Draw circle with mouse. Complex example using math and loops', ascii: 0, hid: 0, shift: false, token: '{SETMOUSE 400 300}{LOOP i 0 1 360}{SETMOUSE {MATH 400 + 100 * cos({MATH {i} * 3.14159 / 180})} {MATH 300 + 100 * sin({MATH {i} * 3.14159 / 180})}}{SLEEP 10}{ENDLOOP}'},
    {char: '', desc: 'ASCII Table Print - Print ASCII table using FOR loop. Ex: characters 32-126', ascii: 0, hid: 0, shift: false, token: '{FOR i 32 1 126}ASCII {i}: {ASCII {i}}{ENTER}{ENDFOR}'},
    {char: '', desc: 'Mouse Drag - Drag operation. Press, move, release pattern', ascii: 0, hid: 0, shift: false, token: '{MOUSEPRESS 1}{MOVEMOUSE 100 50}{SLEEP 100}{MOUSERELEASE 1}'},
    {char: '', desc: 'Unconditional Break - WHILE loop with early exit via {BREAK}', ascii: 0, hid: 0, shift: false, token: '{SET i 0}{WHILE {i} < 10}{SET i {MATH {i} + 1}}{IF {i} == 5}{BREAK}{ENDIF}Value: {i}{ENTER}{ENDWHILE}'},
    {char: '', desc: 'Nested Expressions - Complex nested token usage', ascii: 0, hid: 0, shift: false, token: '{MOVEMOUSE {RAND -100 100} {RAND -100 100}}{SLEEP {RAND 100 1000}}'},
    {char: '', desc: 'Dynamic Timing - Variable delays in loops', ascii: 0, hid: 0, shift: false, token: '{LOOP i 1 1 5}Step {i}{SLEEP {MATH {i} * 1000}}{ENDLOOP}'},
    {char: '', desc: 'Spiral Pattern - Mathematical spiral drawing', ascii: 0, hid: 0, shift: false, token: '{SETMOUSE 400 300}{LOOP i 0 5 720}{SETMOUSE {MATH 400 + {i}/10 * cos({MATH {i} * 3.14159 / 180})} {MATH 300 + {i}/10 * sin({MATH {i} * 3.14159 / 180})}}{SLEEP 5}{ENDLOOP}'},
    {char: '', desc: 'Magic SysRq REISUB - Emergency Linux system reboot sequence. Safely terminates processes, syncs disks, and reboots', ascii: 0, hid: 0, shift: false, token: '{CHORD ALT+SYSRQ+R}{SLEEP 2000}{CHORD ALT+SYSRQ+E}{SLEEP 2000}{CHORD ALT+SYSRQ+I}{SLEEP 2000}{CHORD ALT+SYSRQ+S}{SLEEP 2000}{CHORD ALT+SYSRQ+U}{SLEEP 2000}{CHORD ALT+SYSRQ+B}'}
];

function safeSetText(elementId, text) {
    const element = document.getElementById(elementId);
    if (element) {
        element.textContent = text;
    }
}

function safeSetClass(elementId, className) {
    const element = document.getElementById(elementId);
    if (element) {
        element.className = className;
    }
}

function setCookie(name, value, days = 30) {
    const expires = new Date();
    expires.setTime(expires.getTime() + (days * 24 * 60 * 60 * 1000));
    document.cookie = `${name}=${value};expires=${expires.toUTCString()};path=/`;
}

function getCookie(name) {
    const nameEQ = name + "=";
    const ca = document.cookie.split(';');
    let i = 0;
    while (i !== ca.length) {
        let c = ca[i];
        while (c.charAt(0) === ' ') c = c.substring(1, c.length);
        if (c.indexOf(nameEQ) === 0) return c.substring(nameEQ.length, c.length);
        i++;
    }
    return null;
}

function getApiEndpoint() {
    const savedEndpoint = getCookie('apiEndpoint');
    if (savedEndpoint) {
        return savedEndpoint;
    }
    
    if (ipAddress) {
        return `http://${ipAddress}`;
    }
    
    return window.location.origin;
}

function getApiKey() {
    // Always read from localStorage as source of truth; in-memory var is cache only
    const stored = localStorage.getItem('apiKey');
    if (stored) apiKey = stored;
    return apiKey;
}


// ---- Crypto (pure JS -- no Web Crypto API, works on plain HTTP) ----

// aes-js: MIT License, Richard Moore
(function(){
/*! MIT License. Copyright 2015-2018 Richard Moore <me@ricmoo.com>. See LICENSE.txt. */
(function(root) {
    "use strict";

    function checkInt(value) {
        return (parseInt(value) === value);
    }

    function checkInts(arrayish) {
        if (!checkInt(arrayish.length)) { return false; }

        for (var i = 0; i < arrayish.length; i++) {
            if (!checkInt(arrayish[i]) || arrayish[i] < 0 || arrayish[i] > 255) {
                return false;
            }
        }

        return true;
    }

    function coerceArray(arg, copy) {

        // ArrayBuffer view
        if (arg.buffer && arg.name === 'Uint8Array') {

            if (copy) {
                if (arg.slice) {
                    arg = arg.slice();
                } else {
                    arg = Array.prototype.slice.call(arg);
                }
            }

            return arg;
        }

        // It's an array; check it is a valid representation of a byte
        if (Array.isArray(arg)) {
            if (!checkInts(arg)) {
                throw new Error('Array contains invalid value: ' + arg);
            }

            return new Uint8Array(arg);
        }

        // Something else, but behaves like an array (maybe a Buffer? Arguments?)
        if (checkInt(arg.length) && checkInts(arg)) {
            return new Uint8Array(arg);
        }

        throw new Error('unsupported array-like object');
    }

    function createArray(length) {
        return new Uint8Array(length);
    }

    function copyArray(sourceArray, targetArray, targetStart, sourceStart, sourceEnd) {
        if (sourceStart != null || sourceEnd != null) {
            if (sourceArray.slice) {
                sourceArray = sourceArray.slice(sourceStart, sourceEnd);
            } else {
                sourceArray = Array.prototype.slice.call(sourceArray, sourceStart, sourceEnd);
            }
        }
        targetArray.set(sourceArray, targetStart);
    }



    var convertUtf8 = (function() {
        function toBytes(text) {
            var result = [], i = 0;
            text = encodeURI(text);
            while (i < text.length) {
                var c = text.charCodeAt(i++);

                // if it is a % sign, encode the following 2 bytes as a hex value
                if (c === 37) {
                    result.push(parseInt(text.substr(i, 2), 16))
                    i += 2;

                // otherwise, just the actual byte
                } else {
                    result.push(c)
                }
            }

            return coerceArray(result);
        }

        function fromBytes(bytes) {
            var result = [], i = 0;

            while (i < bytes.length) {
                var c = bytes[i];

                if (c < 128) {
                    result.push(String.fromCharCode(c));
                    i++;
                } else if (c > 191 && c < 224) {
                    result.push(String.fromCharCode(((c & 0x1f) << 6) | (bytes[i + 1] & 0x3f)));
                    i += 2;
                } else {
                    result.push(String.fromCharCode(((c & 0x0f) << 12) | ((bytes[i + 1] & 0x3f) << 6) | (bytes[i + 2] & 0x3f)));
                    i += 3;
                }
            }

            return result.join('');
        }

        return {
            toBytes: toBytes,
            fromBytes: fromBytes,
        }
    })();

    var convertHex = (function() {
        function toBytes(text) {
            var result = [];
            for (var i = 0; i < text.length; i += 2) {
                result.push(parseInt(text.substr(i, 2), 16));
            }

            return result;
        }

        // http://ixti.net/development/javascript/2011/11/11/base64-encodedecode-of-utf8-in-browser-with-js.html
        var Hex = '0123456789abcdef';

        function fromBytes(bytes) {
                var result = [];
                for (var i = 0; i < bytes.length; i++) {
                    var v = bytes[i];
                    result.push(Hex[(v & 0xf0) >> 4] + Hex[v & 0x0f]);
                }
                return result.join('');
        }

        return {
            toBytes: toBytes,
            fromBytes: fromBytes,
        }
    })();


    // Number of rounds by keysize
    var numberOfRounds = {16: 10, 24: 12, 32: 14}

    // Round constant words
    var rcon = [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91];

    // S-box and Inverse S-box (S is for Substitution)
    var S = [0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16];
    var Si =[0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e, 0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84, 0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, 0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73, 0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4, 0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f, 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, 0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61, 0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d];

    // Transformations for encryption
    var T1 = [0xc66363a5, 0xf87c7c84, 0xee777799, 0xf67b7b8d, 0xfff2f20d, 0xd66b6bbd, 0xde6f6fb1, 0x91c5c554, 0x60303050, 0x02010103, 0xce6767a9, 0x562b2b7d, 0xe7fefe19, 0xb5d7d762, 0x4dababe6, 0xec76769a, 0x8fcaca45, 0x1f82829d, 0x89c9c940, 0xfa7d7d87, 0xeffafa15, 0xb25959eb, 0x8e4747c9, 0xfbf0f00b, 0x41adadec, 0xb3d4d467, 0x5fa2a2fd, 0x45afafea, 0x239c9cbf, 0x53a4a4f7, 0xe4727296, 0x9bc0c05b, 0x75b7b7c2, 0xe1fdfd1c, 0x3d9393ae, 0x4c26266a, 0x6c36365a, 0x7e3f3f41, 0xf5f7f702, 0x83cccc4f, 0x6834345c, 0x51a5a5f4, 0xd1e5e534, 0xf9f1f108, 0xe2717193, 0xabd8d873, 0x62313153, 0x2a15153f, 0x0804040c, 0x95c7c752, 0x46232365, 0x9dc3c35e, 0x30181828, 0x379696a1, 0x0a05050f, 0x2f9a9ab5, 0x0e070709, 0x24121236, 0x1b80809b, 0xdfe2e23d, 0xcdebeb26, 0x4e272769, 0x7fb2b2cd, 0xea75759f, 0x1209091b, 0x1d83839e, 0x582c2c74, 0x341a1a2e, 0x361b1b2d, 0xdc6e6eb2, 0xb45a5aee, 0x5ba0a0fb, 0xa45252f6, 0x763b3b4d, 0xb7d6d661, 0x7db3b3ce, 0x5229297b, 0xdde3e33e, 0x5e2f2f71, 0x13848497, 0xa65353f5, 0xb9d1d168, 0x00000000, 0xc1eded2c, 0x40202060, 0xe3fcfc1f, 0x79b1b1c8, 0xb65b5bed, 0xd46a6abe, 0x8dcbcb46, 0x67bebed9, 0x7239394b, 0x944a4ade, 0x984c4cd4, 0xb05858e8, 0x85cfcf4a, 0xbbd0d06b, 0xc5efef2a, 0x4faaaae5, 0xedfbfb16, 0x864343c5, 0x9a4d4dd7, 0x66333355, 0x11858594, 0x8a4545cf, 0xe9f9f910, 0x04020206, 0xfe7f7f81, 0xa05050f0, 0x783c3c44, 0x259f9fba, 0x4ba8a8e3, 0xa25151f3, 0x5da3a3fe, 0x804040c0, 0x058f8f8a, 0x3f9292ad, 0x219d9dbc, 0x70383848, 0xf1f5f504, 0x63bcbcdf, 0x77b6b6c1, 0xafdada75, 0x42212163, 0x20101030, 0xe5ffff1a, 0xfdf3f30e, 0xbfd2d26d, 0x81cdcd4c, 0x180c0c14, 0x26131335, 0xc3ecec2f, 0xbe5f5fe1, 0x359797a2, 0x884444cc, 0x2e171739, 0x93c4c457, 0x55a7a7f2, 0xfc7e7e82, 0x7a3d3d47, 0xc86464ac, 0xba5d5de7, 0x3219192b, 0xe6737395, 0xc06060a0, 0x19818198, 0x9e4f4fd1, 0xa3dcdc7f, 0x44222266, 0x542a2a7e, 0x3b9090ab, 0x0b888883, 0x8c4646ca, 0xc7eeee29, 0x6bb8b8d3, 0x2814143c, 0xa7dede79, 0xbc5e5ee2, 0x160b0b1d, 0xaddbdb76, 0xdbe0e03b, 0x64323256, 0x743a3a4e, 0x140a0a1e, 0x924949db, 0x0c06060a, 0x4824246c, 0xb85c5ce4, 0x9fc2c25d, 0xbdd3d36e, 0x43acacef, 0xc46262a6, 0x399191a8, 0x319595a4, 0xd3e4e437, 0xf279798b, 0xd5e7e732, 0x8bc8c843, 0x6e373759, 0xda6d6db7, 0x018d8d8c, 0xb1d5d564, 0x9c4e4ed2, 0x49a9a9e0, 0xd86c6cb4, 0xac5656fa, 0xf3f4f407, 0xcfeaea25, 0xca6565af, 0xf47a7a8e, 0x47aeaee9, 0x10080818, 0x6fbabad5, 0xf0787888, 0x4a25256f, 0x5c2e2e72, 0x381c1c24, 0x57a6a6f1, 0x73b4b4c7, 0x97c6c651, 0xcbe8e823, 0xa1dddd7c, 0xe874749c, 0x3e1f1f21, 0x964b4bdd, 0x61bdbddc, 0x0d8b8b86, 0x0f8a8a85, 0xe0707090, 0x7c3e3e42, 0x71b5b5c4, 0xcc6666aa, 0x904848d8, 0x06030305, 0xf7f6f601, 0x1c0e0e12, 0xc26161a3, 0x6a35355f, 0xae5757f9, 0x69b9b9d0, 0x17868691, 0x99c1c158, 0x3a1d1d27, 0x279e9eb9, 0xd9e1e138, 0xebf8f813, 0x2b9898b3, 0x22111133, 0xd26969bb, 0xa9d9d970, 0x078e8e89, 0x339494a7, 0x2d9b9bb6, 0x3c1e1e22, 0x15878792, 0xc9e9e920, 0x87cece49, 0xaa5555ff, 0x50282878, 0xa5dfdf7a, 0x038c8c8f, 0x59a1a1f8, 0x09898980, 0x1a0d0d17, 0x65bfbfda, 0xd7e6e631, 0x844242c6, 0xd06868b8, 0x824141c3, 0x299999b0, 0x5a2d2d77, 0x1e0f0f11, 0x7bb0b0cb, 0xa85454fc, 0x6dbbbbd6, 0x2c16163a];
    var T2 = [0xa5c66363, 0x84f87c7c, 0x99ee7777, 0x8df67b7b, 0x0dfff2f2, 0xbdd66b6b, 0xb1de6f6f, 0x5491c5c5, 0x50603030, 0x03020101, 0xa9ce6767, 0x7d562b2b, 0x19e7fefe, 0x62b5d7d7, 0xe64dabab, 0x9aec7676, 0x458fcaca, 0x9d1f8282, 0x4089c9c9, 0x87fa7d7d, 0x15effafa, 0xebb25959, 0xc98e4747, 0x0bfbf0f0, 0xec41adad, 0x67b3d4d4, 0xfd5fa2a2, 0xea45afaf, 0xbf239c9c, 0xf753a4a4, 0x96e47272, 0x5b9bc0c0, 0xc275b7b7, 0x1ce1fdfd, 0xae3d9393, 0x6a4c2626, 0x5a6c3636, 0x417e3f3f, 0x02f5f7f7, 0x4f83cccc, 0x5c683434, 0xf451a5a5, 0x34d1e5e5, 0x08f9f1f1, 0x93e27171, 0x73abd8d8, 0x53623131, 0x3f2a1515, 0x0c080404, 0x5295c7c7, 0x65462323, 0x5e9dc3c3, 0x28301818, 0xa1379696, 0x0f0a0505, 0xb52f9a9a, 0x090e0707, 0x36241212, 0x9b1b8080, 0x3ddfe2e2, 0x26cdebeb, 0x694e2727, 0xcd7fb2b2, 0x9fea7575, 0x1b120909, 0x9e1d8383, 0x74582c2c, 0x2e341a1a, 0x2d361b1b, 0xb2dc6e6e, 0xeeb45a5a, 0xfb5ba0a0, 0xf6a45252, 0x4d763b3b, 0x61b7d6d6, 0xce7db3b3, 0x7b522929, 0x3edde3e3, 0x715e2f2f, 0x97138484, 0xf5a65353, 0x68b9d1d1, 0x00000000, 0x2cc1eded, 0x60402020, 0x1fe3fcfc, 0xc879b1b1, 0xedb65b5b, 0xbed46a6a, 0x468dcbcb, 0xd967bebe, 0x4b723939, 0xde944a4a, 0xd4984c4c, 0xe8b05858, 0x4a85cfcf, 0x6bbbd0d0, 0x2ac5efef, 0xe54faaaa, 0x16edfbfb, 0xc5864343, 0xd79a4d4d, 0x55663333, 0x94118585, 0xcf8a4545, 0x10e9f9f9, 0x06040202, 0x81fe7f7f, 0xf0a05050, 0x44783c3c, 0xba259f9f, 0xe34ba8a8, 0xf3a25151, 0xfe5da3a3, 0xc0804040, 0x8a058f8f, 0xad3f9292, 0xbc219d9d, 0x48703838, 0x04f1f5f5, 0xdf63bcbc, 0xc177b6b6, 0x75afdada, 0x63422121, 0x30201010, 0x1ae5ffff, 0x0efdf3f3, 0x6dbfd2d2, 0x4c81cdcd, 0x14180c0c, 0x35261313, 0x2fc3ecec, 0xe1be5f5f, 0xa2359797, 0xcc884444, 0x392e1717, 0x5793c4c4, 0xf255a7a7, 0x82fc7e7e, 0x477a3d3d, 0xacc86464, 0xe7ba5d5d, 0x2b321919, 0x95e67373, 0xa0c06060, 0x98198181, 0xd19e4f4f, 0x7fa3dcdc, 0x66442222, 0x7e542a2a, 0xab3b9090, 0x830b8888, 0xca8c4646, 0x29c7eeee, 0xd36bb8b8, 0x3c281414, 0x79a7dede, 0xe2bc5e5e, 0x1d160b0b, 0x76addbdb, 0x3bdbe0e0, 0x56643232, 0x4e743a3a, 0x1e140a0a, 0xdb924949, 0x0a0c0606, 0x6c482424, 0xe4b85c5c, 0x5d9fc2c2, 0x6ebdd3d3, 0xef43acac, 0xa6c46262, 0xa8399191, 0xa4319595, 0x37d3e4e4, 0x8bf27979, 0x32d5e7e7, 0x438bc8c8, 0x596e3737, 0xb7da6d6d, 0x8c018d8d, 0x64b1d5d5, 0xd29c4e4e, 0xe049a9a9, 0xb4d86c6c, 0xfaac5656, 0x07f3f4f4, 0x25cfeaea, 0xafca6565, 0x8ef47a7a, 0xe947aeae, 0x18100808, 0xd56fbaba, 0x88f07878, 0x6f4a2525, 0x725c2e2e, 0x24381c1c, 0xf157a6a6, 0xc773b4b4, 0x5197c6c6, 0x23cbe8e8, 0x7ca1dddd, 0x9ce87474, 0x213e1f1f, 0xdd964b4b, 0xdc61bdbd, 0x860d8b8b, 0x850f8a8a, 0x90e07070, 0x427c3e3e, 0xc471b5b5, 0xaacc6666, 0xd8904848, 0x05060303, 0x01f7f6f6, 0x121c0e0e, 0xa3c26161, 0x5f6a3535, 0xf9ae5757, 0xd069b9b9, 0x91178686, 0x5899c1c1, 0x273a1d1d, 0xb9279e9e, 0x38d9e1e1, 0x13ebf8f8, 0xb32b9898, 0x33221111, 0xbbd26969, 0x70a9d9d9, 0x89078e8e, 0xa7339494, 0xb62d9b9b, 0x223c1e1e, 0x92158787, 0x20c9e9e9, 0x4987cece, 0xffaa5555, 0x78502828, 0x7aa5dfdf, 0x8f038c8c, 0xf859a1a1, 0x80098989, 0x171a0d0d, 0xda65bfbf, 0x31d7e6e6, 0xc6844242, 0xb8d06868, 0xc3824141, 0xb0299999, 0x775a2d2d, 0x111e0f0f, 0xcb7bb0b0, 0xfca85454, 0xd66dbbbb, 0x3a2c1616];
    var T3 = [0x63a5c663, 0x7c84f87c, 0x7799ee77, 0x7b8df67b, 0xf20dfff2, 0x6bbdd66b, 0x6fb1de6f, 0xc55491c5, 0x30506030, 0x01030201, 0x67a9ce67, 0x2b7d562b, 0xfe19e7fe, 0xd762b5d7, 0xabe64dab, 0x769aec76, 0xca458fca, 0x829d1f82, 0xc94089c9, 0x7d87fa7d, 0xfa15effa, 0x59ebb259, 0x47c98e47, 0xf00bfbf0, 0xadec41ad, 0xd467b3d4, 0xa2fd5fa2, 0xafea45af, 0x9cbf239c, 0xa4f753a4, 0x7296e472, 0xc05b9bc0, 0xb7c275b7, 0xfd1ce1fd, 0x93ae3d93, 0x266a4c26, 0x365a6c36, 0x3f417e3f, 0xf702f5f7, 0xcc4f83cc, 0x345c6834, 0xa5f451a5, 0xe534d1e5, 0xf108f9f1, 0x7193e271, 0xd873abd8, 0x31536231, 0x153f2a15, 0x040c0804, 0xc75295c7, 0x23654623, 0xc35e9dc3, 0x18283018, 0x96a13796, 0x050f0a05, 0x9ab52f9a, 0x07090e07, 0x12362412, 0x809b1b80, 0xe23ddfe2, 0xeb26cdeb, 0x27694e27, 0xb2cd7fb2, 0x759fea75, 0x091b1209, 0x839e1d83, 0x2c74582c, 0x1a2e341a, 0x1b2d361b, 0x6eb2dc6e, 0x5aeeb45a, 0xa0fb5ba0, 0x52f6a452, 0x3b4d763b, 0xd661b7d6, 0xb3ce7db3, 0x297b5229, 0xe33edde3, 0x2f715e2f, 0x84971384, 0x53f5a653, 0xd168b9d1, 0x00000000, 0xed2cc1ed, 0x20604020, 0xfc1fe3fc, 0xb1c879b1, 0x5bedb65b, 0x6abed46a, 0xcb468dcb, 0xbed967be, 0x394b7239, 0x4ade944a, 0x4cd4984c, 0x58e8b058, 0xcf4a85cf, 0xd06bbbd0, 0xef2ac5ef, 0xaae54faa, 0xfb16edfb, 0x43c58643, 0x4dd79a4d, 0x33556633, 0x85941185, 0x45cf8a45, 0xf910e9f9, 0x02060402, 0x7f81fe7f, 0x50f0a050, 0x3c44783c, 0x9fba259f, 0xa8e34ba8, 0x51f3a251, 0xa3fe5da3, 0x40c08040, 0x8f8a058f, 0x92ad3f92, 0x9dbc219d, 0x38487038, 0xf504f1f5, 0xbcdf63bc, 0xb6c177b6, 0xda75afda, 0x21634221, 0x10302010, 0xff1ae5ff, 0xf30efdf3, 0xd26dbfd2, 0xcd4c81cd, 0x0c14180c, 0x13352613, 0xec2fc3ec, 0x5fe1be5f, 0x97a23597, 0x44cc8844, 0x17392e17, 0xc45793c4, 0xa7f255a7, 0x7e82fc7e, 0x3d477a3d, 0x64acc864, 0x5de7ba5d, 0x192b3219, 0x7395e673, 0x60a0c060, 0x81981981, 0x4fd19e4f, 0xdc7fa3dc, 0x22664422, 0x2a7e542a, 0x90ab3b90, 0x88830b88, 0x46ca8c46, 0xee29c7ee, 0xb8d36bb8, 0x143c2814, 0xde79a7de, 0x5ee2bc5e, 0x0b1d160b, 0xdb76addb, 0xe03bdbe0, 0x32566432, 0x3a4e743a, 0x0a1e140a, 0x49db9249, 0x060a0c06, 0x246c4824, 0x5ce4b85c, 0xc25d9fc2, 0xd36ebdd3, 0xacef43ac, 0x62a6c462, 0x91a83991, 0x95a43195, 0xe437d3e4, 0x798bf279, 0xe732d5e7, 0xc8438bc8, 0x37596e37, 0x6db7da6d, 0x8d8c018d, 0xd564b1d5, 0x4ed29c4e, 0xa9e049a9, 0x6cb4d86c, 0x56faac56, 0xf407f3f4, 0xea25cfea, 0x65afca65, 0x7a8ef47a, 0xaee947ae, 0x08181008, 0xbad56fba, 0x7888f078, 0x256f4a25, 0x2e725c2e, 0x1c24381c, 0xa6f157a6, 0xb4c773b4, 0xc65197c6, 0xe823cbe8, 0xdd7ca1dd, 0x749ce874, 0x1f213e1f, 0x4bdd964b, 0xbddc61bd, 0x8b860d8b, 0x8a850f8a, 0x7090e070, 0x3e427c3e, 0xb5c471b5, 0x66aacc66, 0x48d89048, 0x03050603, 0xf601f7f6, 0x0e121c0e, 0x61a3c261, 0x355f6a35, 0x57f9ae57, 0xb9d069b9, 0x86911786, 0xc15899c1, 0x1d273a1d, 0x9eb9279e, 0xe138d9e1, 0xf813ebf8, 0x98b32b98, 0x11332211, 0x69bbd269, 0xd970a9d9, 0x8e89078e, 0x94a73394, 0x9bb62d9b, 0x1e223c1e, 0x87921587, 0xe920c9e9, 0xce4987ce, 0x55ffaa55, 0x28785028, 0xdf7aa5df, 0x8c8f038c, 0xa1f859a1, 0x89800989, 0x0d171a0d, 0xbfda65bf, 0xe631d7e6, 0x42c68442, 0x68b8d068, 0x41c38241, 0x99b02999, 0x2d775a2d, 0x0f111e0f, 0xb0cb7bb0, 0x54fca854, 0xbbd66dbb, 0x163a2c16];
    var T4 = [0x6363a5c6, 0x7c7c84f8, 0x777799ee, 0x7b7b8df6, 0xf2f20dff, 0x6b6bbdd6, 0x6f6fb1de, 0xc5c55491, 0x30305060, 0x01010302, 0x6767a9ce, 0x2b2b7d56, 0xfefe19e7, 0xd7d762b5, 0xababe64d, 0x76769aec, 0xcaca458f, 0x82829d1f, 0xc9c94089, 0x7d7d87fa, 0xfafa15ef, 0x5959ebb2, 0x4747c98e, 0xf0f00bfb, 0xadadec41, 0xd4d467b3, 0xa2a2fd5f, 0xafafea45, 0x9c9cbf23, 0xa4a4f753, 0x727296e4, 0xc0c05b9b, 0xb7b7c275, 0xfdfd1ce1, 0x9393ae3d, 0x26266a4c, 0x36365a6c, 0x3f3f417e, 0xf7f702f5, 0xcccc4f83, 0x34345c68, 0xa5a5f451, 0xe5e534d1, 0xf1f108f9, 0x717193e2, 0xd8d873ab, 0x31315362, 0x15153f2a, 0x04040c08, 0xc7c75295, 0x23236546, 0xc3c35e9d, 0x18182830, 0x9696a137, 0x05050f0a, 0x9a9ab52f, 0x0707090e, 0x12123624, 0x80809b1b, 0xe2e23ddf, 0xebeb26cd, 0x2727694e, 0xb2b2cd7f, 0x75759fea, 0x09091b12, 0x83839e1d, 0x2c2c7458, 0x1a1a2e34, 0x1b1b2d36, 0x6e6eb2dc, 0x5a5aeeb4, 0xa0a0fb5b, 0x5252f6a4, 0x3b3b4d76, 0xd6d661b7, 0xb3b3ce7d, 0x29297b52, 0xe3e33edd, 0x2f2f715e, 0x84849713, 0x5353f5a6, 0xd1d168b9, 0x00000000, 0xeded2cc1, 0x20206040, 0xfcfc1fe3, 0xb1b1c879, 0x5b5bedb6, 0x6a6abed4, 0xcbcb468d, 0xbebed967, 0x39394b72, 0x4a4ade94, 0x4c4cd498, 0x5858e8b0, 0xcfcf4a85, 0xd0d06bbb, 0xefef2ac5, 0xaaaae54f, 0xfbfb16ed, 0x4343c586, 0x4d4dd79a, 0x33335566, 0x85859411, 0x4545cf8a, 0xf9f910e9, 0x02020604, 0x7f7f81fe, 0x5050f0a0, 0x3c3c4478, 0x9f9fba25, 0xa8a8e34b, 0x5151f3a2, 0xa3a3fe5d, 0x4040c080, 0x8f8f8a05, 0x9292ad3f, 0x9d9dbc21, 0x38384870, 0xf5f504f1, 0xbcbcdf63, 0xb6b6c177, 0xdada75af, 0x21216342, 0x10103020, 0xffff1ae5, 0xf3f30efd, 0xd2d26dbf, 0xcdcd4c81, 0x0c0c1418, 0x13133526, 0xecec2fc3, 0x5f5fe1be, 0x9797a235, 0x4444cc88, 0x1717392e, 0xc4c45793, 0xa7a7f255, 0x7e7e82fc, 0x3d3d477a, 0x6464acc8, 0x5d5de7ba, 0x19192b32, 0x737395e6, 0x6060a0c0, 0x81819819, 0x4f4fd19e, 0xdcdc7fa3, 0x22226644, 0x2a2a7e54, 0x9090ab3b, 0x8888830b, 0x4646ca8c, 0xeeee29c7, 0xb8b8d36b, 0x14143c28, 0xdede79a7, 0x5e5ee2bc, 0x0b0b1d16, 0xdbdb76ad, 0xe0e03bdb, 0x32325664, 0x3a3a4e74, 0x0a0a1e14, 0x4949db92, 0x06060a0c, 0x24246c48, 0x5c5ce4b8, 0xc2c25d9f, 0xd3d36ebd, 0xacacef43, 0x6262a6c4, 0x9191a839, 0x9595a431, 0xe4e437d3, 0x79798bf2, 0xe7e732d5, 0xc8c8438b, 0x3737596e, 0x6d6db7da, 0x8d8d8c01, 0xd5d564b1, 0x4e4ed29c, 0xa9a9e049, 0x6c6cb4d8, 0x5656faac, 0xf4f407f3, 0xeaea25cf, 0x6565afca, 0x7a7a8ef4, 0xaeaee947, 0x08081810, 0xbabad56f, 0x787888f0, 0x25256f4a, 0x2e2e725c, 0x1c1c2438, 0xa6a6f157, 0xb4b4c773, 0xc6c65197, 0xe8e823cb, 0xdddd7ca1, 0x74749ce8, 0x1f1f213e, 0x4b4bdd96, 0xbdbddc61, 0x8b8b860d, 0x8a8a850f, 0x707090e0, 0x3e3e427c, 0xb5b5c471, 0x6666aacc, 0x4848d890, 0x03030506, 0xf6f601f7, 0x0e0e121c, 0x6161a3c2, 0x35355f6a, 0x5757f9ae, 0xb9b9d069, 0x86869117, 0xc1c15899, 0x1d1d273a, 0x9e9eb927, 0xe1e138d9, 0xf8f813eb, 0x9898b32b, 0x11113322, 0x6969bbd2, 0xd9d970a9, 0x8e8e8907, 0x9494a733, 0x9b9bb62d, 0x1e1e223c, 0x87879215, 0xe9e920c9, 0xcece4987, 0x5555ffaa, 0x28287850, 0xdfdf7aa5, 0x8c8c8f03, 0xa1a1f859, 0x89898009, 0x0d0d171a, 0xbfbfda65, 0xe6e631d7, 0x4242c684, 0x6868b8d0, 0x4141c382, 0x9999b029, 0x2d2d775a, 0x0f0f111e, 0xb0b0cb7b, 0x5454fca8, 0xbbbbd66d, 0x16163a2c];

    // Transformations for decryption
    var T5 = [0x51f4a750, 0x7e416553, 0x1a17a4c3, 0x3a275e96, 0x3bab6bcb, 0x1f9d45f1, 0xacfa58ab, 0x4be30393, 0x2030fa55, 0xad766df6, 0x88cc7691, 0xf5024c25, 0x4fe5d7fc, 0xc52acbd7, 0x26354480, 0xb562a38f, 0xdeb15a49, 0x25ba1b67, 0x45ea0e98, 0x5dfec0e1, 0xc32f7502, 0x814cf012, 0x8d4697a3, 0x6bd3f9c6, 0x038f5fe7, 0x15929c95, 0xbf6d7aeb, 0x955259da, 0xd4be832d, 0x587421d3, 0x49e06929, 0x8ec9c844, 0x75c2896a, 0xf48e7978, 0x99583e6b, 0x27b971dd, 0xbee14fb6, 0xf088ad17, 0xc920ac66, 0x7dce3ab4, 0x63df4a18, 0xe51a3182, 0x97513360, 0x62537f45, 0xb16477e0, 0xbb6bae84, 0xfe81a01c, 0xf9082b94, 0x70486858, 0x8f45fd19, 0x94de6c87, 0x527bf8b7, 0xab73d323, 0x724b02e2, 0xe31f8f57, 0x6655ab2a, 0xb2eb2807, 0x2fb5c203, 0x86c57b9a, 0xd33708a5, 0x302887f2, 0x23bfa5b2, 0x02036aba, 0xed16825c, 0x8acf1c2b, 0xa779b492, 0xf307f2f0, 0x4e69e2a1, 0x65daf4cd, 0x0605bed5, 0xd134621f, 0xc4a6fe8a, 0x342e539d, 0xa2f355a0, 0x058ae132, 0xa4f6eb75, 0x0b83ec39, 0x4060efaa, 0x5e719f06, 0xbd6e1051, 0x3e218af9, 0x96dd063d, 0xdd3e05ae, 0x4de6bd46, 0x91548db5, 0x71c45d05, 0x0406d46f, 0x605015ff, 0x1998fb24, 0xd6bde997, 0x894043cc, 0x67d99e77, 0xb0e842bd, 0x07898b88, 0xe7195b38, 0x79c8eedb, 0xa17c0a47, 0x7c420fe9, 0xf8841ec9, 0x00000000, 0x09808683, 0x322bed48, 0x1e1170ac, 0x6c5a724e, 0xfd0efffb, 0x0f853856, 0x3daed51e, 0x362d3927, 0x0a0fd964, 0x685ca621, 0x9b5b54d1, 0x24362e3a, 0x0c0a67b1, 0x9357e70f, 0xb4ee96d2, 0x1b9b919e, 0x80c0c54f, 0x61dc20a2, 0x5a774b69, 0x1c121a16, 0xe293ba0a, 0xc0a02ae5, 0x3c22e043, 0x121b171d, 0x0e090d0b, 0xf28bc7ad, 0x2db6a8b9, 0x141ea9c8, 0x57f11985, 0xaf75074c, 0xee99ddbb, 0xa37f60fd, 0xf701269f, 0x5c72f5bc, 0x44663bc5, 0x5bfb7e34, 0x8b432976, 0xcb23c6dc, 0xb6edfc68, 0xb8e4f163, 0xd731dcca, 0x42638510, 0x13972240, 0x84c61120, 0x854a247d, 0xd2bb3df8, 0xaef93211, 0xc729a16d, 0x1d9e2f4b, 0xdcb230f3, 0x0d8652ec, 0x77c1e3d0, 0x2bb3166c, 0xa970b999, 0x119448fa, 0x47e96422, 0xa8fc8cc4, 0xa0f03f1a, 0x567d2cd8, 0x223390ef, 0x87494ec7, 0xd938d1c1, 0x8ccaa2fe, 0x98d40b36, 0xa6f581cf, 0xa57ade28, 0xdab78e26, 0x3fadbfa4, 0x2c3a9de4, 0x5078920d, 0x6a5fcc9b, 0x547e4662, 0xf68d13c2, 0x90d8b8e8, 0x2e39f75e, 0x82c3aff5, 0x9f5d80be, 0x69d0937c, 0x6fd52da9, 0xcf2512b3, 0xc8ac993b, 0x10187da7, 0xe89c636e, 0xdb3bbb7b, 0xcd267809, 0x6e5918f4, 0xec9ab701, 0x834f9aa8, 0xe6956e65, 0xaaffe67e, 0x21bccf08, 0xef15e8e6, 0xbae79bd9, 0x4a6f36ce, 0xea9f09d4, 0x29b07cd6, 0x31a4b2af, 0x2a3f2331, 0xc6a59430, 0x35a266c0, 0x744ebc37, 0xfc82caa6, 0xe090d0b0, 0x33a7d815, 0xf104984a, 0x41ecdaf7, 0x7fcd500e, 0x1791f62f, 0x764dd68d, 0x43efb04d, 0xccaa4d54, 0xe49604df, 0x9ed1b5e3, 0x4c6a881b, 0xc12c1fb8, 0x4665517f, 0x9d5eea04, 0x018c355d, 0xfa877473, 0xfb0b412e, 0xb3671d5a, 0x92dbd252, 0xe9105633, 0x6dd64713, 0x9ad7618c, 0x37a10c7a, 0x59f8148e, 0xeb133c89, 0xcea927ee, 0xb761c935, 0xe11ce5ed, 0x7a47b13c, 0x9cd2df59, 0x55f2733f, 0x1814ce79, 0x73c737bf, 0x53f7cdea, 0x5ffdaa5b, 0xdf3d6f14, 0x7844db86, 0xcaaff381, 0xb968c43e, 0x3824342c, 0xc2a3405f, 0x161dc372, 0xbce2250c, 0x283c498b, 0xff0d9541, 0x39a80171, 0x080cb3de, 0xd8b4e49c, 0x6456c190, 0x7bcb8461, 0xd532b670, 0x486c5c74, 0xd0b85742];
    var T6 = [0x5051f4a7, 0x537e4165, 0xc31a17a4, 0x963a275e, 0xcb3bab6b, 0xf11f9d45, 0xabacfa58, 0x934be303, 0x552030fa, 0xf6ad766d, 0x9188cc76, 0x25f5024c, 0xfc4fe5d7, 0xd7c52acb, 0x80263544, 0x8fb562a3, 0x49deb15a, 0x6725ba1b, 0x9845ea0e, 0xe15dfec0, 0x02c32f75, 0x12814cf0, 0xa38d4697, 0xc66bd3f9, 0xe7038f5f, 0x9515929c, 0xebbf6d7a, 0xda955259, 0x2dd4be83, 0xd3587421, 0x2949e069, 0x448ec9c8, 0x6a75c289, 0x78f48e79, 0x6b99583e, 0xdd27b971, 0xb6bee14f, 0x17f088ad, 0x66c920ac, 0xb47dce3a, 0x1863df4a, 0x82e51a31, 0x60975133, 0x4562537f, 0xe0b16477, 0x84bb6bae, 0x1cfe81a0, 0x94f9082b, 0x58704868, 0x198f45fd, 0x8794de6c, 0xb7527bf8, 0x23ab73d3, 0xe2724b02, 0x57e31f8f, 0x2a6655ab, 0x07b2eb28, 0x032fb5c2, 0x9a86c57b, 0xa5d33708, 0xf2302887, 0xb223bfa5, 0xba02036a, 0x5ced1682, 0x2b8acf1c, 0x92a779b4, 0xf0f307f2, 0xa14e69e2, 0xcd65daf4, 0xd50605be, 0x1fd13462, 0x8ac4a6fe, 0x9d342e53, 0xa0a2f355, 0x32058ae1, 0x75a4f6eb, 0x390b83ec, 0xaa4060ef, 0x065e719f, 0x51bd6e10, 0xf93e218a, 0x3d96dd06, 0xaedd3e05, 0x464de6bd, 0xb591548d, 0x0571c45d, 0x6f0406d4, 0xff605015, 0x241998fb, 0x97d6bde9, 0xcc894043, 0x7767d99e, 0xbdb0e842, 0x8807898b, 0x38e7195b, 0xdb79c8ee, 0x47a17c0a, 0xe97c420f, 0xc9f8841e, 0x00000000, 0x83098086, 0x48322bed, 0xac1e1170, 0x4e6c5a72, 0xfbfd0eff, 0x560f8538, 0x1e3daed5, 0x27362d39, 0x640a0fd9, 0x21685ca6, 0xd19b5b54, 0x3a24362e, 0xb10c0a67, 0x0f9357e7, 0xd2b4ee96, 0x9e1b9b91, 0x4f80c0c5, 0xa261dc20, 0x695a774b, 0x161c121a, 0x0ae293ba, 0xe5c0a02a, 0x433c22e0, 0x1d121b17, 0x0b0e090d, 0xadf28bc7, 0xb92db6a8, 0xc8141ea9, 0x8557f119, 0x4caf7507, 0xbbee99dd, 0xfda37f60, 0x9ff70126, 0xbc5c72f5, 0xc544663b, 0x345bfb7e, 0x768b4329, 0xdccb23c6, 0x68b6edfc, 0x63b8e4f1, 0xcad731dc, 0x10426385, 0x40139722, 0x2084c611, 0x7d854a24, 0xf8d2bb3d, 0x11aef932, 0x6dc729a1, 0x4b1d9e2f, 0xf3dcb230, 0xec0d8652, 0xd077c1e3, 0x6c2bb316, 0x99a970b9, 0xfa119448, 0x2247e964, 0xc4a8fc8c, 0x1aa0f03f, 0xd8567d2c, 0xef223390, 0xc787494e, 0xc1d938d1, 0xfe8ccaa2, 0x3698d40b, 0xcfa6f581, 0x28a57ade, 0x26dab78e, 0xa43fadbf, 0xe42c3a9d, 0x0d507892, 0x9b6a5fcc, 0x62547e46, 0xc2f68d13, 0xe890d8b8, 0x5e2e39f7, 0xf582c3af, 0xbe9f5d80, 0x7c69d093, 0xa96fd52d, 0xb3cf2512, 0x3bc8ac99, 0xa710187d, 0x6ee89c63, 0x7bdb3bbb, 0x09cd2678, 0xf46e5918, 0x01ec9ab7, 0xa8834f9a, 0x65e6956e, 0x7eaaffe6, 0x0821bccf, 0xe6ef15e8, 0xd9bae79b, 0xce4a6f36, 0xd4ea9f09, 0xd629b07c, 0xaf31a4b2, 0x312a3f23, 0x30c6a594, 0xc035a266, 0x37744ebc, 0xa6fc82ca, 0xb0e090d0, 0x1533a7d8, 0x4af10498, 0xf741ecda, 0x0e7fcd50, 0x2f1791f6, 0x8d764dd6, 0x4d43efb0, 0x54ccaa4d, 0xdfe49604, 0xe39ed1b5, 0x1b4c6a88, 0xb8c12c1f, 0x7f466551, 0x049d5eea, 0x5d018c35, 0x73fa8774, 0x2efb0b41, 0x5ab3671d, 0x5292dbd2, 0x33e91056, 0x136dd647, 0x8c9ad761, 0x7a37a10c, 0x8e59f814, 0x89eb133c, 0xeecea927, 0x35b761c9, 0xede11ce5, 0x3c7a47b1, 0x599cd2df, 0x3f55f273, 0x791814ce, 0xbf73c737, 0xea53f7cd, 0x5b5ffdaa, 0x14df3d6f, 0x867844db, 0x81caaff3, 0x3eb968c4, 0x2c382434, 0x5fc2a340, 0x72161dc3, 0x0cbce225, 0x8b283c49, 0x41ff0d95, 0x7139a801, 0xde080cb3, 0x9cd8b4e4, 0x906456c1, 0x617bcb84, 0x70d532b6, 0x74486c5c, 0x42d0b857];
    var T7 = [0xa75051f4, 0x65537e41, 0xa4c31a17, 0x5e963a27, 0x6bcb3bab, 0x45f11f9d, 0x58abacfa, 0x03934be3, 0xfa552030, 0x6df6ad76, 0x769188cc, 0x4c25f502, 0xd7fc4fe5, 0xcbd7c52a, 0x44802635, 0xa38fb562, 0x5a49deb1, 0x1b6725ba, 0x0e9845ea, 0xc0e15dfe, 0x7502c32f, 0xf012814c, 0x97a38d46, 0xf9c66bd3, 0x5fe7038f, 0x9c951592, 0x7aebbf6d, 0x59da9552, 0x832dd4be, 0x21d35874, 0x692949e0, 0xc8448ec9, 0x896a75c2, 0x7978f48e, 0x3e6b9958, 0x71dd27b9, 0x4fb6bee1, 0xad17f088, 0xac66c920, 0x3ab47dce, 0x4a1863df, 0x3182e51a, 0x33609751, 0x7f456253, 0x77e0b164, 0xae84bb6b, 0xa01cfe81, 0x2b94f908, 0x68587048, 0xfd198f45, 0x6c8794de, 0xf8b7527b, 0xd323ab73, 0x02e2724b, 0x8f57e31f, 0xab2a6655, 0x2807b2eb, 0xc2032fb5, 0x7b9a86c5, 0x08a5d337, 0x87f23028, 0xa5b223bf, 0x6aba0203, 0x825ced16, 0x1c2b8acf, 0xb492a779, 0xf2f0f307, 0xe2a14e69, 0xf4cd65da, 0xbed50605, 0x621fd134, 0xfe8ac4a6, 0x539d342e, 0x55a0a2f3, 0xe132058a, 0xeb75a4f6, 0xec390b83, 0xefaa4060, 0x9f065e71, 0x1051bd6e, 0x8af93e21, 0x063d96dd, 0x05aedd3e, 0xbd464de6, 0x8db59154, 0x5d0571c4, 0xd46f0406, 0x15ff6050, 0xfb241998, 0xe997d6bd, 0x43cc8940, 0x9e7767d9, 0x42bdb0e8, 0x8b880789, 0x5b38e719, 0xeedb79c8, 0x0a47a17c, 0x0fe97c42, 0x1ec9f884, 0x00000000, 0x86830980, 0xed48322b, 0x70ac1e11, 0x724e6c5a, 0xfffbfd0e, 0x38560f85, 0xd51e3dae, 0x3927362d, 0xd9640a0f, 0xa621685c, 0x54d19b5b, 0x2e3a2436, 0x67b10c0a, 0xe70f9357, 0x96d2b4ee, 0x919e1b9b, 0xc54f80c0, 0x20a261dc, 0x4b695a77, 0x1a161c12, 0xba0ae293, 0x2ae5c0a0, 0xe0433c22, 0x171d121b, 0x0d0b0e09, 0xc7adf28b, 0xa8b92db6, 0xa9c8141e, 0x198557f1, 0x074caf75, 0xddbbee99, 0x60fda37f, 0x269ff701, 0xf5bc5c72, 0x3bc54466, 0x7e345bfb, 0x29768b43, 0xc6dccb23, 0xfc68b6ed, 0xf163b8e4, 0xdccad731, 0x85104263, 0x22401397, 0x112084c6, 0x247d854a, 0x3df8d2bb, 0x3211aef9, 0xa16dc729, 0x2f4b1d9e, 0x30f3dcb2, 0x52ec0d86, 0xe3d077c1, 0x166c2bb3, 0xb999a970, 0x48fa1194, 0x642247e9, 0x8cc4a8fc, 0x3f1aa0f0, 0x2cd8567d, 0x90ef2233, 0x4ec78749, 0xd1c1d938, 0xa2fe8cca, 0x0b3698d4, 0x81cfa6f5, 0xde28a57a, 0x8e26dab7, 0xbfa43fad, 0x9de42c3a, 0x920d5078, 0xcc9b6a5f, 0x4662547e, 0x13c2f68d, 0xb8e890d8, 0xf75e2e39, 0xaff582c3, 0x80be9f5d, 0x937c69d0, 0x2da96fd5, 0x12b3cf25, 0x993bc8ac, 0x7da71018, 0x636ee89c, 0xbb7bdb3b, 0x7809cd26, 0x18f46e59, 0xb701ec9a, 0x9aa8834f, 0x6e65e695, 0xe67eaaff, 0xcf0821bc, 0xe8e6ef15, 0x9bd9bae7, 0x36ce4a6f, 0x09d4ea9f, 0x7cd629b0, 0xb2af31a4, 0x23312a3f, 0x9430c6a5, 0x66c035a2, 0xbc37744e, 0xcaa6fc82, 0xd0b0e090, 0xd81533a7, 0x984af104, 0xdaf741ec, 0x500e7fcd, 0xf62f1791, 0xd68d764d, 0xb04d43ef, 0x4d54ccaa, 0x04dfe496, 0xb5e39ed1, 0x881b4c6a, 0x1fb8c12c, 0x517f4665, 0xea049d5e, 0x355d018c, 0x7473fa87, 0x412efb0b, 0x1d5ab367, 0xd25292db, 0x5633e910, 0x47136dd6, 0x618c9ad7, 0x0c7a37a1, 0x148e59f8, 0x3c89eb13, 0x27eecea9, 0xc935b761, 0xe5ede11c, 0xb13c7a47, 0xdf599cd2, 0x733f55f2, 0xce791814, 0x37bf73c7, 0xcdea53f7, 0xaa5b5ffd, 0x6f14df3d, 0xdb867844, 0xf381caaf, 0xc43eb968, 0x342c3824, 0x405fc2a3, 0xc372161d, 0x250cbce2, 0x498b283c, 0x9541ff0d, 0x017139a8, 0xb3de080c, 0xe49cd8b4, 0xc1906456, 0x84617bcb, 0xb670d532, 0x5c74486c, 0x5742d0b8];
    var T8 = [0xf4a75051, 0x4165537e, 0x17a4c31a, 0x275e963a, 0xab6bcb3b, 0x9d45f11f, 0xfa58abac, 0xe303934b, 0x30fa5520, 0x766df6ad, 0xcc769188, 0x024c25f5, 0xe5d7fc4f, 0x2acbd7c5, 0x35448026, 0x62a38fb5, 0xb15a49de, 0xba1b6725, 0xea0e9845, 0xfec0e15d, 0x2f7502c3, 0x4cf01281, 0x4697a38d, 0xd3f9c66b, 0x8f5fe703, 0x929c9515, 0x6d7aebbf, 0x5259da95, 0xbe832dd4, 0x7421d358, 0xe0692949, 0xc9c8448e, 0xc2896a75, 0x8e7978f4, 0x583e6b99, 0xb971dd27, 0xe14fb6be, 0x88ad17f0, 0x20ac66c9, 0xce3ab47d, 0xdf4a1863, 0x1a3182e5, 0x51336097, 0x537f4562, 0x6477e0b1, 0x6bae84bb, 0x81a01cfe, 0x082b94f9, 0x48685870, 0x45fd198f, 0xde6c8794, 0x7bf8b752, 0x73d323ab, 0x4b02e272, 0x1f8f57e3, 0x55ab2a66, 0xeb2807b2, 0xb5c2032f, 0xc57b9a86, 0x3708a5d3, 0x2887f230, 0xbfa5b223, 0x036aba02, 0x16825ced, 0xcf1c2b8a, 0x79b492a7, 0x07f2f0f3, 0x69e2a14e, 0xdaf4cd65, 0x05bed506, 0x34621fd1, 0xa6fe8ac4, 0x2e539d34, 0xf355a0a2, 0x8ae13205, 0xf6eb75a4, 0x83ec390b, 0x60efaa40, 0x719f065e, 0x6e1051bd, 0x218af93e, 0xdd063d96, 0x3e05aedd, 0xe6bd464d, 0x548db591, 0xc45d0571, 0x06d46f04, 0x5015ff60, 0x98fb2419, 0xbde997d6, 0x4043cc89, 0xd99e7767, 0xe842bdb0, 0x898b8807, 0x195b38e7, 0xc8eedb79, 0x7c0a47a1, 0x420fe97c, 0x841ec9f8, 0x00000000, 0x80868309, 0x2bed4832, 0x1170ac1e, 0x5a724e6c, 0x0efffbfd, 0x8538560f, 0xaed51e3d, 0x2d392736, 0x0fd9640a, 0x5ca62168, 0x5b54d19b, 0x362e3a24, 0x0a67b10c, 0x57e70f93, 0xee96d2b4, 0x9b919e1b, 0xc0c54f80, 0xdc20a261, 0x774b695a, 0x121a161c, 0x93ba0ae2, 0xa02ae5c0, 0x22e0433c, 0x1b171d12, 0x090d0b0e, 0x8bc7adf2, 0xb6a8b92d, 0x1ea9c814, 0xf1198557, 0x75074caf, 0x99ddbbee, 0x7f60fda3, 0x01269ff7, 0x72f5bc5c, 0x663bc544, 0xfb7e345b, 0x4329768b, 0x23c6dccb, 0xedfc68b6, 0xe4f163b8, 0x31dccad7, 0x63851042, 0x97224013, 0xc6112084, 0x4a247d85, 0xbb3df8d2, 0xf93211ae, 0x29a16dc7, 0x9e2f4b1d, 0xb230f3dc, 0x8652ec0d, 0xc1e3d077, 0xb3166c2b, 0x70b999a9, 0x9448fa11, 0xe9642247, 0xfc8cc4a8, 0xf03f1aa0, 0x7d2cd856, 0x3390ef22, 0x494ec787, 0x38d1c1d9, 0xcaa2fe8c, 0xd40b3698, 0xf581cfa6, 0x7ade28a5, 0xb78e26da, 0xadbfa43f, 0x3a9de42c, 0x78920d50, 0x5fcc9b6a, 0x7e466254, 0x8d13c2f6, 0xd8b8e890, 0x39f75e2e, 0xc3aff582, 0x5d80be9f, 0xd0937c69, 0xd52da96f, 0x2512b3cf, 0xac993bc8, 0x187da710, 0x9c636ee8, 0x3bbb7bdb, 0x267809cd, 0x5918f46e, 0x9ab701ec, 0x4f9aa883, 0x956e65e6, 0xffe67eaa, 0xbccf0821, 0x15e8e6ef, 0xe79bd9ba, 0x6f36ce4a, 0x9f09d4ea, 0xb07cd629, 0xa4b2af31, 0x3f23312a, 0xa59430c6, 0xa266c035, 0x4ebc3774, 0x82caa6fc, 0x90d0b0e0, 0xa7d81533, 0x04984af1, 0xecdaf741, 0xcd500e7f, 0x91f62f17, 0x4dd68d76, 0xefb04d43, 0xaa4d54cc, 0x9604dfe4, 0xd1b5e39e, 0x6a881b4c, 0x2c1fb8c1, 0x65517f46, 0x5eea049d, 0x8c355d01, 0x877473fa, 0x0b412efb, 0x671d5ab3, 0xdbd25292, 0x105633e9, 0xd647136d, 0xd7618c9a, 0xa10c7a37, 0xf8148e59, 0x133c89eb, 0xa927eece, 0x61c935b7, 0x1ce5ede1, 0x47b13c7a, 0xd2df599c, 0xf2733f55, 0x14ce7918, 0xc737bf73, 0xf7cdea53, 0xfdaa5b5f, 0x3d6f14df, 0x44db8678, 0xaff381ca, 0x68c43eb9, 0x24342c38, 0xa3405fc2, 0x1dc37216, 0xe2250cbc, 0x3c498b28, 0x0d9541ff, 0xa8017139, 0x0cb3de08, 0xb4e49cd8, 0x56c19064, 0xcb84617b, 0x32b670d5, 0x6c5c7448, 0xb85742d0];

    // Transformations for decryption key expansion
    var U1 = [0x00000000, 0x0e090d0b, 0x1c121a16, 0x121b171d, 0x3824342c, 0x362d3927, 0x24362e3a, 0x2a3f2331, 0x70486858, 0x7e416553, 0x6c5a724e, 0x62537f45, 0x486c5c74, 0x4665517f, 0x547e4662, 0x5a774b69, 0xe090d0b0, 0xee99ddbb, 0xfc82caa6, 0xf28bc7ad, 0xd8b4e49c, 0xd6bde997, 0xc4a6fe8a, 0xcaaff381, 0x90d8b8e8, 0x9ed1b5e3, 0x8ccaa2fe, 0x82c3aff5, 0xa8fc8cc4, 0xa6f581cf, 0xb4ee96d2, 0xbae79bd9, 0xdb3bbb7b, 0xd532b670, 0xc729a16d, 0xc920ac66, 0xe31f8f57, 0xed16825c, 0xff0d9541, 0xf104984a, 0xab73d323, 0xa57ade28, 0xb761c935, 0xb968c43e, 0x9357e70f, 0x9d5eea04, 0x8f45fd19, 0x814cf012, 0x3bab6bcb, 0x35a266c0, 0x27b971dd, 0x29b07cd6, 0x038f5fe7, 0x0d8652ec, 0x1f9d45f1, 0x119448fa, 0x4be30393, 0x45ea0e98, 0x57f11985, 0x59f8148e, 0x73c737bf, 0x7dce3ab4, 0x6fd52da9, 0x61dc20a2, 0xad766df6, 0xa37f60fd, 0xb16477e0, 0xbf6d7aeb, 0x955259da, 0x9b5b54d1, 0x894043cc, 0x87494ec7, 0xdd3e05ae, 0xd33708a5, 0xc12c1fb8, 0xcf2512b3, 0xe51a3182, 0xeb133c89, 0xf9082b94, 0xf701269f, 0x4de6bd46, 0x43efb04d, 0x51f4a750, 0x5ffdaa5b, 0x75c2896a, 0x7bcb8461, 0x69d0937c, 0x67d99e77, 0x3daed51e, 0x33a7d815, 0x21bccf08, 0x2fb5c203, 0x058ae132, 0x0b83ec39, 0x1998fb24, 0x1791f62f, 0x764dd68d, 0x7844db86, 0x6a5fcc9b, 0x6456c190, 0x4e69e2a1, 0x4060efaa, 0x527bf8b7, 0x5c72f5bc, 0x0605bed5, 0x080cb3de, 0x1a17a4c3, 0x141ea9c8, 0x3e218af9, 0x302887f2, 0x223390ef, 0x2c3a9de4, 0x96dd063d, 0x98d40b36, 0x8acf1c2b, 0x84c61120, 0xaef93211, 0xa0f03f1a, 0xb2eb2807, 0xbce2250c, 0xe6956e65, 0xe89c636e, 0xfa877473, 0xf48e7978, 0xdeb15a49, 0xd0b85742, 0xc2a3405f, 0xccaa4d54, 0x41ecdaf7, 0x4fe5d7fc, 0x5dfec0e1, 0x53f7cdea, 0x79c8eedb, 0x77c1e3d0, 0x65daf4cd, 0x6bd3f9c6, 0x31a4b2af, 0x3fadbfa4, 0x2db6a8b9, 0x23bfa5b2, 0x09808683, 0x07898b88, 0x15929c95, 0x1b9b919e, 0xa17c0a47, 0xaf75074c, 0xbd6e1051, 0xb3671d5a, 0x99583e6b, 0x97513360, 0x854a247d, 0x8b432976, 0xd134621f, 0xdf3d6f14, 0xcd267809, 0xc32f7502, 0xe9105633, 0xe7195b38, 0xf5024c25, 0xfb0b412e, 0x9ad7618c, 0x94de6c87, 0x86c57b9a, 0x88cc7691, 0xa2f355a0, 0xacfa58ab, 0xbee14fb6, 0xb0e842bd, 0xea9f09d4, 0xe49604df, 0xf68d13c2, 0xf8841ec9, 0xd2bb3df8, 0xdcb230f3, 0xcea927ee, 0xc0a02ae5, 0x7a47b13c, 0x744ebc37, 0x6655ab2a, 0x685ca621, 0x42638510, 0x4c6a881b, 0x5e719f06, 0x5078920d, 0x0a0fd964, 0x0406d46f, 0x161dc372, 0x1814ce79, 0x322bed48, 0x3c22e043, 0x2e39f75e, 0x2030fa55, 0xec9ab701, 0xe293ba0a, 0xf088ad17, 0xfe81a01c, 0xd4be832d, 0xdab78e26, 0xc8ac993b, 0xc6a59430, 0x9cd2df59, 0x92dbd252, 0x80c0c54f, 0x8ec9c844, 0xa4f6eb75, 0xaaffe67e, 0xb8e4f163, 0xb6edfc68, 0x0c0a67b1, 0x02036aba, 0x10187da7, 0x1e1170ac, 0x342e539d, 0x3a275e96, 0x283c498b, 0x26354480, 0x7c420fe9, 0x724b02e2, 0x605015ff, 0x6e5918f4, 0x44663bc5, 0x4a6f36ce, 0x587421d3, 0x567d2cd8, 0x37a10c7a, 0x39a80171, 0x2bb3166c, 0x25ba1b67, 0x0f853856, 0x018c355d, 0x13972240, 0x1d9e2f4b, 0x47e96422, 0x49e06929, 0x5bfb7e34, 0x55f2733f, 0x7fcd500e, 0x71c45d05, 0x63df4a18, 0x6dd64713, 0xd731dcca, 0xd938d1c1, 0xcb23c6dc, 0xc52acbd7, 0xef15e8e6, 0xe11ce5ed, 0xf307f2f0, 0xfd0efffb, 0xa779b492, 0xa970b999, 0xbb6bae84, 0xb562a38f, 0x9f5d80be, 0x91548db5, 0x834f9aa8, 0x8d4697a3];
    var U2 = [0x00000000, 0x0b0e090d, 0x161c121a, 0x1d121b17, 0x2c382434, 0x27362d39, 0x3a24362e, 0x312a3f23, 0x58704868, 0x537e4165, 0x4e6c5a72, 0x4562537f, 0x74486c5c, 0x7f466551, 0x62547e46, 0x695a774b, 0xb0e090d0, 0xbbee99dd, 0xa6fc82ca, 0xadf28bc7, 0x9cd8b4e4, 0x97d6bde9, 0x8ac4a6fe, 0x81caaff3, 0xe890d8b8, 0xe39ed1b5, 0xfe8ccaa2, 0xf582c3af, 0xc4a8fc8c, 0xcfa6f581, 0xd2b4ee96, 0xd9bae79b, 0x7bdb3bbb, 0x70d532b6, 0x6dc729a1, 0x66c920ac, 0x57e31f8f, 0x5ced1682, 0x41ff0d95, 0x4af10498, 0x23ab73d3, 0x28a57ade, 0x35b761c9, 0x3eb968c4, 0x0f9357e7, 0x049d5eea, 0x198f45fd, 0x12814cf0, 0xcb3bab6b, 0xc035a266, 0xdd27b971, 0xd629b07c, 0xe7038f5f, 0xec0d8652, 0xf11f9d45, 0xfa119448, 0x934be303, 0x9845ea0e, 0x8557f119, 0x8e59f814, 0xbf73c737, 0xb47dce3a, 0xa96fd52d, 0xa261dc20, 0xf6ad766d, 0xfda37f60, 0xe0b16477, 0xebbf6d7a, 0xda955259, 0xd19b5b54, 0xcc894043, 0xc787494e, 0xaedd3e05, 0xa5d33708, 0xb8c12c1f, 0xb3cf2512, 0x82e51a31, 0x89eb133c, 0x94f9082b, 0x9ff70126, 0x464de6bd, 0x4d43efb0, 0x5051f4a7, 0x5b5ffdaa, 0x6a75c289, 0x617bcb84, 0x7c69d093, 0x7767d99e, 0x1e3daed5, 0x1533a7d8, 0x0821bccf, 0x032fb5c2, 0x32058ae1, 0x390b83ec, 0x241998fb, 0x2f1791f6, 0x8d764dd6, 0x867844db, 0x9b6a5fcc, 0x906456c1, 0xa14e69e2, 0xaa4060ef, 0xb7527bf8, 0xbc5c72f5, 0xd50605be, 0xde080cb3, 0xc31a17a4, 0xc8141ea9, 0xf93e218a, 0xf2302887, 0xef223390, 0xe42c3a9d, 0x3d96dd06, 0x3698d40b, 0x2b8acf1c, 0x2084c611, 0x11aef932, 0x1aa0f03f, 0x07b2eb28, 0x0cbce225, 0x65e6956e, 0x6ee89c63, 0x73fa8774, 0x78f48e79, 0x49deb15a, 0x42d0b857, 0x5fc2a340, 0x54ccaa4d, 0xf741ecda, 0xfc4fe5d7, 0xe15dfec0, 0xea53f7cd, 0xdb79c8ee, 0xd077c1e3, 0xcd65daf4, 0xc66bd3f9, 0xaf31a4b2, 0xa43fadbf, 0xb92db6a8, 0xb223bfa5, 0x83098086, 0x8807898b, 0x9515929c, 0x9e1b9b91, 0x47a17c0a, 0x4caf7507, 0x51bd6e10, 0x5ab3671d, 0x6b99583e, 0x60975133, 0x7d854a24, 0x768b4329, 0x1fd13462, 0x14df3d6f, 0x09cd2678, 0x02c32f75, 0x33e91056, 0x38e7195b, 0x25f5024c, 0x2efb0b41, 0x8c9ad761, 0x8794de6c, 0x9a86c57b, 0x9188cc76, 0xa0a2f355, 0xabacfa58, 0xb6bee14f, 0xbdb0e842, 0xd4ea9f09, 0xdfe49604, 0xc2f68d13, 0xc9f8841e, 0xf8d2bb3d, 0xf3dcb230, 0xeecea927, 0xe5c0a02a, 0x3c7a47b1, 0x37744ebc, 0x2a6655ab, 0x21685ca6, 0x10426385, 0x1b4c6a88, 0x065e719f, 0x0d507892, 0x640a0fd9, 0x6f0406d4, 0x72161dc3, 0x791814ce, 0x48322bed, 0x433c22e0, 0x5e2e39f7, 0x552030fa, 0x01ec9ab7, 0x0ae293ba, 0x17f088ad, 0x1cfe81a0, 0x2dd4be83, 0x26dab78e, 0x3bc8ac99, 0x30c6a594, 0x599cd2df, 0x5292dbd2, 0x4f80c0c5, 0x448ec9c8, 0x75a4f6eb, 0x7eaaffe6, 0x63b8e4f1, 0x68b6edfc, 0xb10c0a67, 0xba02036a, 0xa710187d, 0xac1e1170, 0x9d342e53, 0x963a275e, 0x8b283c49, 0x80263544, 0xe97c420f, 0xe2724b02, 0xff605015, 0xf46e5918, 0xc544663b, 0xce4a6f36, 0xd3587421, 0xd8567d2c, 0x7a37a10c, 0x7139a801, 0x6c2bb316, 0x6725ba1b, 0x560f8538, 0x5d018c35, 0x40139722, 0x4b1d9e2f, 0x2247e964, 0x2949e069, 0x345bfb7e, 0x3f55f273, 0x0e7fcd50, 0x0571c45d, 0x1863df4a, 0x136dd647, 0xcad731dc, 0xc1d938d1, 0xdccb23c6, 0xd7c52acb, 0xe6ef15e8, 0xede11ce5, 0xf0f307f2, 0xfbfd0eff, 0x92a779b4, 0x99a970b9, 0x84bb6bae, 0x8fb562a3, 0xbe9f5d80, 0xb591548d, 0xa8834f9a, 0xa38d4697];
    var U3 = [0x00000000, 0x0d0b0e09, 0x1a161c12, 0x171d121b, 0x342c3824, 0x3927362d, 0x2e3a2436, 0x23312a3f, 0x68587048, 0x65537e41, 0x724e6c5a, 0x7f456253, 0x5c74486c, 0x517f4665, 0x4662547e, 0x4b695a77, 0xd0b0e090, 0xddbbee99, 0xcaa6fc82, 0xc7adf28b, 0xe49cd8b4, 0xe997d6bd, 0xfe8ac4a6, 0xf381caaf, 0xb8e890d8, 0xb5e39ed1, 0xa2fe8cca, 0xaff582c3, 0x8cc4a8fc, 0x81cfa6f5, 0x96d2b4ee, 0x9bd9bae7, 0xbb7bdb3b, 0xb670d532, 0xa16dc729, 0xac66c920, 0x8f57e31f, 0x825ced16, 0x9541ff0d, 0x984af104, 0xd323ab73, 0xde28a57a, 0xc935b761, 0xc43eb968, 0xe70f9357, 0xea049d5e, 0xfd198f45, 0xf012814c, 0x6bcb3bab, 0x66c035a2, 0x71dd27b9, 0x7cd629b0, 0x5fe7038f, 0x52ec0d86, 0x45f11f9d, 0x48fa1194, 0x03934be3, 0x0e9845ea, 0x198557f1, 0x148e59f8, 0x37bf73c7, 0x3ab47dce, 0x2da96fd5, 0x20a261dc, 0x6df6ad76, 0x60fda37f, 0x77e0b164, 0x7aebbf6d, 0x59da9552, 0x54d19b5b, 0x43cc8940, 0x4ec78749, 0x05aedd3e, 0x08a5d337, 0x1fb8c12c, 0x12b3cf25, 0x3182e51a, 0x3c89eb13, 0x2b94f908, 0x269ff701, 0xbd464de6, 0xb04d43ef, 0xa75051f4, 0xaa5b5ffd, 0x896a75c2, 0x84617bcb, 0x937c69d0, 0x9e7767d9, 0xd51e3dae, 0xd81533a7, 0xcf0821bc, 0xc2032fb5, 0xe132058a, 0xec390b83, 0xfb241998, 0xf62f1791, 0xd68d764d, 0xdb867844, 0xcc9b6a5f, 0xc1906456, 0xe2a14e69, 0xefaa4060, 0xf8b7527b, 0xf5bc5c72, 0xbed50605, 0xb3de080c, 0xa4c31a17, 0xa9c8141e, 0x8af93e21, 0x87f23028, 0x90ef2233, 0x9de42c3a, 0x063d96dd, 0x0b3698d4, 0x1c2b8acf, 0x112084c6, 0x3211aef9, 0x3f1aa0f0, 0x2807b2eb, 0x250cbce2, 0x6e65e695, 0x636ee89c, 0x7473fa87, 0x7978f48e, 0x5a49deb1, 0x5742d0b8, 0x405fc2a3, 0x4d54ccaa, 0xdaf741ec, 0xd7fc4fe5, 0xc0e15dfe, 0xcdea53f7, 0xeedb79c8, 0xe3d077c1, 0xf4cd65da, 0xf9c66bd3, 0xb2af31a4, 0xbfa43fad, 0xa8b92db6, 0xa5b223bf, 0x86830980, 0x8b880789, 0x9c951592, 0x919e1b9b, 0x0a47a17c, 0x074caf75, 0x1051bd6e, 0x1d5ab367, 0x3e6b9958, 0x33609751, 0x247d854a, 0x29768b43, 0x621fd134, 0x6f14df3d, 0x7809cd26, 0x7502c32f, 0x5633e910, 0x5b38e719, 0x4c25f502, 0x412efb0b, 0x618c9ad7, 0x6c8794de, 0x7b9a86c5, 0x769188cc, 0x55a0a2f3, 0x58abacfa, 0x4fb6bee1, 0x42bdb0e8, 0x09d4ea9f, 0x04dfe496, 0x13c2f68d, 0x1ec9f884, 0x3df8d2bb, 0x30f3dcb2, 0x27eecea9, 0x2ae5c0a0, 0xb13c7a47, 0xbc37744e, 0xab2a6655, 0xa621685c, 0x85104263, 0x881b4c6a, 0x9f065e71, 0x920d5078, 0xd9640a0f, 0xd46f0406, 0xc372161d, 0xce791814, 0xed48322b, 0xe0433c22, 0xf75e2e39, 0xfa552030, 0xb701ec9a, 0xba0ae293, 0xad17f088, 0xa01cfe81, 0x832dd4be, 0x8e26dab7, 0x993bc8ac, 0x9430c6a5, 0xdf599cd2, 0xd25292db, 0xc54f80c0, 0xc8448ec9, 0xeb75a4f6, 0xe67eaaff, 0xf163b8e4, 0xfc68b6ed, 0x67b10c0a, 0x6aba0203, 0x7da71018, 0x70ac1e11, 0x539d342e, 0x5e963a27, 0x498b283c, 0x44802635, 0x0fe97c42, 0x02e2724b, 0x15ff6050, 0x18f46e59, 0x3bc54466, 0x36ce4a6f, 0x21d35874, 0x2cd8567d, 0x0c7a37a1, 0x017139a8, 0x166c2bb3, 0x1b6725ba, 0x38560f85, 0x355d018c, 0x22401397, 0x2f4b1d9e, 0x642247e9, 0x692949e0, 0x7e345bfb, 0x733f55f2, 0x500e7fcd, 0x5d0571c4, 0x4a1863df, 0x47136dd6, 0xdccad731, 0xd1c1d938, 0xc6dccb23, 0xcbd7c52a, 0xe8e6ef15, 0xe5ede11c, 0xf2f0f307, 0xfffbfd0e, 0xb492a779, 0xb999a970, 0xae84bb6b, 0xa38fb562, 0x80be9f5d, 0x8db59154, 0x9aa8834f, 0x97a38d46];
    var U4 = [0x00000000, 0x090d0b0e, 0x121a161c, 0x1b171d12, 0x24342c38, 0x2d392736, 0x362e3a24, 0x3f23312a, 0x48685870, 0x4165537e, 0x5a724e6c, 0x537f4562, 0x6c5c7448, 0x65517f46, 0x7e466254, 0x774b695a, 0x90d0b0e0, 0x99ddbbee, 0x82caa6fc, 0x8bc7adf2, 0xb4e49cd8, 0xbde997d6, 0xa6fe8ac4, 0xaff381ca, 0xd8b8e890, 0xd1b5e39e, 0xcaa2fe8c, 0xc3aff582, 0xfc8cc4a8, 0xf581cfa6, 0xee96d2b4, 0xe79bd9ba, 0x3bbb7bdb, 0x32b670d5, 0x29a16dc7, 0x20ac66c9, 0x1f8f57e3, 0x16825ced, 0x0d9541ff, 0x04984af1, 0x73d323ab, 0x7ade28a5, 0x61c935b7, 0x68c43eb9, 0x57e70f93, 0x5eea049d, 0x45fd198f, 0x4cf01281, 0xab6bcb3b, 0xa266c035, 0xb971dd27, 0xb07cd629, 0x8f5fe703, 0x8652ec0d, 0x9d45f11f, 0x9448fa11, 0xe303934b, 0xea0e9845, 0xf1198557, 0xf8148e59, 0xc737bf73, 0xce3ab47d, 0xd52da96f, 0xdc20a261, 0x766df6ad, 0x7f60fda3, 0x6477e0b1, 0x6d7aebbf, 0x5259da95, 0x5b54d19b, 0x4043cc89, 0x494ec787, 0x3e05aedd, 0x3708a5d3, 0x2c1fb8c1, 0x2512b3cf, 0x1a3182e5, 0x133c89eb, 0x082b94f9, 0x01269ff7, 0xe6bd464d, 0xefb04d43, 0xf4a75051, 0xfdaa5b5f, 0xc2896a75, 0xcb84617b, 0xd0937c69, 0xd99e7767, 0xaed51e3d, 0xa7d81533, 0xbccf0821, 0xb5c2032f, 0x8ae13205, 0x83ec390b, 0x98fb2419, 0x91f62f17, 0x4dd68d76, 0x44db8678, 0x5fcc9b6a, 0x56c19064, 0x69e2a14e, 0x60efaa40, 0x7bf8b752, 0x72f5bc5c, 0x05bed506, 0x0cb3de08, 0x17a4c31a, 0x1ea9c814, 0x218af93e, 0x2887f230, 0x3390ef22, 0x3a9de42c, 0xdd063d96, 0xd40b3698, 0xcf1c2b8a, 0xc6112084, 0xf93211ae, 0xf03f1aa0, 0xeb2807b2, 0xe2250cbc, 0x956e65e6, 0x9c636ee8, 0x877473fa, 0x8e7978f4, 0xb15a49de, 0xb85742d0, 0xa3405fc2, 0xaa4d54cc, 0xecdaf741, 0xe5d7fc4f, 0xfec0e15d, 0xf7cdea53, 0xc8eedb79, 0xc1e3d077, 0xdaf4cd65, 0xd3f9c66b, 0xa4b2af31, 0xadbfa43f, 0xb6a8b92d, 0xbfa5b223, 0x80868309, 0x898b8807, 0x929c9515, 0x9b919e1b, 0x7c0a47a1, 0x75074caf, 0x6e1051bd, 0x671d5ab3, 0x583e6b99, 0x51336097, 0x4a247d85, 0x4329768b, 0x34621fd1, 0x3d6f14df, 0x267809cd, 0x2f7502c3, 0x105633e9, 0x195b38e7, 0x024c25f5, 0x0b412efb, 0xd7618c9a, 0xde6c8794, 0xc57b9a86, 0xcc769188, 0xf355a0a2, 0xfa58abac, 0xe14fb6be, 0xe842bdb0, 0x9f09d4ea, 0x9604dfe4, 0x8d13c2f6, 0x841ec9f8, 0xbb3df8d2, 0xb230f3dc, 0xa927eece, 0xa02ae5c0, 0x47b13c7a, 0x4ebc3774, 0x55ab2a66, 0x5ca62168, 0x63851042, 0x6a881b4c, 0x719f065e, 0x78920d50, 0x0fd9640a, 0x06d46f04, 0x1dc37216, 0x14ce7918, 0x2bed4832, 0x22e0433c, 0x39f75e2e, 0x30fa5520, 0x9ab701ec, 0x93ba0ae2, 0x88ad17f0, 0x81a01cfe, 0xbe832dd4, 0xb78e26da, 0xac993bc8, 0xa59430c6, 0xd2df599c, 0xdbd25292, 0xc0c54f80, 0xc9c8448e, 0xf6eb75a4, 0xffe67eaa, 0xe4f163b8, 0xedfc68b6, 0x0a67b10c, 0x036aba02, 0x187da710, 0x1170ac1e, 0x2e539d34, 0x275e963a, 0x3c498b28, 0x35448026, 0x420fe97c, 0x4b02e272, 0x5015ff60, 0x5918f46e, 0x663bc544, 0x6f36ce4a, 0x7421d358, 0x7d2cd856, 0xa10c7a37, 0xa8017139, 0xb3166c2b, 0xba1b6725, 0x8538560f, 0x8c355d01, 0x97224013, 0x9e2f4b1d, 0xe9642247, 0xe0692949, 0xfb7e345b, 0xf2733f55, 0xcd500e7f, 0xc45d0571, 0xdf4a1863, 0xd647136d, 0x31dccad7, 0x38d1c1d9, 0x23c6dccb, 0x2acbd7c5, 0x15e8e6ef, 0x1ce5ede1, 0x07f2f0f3, 0x0efffbfd, 0x79b492a7, 0x70b999a9, 0x6bae84bb, 0x62a38fb5, 0x5d80be9f, 0x548db591, 0x4f9aa883, 0x4697a38d];

    function convertToInt32(bytes) {
        var result = [];
        for (var i = 0; i < bytes.length; i += 4) {
            result.push(
                (bytes[i    ] << 24) |
                (bytes[i + 1] << 16) |
                (bytes[i + 2] <<  8) |
                 bytes[i + 3]
            );
        }
        return result;
    }

    var AES = function(key) {
        if (!(this instanceof AES)) {
            throw Error('AES must be instanitated with `new`');
        }

        Object.defineProperty(this, 'key', {
            value: coerceArray(key, true)
        });

        this._prepare();
    }


    AES.prototype._prepare = function() {

        var rounds = numberOfRounds[this.key.length];
        if (rounds == null) {
            throw new Error('invalid key size (must be 16, 24 or 32 bytes)');
        }

        // encryption round keys
        this._Ke = [];

        // decryption round keys
        this._Kd = [];

        for (var i = 0; i <= rounds; i++) {
            this._Ke.push([0, 0, 0, 0]);
            this._Kd.push([0, 0, 0, 0]);
        }

        var roundKeyCount = (rounds + 1) * 4;
        var KC = this.key.length / 4;

        // convert the key into ints
        var tk = convertToInt32(this.key);

        // copy values into round key arrays
        var index;
        for (var i = 0; i < KC; i++) {
            index = i >> 2;
            this._Ke[index][i % 4] = tk[i];
            this._Kd[rounds - index][i % 4] = tk[i];
        }

        // key expansion (fips-197 section 5.2)
        var rconpointer = 0;
        var t = KC, tt;
        while (t < roundKeyCount) {
            tt = tk[KC - 1];
            tk[0] ^= ((S[(tt >> 16) & 0xFF] << 24) ^
                      (S[(tt >>  8) & 0xFF] << 16) ^
                      (S[ tt        & 0xFF] <<  8) ^
                       S[(tt >> 24) & 0xFF]        ^
                      (rcon[rconpointer] << 24));
            rconpointer += 1;

            // key expansion (for non-256 bit)
            if (KC != 8) {
                for (var i = 1; i < KC; i++) {
                    tk[i] ^= tk[i - 1];
                }

            // key expansion for 256-bit keys is "slightly different" (fips-197)
            } else {
                for (var i = 1; i < (KC / 2); i++) {
                    tk[i] ^= tk[i - 1];
                }
                tt = tk[(KC / 2) - 1];

                tk[KC / 2] ^= (S[ tt        & 0xFF]        ^
                              (S[(tt >>  8) & 0xFF] <<  8) ^
                              (S[(tt >> 16) & 0xFF] << 16) ^
                              (S[(tt >> 24) & 0xFF] << 24));

                for (var i = (KC / 2) + 1; i < KC; i++) {
                    tk[i] ^= tk[i - 1];
                }
            }

            // copy values into round key arrays
            var i = 0, r, c;
            while (i < KC && t < roundKeyCount) {
                r = t >> 2;
                c = t % 4;
                this._Ke[r][c] = tk[i];
                this._Kd[rounds - r][c] = tk[i++];
                t++;
            }
        }

        // inverse-cipher-ify the decryption round key (fips-197 section 5.3)
        for (var r = 1; r < rounds; r++) {
            for (var c = 0; c < 4; c++) {
                tt = this._Kd[r][c];
                this._Kd[r][c] = (U1[(tt >> 24) & 0xFF] ^
                                  U2[(tt >> 16) & 0xFF] ^
                                  U3[(tt >>  8) & 0xFF] ^
                                  U4[ tt        & 0xFF]);
            }
        }
    }

    AES.prototype.encrypt = function(plaintext) {
        if (plaintext.length != 16) {
            throw new Error('invalid plaintext size (must be 16 bytes)');
        }

        var rounds = this._Ke.length - 1;
        var a = [0, 0, 0, 0];

        // convert plaintext to (ints ^ key)
        var t = convertToInt32(plaintext);
        for (var i = 0; i < 4; i++) {
            t[i] ^= this._Ke[0][i];
        }

        // apply round transforms
        for (var r = 1; r < rounds; r++) {
            for (var i = 0; i < 4; i++) {
                a[i] = (T1[(t[ i         ] >> 24) & 0xff] ^
                        T2[(t[(i + 1) % 4] >> 16) & 0xff] ^
                        T3[(t[(i + 2) % 4] >>  8) & 0xff] ^
                        T4[ t[(i + 3) % 4]        & 0xff] ^
                        this._Ke[r][i]);
            }
            t = a.slice();
        }

        // the last round is special
        var result = createArray(16), tt;
        for (var i = 0; i < 4; i++) {
            tt = this._Ke[rounds][i];
            result[4 * i    ] = (S[(t[ i         ] >> 24) & 0xff] ^ (tt >> 24)) & 0xff;
            result[4 * i + 1] = (S[(t[(i + 1) % 4] >> 16) & 0xff] ^ (tt >> 16)) & 0xff;
            result[4 * i + 2] = (S[(t[(i + 2) % 4] >>  8) & 0xff] ^ (tt >>  8)) & 0xff;
            result[4 * i + 3] = (S[ t[(i + 3) % 4]        & 0xff] ^  tt       ) & 0xff;
        }

        return result;
    }

    AES.prototype.decrypt = function(ciphertext) {
        if (ciphertext.length != 16) {
            throw new Error('invalid ciphertext size (must be 16 bytes)');
        }

        var rounds = this._Kd.length - 1;
        var a = [0, 0, 0, 0];

        // convert plaintext to (ints ^ key)
        var t = convertToInt32(ciphertext);
        for (var i = 0; i < 4; i++) {
            t[i] ^= this._Kd[0][i];
        }

        // apply round transforms
        for (var r = 1; r < rounds; r++) {
            for (var i = 0; i < 4; i++) {
                a[i] = (T5[(t[ i          ] >> 24) & 0xff] ^
                        T6[(t[(i + 3) % 4] >> 16) & 0xff] ^
                        T7[(t[(i + 2) % 4] >>  8) & 0xff] ^
                        T8[ t[(i + 1) % 4]        & 0xff] ^
                        this._Kd[r][i]);
            }
            t = a.slice();
        }

        // the last round is special
        var result = createArray(16), tt;
        for (var i = 0; i < 4; i++) {
            tt = this._Kd[rounds][i];
            result[4 * i    ] = (Si[(t[ i         ] >> 24) & 0xff] ^ (tt >> 24)) & 0xff;
            result[4 * i + 1] = (Si[(t[(i + 3) % 4] >> 16) & 0xff] ^ (tt >> 16)) & 0xff;
            result[4 * i + 2] = (Si[(t[(i + 2) % 4] >>  8) & 0xff] ^ (tt >>  8)) & 0xff;
            result[4 * i + 3] = (Si[ t[(i + 1) % 4]        & 0xff] ^  tt       ) & 0xff;
        }

        return result;
    }


    /**
     *  Mode Of Operation - Electonic Codebook (ECB)
     */
    var ModeOfOperationECB = function(key) {
        if (!(this instanceof ModeOfOperationECB)) {
            throw Error('AES must be instanitated with `new`');
        }

        this.description = "Electronic Code Block";
        this.name = "ecb";

        this._aes = new AES(key);
    }

    ModeOfOperationECB.prototype.encrypt = function(plaintext) {
        plaintext = coerceArray(plaintext);

        if ((plaintext.length % 16) !== 0) {
            throw new Error('invalid plaintext size (must be multiple of 16 bytes)');
        }

        var ciphertext = createArray(plaintext.length);
        var block = createArray(16);

        for (var i = 0; i < plaintext.length; i += 16) {
            copyArray(plaintext, block, 0, i, i + 16);
            block = this._aes.encrypt(block);
            copyArray(block, ciphertext, i);
        }

        return ciphertext;
    }

    ModeOfOperationECB.prototype.decrypt = function(ciphertext) {
        ciphertext = coerceArray(ciphertext);

        if ((ciphertext.length % 16) !== 0) {
            throw new Error('invalid ciphertext size (must be multiple of 16 bytes)');
        }

        var plaintext = createArray(ciphertext.length);
        var block = createArray(16);

        for (var i = 0; i < ciphertext.length; i += 16) {
            copyArray(ciphertext, block, 0, i, i + 16);
            block = this._aes.decrypt(block);
            copyArray(block, plaintext, i);
        }

        return plaintext;
    }


    /**
     *  Mode Of Operation - Cipher Block Chaining (CBC)
     */
    var ModeOfOperationCBC = function(key, iv) {
        if (!(this instanceof ModeOfOperationCBC)) {
            throw Error('AES must be instanitated with `new`');
        }

        this.description = "Cipher Block Chaining";
        this.name = "cbc";

        if (!iv) {
            iv = createArray(16);

        } else if (iv.length != 16) {
            throw new Error('invalid initialation vector size (must be 16 bytes)');
        }

        this._lastCipherblock = coerceArray(iv, true);

        this._aes = new AES(key);
    }

    ModeOfOperationCBC.prototype.encrypt = function(plaintext) {
        plaintext = coerceArray(plaintext);

        if ((plaintext.length % 16) !== 0) {
            throw new Error('invalid plaintext size (must be multiple of 16 bytes)');
        }

        var ciphertext = createArray(plaintext.length);
        var block = createArray(16);

        for (var i = 0; i < plaintext.length; i += 16) {
            copyArray(plaintext, block, 0, i, i + 16);

            for (var j = 0; j < 16; j++) {
                block[j] ^= this._lastCipherblock[j];
            }

            this._lastCipherblock = this._aes.encrypt(block);
            copyArray(this._lastCipherblock, ciphertext, i);
        }

        return ciphertext;
    }

    ModeOfOperationCBC.prototype.decrypt = function(ciphertext) {
        ciphertext = coerceArray(ciphertext);

        if ((ciphertext.length % 16) !== 0) {
            throw new Error('invalid ciphertext size (must be multiple of 16 bytes)');
        }

        var plaintext = createArray(ciphertext.length);
        var block = createArray(16);

        for (var i = 0; i < ciphertext.length; i += 16) {
            copyArray(ciphertext, block, 0, i, i + 16);
            block = this._aes.decrypt(block);

            for (var j = 0; j < 16; j++) {
                plaintext[i + j] = block[j] ^ this._lastCipherblock[j];
            }

            copyArray(ciphertext, this._lastCipherblock, 0, i, i + 16);
        }

        return plaintext;
    }


    /**
     *  Mode Of Operation - Cipher Feedback (CFB)
     */
    var ModeOfOperationCFB = function(key, iv, segmentSize) {
        if (!(this instanceof ModeOfOperationCFB)) {
            throw Error('AES must be instanitated with `new`');
        }

        this.description = "Cipher Feedback";
        this.name = "cfb";

        if (!iv) {
            iv = createArray(16);

        } else if (iv.length != 16) {
            throw new Error('invalid initialation vector size (must be 16 size)');
        }

        if (!segmentSize) { segmentSize = 1; }

        this.segmentSize = segmentSize;

        this._shiftRegister = coerceArray(iv, true);

        this._aes = new AES(key);
    }

    ModeOfOperationCFB.prototype.encrypt = function(plaintext) {
        if ((plaintext.length % this.segmentSize) != 0) {
            throw new Error('invalid plaintext size (must be segmentSize bytes)');
        }

        var encrypted = coerceArray(plaintext, true);

        var xorSegment;
        for (var i = 0; i < encrypted.length; i += this.segmentSize) {
            xorSegment = this._aes.encrypt(this._shiftRegister);
            for (var j = 0; j < this.segmentSize; j++) {
                encrypted[i + j] ^= xorSegment[j];
            }

            // Shift the register
            copyArray(this._shiftRegister, this._shiftRegister, 0, this.segmentSize);
            copyArray(encrypted, this._shiftRegister, 16 - this.segmentSize, i, i + this.segmentSize);
        }

        return encrypted;
    }

    ModeOfOperationCFB.prototype.decrypt = function(ciphertext) {
        if ((ciphertext.length % this.segmentSize) != 0) {
            throw new Error('invalid ciphertext size (must be segmentSize bytes)');
        }

        var plaintext = coerceArray(ciphertext, true);

        var xorSegment;
        for (var i = 0; i < plaintext.length; i += this.segmentSize) {
            xorSegment = this._aes.encrypt(this._shiftRegister);

            for (var j = 0; j < this.segmentSize; j++) {
                plaintext[i + j] ^= xorSegment[j];
            }

            // Shift the register
            copyArray(this._shiftRegister, this._shiftRegister, 0, this.segmentSize);
            copyArray(ciphertext, this._shiftRegister, 16 - this.segmentSize, i, i + this.segmentSize);
        }

        return plaintext;
    }

    /**
     *  Mode Of Operation - Output Feedback (OFB)
     */
    var ModeOfOperationOFB = function(key, iv) {
        if (!(this instanceof ModeOfOperationOFB)) {
            throw Error('AES must be instanitated with `new`');
        }

        this.description = "Output Feedback";
        this.name = "ofb";

        if (!iv) {
            iv = createArray(16);

        } else if (iv.length != 16) {
            throw new Error('invalid initialation vector size (must be 16 bytes)');
        }

        this._lastPrecipher = coerceArray(iv, true);
        this._lastPrecipherIndex = 16;

        this._aes = new AES(key);
    }

    ModeOfOperationOFB.prototype.encrypt = function(plaintext) {
        var encrypted = coerceArray(plaintext, true);

        for (var i = 0; i < encrypted.length; i++) {
            if (this._lastPrecipherIndex === 16) {
                this._lastPrecipher = this._aes.encrypt(this._lastPrecipher);
                this._lastPrecipherIndex = 0;
            }
            encrypted[i] ^= this._lastPrecipher[this._lastPrecipherIndex++];
        }

        return encrypted;
    }

    // Decryption is symetric
    ModeOfOperationOFB.prototype.decrypt = ModeOfOperationOFB.prototype.encrypt;


    /**
     *  Counter object for CTR common mode of operation
     */
    var Counter = function(initialValue) {
        if (!(this instanceof Counter)) {
            throw Error('Counter must be instanitated with `new`');
        }

        // We allow 0, but anything false-ish uses the default 1
        if (initialValue !== 0 && !initialValue) { initialValue = 1; }

        if (typeof(initialValue) === 'number') {
            this._counter = createArray(16);
            this.setValue(initialValue);

        } else {
            this.setBytes(initialValue);
        }
    }

    Counter.prototype.setValue = function(value) {
        if (typeof(value) !== 'number' || parseInt(value) != value) {
            throw new Error('invalid counter value (must be an integer)');
        }

        // We cannot safely handle numbers beyond the safe range for integers
        if (value > Number.MAX_SAFE_INTEGER) {
            throw new Error('integer value out of safe range');
        }

        for (var index = 15; index >= 0; --index) {
            this._counter[index] = value % 256;
            value = parseInt(value / 256);
        }
    }

    Counter.prototype.setBytes = function(bytes) {
        bytes = coerceArray(bytes, true);

        if (bytes.length != 16) {
            throw new Error('invalid counter bytes size (must be 16 bytes)');
        }

        this._counter = bytes;
    };

    Counter.prototype.increment = function() {
        for (var i = 15; i >= 0; i--) {
            if (this._counter[i] === 255) {
                this._counter[i] = 0;
            } else {
                this._counter[i]++;
                break;
            }
        }
    }


    /**
     *  Mode Of Operation - Counter (CTR)
     */
    var ModeOfOperationCTR = function(key, counter) {
        if (!(this instanceof ModeOfOperationCTR)) {
            throw Error('AES must be instanitated with `new`');
        }

        this.description = "Counter";
        this.name = "ctr";

        if (!(counter instanceof Counter)) {
            counter = new Counter(counter)
        }

        this._counter = counter;

        this._remainingCounter = null;
        this._remainingCounterIndex = 16;

        this._aes = new AES(key);
    }

    ModeOfOperationCTR.prototype.encrypt = function(plaintext) {
        var encrypted = coerceArray(plaintext, true);

        for (var i = 0; i < encrypted.length; i++) {
            if (this._remainingCounterIndex === 16) {
                this._remainingCounter = this._aes.encrypt(this._counter._counter);
                this._remainingCounterIndex = 0;
                this._counter.increment();
            }
            encrypted[i] ^= this._remainingCounter[this._remainingCounterIndex++];
        }

        return encrypted;
    }

    // Decryption is symetric
    ModeOfOperationCTR.prototype.decrypt = ModeOfOperationCTR.prototype.encrypt;


    ///////////////////////
    // Padding

    // See:https://tools.ietf.org/html/rfc2315
    function pkcs7pad(data) {
        data = coerceArray(data, true);
        var padder = 16 - (data.length % 16);
        var result = createArray(data.length + padder);
        copyArray(data, result);
        for (var i = data.length; i < result.length; i++) {
            result[i] = padder;
        }
        return result;
    }

    function pkcs7strip(data) {
        data = coerceArray(data, true);
        if (data.length < 16) { throw new Error('PKCS#7 invalid length'); }

        var padder = data[data.length - 1];
        if (padder > 16) { throw new Error('PKCS#7 padding byte out of range'); }

        var length = data.length - padder;
        for (var i = 0; i < padder; i++) {
            if (data[length + i] !== padder) {
                throw new Error('PKCS#7 invalid padding byte');
            }
        }

        var result = createArray(length);
        copyArray(data, result, 0, 0, length);
        return result;
    }

    ///////////////////////
    // Exporting


    // The block cipher
    var aesjs = {
        AES: AES,
        Counter: Counter,

        ModeOfOperation: {
            ecb: ModeOfOperationECB,
            cbc: ModeOfOperationCBC,
            cfb: ModeOfOperationCFB,
            ofb: ModeOfOperationOFB,
            ctr: ModeOfOperationCTR
        },

        utils: {
            hex: convertHex,
            utf8: convertUtf8
        },

        padding: {
            pkcs7: {
                pad: pkcs7pad,
                strip: pkcs7strip
            }
        },

        _arrayTest: {
            coerceArray: coerceArray,
            createArray: createArray,
            copyArray: copyArray,
        }
    };


    // node.js
    if (typeof exports !== 'undefined') {
        window.aesjs = aesjs

    // RequireJS/AMD
    // http://www.requirejs.org/docs/api.html
    // https://github.com/amdjs/amdjs-api/wiki/AMD
    } else if (typeof(define) === 'function' && define.amd) {
        define([], function() { return aesjs; });

    // Web Browsers
    } else {

        // If there was an existing library at "aesjs" make sure it's still available
        if (root.aesjs) {
            aesjs._aesjs = root.aesjs;
        }

        root.aesjs = aesjs;
    }


})(this);

})();

// SHA-256 (pure JS)
function sha256(msgBytes) {
    const K = [
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    ];
    let h = [0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
    const msg = Array.from(msgBytes);
    const origLen = msg.length;
    msg.push(0x80);
    while (msg.length % 64 !== 56) msg.push(0);
    const bits = origLen * 8;
    for (let i = 7; i >= 0; i--) msg.push((bits / Math.pow(2, i * 8)) & 0xff);
    for (let i = 0; i < msg.length; i += 64) {
        const w = [];
        for (let j = 0; j < 16; j++)
            w[j] = (msg[i+j*4]<<24)|(msg[i+j*4+1]<<16)|(msg[i+j*4+2]<<8)|msg[i+j*4+3];
        for (let j = 16; j < 64; j++) {
            const s0 = rotr32(w[j-15],7)^rotr32(w[j-15],18)^(w[j-15]>>>3);
            const s1 = rotr32(w[j-2],17)^rotr32(w[j-2],19)^(w[j-2]>>>10);
            w[j] = (w[j-16]+s0+w[j-7]+s1)|0;
        }
        let [a,b,c,d,e,f,g,hh] = h;
        for (let j = 0; j < 64; j++) {
            const S1  = rotr32(e,6)^rotr32(e,11)^rotr32(e,25);
            const ch  = (e&f)^(~e&g);
            const t1  = (hh+S1+ch+K[j]+w[j])|0;
            const S0  = rotr32(a,2)^rotr32(a,13)^rotr32(a,22);
            const maj = (a&b)^(a&c)^(b&c);
            const t2  = (S0+maj)|0;
            hh=g; g=f; f=e; e=(d+t1)|0; d=c; c=b; b=a; a=(t1+t2)|0;
        }
        h = [h[0]+a,h[1]+b,h[2]+c,h[3]+d,h[4]+e,h[5]+f,h[6]+g,h[7]+hh].map(v=>v|0);
    }
    return h.flatMap(v => [(v>>>24)&0xff,(v>>>16)&0xff,(v>>>8)&0xff,v&0xff]);
}

function rotr32(x, n) { return (x >>> n) | (x << (32 - n)); }

function strToBytes(s) {
    const out = [];
    for (let i = 0; i < s.length; i++) {
        let c = s.charCodeAt(i);
        if (c < 0x80) { out.push(c); }
        else if (c < 0x800) { out.push(0xc0|(c>>6), 0x80|(c&0x3f)); }
        else { out.push(0xe0|(c>>12), 0x80|((c>>6)&0x3f), 0x80|(c&0x3f)); }
    }
    return out;
}

function hmacSHA256hex(keyStr, msgStr) {
    const keyBytes = strToBytes(keyStr);
    const msgBytes = strToBytes(msgStr);
    let k = keyBytes.length > 64 ? sha256(keyBytes) : keyBytes.slice();
    while (k.length < 64) k.push(0);
    const ipad = k.map(b => b ^ 0x36);
    const opad = k.map(b => b ^ 0x5c);
    const inner = sha256([...ipad, ...msgBytes]);
    const outer = sha256([...opad, ...inner]);
    return outer.map(b => b.toString(16).padStart(2, '0')).join('');
}

let _cachedKeyBytes = null;
let _cachedKeyStr   = null;

function getAESKeyBytes() {
    const k = getApiKey();
    if (_cachedKeyBytes && _cachedKeyStr === k) return _cachedKeyBytes;
    _cachedKeyBytes = sha256(strToBytes(k));
    _cachedKeyStr   = k;
    return _cachedKeyBytes;
}

// Decrypt AES-256-CTR + HMAC-SHA256 payload from firmware.
// Format: base64( iv[16] + ciphertext[n] + hmac[32] )
function decryptResponse(base64Text) {
    const raw  = Uint8Array.from(atob(base64Text), c => c.charCodeAt(0));
    if (raw.length < 48) throw new Error('Payload too short');
    const iv         = Array.from(raw.slice(0, 16));
    const ciphertext = Array.from(raw.slice(16, raw.length - 32));
    const tag        = Array.from(raw.slice(raw.length - 32));
    const keyBytes   = getAESKeyBytes();

    // Verify HMAC(key, iv || ciphertext)
    const ivHex     = iv.map(b => b.toString(16).padStart(2,'0')).join('');
    const cipherHex = ciphertext.map(b => b.toString(16).padStart(2,'0')).join('');
    const keyStr    = getApiKey();
    // HMAC over raw bytes -- compute using sha256 directly
    let k = keyBytes.length > 64 ? sha256(keyBytes) : keyBytes.slice();
    while (k.length < 64) k.push(0);
    const ipad = k.map(b => b ^ 0x36);
    const opad = k.map(b => b ^ 0x5c);
    const macInput = [...ipad, ...iv, ...ciphertext];
    const inner = sha256(macInput);
    const outer = sha256([...opad, ...inner]);
    let diff = 0;
    for (let i = 0; i < 32; i++) diff |= outer[i] ^ tag[i];
    if (diff !== 0) throw new Error('HMAC verification failed -- wrong API key');

    // Decrypt CTR -- counter starts at iv with byte 15 set to 1
    const ctrIv = iv.slice();
    ctrIv[15] = (ctrIv[15] & 0xfe) | 0x01;
    const ctr = new aesjs.ModeOfOperation.ctr(keyBytes, new aesjs.Counter(ctrIv));
    const plain = ctr.decrypt(ciphertext);
    return aesjs.utils.utf8.fromBytes(plain);
}

function encryptBody(jsonStr) {
    const keyBytes  = getAESKeyBytes();
    const iv        = Array.from({length: 16}, () => Math.random() * 256 | 0);
    const ctrIv     = iv.slice();
    ctrIv[15]       = (ctrIv[15] & 0xfe) | 0x01;
    const plainBytes = aesjs.utils.utf8.toBytes(jsonStr);

    const ctr        = new aesjs.ModeOfOperation.ctr(keyBytes, new aesjs.Counter(ctrIv));
    const ciphertext = Array.from(ctr.encrypt(plainBytes));

    // HMAC-SHA256(aesKey, iv || ciphertext)
    let k = keyBytes.slice();
    while (k.length < 64) k.push(0);
    const inner = sha256([...k.map(b => b ^ 0x36), ...iv, ...ciphertext]);
    const tag   = sha256([...k.map(b => b ^ 0x5c), ...inner]);

    const blob  = new Uint8Array([...iv, ...ciphertext, ...tag]);
    return btoa(String.fromCharCode(...blob));
}

function hmacAuth(nonce) {
    return hmacSHA256hex(getApiKey(), nonce);
}

// Serialization queue -- only one nonce/auth/request cycle at a time,
// since the device holds a single currentNonce.
let _apiQueue = Promise.resolve();

function _enqueue(fn) {
    _apiQueue = _apiQueue.then(fn, fn);
    return _apiQueue;
}

async function getNonce() {
    const endpoint = getApiEndpoint();
    const resp = await fetch(`${endpoint}/api/nonce`);
    const data = await resp.json();
    return data.nonce;
}

async function apiFetch(url, options = {}) {
    return _enqueue(() => _apiFetchInner(url, options));
}

async function _apiFetchInner(url, options = {}) {
    // Encrypt request body if present
    if (options.body && typeof options.body === 'string') {
        options.body = encryptBody(options.body);
        if (!options.headers) options.headers = {};
        options.headers['Content-Type'] = 'text/plain';
        options.headers['X-Encrypted']  = '1';
    }
    // Always fetch nonce and inject X-Auth inside the queue so it's fresh
    if (!options.headers) options.headers = {};
    if (!options.headers['X-Auth']) {
        const nonce   = await getNonce();
        const hmacKey = options._hmacKeyOverride || getApiKey();
        options.headers['X-Auth'] = hmacSHA256hex(hmacKey, nonce);
        if (!options.headers['Content-Type']) {
            options.headers['Content-Type'] = 'application/json';
        }
    }
    delete options._hmacKeyOverride;
    const resp = await fetch(url, options);
    const encrypted = resp.headers.get('X-Encrypted') === '1';
    const text = await resp.text();

    let data;
    if (encrypted) {
        try {
            data = JSON.parse(decryptResponse(text));
        } catch (e) {
            logDebug(`Decrypt error for ${url}: ${e.message}, encrypted header=${encrypted}, text[:40]=${text.substring(0,40)}`, 'error');
            return { ok: false, status: 403, statusText: 'Forbidden', _data: { error: 'Decryption failed -- API key may be incorrect' }, json: async () => ({ error: 'Decryption failed -- API key may be incorrect' }), text: async () => text };
        }
    } else {
        try { data = JSON.parse(text); } catch (e) { logDebug(`JSON parse error for ${url}: ${e.message}, text[:80]=${text.substring(0,80)}`, 'error'); data = { error: text }; }
    }

    return {
        ok: resp.ok,
        status: resp.status,
        statusText: resp.statusText,
        headers: resp.headers,
        _data: data,
        json: async () => data,
        text: async () => text
    };
}

async function authHeaders(extra = {}) {
    const nonce = await getNonce();
    const hmac  = hmacAuth(nonce);
    return { 'Content-Type': 'application/json', 'X-Auth': hmac, ...extra };
}

async function authHeadersRaw(extra = {}) {
    const nonce = await getNonce();
    const hmac  = hmacAuth(nonce);
    return { 'X-Auth': hmac, ...extra };
}

function saveApiKey() {
    const input = document.getElementById('apiKeyInput');
    if (!input || !input.value.trim()) return;
    const key = input.value.trim();
    if (key.length < 8) {
        logDebug('API key must be at least 8 characters', 'warning');
        return;
    }
    apiKey = key;
    localStorage.setItem('apiKey', key);
    input.value = '';
    input.placeholder = '(saved)';
    setTimeout(() => { input.placeholder = 'kprox1337'; }, 2000);
    logDebug('API key saved locally', 'success');
}

async function updateDeviceApiKey() {
    const currentKey = document.getElementById('currentApiKeyInput')?.value || getApiKey();
    const newKey     = document.getElementById('newApiKeyInput')?.value || '';
    const confirmKey = document.getElementById('newApiKeyConfirm')?.value || '';
    const statusEl   = document.getElementById('apiKeyStatus');

    if (newKey !== confirmKey) {
        if (statusEl) { statusEl.textContent = 'Keys do not match.'; statusEl.style.color = '#dc3545'; }
        return;
    }
    if (newKey.length < 8) {
        if (statusEl) { statusEl.textContent = 'API key must be at least 8 characters.'; statusEl.style.color = '#dc3545'; }
        return;
    }

    const endpoint = getApiEndpoint();
    try {
        // Pass old key override so _apiFetchInner uses it for HMAC instead of stored key
        const resp  = await apiFetch(`${endpoint}/api/settings`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ api_key: newKey }),
            _hmacKeyOverride: currentKey
        });
        const data = await resp.json();
        if (resp.ok) {
            apiKey = newKey;
            localStorage.setItem('apiKey', newKey);
            const localInput = document.getElementById('apiKeyInput');
            if (localInput) localInput.value = newKey;
            document.getElementById('currentApiKeyInput').value = '';
            document.getElementById('newApiKeyInput').value = '';
            document.getElementById('newApiKeyConfirm').value = '';
            if (statusEl) { statusEl.textContent = 'Device API key updated.'; statusEl.style.color = '#28a745'; }
        } else {
            if (statusEl) { statusEl.textContent = data.error || 'Update failed.'; statusEl.style.color = '#dc3545'; }
        }
    } catch (e) {
        if (statusEl) { statusEl.textContent = 'Error: ' + e.message; statusEl.style.color = '#dc3545'; }
    }
}


function updateApiEndpoint() {
    const endpoint = document.getElementById('apiEndpoint').value.trim();
    if (endpoint) {
        const cleanEndpoint = endpoint.replace(/\/$/, '');
        setCookie('apiEndpoint', cleanEndpoint);
        document.getElementById('apiEndpoint').value = cleanEndpoint;
        logDebug(`Endpoint saved: ${cleanEndpoint}`, 'success');

        isConnected = false;
        updateConnectionUI();
        connect();
    }
}

async function saveUtcOffset() {
    const input = document.getElementById('utcOffsetInput');
    const status = document.getElementById('utcOffsetStatus');
    const offset = parseInt(input.value, 10);
    if (isNaN(offset)) { status.textContent = 'Invalid value'; status.style.color = 'red'; return; }
    const endpoint = getApiEndpoint();
    try {
        const resp = await apiFetch(`${endpoint}/api/settings`, { method: 'POST', body: JSON.stringify({ utcOffset: offset }) });
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        status.textContent = 'Saved';
        status.style.color = 'green';
    } catch (e) {
        status.textContent = 'Failed: ' + e.message;
        status.style.color = 'red';
    }
}

function logDebug(message, type = 'info') {
    const debugInfo = document.getElementById('debugInfo');
    if (!debugInfo) return;
    
    const timestamp = new Date().toLocaleTimeString();
    debugInfo.style.display = 'block';

    let className = '';
    if (type === 'error') className = 'error';
    else if (type === 'success') className = 'success';
    else if (type === 'warning') className = 'warning';

    debugInfo.innerHTML += `<div class="${className}">[${timestamp}] ${message}</div>`;
    debugInfo.scrollTop = debugInfo.scrollHeight;
    console.log(`[${type.toUpperCase()}] ${message}`);
}

function clearDebugLog() {
    const debugInfo = document.getElementById('debugInfo');
    if (debugInfo) {
        debugInfo.innerHTML = '';
        debugInfo.style.display = 'none';
    }
    logDebug('Debug log cleared');
}

function updateBluetoothUI() {
    const bluetoothToggle = document.getElementById('bluetoothToggle');
    const bluetoothStatus = document.getElementById('bluetoothStatus');

    if (bluetoothToggle) {
        if (bluetoothEnabled) {
            bluetoothToggle.textContent = 'Disable Bluetooth';
            bluetoothToggle.classList.add('enabled');
        } else {
            bluetoothToggle.textContent = 'Enable Bluetooth';
            bluetoothToggle.classList.remove('enabled');
        }
    }

    if (bluetoothStatus) {
        if (bluetoothEnabled) {
            if (bluetoothConnected) {
                bluetoothStatus.textContent = 'Connected';
                bluetoothStatus.className = 'status-value connected';
            } else if (bluetoothInitialized) {
                bluetoothStatus.textContent = 'Enabled (Not Connected)';
                bluetoothStatus.className = 'status-value warning';
            } else {
                bluetoothStatus.textContent = 'Enabling...';
                bluetoothStatus.className = 'status-value warning';
            }
        } else {
            bluetoothStatus.textContent = 'Disabled';
            bluetoothStatus.className = 'status-value disconnected';
        }
    }
}

function updateWiFiStatus(data) {
    if (data.hasOwnProperty('ssid')) {
        safeSetText('wifiSSID', data.ssid || '-');
    }
    
    if (data.hasOwnProperty('connected')) {
        const status = data.connected ? 'Connected' : 'Disconnected';
        const className = data.connected ? 'status-value connected' : 'status-value disconnected';
        
        const statusElement = document.getElementById('wifiConnectionStatus');
        if (statusElement) {
            statusElement.textContent = status;
            statusElement.className = className;
        }
    }
    
    if (data.hasOwnProperty('rssi')) {
        const rssi = data.rssi;
        safeSetText('wifiRSSI', rssi ? `${rssi} dBm` : '-');
    }
}

async function connectToWiFi() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    const ssidInput = document.getElementById('newWifiSSID');
    const passwordInput = document.getElementById('newWifiPassword');
    
    if (!ssidInput || !passwordInput) return;
    
    const ssid = ssidInput.value.trim();
    const password = passwordInput.value.trim();
    
    if (!ssid) {
        logDebug('Please enter a WiFi SSID', 'warning');
        return;
    }

    const endpoint = getApiEndpoint();
    const payload = {
        ssid: ssid,
        password: password
    };

    try {
        const response = await apiFetch(`${endpoint}/api/wifi`, {
            method: 'POST',
            body: JSON.stringify(payload)
        });

        if (response.ok) {
            const result = await response.json();
            logDebug(result.message || 'WiFi connection attempted', 'success');
            
            ssidInput.value = '';
            passwordInput.value = '';
            
            setTimeout(() => {
                connect();
            }, 3000);
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to connect to WiFi: ${error.message}`, 'error');
    }
}

async function loadWiFiSettings() {
    if (!isConnected) {
        return;
    }

    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/wifi`, {
            method: 'GET',
            mode: 'cors',
        });

        if (response.ok) {
            const data = await response.json();
            logDebug(`WiFi data -- ${JSON.stringify(data).substring(0, 120)}`, 'info');
            safeSetText('wifiSSID', data.ssid || '-');
            
            if (data.hasOwnProperty('connected')) {
                const status = data.connected ? 'Connected' : 'Disconnected';
                const className = data.connected ? 'status-value connected' : 'status-value disconnected';
                
                const statusElement = document.getElementById('wifiConnectionStatus');
                if (statusElement) {
                    statusElement.textContent = status;
                    statusElement.className = className;
                }
            }
            
            if (data.hasOwnProperty('rssi')) {
                const rssi = data.rssi;
                safeSetText('wifiRSSI', rssi ? `${rssi} dBm` : '-');
            }

            const ssidInput = document.getElementById('newWifiSSID');
            if (ssidInput && data.ssid) {
                ssidInput.placeholder = `Current: ${data.ssid}`;
            }
        }
    } catch (error) {
        logDebug(`Failed to load WiFi settings: ${error.message}`, 'error');
    }
}

async function toggleBluetooth() {
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }
    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/bluetooth`, {
            method: 'POST',
            body: JSON.stringify({ enabled: !bluetoothEnabled })
        });
        if (response.ok) {
            bluetoothEnabled = !bluetoothEnabled;
            updateBluetoothUI();
            logDebug(`Bluetooth ${bluetoothEnabled ? 'enabled' : 'disabled'}`, 'success');
        } else { throw new Error(`HTTP ${response.status}`); }
    } catch (error) { logDebug(`Failed to toggle Bluetooth: ${error.message}`, 'error'); }
}

async function toggleUSB() {
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }
    const endpoint = getApiEndpoint();
    const curEnabled = document.getElementById('usbToggle')?.classList.contains('enabled') || false;
    try {
        const response = await apiFetch(`${endpoint}/api/usb`, {
            method: 'POST',
            body: JSON.stringify({ enabled: !curEnabled })
        });
        if (response.ok) {
            const btn = document.getElementById('usbToggle');
            if (btn) {
                const nowEnabled = !curEnabled;
                btn.textContent  = nowEnabled ? 'Disable USB HID' : 'Enable USB HID';
                btn.classList.toggle('enabled', nowEnabled);
            }
            logDebug(`USB HID ${!curEnabled ? 'enabled' : 'disabled'}`, 'success');
        } else { throw new Error(`HTTP ${response.status}`); }
    } catch (error) { logDebug(`Failed to toggle USB: ${error.message}`, 'error'); }
}

async function setBleSubEnable(key, value) {
    if (!isConnected) return;
    const endpoint = getApiEndpoint();
    try {
        await apiFetch(`${endpoint}/api/settings`, {
            method: 'POST',
            body: JSON.stringify({ bluetooth: { [key]: value } })
        });
    } catch(e) { logDebug('BLE sub-enable error: ' + e.message, 'error'); }
}

async function setUsbSubEnable(key, value) {
    if (!isConnected) return;
    const endpoint = getApiEndpoint();
    try {
        await apiFetch(`${endpoint}/api/settings`, {
            method: 'POST',
            body: JSON.stringify({ usb: { [key]: value } })
        });
    } catch(e) { logDebug('USB sub-enable error: ' + e.message, 'error'); }
}

// ---- Sink management ----

function updateSinkBadge(bytes) {
    const badge = document.getElementById('globalSinkBadge');
    if (!badge) return;
    if (bytes > 0) {
        badge.textContent = bytes + ' B';
        badge.style.background = '#198754';
        badge.style.color = '#fff';
    } else {
        badge.textContent = '0 B';
        badge.style.background = '#343a40';
        badge.style.color = '#adb5bd';
    }
}

async function sinkFlush() {
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }
    const endpoint = getApiEndpoint();
    try {
        const resp = await apiFetch(`${endpoint}/api/flush`, { method: 'POST', body: '{}' });
        const data = await resp.json();
        logDebug(`Flushed ${data.flushed || 0} bytes to HID`, 'success');
        updateSinkBadge(0);
    } catch(e) { logDebug('Flush error: ' + e.message, 'error'); }
}

async function sinkDelete() {
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }
    if (!confirm('Delete sink buffer without flushing to HID?')) return;
    const endpoint = getApiEndpoint();
    try {
        const resp = await apiFetch(`${endpoint}/api/sink_delete`, { method: 'POST', body: '{}' });
        const data = await resp.json();
        logDebug(`Deleted ${data.deleted || 0} bytes from sink`, 'success');
        updateSinkBadge(0);
    } catch(e) { logDebug('Sink delete error: ' + e.message, 'error'); }
}

let _csAutoLockInterval = null;
let _csAutoLockSecs     = 0;
let _csUnlockedAt       = null;

function _updateCsAutoLockBadge() {
    const badge   = document.getElementById('csAutoLockBadge');
    const gBadge  = document.getElementById('globalCsAutoLockBadge');
    if (_csAutoLockSecs <= 0 || !_csUnlockedAt) {
        if (badge)  badge.style.display  = 'none';
        if (gBadge) gBadge.style.display = 'none';
        return;
    }
    const elapsed    = Math.floor((Date.now() - _csUnlockedAt) / 1000);
    const remaining  = Math.max(0, _csAutoLockSecs - elapsed);
    const text = `🔒 ${remaining}s`;
    const bg   = remaining <= 10 ? '#856404' : '#0d4f8c';
    [badge, gBadge].forEach(el => {
        if (!el) return;
        if (remaining === 0) { el.style.display = 'none'; return; }
        el.style.display    = '';
        el.textContent      = text;
        el.style.background = bg;
    });
    if (remaining === 0 && _csAutoLockInterval) {
        clearInterval(_csAutoLockInterval); _csAutoLockInterval = null;
    }
}

function _startCsAutoLockCountdown(secs) {
    _csAutoLockSecs = secs;
    _csUnlockedAt   = Date.now();
    if (_csAutoLockInterval) clearInterval(_csAutoLockInterval);
    if (secs > 0) _csAutoLockInterval = setInterval(_updateCsAutoLockBadge, 1000);
    _updateCsAutoLockBadge();
}

function _stopCsAutoLockCountdown() {
    _csAutoLockSecs = 0; _csUnlockedAt = null;
    if (_csAutoLockInterval) { clearInterval(_csAutoLockInterval); _csAutoLockInterval = null; }
    ['csAutoLockBadge', 'globalCsAutoLockBadge'].forEach(id => {
        const el = document.getElementById(id);
        if (el) el.style.display = 'none';
    });
}

function _updateCsFailedBadge(fails, wipeAt) {
    const text = wipeAt > 0 ? `⚠ ${fails}/${wipeAt} fails` : `⚠ ${fails} fails`;
    const bg   = fails > 0 ? '#721c24' : '#495057';
    ['csFailedBadge', 'globalCsFailedBadge'].forEach(id => {
        const el = document.getElementById(id);
        if (!el) return;
        if (fails === 0 && wipeAt === 0) { el.style.display = 'none'; return; }
        el.style.display    = '';
        el.textContent      = text;
        el.style.background = bg;
    });
}

async function saveCsSecuritySettings() {
    if (!isConnected) return;
    const autoLock = parseInt(document.getElementById('csAutoLockInput')?.value) || 0;
    const autoWipe = parseInt(document.getElementById('csAutoWipeInput')?.value) || 0;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/settings`, {
            method: 'POST',
            body: JSON.stringify({ cs: { autoLockSecs: autoLock, autoWipeAttempts: autoWipe } })
        });
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        _statusMsg('csSecurityStatus', '✓ Saved', true);
        await loadSettingsTab();
    } catch(e) { _statusMsg('csSecurityStatus', '✗ ' + e.message, false); }
}

async function csResetFails() {
    if (!isConnected) return;
    try {
        await apiFetch(`${getApiEndpoint()}/api/settings`, {
            method: 'POST',
            body: JSON.stringify({ cs: { resetFailedAttempts: true } })
        });
        const el = document.getElementById('csFailedAttemptsVal');
        if (el) { el.textContent = '0'; el.style.color = '#28a745'; }
        _updateCsFailedBadge(0, parseInt(document.getElementById('csAutoWipeInput')?.value) || 0);
        _statusMsg('csSecurityStatus', '✓ Counter reset', true);
    } catch(e) { _statusMsg('csSecurityStatus', '✗ ' + e.message, false); }
}

async function saveSinkMaxSize() {
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }
    const val = parseInt(document.getElementById('sinkMaxSizeInput')?.value) || 0;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/settings`, {
            method: 'POST', body: JSON.stringify({ maxSinkSize: val })
        });
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        _statusMsg('sinkMaxStatus', `✓ Limit set to ${val === 0 ? 'unlimited' : val + ' bytes'}`, true);
        await refreshSinkSize();
    } catch(e) {
        _statusMsg('sinkMaxStatus', '✗ ' + e.message, false);
    }
}

async function refreshSinkSize() {
    if (!isConnected) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/sink_size`, { method: 'GET' });
        if (!resp.ok) return;
        const d = await resp.json();
        const cur = d.size ?? 0;
        const max = d.max_size ?? 0;
        const label = document.getElementById('sinkCurrentSize');
        if (label) {
            label.textContent = max > 0
                ? `${cur} / ${max} bytes`
                : `${cur} bytes (unlimited)`;
        }
        updateSinkBadge(cur);
        if (d.max_size !== undefined) _setVal('sinkMaxSizeInput', d.max_size);
    } catch(e) { /* silently skip */ }
}

function updateSinkCurlExamples() {
    const ip   = ipAddress;
    const host = deviceHostname;

    const ipEl   = document.getElementById('sinkCurlIp');
    const hostEl = document.getElementById('sinkCurlHostname');
    const fileEl = document.getElementById('sinkCurlFile');

    if (!ipEl || !hostEl || !fileEl) return;

    if (!ip) {
        const placeholder = 'Connect to device to generate command';
        ipEl.textContent = hostEl.textContent = fileEl.textContent = placeholder;
        return;
    }

    ipEl.textContent =
`curl -X POST http://${ip}/api/sink \\
  -H "Content-Type: text/plain" \\
  --data-binary "your data here"`;

    hostEl.textContent = host
        ? `curl -X POST http://${host}.local/api/sink \\\n  -H "Content-Type: text/plain" \\\n  --data-binary "your data here"`
        : '(hostname not available — enable mDNS in device settings)';

    fileEl.textContent =
`curl -X POST http://${ip}/api/sink \\
  -H "Content-Type: text/plain" \\
  --data-binary @yourfile.txt`;
}

function updateConnectivityUI(data) {
    // BT
    const btBtn = document.getElementById('bluetoothToggle');
    const btStatus = document.getElementById('bluetoothStatus');
    if (data.connections?.bluetooth) {
        const bt = data.connections.bluetooth;
        bluetoothEnabled = bt.enabled;
        if (btBtn) {
            btBtn.textContent = bt.enabled ? 'Disable Bluetooth' : 'Enable Bluetooth';
            btBtn.classList.toggle('enabled', bt.enabled);
        }
        if (btStatus) {
            btStatus.textContent = bt.connected ? 'Connected' : (bt.enabled ? 'No peer' : 'Disabled');
            btStatus.className = 'status-value ' + (bt.connected ? 'connected' : 'disconnected');
        }
        const bleKb = document.getElementById('bleKeyboardEnabled');
        const bleMo = document.getElementById('bleMouseEnabled');
        if (bleKb) bleKb.checked = bt.keyboard_enabled !== false;
        if (bleMo) bleMo.checked = bt.mouse_enabled    !== false;
    }
    // USB
    if (data.connections?.usb) {
        const usb = data.connections.usb;
        const usbBtn = document.getElementById('usbToggle');
        if (usbBtn) {
            usbBtn.textContent = usb.enabled ? 'Disable USB HID' : 'Enable USB HID';
            usbBtn.classList.toggle('enabled', usb.enabled);
        }
        const usbKb   = document.getElementById('usbKeyboardEnabled');
        const usbMo   = document.getElementById('usbMouseEnabled');
        const fido2El = document.getElementById('fido2Enabled');
        if (usbKb)   usbKb.checked   = usb.keyboard_enabled !== false;
        if (usbMo)   usbMo.checked   = usb.mouse_enabled    !== false;
        if (fido2El) fido2El.checked = usb.fido2_enabled     === true;
    }
}

let isHalted = false;

function updateHaltStatusUI() {
    const haltStatus = document.getElementById('haltStatus');
    if (haltStatus) {
        if (isHalted) {
            haltStatus.textContent = 'Halted';
            haltStatus.className = 'status-value disconnected';
        } else {
            haltStatus.textContent = 'Active';
            haltStatus.className = 'status-value connected';
        }
    }
}

async function toggleHalt() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    const endpoint = getApiEndpoint();
    const payload = {
        halted: !isHalted
    };

    try {
        const response = await apiFetch(`${endpoint}/api/halt`, {
            method: 'POST',
            body: JSON.stringify(payload)
        });

        if (response.ok) {
            isHalted = !isHalted;
            updateHaltStatusUI();
            logDebug(`Operations ${isHalted ? 'halted' : 'resumed'}`, 'success');
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to toggle halt: ${error.message}`, 'error');
    }
}

function updateLEDColor() {
    const ledRed = document.getElementById('ledRed');
    const ledGreen = document.getElementById('ledGreen');
    const ledBlue = document.getElementById('ledBlue');
    
    if (!ledRed || !ledGreen || !ledBlue) return;

    ledColorR = parseInt(ledRed.value);
    ledColorG = parseInt(ledGreen.value);
    ledColorB = parseInt(ledBlue.value);

    safeSetText('ledRedValue', ledColorR);
    safeSetText('ledGreenValue', ledColorG);
    safeSetText('ledBlueValue', ledColorB);

    const colorPreview = document.getElementById('colorPreview');
    if (colorPreview) {
        colorPreview.style.backgroundColor = `rgb(${ledColorR}, ${ledColorG}, ${ledColorB})`;
    }
}

function toggleLED() {
    ledEnabled = !ledEnabled;
    const toggleBtn = document.getElementById('ledToggle');

    if (toggleBtn) {
        if (ledEnabled) {
            toggleBtn.textContent = 'Disable LED';
            toggleBtn.classList.add('enabled');
        } else {
            toggleBtn.textContent = 'Enable LED';
            toggleBtn.classList.remove('enabled');
        }
    }

    if (isConnected) {
        saveLEDSettings();
    }
}

async function saveLEDSettings() {
    if (!isConnected) return;

    const endpoint = getApiEndpoint();
    const payload = {
        enabled: ledEnabled,
        color: {
            r: ledColorR,
            g: ledColorG,
            b: ledColorB
        }
    };

    try {
        const response = await apiFetch(`${endpoint}/api/led`, {
            method: 'POST',
            body: JSON.stringify(payload)
        });

        if (response.ok) {
            logDebug('LED settings saved to device', 'success');
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to save LED settings: ${error.message}`, 'error');
    }
}

async function testLED() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    await saveLEDSettings();
    logDebug('LED test sent to device', 'success');
}

function updateConnectionUI() {
    const connectBtn = document.getElementById('connectBtn');
    const connectionStatus = document.getElementById('connectionStatus');

    if (connectBtn) {
        if (isConnected) {
            connectBtn.textContent = 'Reconnect';
            connectBtn.className = 'connect-btn';
        } else {
            connectBtn.textContent = 'Connect';
            connectBtn.className = 'connect-btn';
        }
    }

    if (connectionStatus) {
        if (isConnected) {
            connectionStatus.textContent = 'Connected';
            connectionStatus.className = 'status-value connected';
        } else {
            connectionStatus.textContent = 'Disconnected';
            connectionStatus.className = 'status-value disconnected';
        }
    }

    if (!isConnected) {
        safeSetText('deviceName', '-');
        safeSetText('ipAddress', '-');
        safeSetText('hostname', '-');
        safeSetText('freeHeap', '-');
        safeSetText('uptime', '-');
        safeSetText('sidebarActiveRegister', '-');
        safeSetText('deviceName', '-');
        safeSetText('chipModel',  '-');
        safeSetText('cpuFreq',    '-');
        safeSetText('flashSize',  '-');
        safeSetText('bluetoothStatus', 'Unknown');
        safeSetClass('bluetoothStatus', 'status-value');
        safeSetText('wifiSSID', '-');
        safeSetText('wifiConnectionStatus', 'Unknown');
        safeSetText('wifiRSSI', '-');
    }
}

function updateRequestStatus() {
    const requestStatus = document.getElementById('requestStatus');
    if (requestStatus) {
        if (requestInProgress) {
            requestStatus.textContent = 'Busy';
            requestStatus.className = 'status-value warning';
        } else {
            requestStatus.textContent = 'Idle';
            requestStatus.className = 'status-value';
        }
    }
}

function updateLoopStatusUI() {
    const loopStatus = document.getElementById('loopStatus');
    if (loopStatus) {
        if (deviceLoopActive) {
            loopStatus.textContent = `Active (Register ${deviceLoopingRegister + 1})`;
            loopStatus.className = 'status-value loop-active';
        } else {
            loopStatus.textContent = 'Inactive';
            loopStatus.className = 'status-value loop-inactive';
        }
    }
}

function updateBootProxStatusUI(bootReg) {
    const el = document.getElementById('bootProxStatus');
    if (!el) return;
    if (!bootReg) { el.textContent = '-'; el.className = 'status-value'; return; }
    if (!bootReg.enabled) {
        el.textContent = 'Off';
        el.className = 'status-value';
        return;
    }
    const limit = bootReg.limit ?? 0;
    const fired = bootReg.firedCount ?? 0;
    const regNum = (bootReg.index ?? 0) + 1;
    if (limit === 0) {
        el.textContent = `Reg ${regNum} — every boot (${fired} fired)`;
    } else {
        el.textContent = `Reg ${regNum} — ${fired}/${limit}`;
    }
    el.className = 'status-value loop-active';
}

function updateActiveRegisterUI() {
    const sidebarActiveRegister = document.getElementById('sidebarActiveRegister');
    if (sidebarActiveRegister) {
        if (numRegisters === 0 || currentActiveRegister === -1) {
            sidebarActiveRegister.textContent = 'None';
        } else {
            sidebarActiveRegister.textContent = currentActiveRegister + 1;
        }
    }

    const registerGrids = document.querySelectorAll('.register-grid');
    registerGrids.forEach((grid, index) => {
        const activeIndicator = grid.querySelector('.register-active-indicator');
        const setActiveBtn = grid.querySelector('.set-active-button');
        if (index === currentActiveRegister) {
            grid.classList.add('active');
            if (activeIndicator) activeIndicator.textContent = '*';
            if (setActiveBtn) setActiveBtn.disabled = true;
        } else {
            grid.classList.remove('active');
            if (activeIndicator) activeIndicator.textContent = '';
            if (setActiveBtn) setActiveBtn.disabled = false;
        }
    });
}

async function showTab(tabName) {
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });

    document.querySelectorAll('.tab').forEach(tab => {
        tab.classList.remove('active');
    });

    const tabContent = document.getElementById(tabName + '-tab');
    if (tabContent) {
        tabContent.classList.add('active');
    }

    if (event && event.target) {
        event.target.classList.add('active');
    }

    if (tabName === 'registers' && isConnected) {
        await loadRegisters();
    }
    if (tabName === 'credstore' && isConnected) {
        await csRefresh();
        try {
            const tr = await apiFetch(`${getApiEndpoint()}/api/totp`, { method: 'GET' });
            if (tr.ok) {
                const td = await tr.json();
                _updateCsNtpBadge(td.time_ready);
                csGateMode = td.gate_mode ?? 0;
                _updateCsUnlockForm();
            }
        } catch(e) { /* silent */ }
    }
    if (tabName === 'gadgets') {
        const list = document.getElementById('gadgetsList');
        if (list && list.querySelector('em')) {
            // auto-load on first visit
        }
    }
    if (tabName === 'keymap' && isConnected) {
        await keymapEditorInit();
    }
    if (tabName === 'reference') {
        await refLoad();
    }
    if (tabName === 'apiref') {
        await apirefLoad();
    }
    if (tabName === 'settings' && isConnected) {
        await loadSettingsTab();
    }
    if (tabName === 'schedtasks' && isConnected) {
        await loadSchedTasks();
    }
    if (tabName === 'totprox' && isConnected) {
        await loadTOTP();
    }
    if (tabName === 'bootprox' && isConnected) {
        await loadBootReg();
    }
    if (tabName === 'filebrowser' && isConnected) {
        await fbRefresh();
    }
    if (tabName === 'kpseditor' && isConnected) {
        await kpsLoadScriptList();
    }
    if (tabName === 'kpsref') {
        await kpsrefLoad();
    }
}

// ---- Credential Store ----

function _csStatus(elId, msg, ok) {
    const el = document.getElementById(elId);
    if (!el) return;
    el.textContent = msg;
    el.style.color = ok ? '#28a745' : '#dc3545';
    if (msg) setTimeout(() => { if (el.textContent === msg) el.textContent = ''; }, 5000);
}

async function csRefreshStatus() {
    await csRefresh();
    if (!isConnected) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/totp`, { method: 'GET' });
        if (!resp.ok) return;
        const d = await resp.json();
        _updateCsNtpBadge(d.time_ready);
        csGateMode = d.gate_mode ?? 0;
        _updateCsUnlockForm();
        totpGateModeChanged();
    } catch(e) { /* silent */ }
}

function _updateCsNtpBadge(timeReady) {
    const updates = [
        { id: 'csNtpBadge',    prefix: 'NTP: ' },
        { id: 'globalNtpBadge', prefix: '' },
    ];
    updates.forEach(({ id, prefix }) => {
        const el = document.getElementById(id);
        if (!el) return;
        if (timeReady === undefined || timeReady === null) {
            el.textContent = prefix + 'unknown';
            el.style.background = '#343a40'; el.style.color = '#adb5bd';
        } else if (timeReady) {
            el.textContent = prefix + 'synced';
            el.style.background = '#198754'; el.style.color = '#fff';
        } else {
            el.textContent = prefix + 'not synced';
            el.style.background = '#856404'; el.style.color = '#fff';
        }
    });
}

let _csLastData = {};

async function csRefresh() {
    if (!isConnected) return;
    const endpoint = getApiEndpoint();
    try {
        const resp = await apiFetch(`${endpoint}/api/credstore`, { method: 'GET' });
        const data = await resp.json();
        _csLastData = data;
        const badge = document.getElementById('credStoreLockBadge');
        const count = document.getElementById('credStoreCountLabel');
        const list  = document.getElementById('csCredentialList');
        const globBadge = document.getElementById('globalCsLockBadge');
        const globCount = document.getElementById('globalCsCountLabel');
        const bg    = data.locked ? '#dc3545' : '#28a745';
        const label = data.locked ? 'LOCKED' : 'UNLOCKED';
        const countTxt = `${data.count} credential${data.count !== 1 ? 's' : ''}`;
        if (badge) { badge.textContent = label; badge.style.background = bg; }
        if (globBadge) { globBadge.textContent = label; globBadge.style.background = bg; }
        if (count) count.textContent = countTxt;
        if (globCount) globCount.textContent = countTxt;

        const secNote = document.getElementById('csSecurityLockedNote');
        if (secNote) secNote.style.display = data.locked ? '' : 'none';

        const hint = document.getElementById('csFirstTimeHint');
        if (hint) hint.style.display = (data.locked && data.has_db === false) ? '' : 'none';

        const keyInput = document.getElementById('csKeyInput');
        if (keyInput && data.locked) {
            keyInput.placeholder = data.has_db === false
                ? 'Choose an encryption key (min 8 chars)'
                : 'Credential store key';
        }

        // Auto-lock countdown badge
        if (!data.locked && data.auto_lock_secs > 0) {
            _startCsAutoLockCountdown(data.auto_lock_secs);
        } else {
            _stopCsAutoLockCountdown();
        }

        // Failed attempts badge — update both credstore tab and global sidebar
        _updateCsFailedBadge(data.failed_attempts || 0, data.auto_wipe_at || 0);
        // Storage location badges
        const locBadge    = document.getElementById('csStorageLocationBadge');
        const sdBadge     = document.getElementById('csSdAvailableBadge');
        const nvsDbBadge  = document.getElementById('csNvsHasDbBadge');
        const sdDbBadge   = document.getElementById('csSdHasDbBadge');
        const fmtBtn      = document.getElementById('csSdFormatBtn');
        if (locBadge) {
            const loc = data.storage_location || 'nvs';
            locBadge.textContent  = `Location: ${loc === 'sd' ? 'SD card' : 'NVS (flash)'}`;
            locBadge.style.background = loc === 'sd' ? '#0d6efd' : '#6c757d';
            locBadge.style.color = '#fff';
        }
        if (sdBadge) {
            const avail = data.sd_available;
            sdBadge.textContent  = avail ? 'SD: available' : 'SD: not found';
            sdBadge.style.background = avail ? '#198754' : '#6c757d';
            sdBadge.style.color = '#fff';
        }
        if (nvsDbBadge) {
            const has = !!data.nvs_has_db;
            nvsDbBadge.style.display    = '';
            nvsDbBadge.textContent      = has ? 'NVS: has db' : 'NVS: empty';
            nvsDbBadge.style.background = has ? '#6f42c1' : '#343a40';
            nvsDbBadge.style.color      = '#fff';
        }
        if (sdDbBadge) {
            const has = !!data.sd_has_db;
            sdDbBadge.style.display    = data.sd_available ? '' : 'none';
            sdDbBadge.textContent      = has ? 'SD: has db' : 'SD: empty';
            sdDbBadge.style.background = has ? '#6f42c1' : '#343a40';
            sdDbBadge.style.color      = '#fff';
        }
        if (fmtBtn) fmtBtn.style.display = data.sd_available ? '' : 'none';

        if (list) {
            if (data.locked) {
                list.innerHTML = '<em style="color:#6c757d;">Unlock the store to view credentials.</em>';
            } else if (!data.labels || data.labels.length === 0) {
                list.innerHTML = '<em style="color:#6c757d;">No credentials stored yet.</em>';
            } else {
                const creds = data.credentials || data.labels.map(l => ({ label: l }));
                list.innerHTML = `
                    <table style="width:100%;border-collapse:collapse;font-size:12px;">
                        <thead><tr style="background:#f8f9fa;border-bottom:2px solid #dee2e6;">
                            <th style="padding:6px 8px;text-align:left;">Label</th>
                            <th style="padding:6px 8px;text-align:left;color:#6c757d;">Fields</th>
                            <th style="padding:6px 8px;width:130px;"></th>
                        </tr></thead>
                        <tbody>${creds.map(cr => {
                            const lbl = typeof cr === 'string' ? cr : cr.label;
                            const hasPw   = cr.has_password !== false;
                            const hasUser = !!cr.has_username;
                            const hasNote = !!cr.has_notes;
                            const badges = [
                                hasPw   ? '<span style="font-size:10px;padding:1px 5px;border-radius:3px;background:#0d6efd;color:#fff;margin-right:3px;">pw</span>' : '',
                                hasUser ? '<span style="font-size:10px;padding:1px 5px;border-radius:3px;background:#198754;color:#fff;margin-right:3px;">user</span>' : '',
                                hasNote ? '<span style="font-size:10px;padding:1px 5px;border-radius:3px;background:#6c757d;color:#fff;">note</span>' : '',
                            ].join('');
                            return `
                            <tr style="border-bottom:1px solid #dee2e6;">
                                <td style="padding:6px 8px;font-family:monospace;font-weight:bold;">${escapeHtml(lbl)}</td>
                                <td style="padding:6px 8px;">${badges}</td>
                                <td style="padding:6px 8px;text-align:right;">
                                    <button data-lbl="${escapeHtml(lbl)}" onclick="csPrefillEdit(this.dataset.lbl)" style="padding:3px 10px;font-size:11px;background:#0d6efd;margin-right:4px;">Edit</button>
                                    <button data-lbl="${escapeHtml(lbl)}" onclick="csDeleteCredential(this.dataset.lbl)" style="padding:3px 10px;font-size:11px;background:#dc3545;">Delete</button>
                                </td>
                            </tr>`;
                        }).join('')}
                        </tbody>
                    </table>`;
            }
        }
    } catch(e) {
        console.error('csRefresh error:', e);
    }
}

function _updateCsUnlockForm() {
    const keyRow  = document.getElementById('csKeyRow');
    const totpRow = document.getElementById('csTotpRow');
    const hintEl  = document.getElementById('csGateModeHint');
    const keyIn   = document.getElementById('csKeyInput');
    if (!keyRow || !totpRow) return;

    if (csGateMode === 2) {
        // TOTP only — hide key field, show TOTP
        keyRow.style.display  = 'none';
        totpRow.style.display = '';
        if (keyIn) keyIn.removeAttribute('required');
        if (hintEl) { hintEl.textContent = 'TOTP-only mode: enter your 6-digit authenticator code.'; hintEl.style.display = ''; }
    } else if (csGateMode === 1) {
        // Key + TOTP — show both
        keyRow.style.display  = '';
        totpRow.style.display = '';
        if (keyIn) keyIn.placeholder = 'PIN (min 4 chars)';
        if (hintEl) { hintEl.textContent = 'PIN + TOTP mode: enter your PIN then your 6-digit code.'; hintEl.style.display = ''; }
    } else {
        // Symmetric key only
        keyRow.style.display  = '';
        totpRow.style.display = 'none';
        if (keyIn) keyIn.placeholder = 'Credential store key (min 8 chars)';
        if (hintEl) hintEl.style.display = 'none';
    }
}

async function csUnlock(e) {
    if (e) e.preventDefault();
    const key      = csGateMode === 2 ? '' : (document.getElementById('csKeyInput')?.value || '');
    const totpCode = (csGateMode === 1 || csGateMode === 2)
        ? (document.getElementById('csTotpInput')?.value.trim() || '')
        : '';

    const minKeyLen = csGateMode === 1 ? 4 : 8;
    if (csGateMode !== 2 && key.length < minKeyLen) {
        _csStatus('csLockStatus', `Key must be at least ${minKeyLen} characters.`, false); return;
    }
    if ((csGateMode === 1 || csGateMode === 2) && totpCode.length < 6) {
        _csStatus('csLockStatus', '6-digit TOTP code required.', false); return;
    }

    const body = csGateMode === 2
        ? { action: 'unlock', key: totpCode, totp_code: totpCode }
        : csGateMode === 1
            ? { action: 'unlock', key, totp_code: totpCode }
            : { action: 'unlock', key };

    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/credstore`, {
            method: 'POST', body: JSON.stringify(body)
        });
        const data = await resp.json();
        if (resp.ok && data.locked === false) {
            _csStatus('csLockStatus', 'Unlocked.', true);
            if (document.getElementById('csKeyInput'))  document.getElementById('csKeyInput').value  = '';
            if (document.getElementById('csTotpInput')) document.getElementById('csTotpInput').value = '';
            await csRefresh();
            await loadTOTP();
        } else {
            _csStatus('csLockStatus', data.error || 'Invalid credentials.', false);
        }
    } catch(e) {
        _csStatus('csLockStatus', 'Error: ' + e.message, false);
    }
}

async function csLock() {
    const endpoint = getApiEndpoint();
    try {
        await apiFetch(`${endpoint}/api/credstore`, {
            method: 'POST',
            body: JSON.stringify({ action: 'lock' })
        });
        _csStatus('csLockStatus', 'Locked.', true);
        await csRefresh();
        await loadTOTP();
    } catch(e) {
        _csStatus('csLockStatus', 'Error: ' + e.message, false);
    }
}

async function csSetStorageLocation(loc) {
    if (!isConnected) return;
    const cur      = _csLastData.storage_location || 'nvs';
    const sdHasDb  = !!_csLastData.sd_has_db;
    const nvsHasDb = !!_csLastData.nvs_has_db;

    if (loc === cur) return;

    if (loc === 'sd' && sdHasDb) {
        const msg =
            'An existing database is already on the SD card.\n\n' +
            'Switching will migrate your NVS database to the SD card, ' +
            'overwriting the file /kprox.kdbx.\n\n' +
            'To keep the SD file, cancel and rename or delete /kprox.kdbx first.\n\n' +
            'Proceed and overwrite the SD database?';
        if (!confirm(msg)) return;
    }

    if (loc === 'nvs' && nvsHasDb) {
        const msg =
            'An existing database is already in NVS (built-in flash).\n\n' +
            'Switching will migrate your SD database to NVS, ' +
            'overwriting the data currently stored there.\n\n' +
            'Proceed and overwrite the NVS database?';
        if (!confirm(msg)) return;
    }

    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/credstore`, {
            method: 'POST',
            body: JSON.stringify({ action: 'set_storage_location', location: loc, force: true })
        });
        const data = await resp.json();
        if (resp.ok) {
            _csStatus('csStorageStatus', `✓ Stored in ${loc === 'sd' ? 'SD card' : 'NVS (flash)'}.`, true);
            await csRefresh();
        } else {
            _csStatus('csStorageStatus', '✗ ' + (data.error || 'Failed'), false);
        }
    } catch(e) { _csStatus('csStorageStatus', '✗ ' + e.message, false); }
}

async function csSdFormat() {
    if (!isConnected) return;
    if (!confirm('Format the SD card database file?\n\nThis removes the credential store from the SD card and cannot be undone.')) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/credstore`, {
            method: 'POST', body: JSON.stringify({ action: 'format_sd' })
        });
        const data = await resp.json();
        if (resp.ok) {
            _csStatus('csWipeStatus', '✓ SD formatted.', true);
            await csRefresh();
        } else {
            _csStatus('csWipeStatus', '✗ ' + (data.error || 'Failed'), false);
        }
    } catch(e) { _csStatus('csWipeStatus', '✗ ' + e.message, false); }
}

async function csSetCredential() {
    const label    = document.getElementById('csNewLabel')?.value.trim()    || '';
    const password = document.getElementById('csNewPassword')?.value        || '';
    const username = document.getElementById('csNewUsername')?.value.trim() || '';
    const notes    = document.getElementById('csNewNotes')?.value.trim()    || '';
    if (!label) { _csStatus('csSetStatus', 'Label is required.', false); return; }
    const endpoint = getApiEndpoint();
    try {
        // Always save password field (even if empty — clears it)
        const saves = [
            apiFetch(`${endpoint}/api/credstore`, { method: 'POST', body: JSON.stringify({ action: 'set', label, value: password, field: 'password' }) }),
        ];
        if (username) saves.push(apiFetch(`${endpoint}/api/credstore`, { method: 'POST', body: JSON.stringify({ action: 'set', label, value: username, field: 'username' }) }));
        if (notes)    saves.push(apiFetch(`${endpoint}/api/credstore`, { method: 'POST', body: JSON.stringify({ action: 'set', label, value: notes,    field: 'notes'    }) }));
        const results = await Promise.all(saves);
        const allOk = results.every(r => r.ok);
        if (allOk) {
            _csStatus('csSetStatus', '✓ Saved.', true);
            document.getElementById('csNewLabel').value    = '';
            document.getElementById('csNewPassword').value = '';
            document.getElementById('csNewUsername').value = '';
            document.getElementById('csNewNotes').value    = '';
            await csRefresh();
        } else {
            _csStatus('csSetStatus', '✗ Save failed.', false);
        }
    } catch(e) {
        _csStatus('csSetStatus', '✗ ' + e.message, false);
    }
}

function csPrefillEdit(label) {
    document.getElementById('csNewLabel').value    = label;
    document.getElementById('csNewPassword').value = '';
    document.getElementById('csNewUsername').value = '';
    document.getElementById('csNewNotes').value    = '';
    document.getElementById('csNewPassword').focus();
    document.getElementById('csNewLabel').scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

async function csWipe() {
    if (!confirm('Wipe ALL credentials and reset the store key?\n\nThis cannot be undone.')) return;
    const endpoint = getApiEndpoint();
    try {
        const resp = await apiFetch(`${endpoint}/api/credstore`, {
            method: 'POST',
            body: JSON.stringify({ action: 'wipe' })
        });
        const data = await resp.json();
        if (resp.ok) {
            _csStatus('csWipeStatus', 'Credential store wiped.', true);
            await csRefresh();
        } else {
            _csStatus('csWipeStatus', data.error || 'Wipe failed.', false);
        }
    } catch(e) {
        _csStatus('csWipeStatus', 'Error: ' + e.message, false);
    }
}

async function csDeleteCredential(label) {
    if (!confirm(`Delete credential "${label}"?`)) return;
    const endpoint = getApiEndpoint();
    try {
        await apiFetch(`${endpoint}/api/credstore`, {
            method: 'POST',
            body: JSON.stringify({ action: 'delete', label })
        });
        await csRefresh();
    } catch(e) {
        alert('Delete failed: ' + e.message);
    }
}

async function csRekey(e) {
    if (e) e.preventDefault();
    const oldKey   = document.getElementById('csOldKey').value;
    const newKey   = document.getElementById('csNewKey').value;
    const confirm2 = document.getElementById('csNewKeyConfirm').value;
    const minLen   = csGateMode === 1 ? 4 : 8;
    if (!oldKey || !newKey) { _csStatus('csRekeyStatus', 'Fill in all fields.', false); return; }
    if (newKey.length < minLen) { _csStatus('csRekeyStatus', `New key must be at least ${minLen} characters.`, false); return; }
    if (newKey !== confirm2) { _csStatus('csRekeyStatus', 'New keys do not match.', false); return; }
    if (!confirm('This will re-encrypt all credentials with the new key. Continue?')) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/credstore/rekey`, {
            method: 'POST',
            body: JSON.stringify({ old_key: oldKey, new_key: newKey })
        });
        const data = await resp.json();
        if (resp.ok) {
            _csStatus('csRekeyStatus', 'Key changed successfully.', true);
            document.getElementById('csOldKey').value        = '';
            document.getElementById('csNewKey').value        = '';
            document.getElementById('csNewKeyConfirm').value = '';
            await csRefresh();
        } else {
            _csStatus('csRekeyStatus', data.error || 'Failed.', false);
        }
    } catch(e) {
        _csStatus('csRekeyStatus', 'Error: ' + e.message, false);
    }
}

// ---- Gadgets ----

const GADGETS_DIR_URL = 'https://api.github.com/repos/akoerner/kprox/contents/gadgets';
const GADGETS_RAW_BASE = 'https://raw.githubusercontent.com/akoerner/kprox/master/gadgets/';

let _gadgetsCache = null;

async function gadgetsFetch() {
    const btn    = document.getElementById('gadgetsFetchBtn');
    const status = document.getElementById('gadgetsLoadStatus');
    const list   = document.getElementById('gadgetsList');
    if (!list) return;

    btn.disabled = true;
    if (status) status.textContent = 'Fetching...';
    list.innerHTML = `
        <div style="display:flex;align-items:center;gap:12px;padding:20px;color:#6c757d;">
            <div style="width:20px;height:20px;border:3px solid #dee2e6;border-top-color:#0d6efd;border-radius:50%;animation:spin 0.8s linear infinite;flex-shrink:0;"></div>
            <span>Fetching gadgets from GitHub...</span>
        </div>`;

    // Ensure spin keyframe exists
    if (!document.getElementById('spinStyle')) {
        const s = document.createElement('style');
        s.id = 'spinStyle';
        s.textContent = '@keyframes spin{to{transform:rotate(360deg)}}';
        document.head.appendChild(s);
    }

    let fetched = 0;

    try {
        const dirResp = await fetch(GADGETS_DIR_URL, {
            headers: { 'Accept': 'application/vnd.github.v3+json' }
        });
        if (!dirResp.ok) throw new Error(`GitHub API ${dirResp.status}`);
        const files = await dirResp.json();

        const jsonFiles = files.filter(f => f.name.endsWith('.json'));
        if (jsonFiles.length === 0) {
            list.innerHTML = '<em style="color:#6c757d;">No gadgets found in repository.</em>';
            if (status) status.textContent = '';
            btn.disabled = false;
            return;
        }

        const gadgets = [];
        for (const file of jsonFiles) {
            try {
                const rawResp = await fetch(GADGETS_RAW_BASE + file.name);
                if (!rawResp.ok) continue;
                const data = await rawResp.json();
                if (data.gadget && data.gadget.content) {
                    gadgets.push({
                        filename: file.name,
                        name:        data.gadget.name        || file.name,
                        description: data.gadget.description || '',
                        content:     data.gadget.content
                    });
                }
            } catch(_) { /* skip malformed */ }
            fetched++;
            if (status) status.textContent = `${fetched}/${jsonFiles.length} files...`;
        }

        _gadgetsCache = gadgets;
        gadgetsRender(gadgets);
        if (status) status.textContent = `${gadgets.length} gadget${gadgets.length !== 1 ? 's' : ''} loaded`;
    } catch(e) {
        list.innerHTML = `<span style="color:#dc3545;">Error: ${escapeHtml(e.message)}</span>`;
        if (status) status.textContent = '';
    } finally {
        btn.disabled = false;
    }
}

function gadgetsRender(gadgets) {
    const list = document.getElementById('gadgetsList');
    if (!list) return;
    if (gadgets.length === 0) {
        list.innerHTML = '<em style="color:#6c757d;">No gadgets available.</em>';
        return;
    }

    list.innerHTML = gadgets.map((g, i) => `
        <div style="border:1px solid #dee2e6;border-radius:6px;padding:12px;margin-bottom:10px;">
            <div style="display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:6px;">
                <strong style="font-size:14px;">${escapeHtml(g.name)}</strong>
                <button onclick="gadgetInstall(${i})" id="gadgetInstallBtn_${i}"
                        style="padding:4px 12px;font-size:12px;background:#198754;color:#fff;border:none;border-radius:4px;cursor:pointer;white-space:nowrap;margin-left:8px;">
                    Install
                </button>
            </div>
            ${g.description ? `<div style="font-size:12px;color:#6c757d;margin-bottom:6px;">${escapeHtml(g.description)}</div>` : ''}
            <div style="font-family:monospace;font-size:11px;background:#f8f9fa;border:1px solid #e9ecef;border-radius:4px;padding:6px;word-break:break-all;max-height:56px;overflow:hidden;color:#495057;">
                ${escapeHtml(g.content)}
            </div>
            <div id="gadgetStatus_${i}" style="font-size:12px;margin-top:4px;min-height:14px;"></div>
        </div>
    `).join('');
}

async function gadgetInstall(index) {
    if (!_gadgetsCache || index >= _gadgetsCache.length) return;
    if (!isConnected) {
        alert('Connect to a device first');
        return;
    }
    const g   = _gadgetsCache[index];
    const btn = document.getElementById(`gadgetInstallBtn_${index}`);
    const st  = document.getElementById(`gadgetStatus_${index}`);
    if (btn) btn.disabled = true;
    if (st)  st.textContent = 'Installing...';
    if (st)  st.style.color = '#6c757d';

    const endpoint = getApiEndpoint();
    try {
        const resp = await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify({ action: 'add', content: g.content, name: g.name })
        });
        if (resp.ok) {
            if (st) { st.textContent = 'v Installed as new register'; st.style.color = '#198754'; }
            await loadRegisters();
        } else {
            const data = await resp.json();
            if (st) { st.textContent = data.error || 'Install failed'; st.style.color = '#dc3545'; }
        }
    } catch(e) {
        if (st) { st.textContent = 'Error: ' + e.message; st.style.color = '#dc3545'; }
    } finally {
        if (btn) btn.disabled = false;
    }
}

// ---- Fuzzy Search ----

let _fuzzySelectedIdx = -1;
let _fuzzyResults = [];

function fuzzyScore(hay, needle) {
    if (!needle) return 0;
    const h = hay.toLowerCase();
    const n = needle.toLowerCase();
    let hi = 0, ni = 0, score = 0, consec = 0;
    while (hi < h.length && ni < n.length) {
        if (h[hi] === n[ni]) { score += 1 + consec * 2; consec++; ni++; }
        else consec = 0;
        hi++;
    }
    return ni === n.length ? score : -1;
}

function fuzzySearch() {
    const query = document.getElementById('fuzzySearchInput')?.value || '';
    const out   = document.getElementById('fuzzyResults');
    if (!out) return;

    if (!isConnected || numRegisters === 0) {
        out.innerHTML = '<div style="padding:8px;color:#6c757d;font-style:italic;">Connect to a device to search registers.</div>';
        _fuzzyResults = [];
        return;
    }

    const scored = [];
    for (let i = 0; i < numRegisters; i++) {
        const reg     = registers[i] || {};
        const name    = reg.name    || '';
        const content = reg.content || '';
        const hay     = (name ? name + ' ' : '') + content;
        const s       = fuzzyScore(hay, query);
        if (!query || s >= 0) scored.push({ idx: i, score: query ? s : -i, name, content });
    }
    if (query) scored.sort((a, b) => b.score - a.score);

    _fuzzyResults = scored.slice(0, 20);
    if (_fuzzySelectedIdx >= _fuzzyResults.length) _fuzzySelectedIdx = _fuzzyResults.length - 1;
    if (_fuzzyResults.length > 0 && _fuzzySelectedIdx < 0) _fuzzySelectedIdx = 0;

    fuzzyRender(out);
}

function fuzzyRender(out) {
    if (!out) out = document.getElementById('fuzzyResults');
    if (!out) return;
    if (_fuzzyResults.length === 0) {
        out.innerHTML = '<div style="padding:8px;color:#6c757d;font-style:italic;">No matches.</div>';
        return;
    }
    out.innerHTML = _fuzzyResults.map(({ idx, name, content }, vi) => {
        const label    = name || (content.substring(0, 60) + (content.length > 60 ? '...' : ''));
        const isActive = idx === currentActiveRegister;
        const isSel    = vi === _fuzzySelectedIdx;
        const rowBg    = isSel ? '#e8f0fe' : (isActive ? '#f0fff4' : '#fff');
        const border   = isSel ? '#4285f4' : (isActive ? '#28a745' : '#dee2e6');
        return `<div id="fuzzyRow_${vi}"
                     style="display:flex;align-items:center;gap:8px;padding:5px 8px;
                            background:${rowBg};border-bottom:1px solid ${border};cursor:pointer;"
                     onclick="fuzzySelectRow(${vi})">
                    <span style="color:#6c757d;font-size:11px;min-width:26px;">#${idx + 1}</span>
                    <span style="color:#28a745;font-size:11px;min-width:10px;">${isActive ? '*' : ''}</span>
                    <span style="flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:13px;"
                          title="${escapeHtml(content)}">${escapeHtml(label)}</span>
                    <button onclick="event.stopPropagation();fuzzyPlayIdx(${idx});"
                            style="padding:2px 8px;font-size:11px;background:#198754;color:#fff;border:none;border-radius:3px;cursor:pointer;white-space:nowrap;">
                        Play
                    </button>
                </div>`;
    }).join('');
}

function _fuzzyPopulateBox(vi) {
    const r = _fuzzyResults[vi];
    if (!r) return;
    const inp = document.getElementById('fuzzySearchInput');
    if (inp) inp.value = r.name || ('#' + r.idx);
}

function fuzzySelectRow(vi) {
    _fuzzySelectedIdx = vi;
    _fuzzyPopulateBox(vi);
    fuzzyRender();
    if (!_fuzzyResults[vi]) return;
    const { idx } = _fuzzyResults[vi];
    if (idx === currentActiveRegister) fuzzyPlayIdx(idx);
    else fuzzySetActive(idx);
}

function fuzzyKeydown(e) {
    if (e.key === 'ArrowDown') {
        e.preventDefault();
        if (_fuzzySelectedIdx < _fuzzyResults.length - 1) _fuzzySelectedIdx++;
        _fuzzyPopulateBox(_fuzzySelectedIdx);
        fuzzyRender();
    } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        if (_fuzzySelectedIdx > 0) _fuzzySelectedIdx--;
        _fuzzyPopulateBox(_fuzzySelectedIdx);
        fuzzyRender();
    } else if (e.key === 'Enter') {
        e.preventDefault();
        if (_fuzzyResults.length === 0) return;
        const si = Math.max(0, _fuzzySelectedIdx);
        const { idx } = _fuzzyResults[si];
        _fuzzyPopulateBox(si);
        fuzzyRender();
        if (idx === currentActiveRegister) fuzzyPlayIdx(idx);
        else fuzzySetActive(idx);
    }
    // All other keys let oninput handle the re-search naturally
}

function fuzzySearchClear() {
    const inp = document.getElementById('fuzzySearchInput');
    if (inp) inp.value = '';
    _fuzzySelectedIdx = -1;
    fuzzySearch();
}

async function fuzzySetActive(idx) {
    const success = await setActiveRegisterOnDevice(idx);
    if (success) {
        currentActiveRegister = idx;
        const savedIdx = _fuzzySelectedIdx;
        await loadRegisters();
        _fuzzySelectedIdx = savedIdx; // restore — next Enter will play
        fuzzyRender();
    }
}

async function fuzzyPlaySelected() {
    if (_fuzzyResults.length === 0) return;
    await fuzzyPlayIdx(_fuzzyResults[Math.max(0, _fuzzySelectedIdx)].idx);
}

async function fuzzyPlayIdx(idx) {
    if (!isConnected) return;
    const endpoint = getApiEndpoint();
    try {
        const resp = await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify({ action: 'play', register: idx })
        });
        if (!resp.ok) logDebug('Play failed: ' + resp.status, 'error');
    } catch(e) { logDebug('Play error: ' + e.message, 'error'); }
}

function escapeHtml(str) {
    return String(str).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/'/g,'&#39;');
}

function populateReferenceTable() {
    const tbody = document.getElementById('referenceTableBody');
    if (!tbody) return;
    
    tbody.innerHTML = '';

    keyReference.forEach(item => {
        const row = document.createElement('tr');
        row.innerHTML = `
            <td>${item.char || '&nbsp;'}</td>
            <td>${item.desc}</td>
            <td>${item.ascii}</td>
            <td>0x${item.ascii.toString(16).toUpperCase().padStart(2, '0')}</td>
            <td>${item.hid}</td>
            <td>0x${item.hid.toString(16).toUpperCase().padStart(2, '0')}</td>
            <td>${item.shift ? 'Yes' : 'No'}</td>
            <td>${item.token || '&nbsp;'}</td>
            `;
        tbody.appendChild(row);
    });
}

// ---- Code Reference tab ----

let _refMd     = null;   // raw markdown string
let _refView   = 'doc';  // 'doc' | 'char'
let _refSections = [];   // [{heading, body, html, text}] for search

function refSetView(v) {
    _refView = v;
    const docView  = document.getElementById('refDocView');
    const charView = document.getElementById('refCharView');
    const btnDoc   = document.getElementById('refBtnDoc');
    const btnChar  = document.getElementById('refBtnChar');
    if (!docView) return;
    if (v === 'doc') {
        docView.style.display  = '';
        charView.style.display = 'none';
        if (btnDoc)  { btnDoc.style.background=''; btnDoc.style.color=''; }
        if (btnChar) { btnChar.style.background='#6c757d'; btnChar.style.color='#fff'; }
    } else {
        docView.style.display  = 'none';
        charView.style.display = '';
        if (btnChar) { btnChar.style.background=''; btnChar.style.color=''; }
        if (btnDoc)  { btnDoc.style.background='#6c757d'; btnDoc.style.color='#fff'; }
    }
    refSearch();
}

// Minimal markdown to HTML renderer (headings, tables, code, bold, lists, hr, paragraphs)
function _mdToHtml(md) {
    const lines = md.split('\n');
    const out = [];
    let inTable = false, inList = false, inCode = false, codeLang = '', codeBuf = [];

    const flushList = () => { if (inList) { out.push('</ul>'); inList = false; } };
    const flushTable = () => { if (inTable) { out.push('</tbody></table>'); inTable = false; } };
    const flushCode = () => {
        if (inCode) {
            const escaped = codeBuf.join('\n').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
            out.push(`<pre style="background:#f8f9fa;padding:10px;border-radius:4px;overflow-x:auto;font-size:12px;"><code>${escaped}</code></pre>`);
            codeBuf = []; inCode = false; codeLang = '';
        }
    };

    const inlineFormat = s =>
        s.replace(/`([^`]+)`/g, '<code style="background:#f0f0f0;padding:1px 4px;border-radius:3px;font-size:12px;">$1</code>')
         .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
         .replace(/\*([^*]+)\*/g, '<em>$1</em>');

    for (let i = 0; i < lines.length; i++) {
        const raw = lines[i];
        const line = raw.trimEnd();

        // fenced code block
        if (line.startsWith('```')) {
            if (inCode) { flushCode(); continue; }
            flushList(); flushTable();
            inCode = true; codeLang = line.slice(3).trim(); continue;
        }
        if (inCode) { codeBuf.push(raw); continue; }

        // HR
        if (/^[-*_]{3,}$/.test(line.trim())) {
            flushList(); flushTable();
            out.push('<hr style="border:none;border-top:1px solid #dee2e6;margin:16px 0;">');
            continue;
        }

        // Headings
        const hm = line.match(/^(#{1,4})\s+(.*)/);
        if (hm) {
            flushList(); flushTable();
            const lvl = hm[1].length;
            const tag = 'h' + (lvl + 1);  // h2-h5 (h1 reserved for page title)
            const styles = [
                'margin:20px 0 6px;font-size:15px;border-bottom:1px solid #dee2e6;padding-bottom:4px;',
                'margin:16px 0 4px;font-size:13px;',
                'margin:12px 0 4px;font-size:12px;',
                'margin:10px 0 3px;font-size:12px;',
            ][lvl - 1] || '';
            out.push(`<${tag} style="${styles}">${inlineFormat(hm[2])}</${tag}>`);
            continue;
        }

        // Table rows
        if (line.startsWith('|')) {
            const cells = line.split('|').slice(1,-1).map(c => c.trim());
            if (/^[\s|:-]+$/.test(line)) {
                // separator row — do nothing
                continue;
            }
            if (!inTable) {
                flushList();
                out.push('<table style="width:100%;font-size:12px;border-collapse:collapse;margin-bottom:10px;">');
                // first row is header
                out.push('<thead><tr>' + cells.map(c => `<th style="padding:5px 8px;text-align:left;border:1px solid #dee2e6;background:#f8f9fa;">${inlineFormat(c)}</th>`).join('') + '</tr></thead><tbody>');
                inTable = true;
            } else {
                out.push('<tr>' + cells.map(c => `<td style="padding:4px 8px;border:1px solid #dee2e6;">${inlineFormat(c)}</td>`).join('') + '</tr>');
            }
            continue;
        }
        if (inTable && !line.startsWith('|')) { flushTable(); }

        // List items
        if (/^[\-\*] /.test(line)) {
            if (!inList) { out.push('<ul style="margin:4px 0 8px 18px;font-size:12px;">'); inList = true; }
            out.push(`<li>${inlineFormat(line.slice(2))}</li>`);
            continue;
        }
        if (inList && line.trim() !== '') { flushList(); }

        // Blank line
        if (line.trim() === '') {
            flushList(); flushTable();
            continue;
        }

        // Paragraph
        flushList(); flushTable();
        out.push(`<p style="margin:4px 0 8px;font-size:13px;">${inlineFormat(line)}</p>`);
    }
    flushCode(); flushList(); flushTable();
    return out.join('\n');
}

// Parse markdown into searchable sections (split on ## headings)
function _mdToSections(md) {
    const sections = [];
    let cur = { heading: '', lines: [] };
    for (const line of md.split('\n')) {
        if (line.startsWith('## ') || line.startsWith('# ')) {
            if (cur.lines.length || cur.heading) sections.push(cur);
            cur = { heading: line.replace(/^#+\s+/, ''), lines: [] };
        } else {
            cur.lines.push(line);
        }
    }
    if (cur.lines.length || cur.heading) sections.push(cur);
    return sections.map(s => {
        const body = s.lines.join('\n');
        return { heading: s.heading, body, html: _mdToHtml((s.heading ? '## ' + s.heading + '\n' : '') + body), text: (s.heading + ' ' + body).toLowerCase() };
    });
}

async function refLoad() {
    const el = document.getElementById('refDocContent');
    if (!el) return;
    if (_refMd) { _renderRef(); return; }  // already loaded
    el.innerHTML = '<p style="color:#6c757d;font-size:13px;">Loading token reference...</p>';
    try {
        const endpoint = getApiEndpoint ? getApiEndpoint() : '';
        const resp = await fetch(endpoint + '/api/docs');
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        _refMd = await resp.text();
        _refSections = _mdToSections(_refMd);
        _renderRef();
    } catch(e) {
        el.innerHTML = '<p style="color:#dc3545;font-size:13px;">Could not load token reference: ' + e.message + '. Connect to device first.</p>';
    }
}

function _renderRef(filter) {
    const el = document.getElementById('refDocContent');
    if (!el || !_refSections.length) return;
    if (!filter) {
        el.innerHTML = _refSections.map(s => s.html).join('');
    } else {
        const q = filter.toLowerCase();
        const matched = _refSections.filter(s => s.text.includes(q));
        if (matched.length === 0) {
            el.innerHTML = '<p style="color:#6c757d;font-size:13px;">No results for "' + _esc(filter) + '"</p>';
        } else {
            el.innerHTML = matched.map(s => {
                const reEsc = q.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
                const re = new RegExp('(' + reEsc + ')', 'gi');
                // Only highlight inside text nodes, not inside HTML tag markup
                return s.html.replace(/<[^>]*>|[^<]+/g, chunk =>
                    chunk.startsWith('<') ? chunk : chunk.replace(re, '<mark style="background:#fff3cd;">$1</mark>')
                );
            }).join('');
        }
    }
}

function refSearch() {
    const q = (document.getElementById('searchBox')?.value || '').trim();
    if (_refView === 'doc') {
        if (_refSections.length) _renderRef(q || null);
    } else {
        // char table filter
        const rows = document.querySelectorAll('#referenceTableBody tr');
        rows.forEach(row => {
            row.style.display = q === '' || row.textContent.toLowerCase().includes(q.toLowerCase()) ? '' : 'none';
        });
    }
}

// Legacy alias kept for any remaining onclick="filterTable()" references
function filterTable() { refSearch(); }

// ---- API Reference ----

let _apirefMd       = null;
let _apirefSections = [];

async function apirefLoad(force) {
    const el = document.getElementById('apirefContent');
    if (!el) return;
    if (_apirefMd && !force) { _renderApiref(); return; }
    el.innerHTML = '<p style="color:#6c757d;font-size:13px;">Loading API reference...</p>';
    try {
        const endpoint = getApiEndpoint ? getApiEndpoint() : '';
        const resp = await fetch(endpoint + '/api/apiref');
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        _apirefMd = await resp.text();
        _apirefSections = _mdToSections(_apirefMd);
        _renderApiref();
    } catch(e) {
        el.innerHTML = '<p style="color:#dc3545;font-size:13px;">Could not load API reference: ' + e.message + '. Connect to device first.</p>';
    }
}

function _renderApiref(filter) {
    const el = document.getElementById('apirefContent');
    if (!el || !_apirefSections.length) return;

    let sections = _apirefSections;
    if (filter) {
        const q = filter.toLowerCase();
        sections = sections.filter(s => s.text.includes(q));
        if (sections.length === 0) {
            el.innerHTML = '<p style="color:#6c757d;font-size:13px;">No results for "' + _esc(filter) + '"</p>';
            return;
        }
    }

    const q = filter ? filter.toLowerCase() : null;
    el.innerHTML = sections.map(s => {
        if (!q) return s.html;
        const reEsc = q.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
        const re = new RegExp('(' + reEsc + ')', 'gi');
        return s.html.replace(/<[^>]*>|[^<]+/g, chunk =>
            chunk.startsWith('<') ? chunk : chunk.replace(re, '<mark style="background:#fff3cd;">$1</mark>')
        );
    }).join('');
}

function apirefSearch() {
    const q = (document.getElementById('apirefSearch')?.value || '').trim();
    if (_apirefSections.length) _renderApiref(q || null);
}

// ---- TOTProx ----

let _totpRefreshInterval = null;

function _buildOtpAuthUri(secret, label, issuer) {
    const enc = encodeURIComponent;
    const fullLabel = issuer ? `${enc(issuer)}:${enc(label)}` : enc(label);
    return `otpauth://totp/${fullLabel}?secret=${secret}&issuer=${enc(issuer || label)}&digits=6&period=30`;
}

function _renderQr(containerId, uri) {
    const el = document.getElementById(containerId);
    if (!el || typeof QRCode === 'undefined') return;
    el.innerHTML = '';
    new QRCode(el, { text: uri, width: 128, height: 128, correctLevel: QRCode.CorrectLevel.M });
}

function totpGateSecretChanged() {
    const secret = document.getElementById('totpGateSecret')?.value.trim().toUpperCase().replace(/\s/g,'');
    const wrapEl = document.getElementById('totpGateQrWrap');
    const uriEl  = document.getElementById('totpGateUri');
    if (!wrapEl) return;
    if (!secret || secret.length < 16) { wrapEl.style.display = 'none'; return; }
    const hostname = document.getElementById('hostname')?.textContent?.trim() || 'kprox';
    const uri = _buildOtpAuthUri(secret, 'CS Gate', hostname);
    if (uriEl) uriEl.textContent = uri;
    wrapEl.style.display = 'block';
    _renderQr('totpGateQr', uri);
}

async function loadTOTP() {
    if (!isConnected) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/totp`, { method: 'GET' });
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        const d = await resp.json();
        _renderTOTP(d);
    } catch(e) {
        logDebug('loadTOTP: ' + e.message, 'error');
    }
}

function _renderTOTP(d) {
    const timeEl  = document.getElementById('totpTimeStatus');
    const gateEl  = document.getElementById('totpGateStatus');
    const listEl  = document.getElementById('totpAccountList');
    const modeEl  = document.getElementById('totpGateMode');

    if (timeEl) {
        timeEl.textContent = d.time_ready
            ? `Time: synced (epoch ${d.time_epoch})`
            : 'Time: no NTP sync';
        timeEl.style.background = d.time_ready ? '#198754' : '#dc3545';
        timeEl.style.color = '#fff';
    }

    const gateModes = ['off', 'key + TOTP', 'TOTP only'];
    if (gateEl) {
        gateEl.textContent = `Gate: ${gateModes[d.gate_mode] || 'off'}`;
        gateEl.style.background = d.gate_mode > 0 ? '#6f42c1' : '#343a40';
        gateEl.style.color = '#fff';
    }
    const csBadge = document.getElementById('totpCsLockBadge');
    if (csBadge) {
        csBadge.textContent = d.cs_locked ? 'CS: locked' : 'CS: unlocked';
        csBadge.style.background = d.cs_locked ? '#dc3545' : '#198754';
        csBadge.style.color = '#fff';
    }
    if (modeEl) modeEl.value = String(d.gate_mode ?? 0);

    csGateMode = d.gate_mode ?? 0;
    _updateCsUnlockForm();
    totpGateModeChanged();

    const addWarn = document.getElementById('totpAddLockedWarn');
    if (addWarn) addWarn.style.display = d.cs_locked ? '' : 'none';

    if (!listEl) return;
    if (!d.accounts || d.accounts.length === 0) {
        const lockedMsg = d.cs_locked
            ? '<em style="color:#dc3545;">Unlock the credential store to view TOTP accounts — secrets are encrypted with it.</em>'
            : '<em>No accounts.</em>';
        listEl.innerHTML = lockedMsg;
        return;
    }

    listEl.innerHTML = d.accounts.map(a => {
        const code = a.code || '------';
        const secsLeft = a.seconds_remaining ?? 0;
        const pct = d.time_ready ? Math.round((secsLeft / (a.period || 30)) * 100) : 0;
        const barColor = secsLeft <= 5 ? '#dc3545' : '#198754';
        return `<div style="display:flex;align-items:center;gap:10px;padding:8px 10px;
                            margin-bottom:6px;background:#f8f9fa;border:1px solid #dee2e6;border-radius:4px;">
            <div style="flex:1;min-width:0;">
                <div style="font-weight:600;font-size:13px;">${_esc(a.name)}</div>
                <div style="font-size:10px;color:#6c757d;">${a.digits}d / ${a.period}s</div>
                ${d.time_ready ? `
                <div style="height:3px;background:#dee2e6;border-radius:2px;margin-top:4px;">
                    <div style="height:3px;width:${pct}%;background:${barColor};border-radius:2px;transition:width 1s linear;"></div>
                </div>` : ''}
            </div>
            <div style="font-family:monospace;font-size:22px;font-weight:700;letter-spacing:3px;
                        color:${secsLeft <= 5 && d.time_ready ? '#dc3545' : '#212529'};">
                ${d.time_ready ? code.substring(0,3) + ' ' + code.substring(3) : '--- ---'}
            </div>
            ${d.time_ready ? `<div style="font-size:11px;color:#6c757d;min-width:26px;">${secsLeft}s</div>` : ''}
            <button onclick="totpDeleteAccount('${_esc(a.name)}')"
                    ${d.cs_locked ? 'disabled title="Unlock the credential store to delete"' : ''}
                    style="padding:3px 8px;font-size:11px;background:#dc3545;color:#fff;border:none;border-radius:3px;cursor:pointer;${d.cs_locked ? 'opacity:0.4;cursor:not-allowed;' : ''}">
                Del
            </button>
        </div>`;
    }).join('');
}

async function totpAddAccount() {
    if (!isConnected) return;
    const name   = document.getElementById('totpNewName')?.value.trim();
    const secret = document.getElementById('totpNewSecret')?.value.trim().toUpperCase().replace(/\s/g,'');
    const digits = parseInt(document.getElementById('totpNewDigits')?.value) || 6;
    const period = parseInt(document.getElementById('totpNewPeriod')?.value) || 30;
    if (!name || !secret) { _statusMsg('totpAddStatus', '✗ Name and secret required', false); return; }
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/totp`, {
            method: 'POST',
            body: JSON.stringify({ action: 'add', name, secret, digits, period })
        });
        if (!resp.ok) { const e = await resp.json(); throw new Error(e.error || 'HTTP ' + resp.status); }
        _statusMsg('totpAddStatus', '✓ Account added', true);
        document.getElementById('totpNewName').value   = '';
        document.getElementById('totpNewSecret').value = '';
        await loadTOTP();
    } catch(e) { _statusMsg('totpAddStatus', '✗ ' + e.message, false); }
}

async function totpDeleteAccount(name) {
    if (!isConnected) return;
    if (!confirm(`Delete TOTP account "${name}"?`)) return;
    try {
        await apiFetch(`${getApiEndpoint()}/api/totp`, {
            method: 'POST', body: JSON.stringify({ action: 'delete', name })
        });
        await loadTOTP();
    } catch(e) { logDebug('totpDeleteAccount: ' + e.message, 'error'); }
}

function totpGateModeChanged() {
    const mode       = parseInt(document.getElementById('totpGateMode')?.value) || 0;
    const newKeyRow  = document.getElementById('totpNewKeyRow');
    const newKeyHint = document.getElementById('totpNewKeyHint');
    if (!newKeyRow) return;
    // Show new key field only when switching away from TOTP-only (current saved mode is 2)
    const needsNewKey = (csGateMode === 2) && (mode !== 2);
    newKeyRow.style.display = needsNewKey ? '' : 'none';
    if (newKeyHint) newKeyHint.textContent = mode === 1 ? 'min 4 chars (PIN)' : 'min 8 chars';
}

async function totpSaveGate() {
    if (!isConnected) return;
    const mode   = parseInt(document.getElementById('totpGateMode')?.value) || 0;
    const secret = document.getElementById('totpGateSecret')?.value.trim().toUpperCase().replace(/\s/g,'');

    // Require new_key when leaving TOTP-only mode
    const needsNewKey = (csGateMode === 2) && (mode !== 2);
    if (needsNewKey) {
        const newKey = document.getElementById('totpNewKeyForGate')?.value || '';
        const minLen = mode === 1 ? 4 : 8;
        if (newKey.length < minLen) {
            _statusMsg('totpGateStatus2', `✗ New key must be at least ${minLen} characters`, false); return;
        }
        // Require store to be unlocked for rekey
        const lockBadge = document.getElementById('credStoreLockBadge');
        if (lockBadge && lockBadge.textContent === 'LOCKED') {
            _statusMsg('totpGateStatus2', '✗ Unlock the credential store first (use the TOTP code to unlock it, then save gate mode)', false); return;
        }
        const body = { action: 'set_gate', gate_mode: mode, new_key: newKey };
        if (secret) body.gate_secret = secret;
        try {
            const resp = await apiFetch(`${getApiEndpoint()}/api/totp`, {
                method: 'POST', body: JSON.stringify(body)
            });
            if (!resp.ok) { const e = await resp.json(); throw new Error(e.error || 'HTTP ' + resp.status); }
            _statusMsg('totpGateStatus2', '✓ Gate mode saved — store re-encrypted with new key', true);
            document.getElementById('totpGateSecret').value    = '';
            document.getElementById('totpNewKeyForGate').value = '';
            document.getElementById('totpNewKeyRow').style.display = 'none';
            await loadTOTP();
        } catch(e) { _statusMsg('totpGateStatus2', '✗ ' + e.message, false); }
        return;
    }

    const body = { action: 'set_gate', gate_mode: mode };
    if (secret) body.gate_secret = secret;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/totp`, {
            method: 'POST', body: JSON.stringify(body)
        });
        if (!resp.ok) { const e = await resp.json(); throw new Error(e.error || 'HTTP ' + resp.status); }
        _statusMsg('totpGateStatus2', '✓ Gate mode saved', true);
        document.getElementById('totpGateSecret').value = '';
        await loadTOTP();
    } catch(e) { _statusMsg('totpGateStatus2', '✗ ' + e.message, false); }
}

async function totpSetCsKey() {
    if (!isConnected) return;
    const newKey     = document.getElementById('totpCsNewKey')?.value;
    const confirmKey = document.getElementById('totpCsConfirmKey')?.value;
    const minLen     = csGateMode === 1 ? 4 : 8;
    if (!newKey || newKey.length < minLen) {
        _statusMsg('totpGateStatus2', `✗ Key must be at least ${minLen} characters`, false); return;
    }
    if (newKey !== confirmKey) { _statusMsg('totpGateStatus2', '✗ Keys do not match', false); return; }
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/totp`, {
            method: 'POST', body: JSON.stringify({ action: 'set_cs_key', new_key: newKey })
        });
        if (!resp.ok) { const e = await resp.json(); throw new Error(e.error || 'HTTP ' + resp.status); }
        const d = await resp.json();
        _statusMsg('totpGateStatus2', '✓ ' + (d.note || 'Key set'), true);
        document.getElementById('totpCsNewKey').value     = '';
        document.getElementById('totpCsConfirmKey').value = '';
    } catch(e) { _statusMsg('totpGateStatus2', '✗ ' + e.message, false); }
}

async function totpWipe() {
    if (!isConnected) return;
    if (!confirm('Wipe ALL TOTP data (accounts + gate settings)?')) return;
    try {
        await apiFetch(`${getApiEndpoint()}/api/totp`, {
            method: 'POST', body: JSON.stringify({ action: 'wipe' })
        });
        await loadTOTP();
    } catch(e) { logDebug('totpWipe: ' + e.message, 'error'); }
}

// ---- Scheduled Tasks ----

async function loadSchedTasks() {
    if (!isConnected) return;
    const el = document.getElementById('schedTaskList');
    if (!el) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/schedtasks`, { method: 'GET' });
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        const data = await resp.json();
        if (!data.tasks || data.tasks.length === 0) {
            el.innerHTML = '<em style="color:#6c757d;">No scheduled tasks.</em>';
            return;
        }
        el.innerHTML = data.tasks.map(t => {
            const when = _schedWhen(t);
            const label = t.label || `Task ${t.id}`;
            return `<div class="sched-task-row" style="display:flex;align-items:center;gap:8px;padding:8px 10px;margin-bottom:6px;background:#f8f9fa;border:1px solid #dee2e6;border-radius:4px;">
                <div style="flex:1;min-width:0;">
                    <div style="font-weight:600;font-size:13px;">${_esc(label)}</div>
                    <div style="font-size:11px;color:#6c757d;margin-top:2px;">${_esc(when)}${t.repeat ? ' · repeat' : ' · once'}</div>
                    <div style="font-size:11px;color:#495057;font-family:monospace;margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;">${_esc(t.payload)}</div>
                </div>
                <div style="display:flex;gap:4px;flex-shrink:0;">
                    <button onclick="toggleSchedTask(${t.id},${!t.enabled})" style="padding:3px 8px;font-size:11px;background:${t.enabled ? '#198754' : '#6c757d'};color:#fff;border:none;border-radius:3px;cursor:pointer;">${t.enabled ? 'ON' : 'OFF'}</button>
                    <button onclick="deleteSchedTask(${t.id})" style="padding:3px 8px;font-size:11px;background:#dc3545;color:#fff;border:none;border-radius:3px;cursor:pointer;">Delete</button>
                </div>
            </div>`;
        }).join('');
    } catch(e) {
        if (el) el.innerHTML = `<span style="color:#dc3545;">Error: ${_esc(e.message)}</span>`;
    }
}

function _schedWhen(t) {
    const yr  = t.year   > 0 ? String(t.year).padStart(4,'0')  : '*';
    const mo  = t.month  > 0 ? String(t.month).padStart(2,'0') : '*';
    const dy  = t.day    > 0 ? String(t.day).padStart(2,'0')   : '*';
    const hr  = String(t.hour).padStart(2,'0');
    const mn  = String(t.minute).padStart(2,'0');
    const sc  = String(t.second).padStart(2,'0');
    return `${yr}-${mo}-${dy}  ${hr}:${mn}:${sc}`;
}

async function addSchedTask() {
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }
    const payload = document.getElementById('schedPayload')?.value.trim();
    if (!payload) { _statusMsg('schedAddStatus', '✗ Payload is required', false); return; }

    const dtVal = document.getElementById('schedDatetime')?.value || '';
    let year = 0, month = 0, day = 0, hour = 0, minute = 0;
    if (dtVal) {
        const dt = new Date(dtVal);
        // year 1970 = "any year" sentinel → send 0
        year   = dt.getFullYear() === 1970 ? 0 : dt.getFullYear();
        month  = dt.getFullYear() === 1970 ? 0 : (dt.getMonth() + 1);
        day    = dt.getFullYear() === 1970 ? 0 : dt.getDate();
        hour   = dt.getHours();
        minute = dt.getMinutes();
    }

    const task = {
        label:   document.getElementById('schedLabel')?.value.trim() || '',
        year,
        month,
        day,
        hour,
        minute,
        second:  parseInt(document.getElementById('schedSecond')?.value) || 0,
        payload,
        enabled: true,
        repeat:  document.getElementById('schedRepeat')?.checked || false,
    };
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/schedtasks`, {
            method: 'POST', body: JSON.stringify(task)
        });
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        _statusMsg('schedAddStatus', '✓ Task added', true);
        document.getElementById('schedLabel').value   = '';
        document.getElementById('schedPayload').value = '';
        document.getElementById('schedDatetime').value = '';
        document.getElementById('schedSecond').value  = '0';
        const rep = document.getElementById('schedRepeat'); if (rep) rep.checked = false;
        await loadSchedTasks();
    } catch(e) {
        _statusMsg('schedAddStatus', '✗ ' + e.message, false);
    }
}

async function deleteSchedTask(id) {
    if (!isConnected) return;
    if (!confirm(`Delete task #${id}?`)) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/schedtasks`, {
            method: 'DELETE', body: JSON.stringify({ id })
        });
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        await loadSchedTasks();
    } catch(e) {
        logDebug('deleteSchedTask: ' + e.message, 'error');
    }
}

async function toggleSchedTask(id, enabled) {
    if (!isConnected) return;
    try {
        await apiFetch(`${getApiEndpoint()}/api/schedtasks`, {
            method: 'POST', body: JSON.stringify({ id, enabled })
        });
        await loadSchedTasks();
    } catch(e) {
        logDebug('toggleSchedTask: ' + e.message, 'error');
    }
}

// ---- Default App ----

async function saveDefaultApp() {
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }
    const sel = document.getElementById('defaultAppSelect');
    if (!sel) return;
    const da = parseInt(sel.value);
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/settings`, {
            method: 'POST', body: JSON.stringify({ defaultApp: da })
        });
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        _statusMsg('defaultAppStatus', '✓ Saved — reboot to apply', true);
    } catch(e) {
        _statusMsg('defaultAppStatus', '✗ ' + e.message, false);
    }
}


// ---- Settings tab population ----

async function loadSettingsTab() {
    try {
        const endpoint = getApiEndpoint();
        const resp = await apiFetch(`${endpoint}/api/settings`, { method: 'GET', mode: 'cors' });
        if (!resp.ok) return;
        const d = await resp.json();

        // Timing
        if (d.timing) {
            _setVal('timingKeyPress',    d.timing.key_press_delay);
            _setVal('timingKeyRelease',  d.timing.key_release_delay);
            _setVal('timingBetweenKeys', d.timing.between_keys_delay);
            _setVal('timingBetweenSend', d.timing.between_send_text_delay);
            _setVal('timingSpecialKey',  d.timing.special_key_delay);
            _setVal('timingToken',       d.timing.token_delay);
        }

        // Device identity
        if (d.device) {
            _setVal('newDeviceManufacturer', d.device.manufacturer);
            _setVal('newDeviceProduct',      d.device.product);
            _setVal('newHostname',           d.device.hostname);
            _setVal('newUsbSerial',          d.device.usb_serial);
        }

        // Sync endpoint input
        const ep = document.getElementById('settingsApiEndpoint');
        if (ep && !ep.value) ep.value = getApiEndpoint();

        // Populate app name list from API (dynamic — reflects registered apps)
        if (d.appNames && d.appNames.length) {
            APP_NAMES = d.appNames;
            const sel = document.getElementById('defaultAppSelect');
            if (sel) {
                const cur = sel.value;
                sel.innerHTML = d.appNames
                    .map((name, i) => `<option value="${i + 1}">${escapeHtml(name)}</option>`)
                    .join('');
                if (cur) sel.value = cur;
            }
        }
        // Default app selector
        if (d.defaultApp !== undefined) {
            const sel = document.getElementById('defaultAppSelect');
            if (sel) sel.value = String(d.defaultApp);
        }

        // Sink buffer max size
        if (d.maxSinkSize !== undefined) {
            _setVal('sinkMaxSizeInput', d.maxSinkSize);
        }
        refreshSinkSize();
        updateSinkCurlExamples();

        // CS Security
        if (d.cs) {
            if (d.cs.autoLockSecs !== undefined)    _setVal('csAutoLockInput', d.cs.autoLockSecs);
            if (d.cs.autoWipeAttempts !== undefined) _setVal('csAutoWipeInput', d.cs.autoWipeAttempts);
            const failEl = document.getElementById('csFailedAttemptsVal');
            if (failEl && d.cs.failedAttempts !== undefined) {
                failEl.textContent = d.cs.failedAttempts;
                failEl.style.color = d.cs.failedAttempts > 0 ? '#dc3545' : '#28a745';
            }
        }
        // Reflect lock state in security note
        {
            const secNote = document.getElementById('csSecurityLockedNote');
            if (secNote) {
                // Re-read from credstore badge which reflects current state
                const lockBadge = document.getElementById('credStoreLockBadge');
                const isLocked = !lockBadge || lockBadge.textContent === 'LOCKED';
                secNote.style.display = isLocked ? '' : 'none';
            }
        }

        // App layout
        if (d.appOrder && d.appHidden) {
            _renderAppLayout(d.appOrder, d.appHidden);
        }

        // Boot register (populate if bootprox tab has been visited)
        if (d.bootReg) { _applyBootReg(d.bootReg); updateBootProxStatusUI(d.bootReg); }
    } catch (e) {
        logDebug('loadSettingsTab: ' + e.message, 'warning');
    }
}

// ---- App Layout ----

let APP_NAMES = ['KProx','FuzzyProx','RegEdit','CredStore','Gadgets','SinkProx','Keyboard','Clock','QRProx','SchedProx','TOTProx','Settings']; // overwritten from API
let _appOrder  = [];
let _appHidden = [];

function _renderAppLayout(order, hidden) {
    _appOrder  = order  ? [...order]  : Array.from({length: APP_NAMES.length}, (_, i) => i + 1);
    _appHidden = hidden ? [...hidden] : Array(_appOrder.length).fill(false);

    const el = document.getElementById('appLayoutList');
    if (!el) return;

    el.innerHTML = _appOrder.map((appIdx, pos) => {
        const name       = APP_NAMES[appIdx - 1] || `App ${appIdx}`;
        const isSettings = appIdx === APP_NAMES.length;
        const hidden_    = _appHidden[pos] || false;
        const rowBg      = hidden_ ? '#f8d7da' : '#f0fff4';
        const badge      = isSettings ? '<span style="font-size:11px;color:#6c757d;margin-left:6px;">always visible</span>'
            : `<button onclick="_appLayoutToggle(${pos})" style="padding:2px 8px;font-size:11px;background:${hidden_ ? '#198754' : '#dc3545'};color:#fff;border:none;border-radius:3px;cursor:pointer;">${hidden_ ? 'Show' : 'Hide'}</button>`;
        const moveUp   = pos > 0 && !isSettings
            ? `<button onclick="_appLayoutMove(${pos},-1)" style="padding:2px 6px;font-size:11px;background:#6c757d;color:#fff;border:none;border-radius:3px;cursor:pointer;">↑</button>` : '';
        const moveDown = pos < _appOrder.length - 2 && !isSettings
            ? `<button onclick="_appLayoutMove(${pos},1)" style="padding:2px 6px;font-size:11px;background:#6c757d;color:#fff;border:none;border-radius:3px;cursor:pointer;">↓</button>` : '';
        return `<div style="display:flex;align-items:center;gap:6px;padding:5px 8px;background:${rowBg};border:1px solid #dee2e6;border-radius:4px;margin-bottom:4px;">
            <span style="color:#6c757d;font-size:11px;min-width:20px;">${pos + 1}.</span>
            <span style="flex:1;font-size:13px;${hidden_ ? 'color:#999;text-decoration:line-through;' : ''}">${_esc(name)}</span>
            ${moveUp}${moveDown}${badge}
        </div>`;
    }).join('');
}

function _appLayoutToggle(pos) {
    if (pos < _appHidden.length && _appOrder[pos] !== APP_NAMES.length) {
        _appHidden[pos] = !_appHidden[pos];
        _renderAppLayout(_appOrder, _appHidden);
    }
}

function _appLayoutMove(pos, delta) {
    const newPos = pos + delta;
    if (newPos < 0 || newPos >= _appOrder.length - 1) return; // can't move past Settings
    [_appOrder[pos],  _appOrder[newPos]]  = [_appOrder[newPos],  _appOrder[pos]];
    [_appHidden[pos], _appHidden[newPos]] = [_appHidden[newPos], _appHidden[pos]];
    _renderAppLayout(_appOrder, _appHidden);
}

async function loadAppLayout() {
    if (!isConnected) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/settings`, { method: 'GET' });
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        const d = await resp.json();
        if (d.appOrder && d.appHidden) _renderAppLayout(d.appOrder, d.appHidden);
    } catch(e) {
        logDebug('loadAppLayout: ' + e.message, 'error');
    }
}

async function saveAppLayout() {
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/settings`, {
            method: 'POST',
            body: JSON.stringify({ appOrder: _appOrder, appHidden: _appHidden })
        });
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        _statusMsg('appLayoutStatus', '✓ Layout saved', true);
    } catch(e) {
        _statusMsg('appLayoutStatus', '✗ ' + e.message, false);
    }
}

function _setVal(id, val) {
    const el = document.getElementById(id);
    if (el && val !== undefined && val !== null) el.value = val;
}

function _statusMsg(id, msg, ok) {
    const el = document.getElementById(id);
    if (!el) return;
    el.textContent = msg;
    el.className = 'settings-status ' + (ok ? 'success' : 'error');
    setTimeout(() => { el.textContent = ''; el.className = 'settings-status'; }, 4000);
}

async function saveTimingSettings() {
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }
    const timing = {
        key_press_delay:         parseInt(document.getElementById('timingKeyPress')?.value)    || 0,
        key_release_delay:       parseInt(document.getElementById('timingKeyRelease')?.value)  || 0,
        between_keys_delay:      parseInt(document.getElementById('timingBetweenKeys')?.value) || 0,
        between_send_text_delay: parseInt(document.getElementById('timingBetweenSend')?.value) || 0,
        special_key_delay:       parseInt(document.getElementById('timingSpecialKey')?.value)  || 0,
        token_delay:             parseInt(document.getElementById('timingToken')?.value)        || 0,
    };
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/settings`, {
            method: 'POST', mode: 'cors',
            body: JSON.stringify({ timing })
        });
        if (resp.ok) {
            logDebug('Timing settings saved', 'success');
            _statusMsg('timingStatus', '✓ Saved', true);
        } else {
            throw new Error('HTTP ' + resp.status);
        }
    } catch(e) {
        logDebug('saveTimingSettings: ' + e.message, 'error');
        _statusMsg('timingStatus', '✗ ' + e.message, false);
    }
}


function toggleTokenExamples(section = 'controls') {
    const content = document.getElementById(`tokenExamplesContent${section === 'controls' ? 'Controls' : 'Registers'}`);
    const toggle = document.getElementById(`tokenToggle${section === 'controls' ? 'Controls' : 'Registers'}`);

    if (content && toggle) {
        if (content.classList.contains('expanded')) {
            content.classList.remove('expanded');
            toggle.classList.remove('expanded');
        } else {
            content.classList.add('expanded');
            toggle.classList.add('expanded');
        }
    }
}

function showBusy(elementId) {
    const element = document.getElementById(elementId);
    if (element) {
        element.classList.add('show');
    }
}

function hideBusy(elementId) {
    const element = document.getElementById(elementId);
    if (element) {
        element.classList.remove('show');
    }
}

async function loadRegisters() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    const endpoint = getApiEndpoint();

    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'GET',
            mode: 'cors',
        });

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();
        logDebug(`Registers data -- ${JSON.stringify(data).substring(0, 120)}`, 'success');

        currentActiveRegister = data.activeRegister;

        registers = [];
        numRegisters = 0;
        if (data.registers && Array.isArray(data.registers)) {
            numRegisters = data.registers.length;
            data.registers.forEach((reg, index) => {
                registers[index] = { content: reg.content || '', name: reg.name || '' };
            });
        }

        updateActiveRegisterUI();
        populateRegisterList();
        fuzzySearch(); // keep fuzzy results in sync

    } catch (error) {
        logDebug(`Failed to load registers: ${error.message}`, 'error');
    }
}

function populateRegisterList() {
    const registerList = document.getElementById('registerList');
    if (!registerList) return;
    
    registerList.innerHTML = '';

    if (numRegisters === 0) {
        registerList.innerHTML = `<p style="text-align: center; color: #6c757d; padding: 20px;">No registers created yet. Click "Add New Register" to get started.</p>`;
        populateRegisterDropdown();
        return;
    }

    let i = 0;
    while (i !== numRegisters) {
        const reg = registers[i] || { content: '', name: '' };
        const regContent = typeof reg === 'string' ? reg : (reg.content || '');
        const regName = typeof reg === 'string' ? '' : (reg.name || '');
        const grid = document.createElement('div');
        grid.className = `register-grid ${i === currentActiveRegister ? 'active' : ''}`;
        grid.setAttribute('data-index', i);

        grid.innerHTML = `
            <div class="register-header">
                <div class="register-number">${i + 1}</div>
                <div class="register-active-indicator">${i === currentActiveRegister ? '*' : ''}</div>
                <div class="register-reorder-buttons">
                    <button class="reorder-btn" onclick="moveRegister(${i}, -1)" ${i === 0 ? 'disabled' : ''} title="Move up">Up</button>
                    <button class="reorder-btn" onclick="moveRegister(${i}, 1)" ${i === numRegisters - 1 ? 'disabled' : ''} title="Move down">Dn</button>
                </div>
            </div>
            <div class="register-name-container">
                <input type="text" class="register-name-input" value="${escapeHtml(regName)}" 
                    data-register-name="${i}" placeholder="Register name (optional)"
                    onblur="saveRegisterName(${i})">
            </div>
            <div class="register-input-container">
                <input type="text" class="register-input" value="${escapeHtml(regContent)}" 
                    data-register="${i}" placeholder="Enter text with tokens...">
            </div>
            <div class="register-controls">
                <button class="register-button set-active-button" onclick="setActiveRegisterButton(${i})" ${i === currentActiveRegister ? 'disabled' : ''}>Set Active</button>
                <button class="register-button play-button" onclick="playRegister(${i})">Play</button>
                <button class="register-button save-button" onclick="saveRegister(${i})">Save</button>
                <button class="register-button delete-button" onclick="deleteRegister(${i})">Del</button>
            </div>
            `;

        registerList.appendChild(grid);
        i++;
    }
    
    populateRegisterDropdown();
}

function populateRegisterDropdown() {
    const dropdown = document.getElementById('rawSendRegisterSelect');
    if (!dropdown) return;
    
    dropdown.innerHTML = '';
    
    const newOption = document.createElement('option');
    newOption.value = '-1';
    newOption.textContent = 'New Register';
    dropdown.appendChild(newOption);
    
    for (let i = 0; i < numRegisters; i++) {
        const option = document.createElement('option');
        option.value = i.toString();
        const reg = registers[i];
        const name = (reg && typeof reg === 'object' && reg.name) ? reg.name : '';
        option.textContent = name ? `${i + 1}: ${name}` : `Register ${i + 1}`;
        dropdown.appendChild(option);
    }
}

function saveToRegister() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    const dropdown = document.getElementById('rawSendRegisterSelect');
    const textInput = document.getElementById('textInput');
    
    if (!dropdown || !textInput) return;
    
    const selectedValue = dropdown.value;
    const content = textInput.value;
    
    if (selectedValue === '-1') {
        addRegister(content);
        logDebug('Created new register with content', 'success');
    } else {
        const regNum = parseInt(selectedValue);
        if (regNum >= 0 && regNum < registers.length) {
            const input = document.querySelector(`input[data-register="${regNum}"]`);
            if (input) input.value = content;
            if (!registers[regNum]) registers[regNum] = { content: '', name: '' };
            if (typeof registers[regNum] === 'string') registers[regNum] = { content: registers[regNum], name: '' };
            registers[regNum].content = content;
            saveRegister(regNum);
            logDebug(`Saved content to register ${regNum + 1}`, 'success');
        }
    }
}

function loadFromRegister() {
    const dropdown = document.getElementById('rawSendRegisterSelect');
    const textInput = document.getElementById('textInput');
    
    if (!dropdown || !textInput) return;
    
    const selectedValue = dropdown.value;
    
    if (selectedValue === '-1') {
        logDebug('Cannot load from "New Register" - select an existing register', 'warning');
        return;
    }
    
    const regNum = parseInt(selectedValue);
    if (regNum >= 0 && regNum < registers.length) {
        const reg = registers[regNum];
        textInput.value = (reg && typeof reg === 'object') ? (reg.content || '') : (reg || '');
        logDebug(`Loaded content from register ${regNum + 1}`, 'success');
    } else {
        logDebug('Invalid register selection', 'error');
    }
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

async function setActiveRegisterButton(regNum) {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    if (regNum === currentActiveRegister) {
        return;
    }

    const success = await setActiveRegisterOnDevice(regNum);
    if (success) {
        currentActiveRegister = regNum;
        updateActiveRegisterUI();
    }
}

async function addRegister(content = '') {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    const endpoint = getApiEndpoint();
    const payload = {
        action: 'add',
        content: content
    };

    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify(payload)
        });

        if (response.ok) {
            logDebug(`Added new register`, 'success');
            await loadRegisters();

            if (numRegisters === 1) {
                currentActiveRegister = 0;
                await setActiveRegisterOnDevice(0);
            }
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to add register: ${error.message}`, 'error');
    }
}

async function deleteRegister(regNum) {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    if (!confirm(`Are you sure you want to delete register ${regNum + 1}? This cannot be undone.`)) {
        return;
    }

    const endpoint = getApiEndpoint();
    const payload = {
        action: 'delete',
        register: regNum
    };

    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify(payload)
        });

        if (response.ok) {
            logDebug(`Deleted register ${regNum + 1}`, 'success');
            await loadRegisters();
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to delete register ${regNum + 1}: ${error.message}`, 'error');
    }
}

async function deleteAllRegisters() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    if (!confirm('Are you sure you want to delete ALL registers? This cannot be undone and will clear all stored macros.')) {
        return;
    }

    const endpoint = getApiEndpoint();
    const payload = {
        action: 'deleteAll'
    };

    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify(payload)
        });

        if (response.ok) {
            logDebug('All registers deleted', 'success');
            await loadRegisters();
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to delete all registers: ${error.message}`, 'error');
    }
}

async function saveRegister(regNum) {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    const input = document.querySelector(`input[data-register="${regNum}"]`);
    if (!input) return;
    
    const content = input.value;
    if (!registers[regNum]) registers[regNum] = { content: '', name: '' };
    if (typeof registers[regNum] === 'string') registers[regNum] = { content: registers[regNum], name: '' };
    registers[regNum].content = content;

    const nameInput = document.querySelector(`input[data-register-name="${regNum}"]`);
    const name = nameInput ? nameInput.value : '';
    registers[regNum].name = name;

    const endpoint = getApiEndpoint();
    const payload = {
        registers: [{
            number: regNum,
            content: content,
            name: name
        }]
    };

    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify(payload)
        });

        if (response.ok) {
            logDebug(`Saved register ${regNum + 1}`, 'success');
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to save register ${regNum + 1}: ${error.message}`, 'error');
    }
}

async function saveRegisterName(regNum) {
    if (!isConnected) return;
    const nameInput = document.querySelector(`input[data-register-name="${regNum}"]`);
    if (!nameInput) return;
    const name = nameInput.value;
    if (!registers[regNum]) registers[regNum] = { content: '', name: '' };
    if (typeof registers[regNum] === 'string') registers[regNum] = { content: registers[regNum], name: '' };
    registers[regNum].name = name;

    const endpoint = getApiEndpoint();
    try {
        await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify({ action: 'setName', register: regNum, name })
        });
    } catch (error) {
        logDebug(`Failed to save register name: ${error.message}`, 'error');
    }
}

async function playRegister(regNum) {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    const reg = registers[regNum];
    const content = (reg && typeof reg === 'object') ? (reg.content || '') : (reg || '');
    if (!content.trim()) {
        logDebug(`Register ${regNum + 1} is empty`, 'warning');
        return;
    }

    logDebug(`Playing register ${regNum + 1}: ${content}`, 'success');
    await sendTextToDevice(content);
}

async function setActiveRegisterOnDevice(regNum) {
    const endpoint = getApiEndpoint();
    const payload = {
        activeRegister: regNum
    };

    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify(payload)
        });

        if (response.ok) {
            logDebug(`Set active register to ${regNum + 1}`, 'success');
            return true;
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to set active register: ${error.message}`, 'error');
        return false;
    }
}

async function saveAllRegisters() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    const registerInputs = document.querySelectorAll('.register-input');
    const registersArray = [];

    registerInputs.forEach((input, index) => {
        const name = document.querySelector(`input[data-register-name="${index}"]`)?.value || '';
        if (!registers[index]) registers[index] = { content: '', name: '' };
        if (typeof registers[index] === 'string') registers[index] = { content: registers[index], name: '' };
        registers[index].content = input.value;
        registers[index].name = name;
        registersArray.push({ number: index, content: input.value, name });
    });

    const endpoint = getApiEndpoint();
    const payload = { registers: registersArray };

    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify(payload)
        });

        if (response.ok) {
            logDebug('Saved all registers to device', 'success');
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to save all registers: ${error.message}`, 'error');
    }
}

async function getAllSettings() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return null;
    }

    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/settings`, {
            method: 'GET',
            mode: 'cors',
        });

        if (response.ok) {
            return await response.json();
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to get settings: ${error.message}`, 'error');
        return null;
    }
}

async function setAllSettings(settings) {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return false;
    }

    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/settings`, {
            method: 'POST',
            body: JSON.stringify(settings),
            mode: 'cors'
        });

        if (response.ok) {
            logDebug('Settings updated successfully', 'success');
            return true;
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to set settings: ${error.message}`, 'error');
        return false;
    }
}

async function deleteAllSettings() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return false;
    }

    if (!confirm('Are you sure you want to delete ALL settings? This cannot be undone.')) {
        return false;
    }

    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/settings`, {
            method: 'DELETE',
            mode: 'cors'
        });

        if (response.ok) {
            logDebug('All settings deleted successfully', 'success');
            return true;
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to delete settings: ${error.message}`, 'error');
        return false;
    }
}

async function getAllRegisters() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return null;
    }

    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'GET',
            mode: 'cors',
        });

        if (response.ok) {
            return await response.json();
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to get registers: ${error.message}`, 'error');
        return null;
    }
}

async function setAllRegisters(registersData) {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return false;
    }

    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify(registersData),
            mode: 'cors'
        });

        if (response.ok) {
            logDebug('Registers updated successfully', 'success');
            await loadRegisters();
            return true;
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to set registers: ${error.message}`, 'error');
        return false;
    }
}

async function deleteAllRegistersNormalized() {
    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return false;
    }

    if (!confirm('Are you sure you want to delete ALL registers? This cannot be undone.')) {
        return false;
    }

    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'DELETE',
            mode: 'cors'
        });

        if (response.ok) {
            logDebug('All registers deleted successfully', 'success');
            await loadRegisters();
            return true;
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to delete all registers: ${error.message}`, 'error');
        return false;
    }
}

// Pre-fetch a nonce into the mouse pipeline cache so the first movement sends immediately.
async function _prefetchMouseNonce() {
    if (_mouseNonceFetching || _mouseNonce) return;
    _mouseNonceFetching = true;
    try {
        const endpoint = getApiEndpoint();
        const resp = await fetch(`${endpoint}/api/nonce`);
        const data = await resp.json();
        _mouseNonce = data.nonce;
    } catch (_) {}
    _mouseNonceFetching = false;
}

// Send accumulated mouse movement directly without the main API queue so that
// keyboard/register requests don't stall pointer movement.
async function _flushMouseMovementDirect() {
    if (_mouseSendInFlight || !isConnected) return;
    if (trackpadPendingDx === 0 && trackpadPendingDy === 0) return;

    const dx = Math.round(trackpadPendingDx);
    const dy = Math.round(trackpadPendingDy);
    trackpadPendingDx = 0;
    trackpadPendingDy = 0;

    _mouseSendInFlight = true;
    try {
        wsClient.ws.send(JSON.stringify({ x: dx, y: dy }));
    } catch (_) {
        // Restore on error so the movement isn't dropped silently
        trackpadPendingDx += dx;
        trackpadPendingDy += dy;
    }
    _mouseSendInFlight = false;
    _prefetchMouseNonce();
}

// Fixed-rate loop -- fires every 1000/MOUSE_HZ ms while the finger/pointer is down,
// draining accumulated deltas at a controlled rate.
function _mouseTickLoop() {
    _flushMouseMovementDirect();
    if (!isTracking) {
        clearInterval(_mouseIntervalId);
        _mouseIntervalId = null;
        _flushMouseMovementDirect(); // final drain
    }
}

function initTrackpad() {
    trackpadElement = document.getElementById('trackpad');
    if (!trackpadElement) return;

    const sensitivitySlider = document.getElementById('sensitivity');
    const sensitivityValue  = document.getElementById('sensitivityValue');
    const trackpadStatus    = document.getElementById('trackpadStatus');

    if (sensitivitySlider && sensitivityValue) {
        sensitivitySlider.addEventListener('input', function() {
            sensitivityValue.textContent = (this.value / 10).toFixed(1);
        });
        sensitivityValue.textContent = (sensitivitySlider.value / 10).toFixed(1);
    }

    function getSensitivity() {
        return parseFloat(sensitivitySlider?.value || 10) / 10;
    }

    function getClientPos(e) {
        if (e.touches && e.touches.length > 0) {
            return { x: e.touches[0].clientX, y: e.touches[0].clientY };
        }
        return { x: e.clientX, y: e.clientY };
    }

    function onStart(e) {
        e.preventDefault();
        isTracking = true;
        const pos = getClientPos(e);
        lastTrackpadPosition = { x: pos.x, y: pos.y };
        trackpadElement.style.cursor = 'grabbing';
        if (!_mouseIntervalId) {
            _mouseIntervalId = setInterval(_mouseTickLoop, 1000 / MOUSE_HZ);
        }
    }

    function onMove(e) {
        if (!isTracking) return;
        e.preventDefault();
        const pos = getClientPos(e);
        const sensitivity = getSensitivity();
        // Accumulate as floats; rounding happens at send time to avoid quantisation drift.
        trackpadPendingDx += (pos.x - lastTrackpadPosition.x) * sensitivity;
        trackpadPendingDy += (pos.y - lastTrackpadPosition.y) * sensitivity;
        lastTrackpadPosition = { x: pos.x, y: pos.y };
        if (trackpadStatus) {
            trackpadStatus.textContent = `Delta: ${Math.round(trackpadPendingDx)}, ${Math.round(trackpadPendingDy)}`;
        }
    }

    function onEnd(e) {
        e.preventDefault();
        isTracking = false;
        trackpadElement.style.cursor = 'crosshair';
        // rAF loop will do the final flush and cancel itself
    }

    trackpadElement.addEventListener('mousedown',   onStart);
    trackpadElement.addEventListener('mousemove',   onMove);
    trackpadElement.addEventListener('mouseup',     onEnd);
    trackpadElement.addEventListener('mouseleave',  onEnd);
    trackpadElement.addEventListener('touchstart',  onStart,  { passive: false });
    trackpadElement.addEventListener('touchmove',   onMove,   { passive: false });
    trackpadElement.addEventListener('touchend',    onEnd,    { passive: false });
    trackpadElement.addEventListener('touchcancel', onEnd,    { passive: false });
    trackpadElement.addEventListener('contextmenu', e => e.preventDefault());
}

function accumulateTrackpadMovement(deltaX, deltaY) {
    trackpadPendingDx += deltaX;
    trackpadPendingDy += deltaY;
    if (!_mouseIntervalId) {
        _mouseIntervalId = setInterval(_mouseTickLoop, 1000 / MOUSE_HZ);
    }
}

// Legacy alias kept for any external callers
async function flushTrackpadBatch() {
    await _flushMouseMovementDirect();
}

async function _sendMouseActionDirect(payload) {
    if (!isConnected) return;
    wsClient.ws.send(JSON.stringify(payload));

}

async function sendTrackpadClick(button) {
    await _sendMouseActionDirect({ button, action: 'click' });
}

async function sendTrackpadDoubleClick(button) {
    await _sendMouseActionDirect({ button, action: 'double' });
}

window.onload = function() {
    const savedKey = localStorage.getItem('apiKey');
    if (savedKey) apiKey = savedKey;

    const apiKeyInput = document.getElementById('apiKeyInput');
    if (apiKeyInput) apiKeyInput.value = apiKey;

    const apiEndpointInput = document.getElementById('apiEndpoint');
    if (apiEndpointInput) {
        if (!apiEndpointInput.value || apiEndpointInput.value === 'null') {
            const endpoint = getApiEndpoint();
            apiEndpointInput.value = endpoint;
        }
        logDebug(`Initialized with endpoint: ${apiEndpointInput.value}`);
    }

    if (window.location.protocol === 'https:') {
        logDebug('Warning: Running on HTTPS - HTTP device connections may be blocked', 'warning');
        logDebug('Consider serving this page over HTTP for device connectivity', 'warning');
    }

    cacheLogo();
    initTrackpad();
    populateReferenceTable();
    updateLEDColor();
    updateBluetoothUI();

    registers = [];
    numRegisters = 0;
    currentActiveRegister = -1;
    populateRegisterList();

    updateConnectionUI();
    updateRequestStatus();
    updateActiveRegisterUI();
    updateLoopStatusUI();

    connect();
};

function cacheLogo() {
    const img = document.getElementById('sidebarLogo');
    if (!img) return;
    const cached = sessionStorage.getItem('kprox_logo');
    if (cached) {
        img.src = cached;
        return;
    }
    const endpoint = getApiEndpoint();
    fetch(`${endpoint}/kprox.png`)
        .then(r => r.ok ? r.blob() : null)
        .then(blob => {
            if (!blob) return;
            const reader = new FileReader();
            reader.onload = () => {
                sessionStorage.setItem('kprox_logo', reader.result);
                img.src = reader.result;
            };
            reader.readAsDataURL(blob);
        })
        .catch(() => {});
}
class mouseWebSocket {
  constructor(url) {
    this.url = url;
    this.ws = null;
    this.reconnectAttempts = 0;
    this.maxReconnectAttempts = 10; // Stop after 10 retries
    this.reconnectDelay = 1000; // Start with 1s delay
  }
 
  connect() {
    this.ws = new WebSocket(this.url);
 
    // Handle connection open
    this.ws.onopen = () => {
      console.log("Connected to WebSocket server");
      this.reconnectAttempts = 0; // Reset retries on success
      this.reconnectDelay = 1000; // Reset delay
    };
 
    // Handle disconnection
    this.ws.onclose = (event) => {
      console.log(`Disconnected. Code: ${event.code}, Reason: ${event.reason}`);
      this.reconnect();
    };
 
    // Handle errors
    this.ws.onerror = (error) => {
      console.error("WebSocket error:", error);
      this.ws.close(); // Trigger onclose for reconnection
    };
  }
 
  reconnect() {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.error("Max reconnection attempts reached. Stopping.");
      return;
    }
 
    this.reconnectAttempts++;
    console.log(`Reconnecting (attempt ${this.reconnectAttempts})...`);
 
    // Attempt reconnection after delay
    setTimeout(() => this.connect(), this.reconnectDelay);
 
    // Increase delay (exponential backoff)
    this.reconnectDelay *= 2;
  }
}
 
const wsUrl = getApiEndpoint().replace(/^http/, 'ws') + ':81/';
const wsClient = new mouseWebSocket(wsUrl); 

async function connect() {
    const endpoint = getApiEndpoint();
    const connectBtn = document.getElementById('connectBtn');

    if (connectBtn) {
        connectBtn.textContent = 'Connecting...';
        connectBtn.className = 'connect-btn connecting';
        connectBtn.disabled = true;
    }

    logDebug(`Attempting to connect to: ${endpoint}/api/status`);

    try {
        const response = await apiFetch(`${endpoint}/api/status`, {
            method: 'GET',
            mode: 'cors',
        });

        logDebug(`Response status: ${response.status}`);

        if (!response.ok) {
            const errData = await response.json();
            throw new Error(`HTTP ${response.status}: ${errData.error || 'Request failed'}`);
        }

        wsClient.connect();

        const data = await response.json();
        logDebug(`rx: ${JSON.stringify(data)}`, 'success');

        isConnected = true;
        updateConnectionUI();

        // Extract basic info
        safeSetText('deviceName', data.boardType || '-');
        safeSetText('hostname',   data.hostname  || '-');
        if (data.hostname) deviceHostname = data.hostname;
        safeSetText('freeHeap',   data.free_heap ? `${(data.free_heap / 1024).toFixed(1)} KB` : '-');
        safeSetText('uptime',     data.uptime    ? `${Math.floor(data.uptime / 1000)}s` : '-');
        updateMemStatsUI(data);

        // Chip details
        const chipStr = data.chipModel
            ? `${data.chipModel} rev${data.chipRevision || 0}`
            : '-';
        safeSetText('chipModel',  chipStr);
        safeSetText('cpuFreq',    data.cpuFreq  ? `${data.cpuFreq} MHz` : '-');
        safeSetText('flashSize',  data.flashSize ? `${(data.flashSize / (1024 * 1024)).toFixed(0)} MB` : '-');

        // IP -- available directly on response and also nested under wifi
        const ip = data.ip || (data.connections && data.connections.wifi && data.connections.wifi.ip) || '-';
        safeSetText('ipAddress', ip);
        if (ip !== '-') { ipAddress = ip; updateSinkCurlExamples(); }

        if (data.activeKeymap) {
            safeSetText('keymapActive', data.activeKeymap);
        }

        if (data.connections && data.connections.wifi) {
            updateWiFiStatus(data.connections.wifi);
        }

        if (data.ntp_synced !== undefined) {
            _updateCsNtpBadge(data.ntp_synced);
        }

        // Update all connectivity toggles (BT + USB sub-enables + FIDO2)
        if (data.connections) {
            updateConnectivityUI(data);
            // Keep old BT vars in sync for legacy code
            if (data.connections.bluetooth) {
                const bt = data.connections.bluetooth;
                if (bt.hasOwnProperty('enabled'))     bluetoothEnabled     = bt.enabled;
                if (bt.hasOwnProperty('initialized'))  bluetoothInitialized = bt.initialized;
                if (bt.hasOwnProperty('connected'))    bluetoothConnected   = bt.connected;
            }
        }

        if (data.hasOwnProperty('halted')) {
            isHalted = data.halted;
            updateHaltStatusUI();
        }

        if (data.hasOwnProperty('active_register')) {
            currentActiveRegister = data.active_register;
        }
        if (data.hasOwnProperty('total_registers')) {
            numRegisters = data.total_registers;
            if (numRegisters === 0) {
                currentActiveRegister = -1;
            }
        }
        updateActiveRegisterUI();

        if (data.hasOwnProperty('looping')) {
            deviceLoopActive = data.looping;
        }
        if (data.hasOwnProperty('looping_register')) {
            deviceLoopingRegister = data.looping_register;
        }
        updateLoopStatusUI();
        if (data.bootReg) updateBootProxStatusUI(data.bootReg);
        updateMemStatsUI(data);

        if (data.hasOwnProperty('request_in_progress')) {
            requestInProgress = data.request_in_progress;
            updateRequestStatus();
        }

        // Update LED status from nested structure
        if (data.led) {
            ledEnabled = data.led.enabled;
            if (data.led.color) {
                ledColorR = data.led.color.r;
                ledColorG = data.led.color.g;
                ledColorB = data.led.color.b;
            }

            const ledRed = document.getElementById('ledRed');
            const ledGreen = document.getElementById('ledGreen');
            const ledBlue = document.getElementById('ledBlue');
            
            if (ledRed) ledRed.value = ledColorR;
            if (ledGreen) ledGreen.value = ledColorG;
            if (ledBlue) ledBlue.value = ledColorB;
            
            updateLEDColor();

            const toggleBtn = document.getElementById('ledToggle');
            if (toggleBtn) {
                if (ledEnabled) {
                    toggleBtn.textContent = 'Disable LED';
                    toggleBtn.classList.add('enabled');
                } else {
                    toggleBtn.textContent = 'Enable LED';
                    toggleBtn.classList.remove('enabled');
                }
            }
        }

        // Load device settings
        await loadDeviceSettings();

        if (data.hasOwnProperty('utcOffset')) {
            const inp = document.getElementById('utcOffsetInput');
            if (inp) inp.value = data.utcOffset;
        }
        
        // Load WiFi settings
        await loadWiFiSettings();
        await loadMTLSStatus();
        await loadKeymapSettings();

        // Sync settings tab endpoint field
        const sepEl = document.getElementById('settingsApiEndpoint');
        if (sepEl) sepEl.value = getApiEndpoint();

        const registersTab = document.getElementById('registers-tab');
        if (registersTab && registersTab.classList.contains('active')) {
            await loadRegisters();
        }

        const settingsTabEl = document.getElementById('settings-tab');
        if (settingsTabEl && settingsTabEl.classList.contains('active')) {
            await loadSettingsTab();
        }

        const schedTab = document.getElementById('schedtasks-tab');
        if (schedTab && schedTab.classList.contains('active')) {
            await loadSchedTasks();
        }

        const totpTab = document.getElementById('totprox-tab');
        if (totpTab && totpTab.classList.contains('active')) {
            await loadTOTP();
        }

        // Update credstore lock badge (tab + global sidebar)
        if (data.hasOwnProperty('credStoreLocked')) {
            const locked = data.credStoreLocked;
            const countTxt = `${data.credStoreCount || 0} credential${data.credStoreCount !== 1 ? 's' : ''}`;
            const bg    = locked ? '#dc3545' : '#28a745';
            const label = locked ? 'LOCKED' : 'UNLOCKED';

            const tabBadge  = document.getElementById('credStoreLockBadge');
            const tabCount  = document.getElementById('credStoreCountLabel');
            const globBadge = document.getElementById('globalCsLockBadge');
            const globCount = document.getElementById('globalCsCountLabel');

            if (tabBadge)  { tabBadge.textContent  = label; tabBadge.style.background  = bg; }
            if (tabCount)  { tabCount.textContent   = countTxt; }
            if (globBadge) { globBadge.textContent  = label; globBadge.style.background = bg; }
            if (globCount) { globCount.textContent  = countTxt; }
        }

        // Update sink size badge from status (if device includes it)
        if (data.hasOwnProperty('sinkSize')) {
            updateSinkBadge(data.sinkSize);
        }

        logDebug('Connection successful', 'success');
    } catch (error) {
        isConnected = false;
        updateConnectionUI();
        logDebug(`Connection failed: ${error.message}`, 'error');

        if (error.name === 'TypeError' && error.message.includes('fetch')) {
            logDebug('Network error - check if device is reachable and CORS is enabled', 'error');
        } else if (error.message.includes('CORS')) {
            logDebug('CORS error - make sure device firmware includes CORS headers', 'error');
        } else if (error.message.includes('blocked')) {
            logDebug('Mixed content blocked - try accessing this page via HTTP instead of HTTPS', 'warning');
        }
    } finally {
        if (connectBtn) {
            connectBtn.disabled = false;
        }
    }
}

function stripNonAscii(inputString) {
    return inputString.replace(/[^\x00-\x7F]/g, '');
}

async function sendTextToDevice(text) {
    text = stripNonAscii(text);
    const endpoint = getApiEndpoint();

    try {
        const response = await apiFetch(`${endpoint}/send/text`, {
            method: 'POST',
            body: JSON.stringify({ text: text })
        });

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${await response.text()}`);
        }

        logDebug(`tx: "${text.substring(0, 100)}..." (truncated for log)`, 'success');
        return true;
    } catch (error) {
        logDebug(`Text send failed: ${error.message}`, 'error');
        return false;
    }
}

async function sendText() {
    if (requestInProgress) { logDebug('Request already in progress', 'warning'); return; }
    if (!isConnected)      { logDebug('Not connected', 'warning'); return; }

    const textInput = document.getElementById('textInput');
    if (!textInput) return;

    const text = textInput.value;
    const toSink = document.getElementById('sendToSinkCheck')?.checked || false;

    if (toSink) {
        // Route to sink endpoint — no batching needed, single POST
        const endpoint = getApiEndpoint();
        const responseBox = document.getElementById('textResponse');
        showBusy('textBusy');
        try {
            const resp = await apiFetch(`${endpoint}/api/sink`, {
                method: 'POST',
                body: JSON.stringify({ text })
            });
            const data = await resp.json();
            if (responseBox) responseBox.textContent = JSON.stringify(data);
            logDebug(`Sent ${text.length} chars to sink (${data.size || '?'} bytes total)`, 'success');
            updateSinkBadge(data.size || 0);
        } catch(e) {
            logDebug('Sink send error: ' + e.message, 'error');
        } finally { hideBusy('textBusy'); }
        return;
    }

    requestInProgress = true;
    updateRequestStatus();
    showBusy('textBusy');

    const responseBox = document.getElementById('textResponse');
    const BATCH_SIZE = 100;

    try {
        let currentIndex = 0;
        let allBatchesSentSuccessfully = true;

        while (currentIndex !== text.length && text.length > currentIndex) {
            const batch = text.substring(currentIndex, currentIndex + BATCH_SIZE);
            logDebug(`Sending text batch (bytes ${currentIndex} to ${Math.min(currentIndex + BATCH_SIZE, text.length)} of ${text.length})...`, 'info');

            const success = await sendTextToDevice(batch);

            if (!success) {
                allBatchesSentSuccessfully = false;
                if (responseBox) responseBox.textContent = `Error: Failed to send text batch starting at index ${currentIndex}`;
                break;
            }

            currentIndex += BATCH_SIZE;
            await new Promise(resolve => setTimeout(resolve, 250));
        }

        if (allBatchesSentSuccessfully) {
            if (responseBox) responseBox.textContent = `Response: {"status":"ok", "message":"All text batches sent successfully"}`;
            logDebug('All text batches sent successfully', 'success');
        } else {
            logDebug('One or more text batches failed to send', 'error');
        }

    } catch (error) {
        if (responseBox) responseBox.textContent = `Error: ${error.message}`;
        logDebug(`An unexpected error occurred: ${error.message}`, 'error');
    } finally {
        requestInProgress = false;
        updateRequestStatus();
        hideBusy('textBusy');
    }
}

function toggleTextLoop() {
    const toggleButton = document.getElementById('textLoopToggle');
    const loopStatus = document.getElementById('textLoopStatus');

    if (!toggleButton || !loopStatus) return;

    if (isTextLooping) {
        clearInterval(textLoopInterval);
        isTextLooping = false;
        toggleButton.textContent = 'Start Text Loop';
        toggleButton.classList.remove('active');
        loopStatus.textContent = 'Text Loop: Inactive';
        loopStatus.classList.remove('active');
        loopStatus.classList.add('inactive');
        logDebug('Text loop stopped', 'warning');
    } else {
        if (!isConnected) {
            logDebug('Not connected - please connect first', 'warning');
            return;
        }

        const textInput = document.getElementById('textInput');
        if (!textInput || !textInput.value.trim()) {
            logDebug('Please enter some text first', 'warning');
            return;
        }

        const loopDelayInput = document.getElementById('textLoopDelay');
        const loopDelay = parseInt(loopDelayInput?.value || 2000);
        
        isTextLooping = true;
        toggleButton.textContent = 'Stop Text Loop';
        toggleButton.classList.add('active');
        loopStatus.textContent = `Text Loop: Active (every ${loopDelay}ms)`;
        loopStatus.classList.remove('inactive');
        loopStatus.classList.add('active');
        logDebug(`Text loop started with ${loopDelay}ms delay`, 'success');

        textLoopInterval = setInterval(() => {
            if (!requestInProgress) {
                sendText();
            }
        }, loopDelay);
    }
}

function addCoord() {
    const coordsContainer = document.getElementById('mouseCoords');
    if (!coordsContainer) return;
    
    const newCoord = document.createElement('div');
    newCoord.className = 'mouse-coords';
    newCoord.innerHTML = `
        <input type="number" class="coords-input" placeholder="Delta X" value="0">
        <input type="number" class="coords-input" placeholder="Delta Y" value="0">
        <input type="number" class="coords-input" placeholder="Delay (ms)" value="100">
        <button class="remove-coord" onclick="removeCoord(this)">Remove</button>
        `;
    coordsContainer.appendChild(newCoord);
    logDebug('Mouse coordinate added');
}

function removeCoord(button) {
    const coordsContainer = document.getElementById('mouseCoords');
    if (coordsContainer && coordsContainer.children.length > 1) {
        button.parentElement.remove();
        logDebug('Mouse coordinate removed');
    }
}

async function sendMouseMovement() {
    if (requestInProgress) {
        logDebug('Request already in progress, please wait', 'warning');
        return;
    }

    if (!isConnected) {
        logDebug('Not connected - please connect first', 'warning');
        return;
    }

    const coordsContainer = document.getElementById('mouseCoords');
    if (!coordsContainer) return;

    updateRequestStatus();
    showBusy('mouseBusy');

    const coords = [];

    let i = 0;
    while (i !== coordsContainer.children.length) {
        const inputs = coordsContainer.children[i].querySelectorAll('.coords-input');
        coords.push({
            x: parseInt(inputs[0]?.value || 0),
            y: parseInt(inputs[1]?.value || 0),
            d: parseInt(inputs[2]?.value || 0),
            batch: false
        });
        i++;
    }

    wsClient.ws.send(JSON.stringify(coords));
}

async function updateDeviceSettings() {
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }

    const manufacturer = document.getElementById('newDeviceManufacturer')?.value.trim();
    const product      = document.getElementById('newDeviceProduct')?.value.trim();
    const hostname_    = document.getElementById('newHostname')?.value.trim();
    const usbSerial    = document.getElementById('newUsbSerial')?.value.trim();

    if (!manufacturer && !product && !hostname_ && !usbSerial) {
        logDebug('No device identity fields filled in', 'warning');
        return;
    }

    const device = {};
    if (manufacturer) device.manufacturer = manufacturer;
    if (product)      device.product      = product;
    if (hostname_)    device.hostname     = hostname_;
    if (usbSerial)    device.usb_serial   = usbSerial;

    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/settings`, {
            method: 'POST', mode: 'cors',
            body: JSON.stringify({ device })
        });
        if (resp.ok) {
            logDebug('Device identity saved', 'success');
            await loadDeviceSettings();
        } else {
            throw new Error('HTTP ' + resp.status);
        }
    } catch(e) {
        logDebug('updateDeviceSettings: ' + e.message, 'error');
    }
}

async function loadDeviceSettings() {
    try {
        const endpoint = getApiEndpoint();
        const response = await apiFetch(`${endpoint}/api/device`, {
            method: 'GET',
            mode: 'cors',
        });

        if (response.ok) {
            const data = await response.json();
            logDebug(`Device data -- ${JSON.stringify(data).substring(0, 120)}`, 'info');
            safeSetText('deviceManufacturer', data.manufacturer || '-');
            safeSetText('deviceProduct', data.product || '-');
            // populate settings tab fields if present
            _setVal('newDeviceManufacturer', data.manufacturer);
            _setVal('newDeviceProduct',      data.product);
        }
    } catch (error) {
        console.log('Failed to load device settings:', error.message);
    }
}

async function wipeSettings() {
    if (!confirm('Are you sure you want to wipe all device settings? This cannot be undone.')) {
        return;
    }
    
    if (requestInProgress) {
        logDebug('Settings wipe skipped - another request in progress', 'warning');
        return;
    }

    try {
        requestInProgress = true;
        updateRequestStatus();

        const endpoint = getApiEndpoint();
        const response = await apiFetch(`${endpoint}/api/settings`, {
            method: 'DELETE',
            mode: 'cors'
        });

        if (response.ok) {
            const result = await response.json();
            logDebug('All settings wiped successfully', 'success');
            alert('All settings have been wiped. Device will use defaults.');
            
            // Refresh all status displays
            setTimeout(connect, 2000);
        } else {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
    } catch (error) {
        logDebug(`Settings wipe failed: ${error.message}`, 'error');
        alert(`Failed to wipe settings: ${error.message}`);
    } finally {
        requestInProgress = false;
        updateRequestStatus();
    }
}

async function wipeRegisters() {
    if (!confirm('Are you sure you want to wipe all registers? This cannot be undone.')) {
        return;
    }
    
    if (requestInProgress) {
        logDebug('Registers wipe skipped - another request in progress', 'warning');
        return;
    }

    try {
        requestInProgress = true;
        updateRequestStatus();

        const endpoint = getApiEndpoint();
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'DELETE',
            mode: 'cors'
        });

        if (response.ok) {
            logDebug('All registers wiped successfully', 'success');
            alert('All registers have been wiped.');
            
            await loadRegisters();
        } else {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
    } catch (error) {
        logDebug(`Registers wipe failed: ${error.message}`, 'error');
        alert(`Failed to wipe registers: ${error.message}`);
    } finally {
        requestInProgress = false;
        updateRequestStatus();
    }
}

async function wipeEverything() {
    if (!confirm('Are you sure you want to wipe EVERYTHING (settings and registers)? This cannot be undone.')) {
        return;
    }
    
    if (!confirm('This will completely reset the device. Are you absolutely sure?')) {
        return;
    }
    
    if (requestInProgress) {
        logDebug('Everything wipe skipped - another request in progress', 'warning');
        return;
    }

    try {
        requestInProgress = true;
        updateRequestStatus();

        const endpoint = getApiEndpoint();
        const response = await apiFetch(`${endpoint}/api/wipe`, {
            method: 'DELETE',
            mode: 'cors'
        });

        if (response.ok) {
            logDebug('Everything wiped successfully', 'success');
            alert('Everything has been wiped. Device will use defaults.');
            
            // Refresh all status displays
            setTimeout(connect, 2000);
        } else {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
    } catch (error) {
        logDebug(`Everything wipe failed: ${error.message}`, 'error');
        alert(`Failed to wipe everything: ${error.message}`);
    } finally {
        requestInProgress = false;
        updateRequestStatus();
    }
}

async function moveRegister(index, direction) {
    if (!isConnected) return;
    const newIndex = index + direction;
    if (newIndex < 0 || newIndex >= numRegisters) return;

    const order = Array.from({ length: numRegisters }, (_, i) => i);
    [order[index], order[newIndex]] = [order[newIndex], order[index]];

    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/registers`, {
            method: 'POST',
            body: JSON.stringify({ action: 'reorder', order })
        });
        if (response.ok) {
            logDebug(`Moved register ${index + 1} ${direction < 0 ? 'up' : 'down'}`, 'success');
            await loadRegisters();
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
    } catch (error) {
        logDebug(`Failed to reorder registers: ${error.message}`, 'error');
    }
}

async function exportRegisters() {
    if (!isConnected) {
        logDebug('Not connected', 'warning');
        return;
    }
    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/registers/export`);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const blob = await response.blob();
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'kprox_registers.json';
        a.click();
        URL.revokeObjectURL(url);
        logDebug('Registers exported', 'success');
    } catch (error) {
        logDebug(`Export failed: ${error.message}`, 'error');
    }
}

async function importRegisters() {
    if (!isConnected) {
        logDebug('Not connected', 'warning');
        return;
    }
    const fileInput = document.getElementById('importRegistersFile');
    if (!fileInput || !fileInput.files.length) {
        logDebug('Select a JSON file first', 'warning');
        return;
    }
    const file = fileInput.files[0];
    const text = await file.text();
    const endpoint = getApiEndpoint();
    try {
        const response = await apiFetch(`${endpoint}/api/registers/import`, {
            method: 'POST',
            body: text
        });
        if (response.ok) {
            logDebug('Registers imported successfully', 'success');
            fileInput.value = '';
            await loadRegisters();
        } else {
            throw new Error(`HTTP ${response.status}: ${await response.text()}`);
        }
    } catch (error) {
        logDebug(`Import failed: ${error.message}`, 'error');
    }
}

async function uploadOTA(type) {
    const isFirmware   = type === 'firmware';
    const fileInputId  = isFirmware ? 'otaFile'       : 'spiffsFile';
    const progressId   = isFirmware ? 'otaProgress'   : 'spiffsProgress';
    const statusId     = isFirmware ? 'otaStatus'     : 'spiffsStatus';
    const btnId        = isFirmware ? 'otaUploadBtn'  : 'spiffsUploadBtn';
    const apiPath      = isFirmware ? '/api/ota'      : '/api/ota/spiffs';
    const label        = isFirmware ? 'Firmware'      : 'Filesystem';

    const fileInput = document.getElementById(fileInputId);
    if (!fileInput || !fileInput.files.length) {
        logDebug(`Select a .bin ${label.toLowerCase()} file first`, 'warning');
        return;
    }
    if (!isConnected) { logDebug('Not connected', 'warning'); return; }

    const file       = fileInput.files[0];
    const statusEl   = document.getElementById(statusId);
    const progressEl = document.getElementById(progressId);
    const uploadBtn  = document.getElementById(btnId);
    const endpoint   = getApiEndpoint();

    if (uploadBtn)  uploadBtn.disabled = true;
    if (statusEl)   { statusEl.textContent = 'Uploading...'; statusEl.className = 'status warning'; }

    try {
        const nonce   = await getNonce();
        const hmac    = hmacAuth(nonce);
        const formData = new FormData();
        formData.append(isFirmware ? 'firmware' : 'spiffs', file);

        const xhr = new XMLHttpRequest();
        xhr.open('POST', `${endpoint}${apiPath}`);
        xhr.setRequestHeader('X-Auth', hmac);

        if (progressEl) {
            xhr.upload.onprogress = (e) => {
                if (e.lengthComputable) {
                    progressEl.style.display = 'block';
                    progressEl.value = (e.loaded / e.total) * 100;
                }
            };
        }

        xhr.onload = () => {
            if (progressEl) progressEl.style.display = 'none';
            if (xhr.status === 200) {
                if (statusEl) { statusEl.textContent = `${label} OTA complete! Device is restarting...`; statusEl.className = 'status success'; }
                logDebug(`${label} OTA successful. Device restarting.`, 'success');
                fileInput.value = '';
                setTimeout(() => connect(), 8000);
            } else {
                if (statusEl) { statusEl.textContent = `${label} OTA failed: ${xhr.responseText}`; statusEl.className = 'status error'; }
                logDebug(`${label} OTA failed: ${xhr.responseText}`, 'error');
            }
            if (uploadBtn) uploadBtn.disabled = false;
        };

        xhr.onerror = () => {
            if (statusEl)   { statusEl.textContent = `${label} OTA upload error`; statusEl.className = 'status error'; }
            if (progressEl) progressEl.style.display = 'none';
            if (uploadBtn)  uploadBtn.disabled = false;
            logDebug(`${label} OTA network error`, 'error');
        };

        xhr.send(formData);
        logDebug(`Uploading ${label.toLowerCase()}: ${file.name} (${(file.size / 1024).toFixed(1)} KB)`, 'info');
    } catch (error) {
        if (statusEl)  { statusEl.textContent = `Error: ${error.message}`; statusEl.className = 'status error'; }
        if (uploadBtn) uploadBtn.disabled = false;
        logDebug(`${label} OTA error: ${error.message}`, 'error');
    }
}


setInterval(async () => {
    if (isConnected && !requestInProgress) {
        try {
            const endpoint = getApiEndpoint();
            const response = await apiFetch(`${endpoint}/api/status`, {
                method: 'GET',
                mode: 'cors',
            });

            if (response.ok) {
                const data = await response.json();

                if (data.hasOwnProperty('request_in_progress')) {
                    requestInProgress = data.request_in_progress;
                    updateRequestStatus();
                }

                // Update Bluetooth status from nested structure
                if (data.connections && data.connections.bluetooth) {
                    const bt = data.connections.bluetooth;
                    if (bt.hasOwnProperty('enabled') && bt.enabled !== bluetoothEnabled) {
                        bluetoothEnabled = bt.enabled;
                        updateBluetoothUI();
                    }
                    if (bt.hasOwnProperty('initialized') && bt.initialized !== bluetoothInitialized) {
                        bluetoothInitialized = bt.initialized;
                        updateBluetoothUI();
                    }
                    if (bt.hasOwnProperty('connected') && bt.connected !== bluetoothConnected) {
                        bluetoothConnected = bt.connected;
                        updateBluetoothUI();
                    }
                }

                // Update WiFi status from nested structure
                if (data.connections && data.connections.wifi) {
                    updateWiFiStatus(data.connections.wifi);
                }

                if (data.hasOwnProperty('halted') && data.halted !== isHalted) {
                    isHalted = data.halted;
                    updateHaltStatusUI();
                }

                if (data.hasOwnProperty('active_register') && data.active_register !== currentActiveRegister) {
                    currentActiveRegister = data.active_register;
                    updateActiveRegisterUI();
                }

                if (data.hasOwnProperty('total_registers') && data.total_registers !== numRegisters) {
                    numRegisters = data.total_registers;
                    if (numRegisters === 0) {
                        currentActiveRegister = -1;
                    }
                    updateActiveRegisterUI();

                    const registersTab = document.getElementById('registers-tab');
                    if (registersTab && registersTab.classList.contains('active')) {
                        populateRegisterList();
                    }
                }

                if (data.hasOwnProperty('looping') && data.looping !== deviceLoopActive) {
                    deviceLoopActive = data.looping;
                    updateLoopStatusUI();
                }
                if (data.hasOwnProperty('looping_register') && data.looping_register !== deviceLoopingRegister) {
                    deviceLoopingRegister = data.looping_register;
                    updateLoopStatusUI();
                }
                if (data.bootReg) updateBootProxStatusUI(data.bootReg);

                safeSetText('freeHeap', data.free_heap ? `${(data.free_heap / 1024).toFixed(1)} KB` : '-');
                updateMemStatsUI(data);
                safeSetText('uptime', data.uptime ? `${Math.floor(data.uptime / 1000)}s` : '-');
            }
        } catch (error) {
            console.log('Status poll failed:', error.message);
        }
    }
}, 60000);

window.addEventListener('beforeunload', function() {
    if (isTextLooping) {
        clearInterval(textLoopInterval);
    }
    if (trackpadPendingDx !== 0 || trackpadPendingDy !== 0) {
        flushTrackpadBatch();
    }
});

document.addEventListener('keydown', function(event) {
    if (event.ctrlKey && event.key === 'Enter') {
        event.preventDefault();
        const textInput = document.getElementById('textInput');
        if (textInput && document.activeElement === textInput) {
            sendText();
        }
    }

    if (event.key === 'Escape') {
        if (isTextLooping) {
            toggleTextLoop();
        }

        if (isTracking) {
            isTracking = false;
            if (trackpadElement) {
                trackpadElement.style.cursor = 'crosshair';
            }
            flushTrackpadBatch();
        }
    }
});

window.addEventListener('focus', function() {
    if (!isConnected) {
        setTimeout(connect, 1000);
    }
});

// ---- mTLS / Security ----

async function loadMTLSStatus() {
    const endpoint = getApiEndpoint();
    try {
        const resp = await apiFetch(`${endpoint}/api/mtls`, {
        });
        if (!resp.ok) return;
        const data = await resp.json();

        const enabledEl = document.getElementById('mtlsEnabledStatus');
        const certsEl   = document.getElementById('mtlsCertsStatus');
        const toggle    = document.getElementById('mtlsEnabledToggle');

        if (enabledEl) enabledEl.textContent = data.enabled ? 'Enabled' : 'Disabled';
        if (certsEl) {
            const hasCerts = data.has_server_cert && data.has_server_key;
            certsEl.textContent = hasCerts
                ? `Server cert v, Key v${data.has_ca_cert ? ', CA v' : ''}`
                : 'No certificates';
        }
        if (toggle) toggle.checked = data.enabled;
    } catch (e) {
        logDebug('mTLS status fetch failed: ' + e.message, 'error');
    }
}

async function toggleMTLS(enabled) {
    const endpoint = getApiEndpoint();
    const statusEl = document.getElementById('mtlsStatus');
    try {
        const resp = await apiFetch(`${endpoint}/api/mtls`, {
            method: 'POST',
            body: JSON.stringify({ enabled })
        });
        const data = await resp.json();
        if (statusEl) {
            statusEl.textContent = data.message || (resp.ok ? 'Saved. Restart required.' : data.error);
            statusEl.className   = resp.ok ? 'success' : 'error';
        }
        await loadMTLSStatus();
    } catch (e) {
        if (statusEl) { statusEl.textContent = 'Error: ' + e.message; statusEl.className = 'error'; }
    }
}

async function uploadMTLSCerts() {
    const cert = document.getElementById('serverCertPem')?.value?.trim() || '';
    const key  = document.getElementById('serverKeyPem')?.value?.trim()  || '';
    const ca   = document.getElementById('caCertPem')?.value?.trim()     || '';
    const statusEl = document.getElementById('mtlsStatus');

    if (!cert && !key && !ca) {
        if (statusEl) { statusEl.textContent = 'No certificate data entered.'; statusEl.className = 'warning'; }
        return;
    }

    const endpoint = getApiEndpoint();
    const payload  = {};
    if (cert) payload.server_cert = cert;
    if (key)  payload.server_key  = key;
    if (ca)   payload.ca_cert     = ca;

    try {
        const resp = await apiFetch(`${endpoint}/api/mtls/certs`, {
            method: 'POST',
            body: JSON.stringify(payload)
        });
        const data = await resp.json();
        if (statusEl) {
            statusEl.textContent = data.message || (resp.ok ? 'Certificates uploaded.' : data.error);
            statusEl.className   = resp.ok ? 'success' : 'error';
        }
        if (resp.ok) {
            document.getElementById('serverCertPem').value = '';
            document.getElementById('serverKeyPem').value  = '';
            document.getElementById('caCertPem').value     = '';
            await loadMTLSStatus();
        }
    } catch (e) {
        if (statusEl) { statusEl.textContent = 'Error: ' + e.message; statusEl.className = 'error'; }
    }
}


async function clearMTLSCerts() {
    if (!confirm('Clear all certificates and disable mTLS? This requires a restart.')) return;
    const endpoint = getApiEndpoint();
    const statusEl = document.getElementById('mtlsStatus');
    try {
        const resp = await apiFetch(`${endpoint}/api/mtls/certs`, {
            method: 'DELETE',
        });
        const data = await resp.json();
        if (statusEl) {
            statusEl.textContent = data.message || (resp.ok ? 'Cleared.' : data.error);
            statusEl.className   = resp.ok ? 'success' : 'error';
        }
        await loadMTLSStatus();
    } catch (e) {
        if (statusEl) { statusEl.textContent = 'Error: ' + e.message; statusEl.className = 'error'; }
    }
}

// ---- Keymap Editor ----

let _keymapRows  = [];   // [{char, key, mod}]
let _keymapId    = '';
let _keymapName  = '';
let _keymapDirty = false;
let _keymapActive = 'en';

function _kmStatus(msg, ok) {
    const el = document.getElementById('keymapStatus');
    if (!el) return;
    el.textContent = msg;
    el.style.color = ok ? '#28a745' : '#dc3545';
    if (msg) setTimeout(() => { if (el.textContent === msg) el.textContent = ''; }, 5000);
}

async function keymapEditorInit() {
    try {
        const resp = await apiFetch('/api/keymap');
        if (!resp.ok) return;
        const data = await resp.json();
        _keymapActive = data.active || 'en';

        const sel = document.getElementById('keymapSelect');
        if (!sel) return;
        sel.innerHTML = '<option value="">-- select keymap --</option>';
        (data.available || []).forEach(id => {
            const opt = document.createElement('option');
            opt.value = id;
            opt.textContent = id + (id === _keymapActive ? ' (active)' : '');
            sel.appendChild(opt);
        });
        _updateActiveBadge();

        // Also update the settings tab selector if it exists
        const settingsSel = document.getElementById('keymapActive');
        if (settingsSel) settingsSel.textContent = _keymapActive;
    } catch(e) {
        _kmStatus('Load error: ' + e.message, false);
    }
}

function _updateActiveBadge() {
    const el = document.getElementById('keymapActiveBadge');
    if (el) el.textContent = 'Active keymap: ' + _keymapActive;
    const delBtn = document.getElementById('keymapDeleteBtn');
    if (delBtn) {
        const sel = document.getElementById('keymapSelect');
        delBtn.disabled = !sel?.value || sel.value === 'en';
    }
}

async function keymapSelected() {
    const sel = document.getElementById('keymapSelect');
    const id  = sel?.value;
    _updateActiveBadge();
    if (!id) { document.getElementById('keymapEditorArea').style.display = 'none'; return; }
    await _keymapLoad(id);
}

async function _keymapLoad(id) {
    try {
        const resp = await apiFetch('/api/keymap?id=' + encodeURIComponent(id));
        if (!resp.ok) throw new Error(resp.statusText);
        const data = await resp.json();
        _keymapId    = id;
        _keymapName  = data.name || id;
        _keymapRows  = (data.map || []).map(e => ({char: e.char||'', key: Number(e.key||0), mod: Number(e.mod||0)}));
        _keymapDirty = false;
        _renderTable();
        document.getElementById('keymapEditorTitle').textContent = _keymapName + ' (' + id + ')';
        document.getElementById('keymapEditorArea').style.display = '';
        _syncRawJson();
    } catch(e) {
        _kmStatus('Load error: ' + e.message, false);
    }
}

function _modLabel(mod) {
    if (!mod) return '—';
    const parts = [];
    if (mod & 2)  parts.push('Shift');
    if (mod & 4)  parts.push('Alt');
    if (mod & 8)  parts.push('GUI');
    if (mod & 64) parts.push('AltGr');
    const rem = mod & ~(2|4|8|64);
    if (rem) parts.push('0x'+rem.toString(16));
    return parts.join('+');
}

function _renderTable() {
    const tbody = document.getElementById('keymapTableBody');
    if (!tbody) return;
    tbody.innerHTML = '';
    const isBuiltin = _keymapId === 'en';
    _keymapRows.forEach((row, i) => {
        const tr = document.createElement('tr');
        tr.style.borderBottom = '1px solid #dee2e6';
        if (isBuiltin) {
            tr.innerHTML = `
                <td style="padding:4px 8px;border:1px solid #dee2e6;">${_esc(row.char)}</td>
                <td style="padding:4px 8px;border:1px solid #dee2e6;">${row.key}</td>
                <td style="padding:4px 8px;border:1px solid #dee2e6;">${row.mod}</td>
                <td style="padding:4px 8px;border:1px solid #dee2e6;color:#6c757d;">${_modLabel(row.mod)}</td>
                <td style="padding:4px 8px;border:1px solid #dee2e6;text-align:center;color:#aaa;">—</td>`;
        } else {
            tr.innerHTML = `
                <td style="padding:2px 4px;border:1px solid #dee2e6;">
                    <input type="text" maxlength="4" value="${_esc(row.char)}" style="width:44px;font-size:13px;" data-i="${i}" data-f="char" onchange="_rowChange(this)">
                </td>
                <td style="padding:2px 4px;border:1px solid #dee2e6;">
                    <input type="number" min="0" max="255" value="${row.key}" style="width:60px;font-size:13px;" data-i="${i}" data-f="key" onchange="_rowChange(this)">
                </td>
                <td style="padding:2px 4px;border:1px solid #dee2e6;">
                    <input type="number" min="0" max="255" value="${row.mod}" style="width:60px;font-size:13px;" data-i="${i}" data-f="mod" onchange="_rowChange(this)">
                </td>
                <td style="padding:2px 4px;border:1px solid #dee2e6;color:#6c757d;font-size:11px;" id="modlabel_${i}">${_modLabel(row.mod)}</td>
                <td style="padding:2px 4px;border:1px solid #dee2e6;text-align:center;">
                    <button onclick="_rowDelete(${i})" style="padding:1px 6px;background:#dc3545;color:#fff;border:none;border-radius:3px;cursor:pointer;font-size:11px;">✕</button>
                </td>`;
        }
        tbody.appendChild(tr);
    });
}

function _esc(s) {
    return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function _rowChange(input) {
    const i = Number(input.dataset.i);
    const f = input.dataset.f;
    if (f === 'char') {
        _keymapRows[i].char = input.value;
    } else if (f === 'key') {
        _keymapRows[i].key  = Number(input.value) || 0;
    } else if (f === 'mod') {
        _keymapRows[i].mod  = Number(input.value) || 0;
        const lbl = document.getElementById('modlabel_' + i);
        if (lbl) lbl.textContent = _modLabel(_keymapRows[i].mod);
    }
    _keymapDirty = true;
    _syncRawJson();
}

function _rowDelete(i) {
    _keymapRows.splice(i, 1);
    _keymapDirty = true;
    _renderTable();
    _syncRawJson();
}

function keymapAddRow() {
    _keymapRows.push({char: '', key: 0, mod: 0});
    _keymapDirty = true;
    _renderTable();
    _syncRawJson();
    // Scroll to bottom of table
    const t = document.getElementById('keymapTable');
    if (t) t.scrollIntoView({block:'end', behavior:'smooth'});
}

function _buildJson() {
    return JSON.stringify({id: _keymapId, name: _keymapName, map: _keymapRows}, null, 2);
}

function _syncRawJson() {
    const ta = document.getElementById('keymapRawJson');
    if (ta && document.getElementById('keymapRawArea')?.style.display !== 'none') {
        ta.value = _buildJson();
    }
}

function keymapToggleRaw() {
    const area = document.getElementById('keymapRawArea');
    if (!area) return;
    const showing = area.style.display !== 'none';
    area.style.display = showing ? 'none' : '';
    if (!showing) {
        document.getElementById('keymapRawJson').value = _buildJson();
    }
}

function keymapApplyRaw() {
    const ta = document.getElementById('keymapRawJson');
    if (!ta) return;
    try {
        const data = JSON.parse(ta.value);
        if (!Array.isArray(data.map)) throw new Error('Missing "map" array');
        _keymapName = data.name || _keymapId;
        _keymapRows = data.map.map(e => ({char: String(e.char||''), key: Number(e.key||0), mod: Number(e.mod||0)}));
        _keymapDirty = true;
        _renderTable();
        document.getElementById('keymapEditorTitle').textContent = _keymapName + ' (' + _keymapId + ')';
        _kmStatus('Raw JSON applied.', true);
    } catch(e) {
        _kmStatus('JSON parse error: ' + e.message, false);
    }
}

async function keymapSave() {
    if (!_keymapId || _keymapId === 'en') { _kmStatus('Cannot save built-in keymap.', false); return; }
    const json = _buildJson();
    try {
        const resp = await apiFetch('/api/keymap', {
            method: 'PUT',
            body: JSON.stringify({id: _keymapId, json})
        });
        if (!resp.ok) { const e = await resp.json(); throw new Error(e.error || resp.statusText); }
        _keymapDirty = false;
        _kmStatus('Saved: ' + _keymapId, true);
    } catch(e) {
        _kmStatus('Save error: ' + e.message, false);
    }
}

async function keymapSetActive() {
    const sel = document.getElementById('keymapSelect');
    const id  = sel?.value;
    if (!id) { _kmStatus('Select a keymap first.', false); return; }
    try {
        const resp = await apiFetch('/api/keymap', {
            method: 'POST',
            body: JSON.stringify({keymap: id})
        });
        if (!resp.ok) { const e = await resp.json(); throw new Error(e.error || resp.statusText); }
        _keymapActive = id;
        _kmStatus('Active keymap set to: ' + id, true);
        await keymapEditorInit();
    } catch(e) {
        _kmStatus('Error: ' + e.message, false);
    }
}

async function keymapDeleteSelected() {
    const sel = document.getElementById('keymapSelect');
    const id  = sel?.value;
    if (!id || id === 'en') { _kmStatus('Cannot delete built-in keymap.', false); return; }
    if (!confirm('Delete keymap "' + id + '"?')) return;
    try {
        const resp = await apiFetch('/api/keymap', {
            method: 'DELETE',
            body: JSON.stringify({keymap: id})
        });
        if (!resp.ok) { const e = await resp.json(); throw new Error(e.error || resp.statusText); }
        _keymapDirty = false;
        document.getElementById('keymapEditorArea').style.display = 'none';
        _keymapId = '';
        _kmStatus('Deleted: ' + id, true);
        await keymapEditorInit();
    } catch(e) {
        _kmStatus('Error: ' + e.message, false);
    }
}

async function keymapCreate() {
    const input = document.getElementById('keymapNewId');
    const id    = (input?.value || '').trim().toLowerCase().replace(/[^a-z0-9-]/g, '');
    if (!id || id === 'en') { _kmStatus('Enter a valid id (a-z, 0-9, -).', false); return; }
    // Create an empty keymap skeleton and upload it
    const json = JSON.stringify({id, name: id, map: []});
    try {
        const resp = await apiFetch('/api/keymap', {
            method: 'PUT',
            body: JSON.stringify({id, json})
        });
        if (!resp.ok) { const e = await resp.json(); throw new Error(e.error || resp.statusText); }
        if (input) input.value = '';
        _kmStatus('Created: ' + id, true);
        await keymapEditorInit();
        // Select and load the new keymap
        const sel = document.getElementById('keymapSelect');
        if (sel) { sel.value = id; await keymapSelected(); }
    } catch(e) {
        _kmStatus('Error: ' + e.message, false);
    }
}

async function keymapUploadFile() {
    const fi = document.getElementById('keymapFileInput');
    if (!fi?.files.length) return;
    const file = fi.files[0];
    const id   = file.name.replace(/\.json$/i, '').toLowerCase().replace(/[^a-z0-9-]/g, '');
    if (!id || id === 'en') { _kmStatus('Invalid filename — use e.g. de.json', false); fi.value=''; return; }
    try {
        const json = await file.text();
        const parsed = JSON.parse(json);  // validate
        const resp = await apiFetch('/api/keymap', {
            method: 'PUT',
            body: JSON.stringify({id, json})
        });
        if (!resp.ok) { const e = await resp.json(); throw new Error(e.error || resp.statusText); }
        fi.value = '';
        _kmStatus('Uploaded: ' + id, true);
        await keymapEditorInit();
        const sel = document.getElementById('keymapSelect');
        if (sel) { sel.value = id; await keymapSelected(); }
    } catch(e) {
        fi.value = '';
        _kmStatus('Upload error: ' + e.message, false);
    }
}

function keymapDownload() {
    if (!_keymapId) return;
    const blob = new Blob([_buildJson()], {type: 'application/json'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = _keymapId + '.json';
    a.click();
    URL.revokeObjectURL(a.href);
}

// Legacy aliases used by settings tab
async function loadKeymapSettings() { return keymapEditorInit(); }
async function setKeymap()          { return keymapSetActive(); }
async function uploadKeymap()       { return keymapUploadFile(); }
async function deleteKeymap()       { return keymapDeleteSelected(); }

// ============================================================
// File Browser
// ============================================================

let _fbPath     = '/';
let _fbEditPath = '';

async function fbRefresh() {
    if (!isConnected) return;
    const badge  = document.getElementById('fbSdBadge');
    const list   = document.getElementById('fbFileList');
    const pathEl = document.getElementById('fbPathLabel');
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/sd?path=${encodeURIComponent(_fbPath)}`);
        if (resp.status === 404) {
            if (badge) { badge.textContent = 'SD: reflash firmware'; badge.style.background = '#856404'; badge.style.color = '#fff'; }
            if (list)  list.innerHTML = '<p style="padding:10px;color:#856404;">SD API not available — reflash firmware with latest build.</p>';
            return;
        }
        if (resp.status === 503) {
            if (badge) { badge.textContent = 'SD: not inserted'; badge.style.background = '#6c757d'; badge.style.color = '#fff'; }
            if (list)  list.innerHTML = '<p style="padding:10px;color:#6c757d;">SD card not inserted or not readable.</p>';
            return;
        }
        if (!resp.ok) {
            if (badge) { badge.textContent = 'SD: error'; badge.style.background = '#dc3545'; badge.style.color = '#fff'; }
            if (list)  list.innerHTML = `<p style="padding:10px;color:#dc3545;">Error ${resp.status}</p>`;
            return;
        }
        // The directory listing returns a plain JSON array (not encrypted)
        const data = resp._data;
        const entries = Array.isArray(data) ? data : (await resp.json());
        if (badge) { badge.textContent = 'SD: available'; badge.style.background = '#198754'; badge.style.color = '#fff'; }
        if (pathEl) pathEl.textContent = _fbPath || '/';
        _fbRenderList(Array.isArray(entries) ? entries : []);
    } catch(e) {
        if (badge) { badge.textContent = 'SD: unknown'; badge.style.background = '#343a40'; badge.style.color = '#adb5bd'; }
        if (list)  list.innerHTML = '<p style="padding:10px;color:#dc3545;">Error: ' + e.message + '. Ensure device is connected and firmware is up to date.</p>';
    }
}

function _fbRenderList(entries) {
    const list = document.getElementById('fbFileList');
    if (!list) return;
    if (!entries || entries.length === 0) {
        list.innerHTML = '<p style="padding:10px;color:#6c757d;font-style:italic;">Empty directory.</p>';
        return;
    }
    const dirs  = entries.filter(e => e.type === 'dir');
    const files = entries.filter(e => e.type === 'file');
    const sorted = [...dirs, ...files];
    let html = '<table style="width:100%;border-collapse:collapse;font-size:13px;">';
    html += '<thead><tr style="background:#f8f9fa;border-bottom:2px solid #dee2e6;">';
    html += '<th style="padding:5px 8px;text-align:left;">Name</th>';
    html += '<th style="padding:5px 8px;text-align:right;width:70px;">Size</th>';
    html += '<th style="padding:5px 8px;width:80px;"></th>';
    html += '</tr></thead><tbody>';
    if (_fbPath && _fbPath !== '/') {
        html += `<tr style="border-bottom:1px solid #dee2e6;cursor:pointer;" onclick="fbNavUp()">
            <td style="padding:5px 8px;font-family:monospace;">📁 ..</td>
            <td></td><td></td></tr>`;
    }
    for (const e of sorted) {
        const icon = e.type === 'dir' ? '📁' : (e.name.endsWith('.kps') ? '📜' : '📄');
        const sizeStr = e.type === 'file' ? (e.size >= 1024 ? Math.round(e.size/1024)+'k' : e.size+'b') : '';
        const nameEnc = escapeHtml(e.name);
        const isKps = e.type === 'file' && e.name.toLowerCase().endsWith('.kps');
        html += `<tr style="border-bottom:1px solid #dee2e6;">
            <td style="padding:5px 8px;font-family:monospace;cursor:pointer;"
                onclick="${e.type === 'dir' ? `fbNavTo('${e.name.replace(/'/g,"\'")}')` : `fbOpenFile('${e.name.replace(/'/g,"\'")}')` }">${icon} ${nameEnc}</td>
            <td style="padding:5px 8px;text-align:right;color:#6c757d;font-size:11px;">${sizeStr}</td>
            <td style="padding:5px 8px;text-align:right;">
                ${isKps ? `<button onclick="fbRunKps('${e.name.replace(/'/g,"\'")}',event)" style="padding:2px 7px;font-size:11px;background:#6f42c1;color:#fff;border:none;border-radius:3px;cursor:pointer;margin-right:3px;">Run</button>` : ''}
                <button onclick="fbDeleteEntry('${e.name.replace(/'/g,"\'")}',event)" style="padding:2px 7px;font-size:11px;background:#dc3545;color:#fff;border:none;border-radius:3px;cursor:pointer;">Del</button>
            </td>
        </tr>`;
    }
    html += '</tbody></table>';
    list.innerHTML = html;
}

function fbNavTo(name) {
    _fbPath = (_fbPath === '/' ? '' : _fbPath) + '/' + name;
    fbRefresh();
    fbCloseEditor();
}

function fbNavUp() {
    const parts = _fbPath.split('/').filter(Boolean);
    parts.pop();
    _fbPath = parts.length ? '/' + parts.join('/') : '/';
    fbRefresh();
    fbCloseEditor();
}

async function fbOpenFile(name) {
    const path = (_fbPath === '/' ? '' : _fbPath) + '/' + name;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/sd?path=${encodeURIComponent(path)}&action=read`);
        const data = await resp.json();
        _fbEditPath = path;
        document.getElementById('fbEditorFilename').textContent = path;
        document.getElementById('fbEditorContent').value = data.content || '';
        document.getElementById('fbEditorPanel').style.display = '';
        const runBtn = document.getElementById('fbExecBtn');
        if (runBtn) runBtn.style.display = name.toLowerCase().endsWith('.kps') ? '' : 'none';
        document.getElementById('fbEditorStatus').textContent = '';
        document.getElementById('fbEditorContent').focus();
    } catch(e) {
        alert('Could not open file: ' + e.message);
    }
}

async function fbSaveFile() {
    const content = document.getElementById('fbEditorContent').value;
    const status  = document.getElementById('fbEditorStatus');
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/sd`, {
            method: 'POST',
            body: JSON.stringify({ action: 'write', path: _fbEditPath, content })
        });
        const data = await resp.json();
        if (resp.ok) {
            status.textContent = '✓ Saved'; status.style.color = '#198754';
            fbRefresh();
        } else {
            status.textContent = '✗ ' + (data.error || 'Save failed'); status.style.color = '#dc3545';
        }
    } catch(e) { status.textContent = '✗ ' + e.message; status.style.color = '#dc3545'; }
}

async function fbDeleteFile() {
    if (!_fbEditPath || !confirm('Delete ' + _fbEditPath + '?')) return;
    try {
        await apiFetch(`${getApiEndpoint()}/api/sd`, {
            method: 'POST', body: JSON.stringify({ action: 'delete', path: _fbEditPath })
        });
        fbCloseEditor();
        fbRefresh();
    } catch(e) { alert('Delete failed: ' + e.message); }
}

async function fbDeleteEntry(name, evt) {
    if (evt) evt.stopPropagation();
    const path = (_fbPath === '/' ? '' : _fbPath) + '/' + name;
    if (!confirm('Delete ' + path + '?')) return;
    try {
        await apiFetch(`${getApiEndpoint()}/api/sd`, {
            method: 'POST', body: JSON.stringify({ action: 'delete', path })
        });
        fbRefresh();
    } catch(e) { alert('Delete failed: ' + e.message); }
}

async function fbExecFile() {
    if (!_fbEditPath) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/sd`, {
            method: 'POST', body: JSON.stringify({ action: 'exec', path: _fbEditPath })
        });
        const st = document.getElementById('fbEditorStatus');
        if (resp.ok) { st.textContent = '✓ Executed'; st.style.color = '#198754'; }
        else { const d = await resp.json(); st.textContent = '✗ ' + (d.error||'Failed'); st.style.color = '#dc3545'; }
    } catch(e) { alert('Execute failed: ' + e.message); }
}

async function fbRunKps(name, evt) {
    if (evt) evt.stopPropagation();
    const path = (_fbPath === '/' ? '' : _fbPath) + '/' + name;
    if (!confirm('Run ' + name + '?')) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/sd`, {
            method: 'POST', body: JSON.stringify({ action: 'exec', path })
        });
        if (!resp.ok) { const d = await resp.json(); alert('Run failed: ' + (d.error||'unknown')); }
    } catch(e) { alert('Run failed: ' + e.message); }
}

async function fbMkdir() {
    const name = prompt('New folder name:');
    if (!name) return;
    const path = (_fbPath === '/' ? '' : _fbPath) + '/' + name;
    try {
        await apiFetch(`${getApiEndpoint()}/api/sd`, {
            method: 'POST', body: JSON.stringify({ action: 'mkdir', path })
        });
        fbRefresh();
    } catch(e) { alert('mkdir failed: ' + e.message); }
}

async function fbNewFile() {
    const name = prompt('File name (e.g. script.kps):');
    if (!name) return;
    const path = (_fbPath === '/' ? '' : _fbPath) + '/' + name;
    await apiFetch(`${getApiEndpoint()}/api/sd`, {
        method: 'POST', body: JSON.stringify({ action: 'write', path, content: '' })
    });
    fbRefresh();
    fbOpenFile(name);
}

function fbCloseEditor() {
    _fbEditPath = '';
    const panel = document.getElementById('fbEditorPanel');
    if (panel) panel.style.display = 'none';
}

// ============================================================
// KPS Script Editor
// ============================================================

let _kpsScripts = [];

async function kpsLoadScriptList() {
    if (!isConnected) return;
    const sel = document.getElementById('kpsScriptSelect');
    if (!sel) return;
    try {
        const entries = await _kpsScanDir('/scripts', []);
        _kpsScripts = entries;
        const current = sel.value;
        sel.innerHTML = '<option value="">-- New script --</option>';
        for (const p of entries) sel.innerHTML += `<option value="${escapeHtml(p)}">${escapeHtml(p)}</option>`;
        if (current && entries.includes(current)) sel.value = current;
    } catch(e) { /* silent — SD may be absent */ }
}

async function _kpsScanDir(dir, results) {
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/sd?path=${encodeURIComponent(dir)}`);
        if (!resp.ok) return results;
        const entries = await resp.json();
        for (const e of entries) {
            const full = (dir === '/' ? '' : dir) + '/' + e.name;
            if (e.type === 'dir') { await _kpsScanDir(full, results); }
            else if (e.name.toLowerCase().endsWith('.kps')) results.push(full);
        }
    } catch(e) { /* silent */ }
    return results;
}

async function kpsSelectScript() {
    const sel = document.getElementById('kpsScriptSelect');
    if (!sel || !sel.value) return;
    const path = sel.value;
    document.getElementById('kpsFilePath').value = path;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/sd?path=${encodeURIComponent(path)}&action=read`);
        const data = await resp.json();
        document.getElementById('kpsEditorContent').value = data.content || '';
        _statusMsg2('kpsStatus', '', true);
    } catch(e) { _statusMsg2('kpsStatus', '✗ Load failed: ' + e.message, false); }
}

function kpsNew() {
    document.getElementById('kpsScriptSelect').value = '';
    document.getElementById('kpsFilePath').value = '/scripts/';
    document.getElementById('kpsEditorContent').value = '# New KProx Script\n\n';
    document.getElementById('kpsFilePath').focus();
}

async function kpsSave() {
    const path    = document.getElementById('kpsFilePath')?.value.trim();
    const content = document.getElementById('kpsEditorContent')?.value;
    if (!path) { _statusMsg2('kpsStatus', '✗ File path required', false); return; }
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/sd`, {
            method: 'POST', body: JSON.stringify({ action: 'write', path, content })
        });
        const data = await resp.json();
        if (resp.ok) {
            _statusMsg2('kpsStatus', '✓ Saved to ' + path, true);
            await kpsLoadScriptList();
            document.getElementById('kpsScriptSelect').value = path;
        } else {
            _statusMsg2('kpsStatus', '✗ ' + (data.error || 'Save failed'), false);
        }
    } catch(e) { _statusMsg2('kpsStatus', '✗ ' + e.message, false); }
}

async function kpsDelete() {
    const path = document.getElementById('kpsFilePath')?.value.trim();
    if (!path || !confirm('Delete ' + path + '?')) return;
    try {
        await apiFetch(`${getApiEndpoint()}/api/sd`, {
            method: 'POST', body: JSON.stringify({ action: 'delete', path })
        });
        _statusMsg2('kpsStatus', '✓ Deleted', true);
        kpsNew();
        await kpsLoadScriptList();
    } catch(e) { _statusMsg2('kpsStatus', '✗ ' + e.message, false); }
}

async function kpsRun() {
    const path    = document.getElementById('kpsFilePath')?.value.trim();
    const content = document.getElementById('kpsEditorContent')?.value;
    if (!path) { _statusMsg2('kpsStatus', '✗ Save the script first', false); return; }
    // Auto-save then exec
    try {
        await apiFetch(`${getApiEndpoint()}/api/sd`, {
            method: 'POST', body: JSON.stringify({ action: 'write', path, content })
        });
        const resp = await apiFetch(`${getApiEndpoint()}/api/sd`, {
            method: 'POST', body: JSON.stringify({ action: 'exec', path })
        });
        const data = await resp.json();
        if (resp.ok) _statusMsg2('kpsStatus', '✓ Running...', true);
        else         _statusMsg2('kpsStatus', '✗ ' + (data.error || 'Run failed'), false);
    } catch(e) { _statusMsg2('kpsStatus', '✗ ' + e.message, false); }
}

function kpsHandleTab(e) {
    if (e.key !== 'Tab') return;
    e.preventDefault();
    const ta = e.target;
    const s = ta.selectionStart, end = ta.selectionEnd;
    ta.value = ta.value.substring(0, s) + '    ' + ta.value.substring(end);
    ta.selectionStart = ta.selectionEnd = s + 4;
}

function _statusMsg2(id, msg, ok) {
    const el = document.getElementById(id);
    if (!el) return;
    el.textContent = msg;
    el.style.color = ok ? '#198754' : '#dc3545';
}

// ============================================================
// KPS Reference
// ============================================================

let _kpsrefMd  = null;

async function kpsrefLoad(force) {
    if (_kpsrefMd && !force) { _kpsrefRender(); return; }
    const el = document.getElementById('kpsrefContent');
    if (!el) return;
    el.innerHTML = '<p style="color:#6c757d;font-size:13px;">Loading KPS reference...</p>';
    try {
        const endpoint = getApiEndpoint ? getApiEndpoint() : '';
        const resp = await fetch(endpoint + '/api/kpsref');

        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        _kpsrefMd = await resp.text();
        _kpsrefRender();
    } catch(e) {
        el.innerHTML = '<p style="color:#dc3545;font-size:13px;">Could not load reference: ' + e.message + '. Connect to device first.</p>';
    }
}

function _kpsrefRender(filter) {
    const el = document.getElementById('kpsrefContent');
    if (!el || !_kpsrefMd) return;
    const html = _mdToHtml ? _mdToHtml(_kpsrefMd) : '<pre>' + escapeHtml(_kpsrefMd) + '</pre>';
    if (!filter) {
        el.innerHTML = html;
    } else {
        const q = filter.toLowerCase();
        const secs = _kpsrefMd.split(/(?=^## )/m);
        const matched = secs.filter(s => s.toLowerCase().includes(q));
        el.innerHTML = matched.length
            ? (_mdToHtml ? matched.map(_mdToHtml).join('') : '<pre>' + escapeHtml(matched.join('')) + '</pre>')
            : '<p style="color:#6c757d;font-style:italic;">No matches.</p>';
    }
}

function kpsrefDoSearch() {
    const q = document.getElementById('kpsrefSearch')?.value.trim();
    _kpsrefRender(q || undefined);
}

// ============================================================
// BootProx
// ============================================================

async function loadBootReg() {
    if (!isConnected) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/settings`, { method: 'GET' });
        if (!resp.ok) return;
        const d = await resp.json();
        if (d.bootReg) _applyBootReg(d.bootReg);
        // Populate register select
        const sel = document.getElementById('bootRegSelect');
        if (sel && d.appNames) { /* names not registers */ }
        await _populateBootRegSelect(d.bootReg?.index ?? 0);
    } catch(e) { logDebug('loadBootReg: ' + e.message, 'error'); }
}

async function _populateBootRegSelect(currentIdx) {
    const sel = document.getElementById('bootRegSelect');
    if (!sel) return;
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/registers`, { method: 'GET' });
        if (!resp.ok) return;
        const d = await resp.json();
        const regs = d.registers || [];
        sel.innerHTML = regs.map((r, i) => {
            const label = r.name ? `[${i+1}] ${escapeHtml(r.name)}` : `[${i+1}]`;
            return `<option value="${i}">${label}</option>`;
        }).join('');
        sel.value = String(currentIdx);
    } catch(e) { /* silent */ }
}

function _applyBootReg(br) {
    const enabledEl = document.getElementById('bootRegEnabled');
    const limitEl   = document.getElementById('bootRegLimit');
    const badge     = document.getElementById('bootRegStatusBadge');
    const fired     = document.getElementById('bootRegFiredBadge');
    if (enabledEl) enabledEl.checked   = !!br.enabled;
    if (limitEl)   limitEl.value       = br.limit ?? 0;
    if (badge) {
        badge.textContent = br.enabled ? 'ENABLED' : 'OFF';
        badge.style.background = br.enabled ? '#198754' : '#343a40';
        badge.style.color = '#fff';
    }
    const limit = br.limit ?? 0;
    const fired2 = br.firedCount ?? 0;
    if (fired) {
        fired.textContent = limit > 0
            ? `Fired: ${fired2} / ${limit}`
            : `Fired: ${fired2}`;
        fired.style.background = (limit > 0 && fired2 >= limit) ? '#dc3545' : '#343a40';
        fired.style.color = '#fff';
    }
}

async function saveBootReg() {
    if (!isConnected) return;
    const enabled = document.getElementById('bootRegEnabled')?.checked ?? false;
    const index   = parseInt(document.getElementById('bootRegSelect')?.value ?? '0');
    const limit   = parseInt(document.getElementById('bootRegLimit')?.value ?? '0');
    try {
        const resp = await apiFetch(`${getApiEndpoint()}/api/settings`, {
            method: 'POST',
            body: JSON.stringify({ bootReg: { enabled, index, limit } })
        });
        if (resp.ok) {
            _statusMsg('bootRegStatus', '\u2713 Saved.', true);
            await loadBootReg();
        } else {
            const d = await resp.json();
            _statusMsg('bootRegStatus', '\u2717 ' + (d.error || 'Save failed'), false);
        }
    } catch(e) { _statusMsg('bootRegStatus', '\u2717 ' + e.message, false); }
}

async function bootRegResetCount() {
    if (!isConnected) return;
    try {
        const enabled = document.getElementById('bootRegEnabled')?.checked ?? false;
        const index   = parseInt(document.getElementById('bootRegSelect')?.value ?? '0');
        const limit   = parseInt(document.getElementById('bootRegLimit')?.value ?? '0');
        const resp = await apiFetch(`${getApiEndpoint()}/api/settings`, {
            method: 'POST',
            body: JSON.stringify({ bootReg: { enabled: limit > 0 ? true : enabled, index, limit, firedCount: 0 } })
        });
        if (resp.ok) { _statusMsg('bootRegStatus', '\u2713 Count reset.', true); await loadBootReg(); }
    } catch(e) { _statusMsg('bootRegStatus', '\u2717 ' + e.message, false); }
}

// ============================================================
// Memory Statistics Panel
// ============================================================

function _memBar(used, total, color) {
    if (!total) return '';
    const pct = Math.min(100, Math.round(used / total * 100));
    const barColor = pct > 85 ? '#dc3545' : pct > 65 ? '#ffc107' : color;
    return `<div style="margin:2px 0;">
        <div style="display:flex;justify-content:space-between;margin-bottom:1px;">
            <span style="color:#adb5bd;">${_fmt(total - used)} free / ${_fmt(total)}</span>
            <span style="color:${barColor};">${pct}%</span>
        </div>
        <div style="background:#343a40;border-radius:2px;height:5px;overflow:hidden;">
            <div style="width:${pct}%;height:100%;background:${barColor};border-radius:2px;transition:width 0.4s;"></div>
        </div>
    </div>`;
}

function _fmt(bytes) {
    if (bytes === undefined || bytes === null) return '-';
    if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + ' MB';
    if (bytes >= 1024)    return (bytes / 1024).toFixed(1) + ' KB';
    return bytes + ' B';
}

function updateMemStatsUI(d) {
    const panel = document.getElementById('memStatsPanel');
    if (!panel) return;

    let html = '';

    // Heap
    if (d.total_heap) {
        const used = d.total_heap - d.free_heap;
        html += `<div style="color:#adb5bd;margin-top:3px;font-weight:600;">Heap</div>`;
        html += _memBar(used, d.total_heap, '#0d6efd');
        html += `<div style="display:flex;justify-content:space-between;color:#6c757d;margin-top:1px;">
            <span>Low watermark: ${_fmt(d.min_free_heap)}</span>
            <span>Max block: ${_fmt(d.max_alloc_heap)}</span>
        </div>`;
    }

    // PSRAM
    if (d.psram_found && d.psram_size > 0) {
        const used = d.psram_size - d.psram_free;
        html += `<div style="color:#adb5bd;margin-top:5px;font-weight:600;">PSRAM</div>`;
        html += _memBar(used, d.psram_size, '#6f42c1');
        html += `<div style="color:#6c757d;margin-top:1px;">Low watermark: ${_fmt(d.psram_min_free)}</div>`;
    } else if (d.psram_found === false) {
        html += `<div style="color:#6c757d;margin-top:3px;">PSRAM: not present</div>`;
    }

    // Flash (sketch + free sketch space)
    if (d.sketch_size !== undefined) {
        const sketchTotal = (d.sketch_size || 0) + (d.free_sketch_space || 0);
        html += `<div style="color:#adb5bd;margin-top:5px;font-weight:600;">Flash (OTA partition)</div>`;
        html += _memBar(d.sketch_size, sketchTotal, '#198754');
        html += `<div style="color:#6c757d;margin-top:1px;">Sketch: ${_fmt(d.sketch_size)} &nbsp; Free: ${_fmt(d.free_sketch_space)}</div>`;
    }

    // SPIFFS
    if (d.spiffs_total) {
        html += `<div style="color:#adb5bd;margin-top:5px;font-weight:600;">SPIFFS</div>`;
        html += _memBar(d.spiffs_used, d.spiffs_total, '#fd7e14');
        html += `<div style="color:#6c757d;margin-top:1px;">Used: ${_fmt(d.spiffs_used)} / ${_fmt(d.spiffs_total)}</div>`;
    }

    // SD card
    if (d.sd_available && d.sd_total) {
        html += `<div style="color:#adb5bd;margin-top:5px;font-weight:600;">SD Card</div>`;
        html += _memBar(d.sd_used, d.sd_total, '#20c997');
        html += `<div style="color:#6c757d;margin-top:1px;">Used: ${_fmt(d.sd_used)} / ${_fmt(d.sd_total)}</div>`;
    } else if (d.sd_available === false) {
        html += `<div style="color:#6c757d;margin-top:3px;">SD Card: not inserted</div>`;
    }

    panel.innerHTML = html || '<div style="color:#6c757d;font-style:italic;">No data</div>';
}
