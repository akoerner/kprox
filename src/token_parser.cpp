#include "token_parser.h"
#include "hid.h"
#include "registers.h"
#include "kps_parser.h"
#include "sd_utils.h"
#include "connection.h"
#include "keymap.h"
#include "storage.h"
#include "credential_store.h"
#include "totp.h"
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#ifdef BOARD_M5STACK_CARDPUTER
#include <M5Cardputer.h>
#endif

// ---- Interrupt check ----
// Called at every delay/yield point inside the parser. Feeds the watchdog and
// checks whether BtnA or ESC/backtick has been pressed; if so, halts execution.
void checkParseInterrupt() {
    feedWatchdog();
#ifdef BOARD_M5STACK_CARDPUTER
    M5Cardputer.update();
    if (M5Cardputer.BtnA.wasPressed()) {
        g_parserAbort         = true;
        g_btnAHaltedPlayback  = true;
        haltAllOperations();
        return;
    }
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        auto ks = M5Cardputer.Keyboard.keysState();
        if (ks.tab) { g_parserAbort = true; haltAllOperations(); return; }
        for (char c : ks.word) {
            if (c == 0x1B || c == '`') { g_parserAbort = true; haltAllOperations(); return; }
        }
        for (uint8_t hk : ks.hid_keys) {
            if (hk == 0x29) { g_parserAbort = true; haltAllOperations(); return; }
        }
    }
#endif
}

static String processEscapeSequences(const String& input) {
    String result = input;
    result.replace("\\n",  "\n");
    result.replace("\\t",  "\t");
    result.replace("\\r",  "\r");
    result.replace("\\b",  "\b");
    result.replace("\\f",  "\f");
    result.replace("\\v",  "\v");
    result.replace("\\\\", "\\");
    result.replace("\\{",  "{");
    result.replace("\\}",  "}");
    return result;
}

static int findMatchingBrace(const String& text, int startPos) {
    int braceCount = 1;
    int pos = startPos + 1;
    while (pos < (int)text.length() && braceCount > 0) {
        if      (text[pos] == '{') braceCount++;
        else if (text[pos] == '}') braceCount--;
        if (braceCount == 0) return pos;
        pos++;
    }
    return -1;
}

static int findMatchingEndLoop(const String& text, int startPos) {
    int loopCount = 1;
    int pos = startPos;
    while (pos < (int)text.length() && loopCount > 0) {
        if (text.substring(pos).startsWith("{LOOP")) {
            int closePos = findMatchingBrace(text, pos);
            if (closePos != -1) { loopCount++; pos = closePos + 1; }
            else pos++;
        } else if (text.substring(pos).startsWith("{ENDLOOP}")) {
            loopCount--;
            if (loopCount == 0) return pos;
            pos += 9;
        } else {
            pos++;
        }
    }
    return -1;
}

static int findMatchingEndFor(const String& text, int startPos) {
    int depth = 1;
    int pos = startPos;
    while (pos < (int)text.length() && depth > 0) {
        if (text.substring(pos).startsWith("{FOR ")) {
            int closePos = findMatchingBrace(text, pos);
            if (closePos != -1) { depth++; pos = closePos + 1; }
            else pos++;
        } else if (text.substring(pos).startsWith("{ENDFOR}")) {
            depth--;
            if (depth == 0) return pos;
            pos += 8;
        } else {
            pos++;
        }
    }
    return -1;
}

static int findMatchingEndWhile(const String& text, int startPos) {
    int depth = 1;
    int pos = startPos;
    while (pos < (int)text.length() && depth > 0) {
        if (text.substring(pos).startsWith("{WHILE ")) {
            int closePos = findMatchingBrace(text, pos);
            if (closePos != -1) { depth++; pos = closePos + 1; }
            else pos++;
        } else if (text.substring(pos).startsWith("{ENDWHILE}")) {
            depth--;
            if (depth == 0) return pos;
            pos += 10;
        } else {
            pos++;
        }
    }
    return -1;
}

static int findMatchingEndIf(const String& text, int startPos) {
    int depth = 1;
    int pos = startPos;
    while (pos < (int)text.length() && depth > 0) {
        if (text.substring(pos).startsWith("{IF ")) {
            int closePos = findMatchingBrace(text, pos);
            if (closePos != -1) { depth++; pos = closePos + 1; }
            else pos++;
        } else if (text.substring(pos).startsWith("{ENDIF}")) {
            depth--;
            if (depth == 0) return pos;
            pos += 7;
        } else {
            pos++;
        }
    }
    return -1;
}

static int findMatchingElse(const String& text, int startPos, int endIfPos) {
    int depth = 0;
    int pos = startPos;
    while (pos < endIfPos) {
        if (text.substring(pos).startsWith("{IF ")) {
            int closePos = findMatchingBrace(text, pos);
            if (closePos != -1) { depth++; pos = closePos + 1; }
            else pos++;
        } else if (text.substring(pos).startsWith("{ENDIF}")) {
            depth--; pos += 7;
        } else if (text.substring(pos).startsWith("{ELSE}") && depth == 0) {
            return pos;
        } else {
            pos++;
        }
    }
    return -1;
}

static int findInnermostToken(const String& input, int& tokenStart, int& tokenEnd) {
    int maxDepth  = -1;
    int bestStart = -1;
    int bestEnd   = -1;

    for (int i = 0; i < (int)input.length(); i++) {
        if (input[i] != '{') continue;

        int depth = 0;
        for (int j = 0; j <= i; j++) {
            if (input[j] == '{') depth++;
            if (input[j] == '}') depth--;
        }

        int end = findMatchingBrace(input, i);
        if (end == -1 || depth <= maxDepth) continue;

        bool hasNested = false;
        for (int k = i + 1; k < end && !hasNested; k++) {
            if (input[k] == '{') hasNested = true;
        }

        if (!hasNested) {
            maxDepth  = depth;
            bestStart = i;
            bestEnd   = end;
        }
    }

    tokenStart = bestStart;
    tokenEnd   = bestEnd;
    return bestStart;
}

static double evaluateMathExpression(const String& expression, std::map<String, String>& vars) {
    String expr = expression;
    expr.trim();

    if (expr == "PI") return 3.14159265358979;
    if (expr == "E")  return 2.71828182845905;

    auto tryUnary = [&](const char* prefix, int pLen, double(*fn)(double)) -> double* {
        static double val;
        if (expr.startsWith(prefix) && expr.endsWith(")")) {
            val = fn(evaluateMathExpression(expr.substring(pLen, expr.length() - 1), vars));
            return &val;
        }
        return nullptr;
    };

    if (expr.startsWith("cos(")   && expr.endsWith(")")) return cos(evaluateMathExpression(expr.substring(4, expr.length()-1), vars));
    if (expr.startsWith("sin(")   && expr.endsWith(")")) return sin(evaluateMathExpression(expr.substring(4, expr.length()-1), vars));
    if (expr.startsWith("tan(")   && expr.endsWith(")")) return tan(evaluateMathExpression(expr.substring(4, expr.length()-1), vars));
    if (expr.startsWith("sqrt(")  && expr.endsWith(")")) return sqrt(evaluateMathExpression(expr.substring(5, expr.length()-1), vars));
    if (expr.startsWith("abs(")   && expr.endsWith(")")) return fabs(evaluateMathExpression(expr.substring(4, expr.length()-1), vars));
    if (expr.startsWith("floor(") && expr.endsWith(")")) return floor(evaluateMathExpression(expr.substring(6, expr.length()-1), vars));
    if (expr.startsWith("ceil(")  && expr.endsWith(")")) return ceil(evaluateMathExpression(expr.substring(5, expr.length()-1), vars));
    if (expr.startsWith("round(") && expr.endsWith(")")) return round(evaluateMathExpression(expr.substring(6, expr.length()-1), vars));

    if (vars.count(expr)) return vars[expr].toDouble();

    int opPos = -1;
    char op = '\0';
    int parenDepth = 0;

    for (int i = expr.length() - 1; i >= 0; i--) {
        if      (expr[i] == ')') parenDepth++;
        else if (expr[i] == '(') parenDepth--;
        if (parenDepth != 0) continue;
        if ((expr[i] == '+' || expr[i] == '-') && i > 0) { opPos = i; op = expr[i]; break; }
    }

    if (opPos == -1) {
        parenDepth = 0;
        for (int i = expr.length() - 1; i >= 0; i--) {
            if      (expr[i] == ')') parenDepth++;
            else if (expr[i] == '(') parenDepth--;
            if (parenDepth != 0) continue;
            if (expr[i] == '*' || expr[i] == '/' || expr[i] == '%') { opPos = i; op = expr[i]; break; }
        }
    }

    if (opPos == -1) return expr.toDouble();

    double lv = evaluateMathExpression(expr.substring(0, opPos), vars);
    double rv = evaluateMathExpression(expr.substring(opPos + 1), vars);

    switch (op) {
        case '+': return lv + rv;
        case '-': return lv - rv;
        case '*': return lv * rv;
        case '/': return rv != 0 ? lv / rv : 0;
        case '%': return rv != 0 ? fmod(lv, rv) : 0;
        default:  return 0;
    }
}

String evaluateAllTokens(const String& text, std::map<String, String>& vars);

static bool evaluateCondition(const String& condition, std::map<String, String>& vars) {
    int opStart = -1, opLen = 0, depth = 0;

    for (int i = 0; i < (int)condition.length() && opStart == -1; i++) {
        char c = condition[i];
        if      (c == '{') { depth++; continue; }
        else if (c == '}') { depth--; continue; }
        if (depth != 0) continue;

        if (i + 1 < (int)condition.length()) {
            String two = condition.substring(i, i + 2);
            if (two == "==" || two == "!=" || two == "<=" || two == ">=") {
                opStart = i; opLen = 2; break;
            }
        }
        if (c == '<' || c == '>') { opStart = i; opLen = 1; }
    }

    if (opStart == -1) return false;

    String left  = condition.substring(0, opStart);
    String op    = condition.substring(opStart, opStart + opLen);
    String right = condition.substring(opStart + opLen);
    left.trim(); right.trim();

    String lv = evaluateAllTokens(left,  vars);
    String rv = evaluateAllTokens(right, vars);

    bool lNum = lv.length() > 0, rNum = rv.length() > 0;
    for (int i = 0; i < (int)lv.length() && lNum; i++) { char c = lv[i]; if (!isdigit(c) && c != '-' && c != '.') lNum = false; }
    for (int i = 0; i < (int)rv.length() && rNum; i++) { char c = rv[i]; if (!isdigit(c) && c != '-' && c != '.') rNum = false; }

    if (lNum && rNum) {
        double l = lv.toDouble(), r = rv.toDouble();
        if (op == "==") return l == r;
        if (op == "!=") return l != r;
        if (op == "<")  return l <  r;
        if (op == ">")  return l >  r;
        if (op == "<=") return l <= r;
        if (op == ">=") return l >= r;
    } else {
        if (op == "==") return lv == rv;
        if (op == "!=") return lv != rv;
        if (op == "<")  return lv <  rv;
        if (op == ">")  return lv >  rv;
        if (op == "<=") return lv <= rv;
        if (op == ">=") return lv >= rv;
    }
    return false;
}

String evaluateAllTokens(const String& text, std::map<String, String>& vars) {
    String input = text;
    bool changed = true;
    int iterations = 0;

    while (changed && iterations < 100) {
        changed = false;
        iterations++;

        int tokenStart, tokenEnd;
        if (findInnermostToken(input, tokenStart, tokenEnd) == -1) break;

        String token = input.substring(tokenStart + 1, tokenEnd);
        token.trim();
        String upperToken = token;
        upperToken.toUpperCase();

        String replacement;
        bool resolved = false;

        if (vars.count(token)) {
            replacement = vars[token];
            resolved    = true;
        } else if (upperToken.startsWith("MATH ")) {
            double result = evaluateMathExpression(token.substring(5), vars);
            replacement   = (result == (int)result) ? String((int)result) : String(result, 2);
            resolved      = true;
        } else if (upperToken == "RAND" || upperToken.startsWith("RAND ")) {
            // Seed a CTR-DRBG from the hardware entropy source (esp_random)
            mbedtls_ctr_drbg_context ctr;
            mbedtls_entropy_context   ent;
            mbedtls_entropy_init(&ent);
            mbedtls_ctr_drbg_init(&ctr);
            mbedtls_ctr_drbg_seed(&ctr, mbedtls_entropy_func, &ent, nullptr, 0);

            if (upperToken == "RAND") {
                // {RAND} with no args — return raw 32-bit unsigned integer
                uint32_t raw = 0;
                mbedtls_ctr_drbg_random(&ctr, (uint8_t*)&raw, sizeof(raw));
                replacement = String(raw);
            } else {
                // {RAND min max} — cryptographically random integer in [min, max]
                int argStart = 5;
                int space = token.indexOf(' ', argStart);
                if (space != -1) {
                    long a = token.substring(argStart, space).toInt();
                    long b = token.substring(space + 1).toInt();
                    if (b >= a) {
                        uint32_t raw = 0;
                        mbedtls_ctr_drbg_random(&ctr, (uint8_t*)&raw, sizeof(raw));
                        replacement = String((long)(a + (raw % (uint32_t)(b - a + 1))));
                    }
                }
            }

            mbedtls_ctr_drbg_free(&ctr);
            mbedtls_entropy_free(&ent);
            resolved = true;
        } else if (upperToken.startsWith("RAW ") || upperToken.startsWith("ASCII ")) {
            int offset = upperToken.startsWith("RAW ") ? 4 : 6;
            String arg = token.substring(offset);
            arg.trim();
            int value = (arg.startsWith("0X") || arg.startsWith("0x"))
                        ? strtol(arg.c_str(), nullptr, 16)
                        : arg.toInt();
            if (value >= 0 && value <= 255) {
                replacement = String((char)value);
                resolved    = true;
            }
        } else if (upperToken.startsWith("CREDSTORE ")) {
            String rest = token.substring(10);
            rest.trim();
            CredField field = CredField::PASSWORD;
            String label    = rest;
            // Optional first word selects field: password/username/notes
            int sp = rest.indexOf(' ');
            if (sp > 0) {
                String first = rest.substring(0, sp);
                first.toLowerCase();
                if (first == "username") { field = CredField::USERNAME; label = rest.substring(sp + 1); label.trim(); }
                else if (first == "notes") { field = CredField::NOTES;    label = rest.substring(sp + 1); label.trim(); }
                else if (first == "password") { field = CredField::PASSWORD; label = rest.substring(sp + 1); label.trim(); }
                // else treat the whole rest as the label, default field = PASSWORD
            }
            replacement = credStoreLocked ? "" : credStoreGet(label, field);
            resolved    = true;
        } else if (upperToken == "KPROX_IP") {
#ifdef BOARD_M5STACK_CARDPUTER
            replacement = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "";
#else
            replacement = WiFi.localIP().toString();
#endif
            resolved    = true;
        } else if (upperToken.startsWith("TOTP ")) {
            String label = token.substring(5);
            label.trim();
            int32_t code = totpGetCode(label);
            if (code >= 0) {
                char buf[8]; snprintf(buf, sizeof(buf), "%06" PRId32, code);
                replacement = String(buf);
            }
            resolved = true;
        } else if (upperToken.startsWith("SD_READ ")) {
            String path = token.substring(8);
            path.trim();
            if (path.startsWith(""") && path.endsWith("""))
                path = path.substring(1, path.length() - 1);
            replacement = sdReadFile(path);
            resolved    = true;
        } else if (upperToken.startsWith("EXEC ")) {
            String arg = evaluateAllTokens(token.substring(5), vars);
            arg.trim();
            int idx = resolveRegisterArg(arg);
            if (idx >= 0 && !registers[idx].isEmpty())
                parseAndSendText(registers[idx], vars);
            resolved = true;

        // ---- Sink ----
        } else if (upperToken == "SINK") {
            if (SPIFFS.exists("/sink.txt")) {
                File f = SPIFFS.open("/sink.txt", "r");
                if (f) { replacement = f.readString(); f.close(); }
            }
            resolved = true;
        } else if (upperToken == "SINK_SIZE") {
            size_t sz = 0;
            if (SPIFFS.exists("/sink.txt")) {
                File f = SPIFFS.open("/sink.txt", "r");
                if (f) { sz = f.size(); f.close(); }
            }
            replacement = String(sz);
            resolved = true;

        // ---- Time / date ----
        } else if (upperToken == "TIMESTAMP") {
            replacement = String((long)time(nullptr));
            resolved = true;
        } else if (upperToken.startsWith("DATE")) {
            String fmt = token.length() > 4 ? token.substring(4) : String("");
            fmt.trim();
            // Strip optional leading + and quotes
            if (!fmt.isEmpty() && fmt[0] == '+') fmt = fmt.substring(1);
            if (fmt.length() >= 2 && fmt[0] == '"' && fmt[fmt.length()-1] == '"')
                fmt = fmt.substring(1, fmt.length()-1);
            if (fmt.isEmpty()) fmt = "%Y-%m-%d";
            time_t now = time(nullptr);
            struct tm* t = localtime(&now);
            char buf[64];
            strftime(buf, sizeof(buf), fmt.c_str(), t);
            replacement = String(buf);
            resolved = true;

        // ---- Device diagnostics ----
        } else if (upperToken == "FREE_HEAP") {
            replacement = String(ESP.getFreeHeap());
            resolved = true;
        } else if (upperToken == "UPTIME") {
            replacement = String(millis() / 1000UL);
            resolved = true;
        } else if (upperToken == "WIFI_RSSI") {
            replacement = (WiFi.status() == WL_CONNECTED) ? String(WiFi.RSSI()) : String("");
            resolved = true;
        } else if (upperToken == "WIFI_SSID") {
            replacement = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : String("");
            resolved = true;
#ifdef BOARD_M5STACK_CARDPUTER
        } else if (upperToken == "BATTERY") {
            replacement = String((int)constrain(M5Cardputer.Power.getBatteryLevel(), 0, 100));
            resolved = true;
#endif
        }

        if (resolved) {
            input   = input.substring(0, tokenStart) + replacement + input.substring(tokenEnd + 1);
            changed = true;
        }
    }

    return processEscapeSequences(input);
}

// Returns true for any token that acts as a control/command vs plain typed text.
// Key tokens now support optional arguments: {ENTER} {ENTER 500} {ENTER press} {ENTER release}
static bool isKeyToken(const String& upper) {
    return (upper == "ENTER"      || upper == "RETURN"    ||
            upper == "TAB"        || upper == "ESC"       || upper == "ESCAPE"      ||
            upper == "SPACE"      || upper == "BACKSPACE" || upper == "BKSP"        ||
            upper == "DELETE"     || upper == "DEL"       || upper == "INSERT"      ||
            upper == "LEFT"       || upper == "RIGHT"     || upper == "UP"          ||
            upper == "DOWN"       || upper == "HOME"      || upper == "END"         ||
            upper == "PAGEUP"     || upper == "PAGEDOWN"  ||
            upper == "PRINTSCREEN"|| upper == "PRTSC"     || upper == "SYSRQ"       ||
            upper == "CAPSLOCK"   || upper == "CAPS"      ||
            upper == "NUMLOCK"    || upper == "SCROLLLOCK"|| upper == "SCRLK"       ||
            upper == "PAUSE"      || upper == "PAUSEBREAK"||
            upper == "APPLICATION"|| upper == "MENU"      || upper == "APP"         ||
            upper == "GUI"        || upper == "MOD"       || upper == "WIN"         ||
            upper == "CMD"        || upper == "SUPER"     || upper == "WINDOWS"     ||
            upper == "CTRL"       || upper == "LCTRL"     || upper == "RCTRL"       ||
            upper == "ALT"        || upper == "LALT"      || upper == "RALT"        ||
            upper == "ALTGR"      || upper == "SHIFT"     || upper == "LSHIFT"      ||
            upper == "RSHIFT"     ||
            upper == "KPENTER"    || upper == "KPPLUS"    || upper == "KPMINUS"     ||
            upper == "KPMULTIPLY" || upper == "KPSTAR"    || upper == "KPDIVIDE"    ||
            upper == "KPSLASH"    || upper == "KPDOT"     || upper == "KPDECIMAL"   ||
            upper == "KP0"        || upper == "KP1"       || upper == "KP2"         ||
            upper == "KP3"        || upper == "KP4"       || upper == "KP5"         ||
            upper == "KP6"        || upper == "KP7"       || upper == "KP8"         ||
            upper == "KP9"        ||
            // Media / consumer keys
            upper == "MUTE"       || upper == "VOLUMEUP"  || upper == "VOLUP"       ||
            upper == "VOLUMEDOWN" || upper == "VOLDOWN"   || upper == "PLAYPAUSE"   ||
            upper == "NEXTTRACK"  || upper == "NEXT"      || upper == "PREVTRACK"   ||
            upper == "PREV"       || upper == "STOPTRACK" || upper == "STOP"        ||
            upper == "WWWHOME"    || upper == "BROWSER"   || upper == "EMAIL"       ||
            upper == "CALCULATOR" || upper == "CALC"      || upper == "MYCOMPUTER"  ||
            upper == "WWWSEARCH"  || upper == "WWWBACK"   || upper == "WWWFORWARD"  ||
            upper == "WWWSTOP"    || upper == "WWWREFRESH"|| upper == "BOOKMARKS"   ||
            upper == "MEDIASEL"   ||
            // System control keys
            upper == "SYSTEMPOWER"|| upper == "SYSPOWER"  || upper == "POWERDOWN"   ||
            upper == "SYSTEMSLEEP"|| upper == "SYSSLEEP"  ||
            upper == "SYSTEMWAKE" || upper == "SYSWAKE"   || upper == "WAKEUP"      ||
            // F1-F24
            (upper.startsWith("F") && upper.length() <= 3 && upper.substring(1).toInt() >= 1  && upper.substring(1).toInt() <= 12) ||
            (upper.startsWith("F") && upper.length() <= 3 && upper.substring(1).toInt() >= 13 && upper.substring(1).toInt() <= 24));
}

int resolveRegisterArg(const String& arg) {
    bool allDigits = !arg.isEmpty();
    for (char c : arg) { if (!isDigit(c)) { allDigits = false; break; } }
    if (allDigits) {
        int idx = arg.toInt() - 1;
        return (idx >= 0 && (size_t)idx < registers.size()) ? idx : -1;
    }
    String argUp = arg; argUp.toUpperCase();
    for (int i = 0; i < (int)registerNames.size(); i++) {
        String n = registerNames[i]; n.toUpperCase();
        if (n == argUp) return i;
    }
    return -1;
}

static bool isControlToken(const String& token) {
    String u = token;
    u.toUpperCase();

    // Key tokens with optional arg: "ENTER", "ENTER 500", "ENTER press", "ENTER release"
    int sp = u.indexOf(' ');
    String base = (sp >= 0) ? u.substring(0, sp) : u;
    if (isKeyToken(base)) return true;

    return (u == "RELEASEALL" ||
            u == "BLUETOOTH_ENABLE" || u == "BLUETOOTH_DISABLE" ||
            u == "USB_ENABLE"       || u == "USB_DISABLE"       ||
            u.startsWith("BLUETOOTH_HID ")      || u.startsWith("BLUETOOTH_KEYBOARD ") ||
            u.startsWith("BLUETOOTH_MOUSE ")    ||
            u.startsWith("USB_HID ")            || u.startsWith("USB_KEYBOARD ")       ||
            u.startsWith("USB_MOUSE ")          ||
            u == "HALT"     || u == "RESUME"  || u == "SINKPROX"  ||
            u == "ENDLOOP"  || u == "ENDFOR"  || u == "ENDWHILE"  ||
            u == "ELSE"     || u == "ENDIF"   || u == "BREAK"     ||
            u.startsWith("SLEEP ")      || u.startsWith("CHORD ")       || u.startsWith("HID ")        ||
            u == "WAIT_WIFI"            || u == "TIME"                  ||
            u.startsWith("WIFI ")       || u.startsWith("SETMOUSE ")    || u.startsWith("MOVEMOUSE ")  ||
            u.startsWith("MOUSECLICK")  || u.startsWith("MOUSEPRESS")   ||
            u.startsWith("MOUSERELEASE")|| u.startsWith("MOUSEDOUBLECLICK") ||
            u.startsWith("LOOP")        || u.startsWith("FOR ")         || u.startsWith("WHILE ")      ||
            u.startsWith("BREAK ")      || u.startsWith("SCHEDULE ")    ||
            u.startsWith("SET ")        || u.startsWith("IF ")          || u.startsWith("KEYMAP") ||
            u.startsWith("SET_ACTIVE_REGISTER ") || u.startsWith("PLAY_REGISTER ") ||
            u.startsWith("SD_WRITE ")  || u.startsWith("SD_APPEND ") ||
            u.startsWith("SD_EXEC ")   || u.startsWith("EXEC "));
}

// ---- Key token resolution ----

static uint8_t resolveKeyToken(const String& upper) {
    if (upper == "ENTER"      || upper == "RETURN")     return KEY_RETURN;
    if (upper == "TAB")                                  return KEY_TAB;
    if (upper == "ESC"        || upper == "ESCAPE")      return KEY_ESC;
    if (upper == "SPACE")                                return KEY_SPACE;
    if (upper == "BACKSPACE"  || upper == "BKSP")        return KEY_BACKSPACE;
    if (upper == "DELETE"     || upper == "DEL")         return KEY_DELETE;
    if (upper == "INSERT")                               return KEY_INSERT;
    if (upper == "LEFT")                                 return KEY_LEFT_ARROW;
    if (upper == "RIGHT")                                return KEY_RIGHT_ARROW;
    if (upper == "UP")                                   return KEY_UP_ARROW;
    if (upper == "DOWN")                                 return KEY_DOWN_ARROW;
    if (upper == "HOME")                                 return KEY_HOME;
    if (upper == "END")                                  return KEY_END;
    if (upper == "PAGEUP")                               return KEY_PAGE_UP;
    if (upper == "PAGEDOWN")                             return KEY_PAGE_DOWN;
    if (upper == "PRINTSCREEN"|| upper == "PRTSC"
                              || upper == "SYSRQ")       return KEY_PRINTSCREEN;
    if (upper == "CAPSLOCK"   || upper == "CAPS")        return KEY_CAPS_LOCK;
    if (upper == "NUMLOCK")                              return KEY_NUM_LOCK;
    if (upper == "SCROLLLOCK" || upper == "SCRLK")       return KEY_SCROLL_LOCK;
    if (upper == "PAUSE"      || upper == "PAUSEBREAK")  return KEY_PAUSE;
    if (upper == "APPLICATION"|| upper == "MENU"
                              || upper == "APP")         return KEY_APPLICATION;
    if (upper == "GUI"  || upper == "MOD"   || upper == "WIN"
     || upper == "CMD"  || upper == "SUPER" || upper == "WINDOWS") return KEY_LEFT_GUI;
    if (upper == "CTRL" || upper == "LCTRL")             return KEY_LEFT_CTRL;
    if (upper == "RCTRL")                                return KEY_RIGHT_CTRL;
    if (upper == "ALT"  || upper == "LALT")              return KEY_LEFT_ALT;
    if (upper == "RALT" || upper == "ALTGR")             return KEY_RIGHT_ALT;
    if (upper == "SHIFT"|| upper == "LSHIFT")            return KEY_LEFT_SHIFT;
    if (upper == "RSHIFT")                               return KEY_RIGHT_SHIFT;
    if (upper == "KPENTER")                              return KEY_KP_ENTER;
    if (upper == "KPPLUS")                               return KEY_KP_PLUS;
    if (upper == "KPMINUS")                              return KEY_KP_MINUS;
    if (upper == "KPMULTIPLY" || upper == "KPSTAR")      return KEY_KP_MULTIPLY;
    if (upper == "KPDIVIDE"   || upper == "KPSLASH")     return KEY_KP_DIVIDE;
    if (upper == "KPDOT"      || upper == "KPDECIMAL")   return KEY_KP_DOT;
    if (upper == "KP0") return KEY_KP0;
    if (upper == "KP1") return KEY_KP1;
    if (upper == "KP2") return KEY_KP2;
    if (upper == "KP3") return KEY_KP3;
    if (upper == "KP4") return KEY_KP4;
    if (upper == "KP5") return KEY_KP5;
    if (upper == "KP6") return KEY_KP6;
    if (upper == "KP7") return KEY_KP7;
    if (upper == "KP8") return KEY_KP8;
    if (upper == "KP9") return KEY_KP9;
    if (upper.startsWith("F")) {
        int n = upper.substring(1).toInt();
        if (n >= 1  && n <= 12) return KEY_F1  + (n - 1);
        if (n >= 13 && n <= 24) return KEY_F13 + (n - 13);
    }
    return 0;
}

static bool tryDispatchConsumerKey(const String& upper) {
    if (upper == "MUTE")                                { sendConsumerKey(KEY_MEDIA_MUTE);                         return true; }
    if (upper == "VOLUMEUP"   || upper == "VOLUP")      { sendConsumerKey(KEY_MEDIA_VOLUME_UP);                    return true; }
    if (upper == "VOLUMEDOWN" || upper == "VOLDOWN")    { sendConsumerKey(KEY_MEDIA_VOLUME_DOWN);                   return true; }
    if (upper == "PLAYPAUSE")                           { sendConsumerKey(KEY_MEDIA_PLAY_PAUSE);                   return true; }
    if (upper == "NEXTTRACK"  || upper == "NEXT")       { sendConsumerKey(KEY_MEDIA_NEXT_TRACK);                   return true; }
    if (upper == "PREVTRACK"  || upper == "PREV")       { sendConsumerKey(KEY_MEDIA_PREVIOUS_TRACK);               return true; }
    if (upper == "STOPTRACK"  || upper == "STOP")       { sendConsumerKey(KEY_MEDIA_STOP);                         return true; }
    if (upper == "WWWHOME"    || upper == "BROWSER")    { sendConsumerKey(KEY_MEDIA_WWW_HOME);                     return true; }
    if (upper == "EMAIL")                               { sendConsumerKey(KEY_MEDIA_EMAIL_READER);                 return true; }
    if (upper == "CALCULATOR" || upper == "CALC")       { sendConsumerKey(KEY_MEDIA_CALCULATOR);                   return true; }
    if (upper == "MYCOMPUTER")                          { sendConsumerKey(KEY_MEDIA_LOCAL_MACHINE_BROWSER);        return true; }
    if (upper == "WWWSEARCH")                           { sendConsumerKey(KEY_MEDIA_WWW_SEARCH);                   return true; }
    if (upper == "WWWBACK")                             { sendConsumerKey(KEY_MEDIA_WWW_BACK);                     return true; }
    if (upper == "WWWSTOP")                             { sendConsumerKey(KEY_MEDIA_WWW_STOP);                     return true; }
    if (upper == "BOOKMARKS")                           { sendConsumerKey(KEY_MEDIA_WWW_BOOKMARKS);                return true; }
    if (upper == "MEDIASEL")                            { sendConsumerKey(KEY_MEDIA_CONSUMER_CONTROL_CONFIGURATION); return true; }
    // System control keys
    if (upper == "SYSTEMPOWER"|| upper == "SYSPOWER" || upper == "POWERDOWN")  { sendSystemKey(KEY_SYSTEM_POWER); return true; }
    if (upper == "SYSTEMSLEEP"|| upper == "SYSSLEEP")                           { sendSystemKey(KEY_SYSTEM_SLEEP); return true; }
    if (upper == "SYSTEMWAKE" || upper == "SYSWAKE"  || upper == "WAKEUP")      { sendSystemKey(KEY_SYSTEM_WAKE);  return true; }
    return false;
}

// Dispatch a key token with an optional argument:
//   {KEY}           — normal tap
//   {KEY ms}        — hold for ms milliseconds
//   {KEY press}     — press down only (no auto-release)
//   {KEY release}   — release only
static void dispatchKeyToken(const String& tokenRaw, std::map<String, String>& vars) {
    String u = tokenRaw;
    u.toUpperCase();
    u.trim();

    int sp = u.indexOf(' ');
    String base = (sp >= 0) ? u.substring(0, sp) : u;
    String arg  = (sp >= 0) ? u.substring(sp + 1) : String("");
    arg.trim();

    // Consumer / media keys — no hold/press/release variants
    if (tryDispatchConsumerKey(base)) return;

    uint8_t kc = resolveKeyToken(base);
    if (!kc) return;

    if (arg.length() == 0) {
        sendSpecialKey(kc);
    } else {
        String argU = arg;
        argU.toUpperCase();
        if (argU == "PRESS") {
            pressKey(kc);
        } else if (argU == "RELEASE") {
            releaseKey(kc);
        } else {
            int holdMs = evaluateAllTokens(arg, vars).toInt();
            if (holdMs > 0) sendSpecialKeyTimed(kc, holdMs);
            else            sendSpecialKey(kc);
        }
    }
}

// ---- Parser ----

void parseAndSendText(const String& text, std::map<String, String>& vars) {
    if (g_parserAbort) return;
    // Snapshot HID routing flags so per-string overrides don't persist
    const bool _saveBleKb  = bleKeyboardEnabled;
    const bool _saveBleMo  = bleMouseEnabled;
#ifdef BOARD_HAS_USB_HID
    const bool _saveUsbKb  = usbKeyboardEnabled;
    const bool _saveUsbMo  = usbMouseEnabled;
#endif
    struct _HIDRestore {
        bool bleKb, bleMo;
#ifdef BOARD_HAS_USB_HID
        bool usbKb, usbMo;
#endif
        ~_HIDRestore() {
            bleKeyboardEnabled = bleKb;
            bleMouseEnabled    = bleMo;
#ifdef BOARD_HAS_USB_HID
            usbKeyboardEnabled = usbKb;
            usbMouseEnabled    = usbMo;
#endif
        }
    } _restore{_saveBleKb, _saveBleMo
#ifdef BOARD_HAS_USB_HID
        , _saveUsbKb, _saveUsbMo
#endif
    };

    int pos = 0;
    String currentSegment;

    while (pos < (int)text.length() && !g_parserAbort) {
        if (text[pos] != '{') {
            currentSegment += text[pos++];
            continue;
        }

        int endPos = findMatchingBrace(text, pos);
        if (endPos == -1) { currentSegment += text[pos++]; continue; }

        String token = text.substring(pos + 1, endPos);
        token.trim();

        if (!isControlToken(token)) {
            currentSegment += text.substring(pos, endPos + 1);
            pos = endPos + 1;
            continue;
        }

        if (currentSegment.length() > 0) {
            sendPlainText(evaluateAllTokens(currentSegment, vars));
            currentSegment = "";
        }

        String u = token;
        u.toUpperCase();

        // ---- Key tokens with optional argument ----
        if (isKeyToken(u.substring(0, u.indexOf(' ') >= 0 ? u.indexOf(' ') : u.length()))) {
            dispatchKeyToken(token, vars);
        }
        else if (u == "RELEASEALL") {
            hidReleaseAll();
        }
        else if (u == "BLUETOOTH_ENABLE")  enableBluetooth();
        else if (u == "BLUETOOTH_DISABLE") disableBluetooth();
        else if (u == "USB_ENABLE")        enableUSB();
        else if (u == "USB_DISABLE")       disableUSB();
        else if (u.startsWith("BLUETOOTH_HID ")) {
            String v = evaluateAllTokens(token.substring(14), vars); v.trim(); v.toLowerCase();
            bool en = (v == "1" || v == "true" || v == "enabled" || v == "on");
            bleKeyboardEnabled = en;
            bleMouseEnabled    = en;
        }
        else if (u.startsWith("BLUETOOTH_KEYBOARD ")) {
            String v = evaluateAllTokens(token.substring(19), vars); v.trim(); v.toLowerCase();
            bleKeyboardEnabled = (v == "1" || v == "true" || v == "enabled" || v == "on");
        }
        else if (u.startsWith("BLUETOOTH_MOUSE ")) {
            String v = evaluateAllTokens(token.substring(16), vars); v.trim(); v.toLowerCase();
            bleMouseEnabled = (v == "1" || v == "true" || v == "enabled" || v == "on");
        }
#ifdef BOARD_HAS_USB_HID
        else if (u.startsWith("USB_HID ")) {
            String v = evaluateAllTokens(token.substring(8), vars); v.trim(); v.toLowerCase();
            bool en = (v == "1" || v == "true" || v == "enabled" || v == "on");
            usbKeyboardEnabled = en;
            usbMouseEnabled    = en;
        }
        else if (u.startsWith("USB_KEYBOARD ")) {
            String v = evaluateAllTokens(token.substring(13), vars); v.trim(); v.toLowerCase();
            usbKeyboardEnabled = (v == "1" || v == "true" || v == "enabled" || v == "on");
        }
        else if (u.startsWith("USB_MOUSE ")) {
            String v = evaluateAllTokens(token.substring(10), vars); v.trim(); v.toLowerCase();
            usbMouseEnabled = (v == "1" || v == "true" || v == "enabled" || v == "on");
        }
#endif
        else if (u == "SINKPROX") {
            if (SPIFFS.exists("/sink.txt")) {
                File f = SPIFFS.open("/sink.txt", "r");
                if (f) {
                    String content = f.readString();
                    f.close();
                    SPIFFS.remove("/sink.txt");
                    if (!content.isEmpty()) pendingTokenStrings.push_back(content);
                }
            }
        }
        else if (u == "HALT")   { haltAllOperations(); return; }
        else if (u == "RESUME") { resumeOperations(); }
        else if (u == "WAIT_WIFI") {
            // Block until WiFi connects or execution is aborted
            while (WiFi.status() != WL_CONNECTED && !g_parserAbort) {
                server.handleClient();
                delay(200);
                checkParseInterrupt();
            }
        }
        else if (u == "TIME") {
            // {TIME} types the current time as HH:MM:SS via HID
            time_t now = time(nullptr);
            struct tm* t = localtime(&now);
            char buf[10];
            strftime(buf, sizeof(buf), "%H:%M:%S", t);
            sendPlainText(String(buf));
        }
        else if (u.startsWith("SLEEP ")) {
            int sleepTime = evaluateAllTokens(token.substring(6), vars).toInt();
            unsigned long start = millis();
            while ((millis() - start < (unsigned long)sleepTime) && !g_parserAbort) {
                server.handleClient();
                delay(10); checkParseInterrupt();
            }
        }
        else if (u.startsWith("CHORD ")) {
            processChord(evaluateAllTokens(token.substring(6), vars));
        }
        else if (u.startsWith("HID ")) {
            String args = token.substring(4);
            std::vector<String> parts;
            int start = 0;
            while (start < (int)args.length()) {
                int space = args.indexOf(' ', start);
                if (space == -1) space = args.length();
                if (start < space) {
                    String part = args.substring(start, space);
                    part.trim();
                    if (part.length()) parts.push_back(evaluateAllTokens(part, vars));
                }
                start = space + 1;
            }
            if (parts.size() >= 1) {
                std::vector<uint8_t> keycodes;
                uint8_t modifiers = 0;
                if (parts.size() > 1) {
                    modifiers = (uint8_t)strtol(parts[0].c_str(), nullptr, 16);
                    for (size_t i = 1; i < parts.size(); i++)
                        keycodes.push_back((uint8_t)strtol(parts[i].c_str(), nullptr, 16));
                } else {
                    keycodes.push_back((uint8_t)strtol(parts[0].c_str(), nullptr, 16));
                }
                sendKeyChord(keycodes, modifiers);
            }
        }
        else if (u.startsWith("WIFI ")) {
            String args = token.substring(5);
            int sp = args.indexOf(' ');
            if (sp > 0) connectToNewWiFi(args.substring(0, sp), args.substring(sp + 1));
        }
        else if (u.startsWith("SETMOUSE ") || u.startsWith("MOVEMOUSE ")) {
            bool isSet = u.startsWith("SETMOUSE ");
            String args = token.substring(isSet ? 9 : 10);
            int spacePos = -1, braceDepth = 0;
            for (int i = 0; i < (int)args.length(); i++) {
                if      (args[i] == '{') braceDepth++;
                else if (args[i] == '}') braceDepth--;
                else if (args[i] == ' ' && braceDepth == 0) { spacePos = i; break; }
            }
            if (spacePos > 0) {
                int x = evaluateAllTokens(args.substring(0, spacePos), vars).toInt();
                int y = evaluateAllTokens(args.substring(spacePos + 1), vars).toInt();
                if (isSet) setMousePosition(x, y);
                else       sendMouseMovement(x, y);
            }
        }
        else if (u.startsWith("MOUSEDOUBLECLICK")) {
            int btn = (token.length() > 16) ? evaluateAllTokens(token.substring(17), vars).toInt() : MOUSE_LEFT;
            sendMouseDoubleClick(btn);
        }
        else if (u.startsWith("MOUSECLICK")) {
            int btn = (token.length() > 10) ? evaluateAllTokens(token.substring(11), vars).toInt() : MOUSE_LEFT;
            sendMouseClick(btn);
        }
        else if (u.startsWith("MOUSEPRESS")) {
            int btn = (token.length() > 10) ? evaluateAllTokens(token.substring(11), vars).toInt() : MOUSE_LEFT;
            sendMousePress(btn);
        }
        else if (u.startsWith("MOUSERELEASE")) {
            int btn = (token.length() > 12) ? evaluateAllTokens(token.substring(13), vars).toInt() : MOUSE_LEFT;
            sendMouseRelease(btn);
        }
        else if (u.startsWith("LOOP")) {
            std::vector<String> parts;
            int start = (token.length() > 4 && token[4] == ' ') ? 5 : 4;
            while (start < (int)token.length()) {
                int space = token.indexOf(' ', start);
                if (space == -1) space = token.length();
                if (start < space) parts.push_back(token.substring(start, space));
                start = space + 1;
            }

            int endLoopPos = findMatchingEndLoop(text, endPos + 1);
            if (endLoopPos == -1) { pos = endPos + 1; continue; }

            String body = text.substring(endPos + 1, endLoopPos);

            if (parts.size() == 0) {
                while (!g_parserAbort) {
                    parseAndSendText(body, vars);
                    if (vars.count("__break__")) { vars.erase("__break__"); break; }
                    server.handleClient();
                    checkParseInterrupt();
                }
            } else if (parts.size() == 1) {
                unsigned long dur = evaluateAllTokens(parts[0], vars).toInt();
                unsigned long t0  = millis();
                while (!g_parserAbort && (millis() - t0 < dur)) {
                    parseAndSendText(body, vars);
                    if (vars.count("__break__")) { vars.erase("__break__"); break; }
                    server.handleClient();
                    checkParseInterrupt();
                }
            } else if (parts.size() == 4) {
                String varName  = parts[0];
                int startVal    = evaluateAllTokens(parts[1], vars).toInt();
                int increment   = evaluateAllTokens(parts[2], vars).toInt();
                int stopVal     = evaluateAllTokens(parts[3], vars).toInt();
                bool broken     = false;
                if (increment != 0) {
                    for (int i = startVal; !g_parserAbort && !broken; i += increment) {
                        vars[varName] = String(i);
                        if ((increment > 0 && i > stopVal) || (increment < 0 && i < stopVal)) break;
                        parseAndSendText(body, vars);
                        if (vars.count("__break__")) { vars.erase("__break__"); broken = true; break; }
                        server.handleClient();
                        checkParseInterrupt();
                    }
                    vars.erase(varName);
                }
            }

            pos = endLoopPos + 9;
            continue;
        }
        else if (u.startsWith("FOR ")) {
            // {FOR varName start increment end}...{ENDFOR}
            String args = token.substring(4);
            std::vector<String> parts;
            int start = 0;
            while (start < (int)args.length()) {
                int space = args.indexOf(' ', start);
                if (space == -1) space = args.length();
                if (start < space) parts.push_back(args.substring(start, space));
                start = space + 1;
            }

            int endForPos = findMatchingEndFor(text, endPos + 1);
            if (endForPos == -1 || parts.size() != 4) { pos = endPos + 1; continue; }

            String body     = text.substring(endPos + 1, endForPos);
            String varName  = parts[0];
            int startVal    = evaluateAllTokens(parts[1], vars).toInt();
            int increment   = evaluateAllTokens(parts[2], vars).toInt();
            int stopVal     = evaluateAllTokens(parts[3], vars).toInt();
            bool broken     = false;
            if (increment != 0) {
                for (int i = startVal; !g_parserAbort && !broken; i += increment) {
                    vars[varName] = String(i);
                    if ((increment > 0 && i > stopVal) || (increment < 0 && i < stopVal)) break;
                    parseAndSendText(body, vars);
                    if (vars.count("__break__")) { vars.erase("__break__"); broken = true; break; }
                    checkParseInterrupt();
                }
                vars.erase(varName);
            }

            pos = endForPos + 8;
            continue;
        }
        else if (u.startsWith("WHILE ")) {
            // {WHILE left op right}...{ENDWHILE}
            String condition = token.substring(6);
            condition.trim();

            int endWhilePos = findMatchingEndWhile(text, endPos + 1);
            if (endWhilePos == -1) { pos = endPos + 1; continue; }

            String body = text.substring(endPos + 1, endWhilePos);

            while (!g_parserAbort && evaluateCondition(condition, vars)) {
                parseAndSendText(body, vars);
                if (vars.count("__break__")) { vars.erase("__break__"); break; }
                checkParseInterrupt();
            }

            pos = endWhilePos + 10;
            continue;
        }
        else if (u == "ENDLOOP" || u == "ENDFOR" || u == "ENDWHILE") { /* handled by their respective loop tokens */ }
        else if (u == "BREAK") {
            vars["__break__"] = "1";
            return;
        }
        else if (u.startsWith("BREAK ")) {
            String rest = token.substring(6);
            rest.trim();
            int sp = rest.indexOf(' ');
            if (sp != -1) {
                String varName = rest.substring(0, sp);
                int breakVal   = evaluateAllTokens(rest.substring(sp + 1), vars).toInt();
                if (vars.count(varName) && vars[varName].toInt() == breakVal) {
                    vars["__break__"] = "1";
                    return;
                }
            }
        }
        else if (u.startsWith("SCHEDULE ")) {
            // Non-blocking: store the schedule target in vars and skip the rest
            // of this token string. The pending re-execution is handled by
            // re-queuing with a "__SCHEDULE__" prefix that loop() checks.
            String arg = token.substring(9);
            arg.trim();
            vars["__sched_H__"] = arg.substring(0, 2);
            vars["__sched_M__"] = arg.substring(3, 5);
            vars["__sched_S__"] = (arg.length() >= 8) ? arg.substring(6, 8) : String("0");
            // Remaining tokens after SCHEDULE (everything from pos onwards) get
            // re-queued prefixed with a sentinel so the loop can fire them at the right time.
            String remaining = text.substring(endPos + 1);
            remaining.trim();
            // Pack: "SCHED|HH|MM|SS|<remaining tokens>"
            String queued = String("SCHED|") + vars["__sched_H__"] + "|" +
                            vars["__sched_M__"] + "|" + vars["__sched_S__"] + "|" + remaining;
            pendingTokenStrings.push_back(queued);
            return; // stop processing this token string — continuation is queued
        }
        else if (u.startsWith("SET ")) {
            String rest = token.substring(4);
            rest.trim();
            int sp = rest.indexOf(' ');
            if (sp > 0) vars[rest.substring(0, sp)] = evaluateAllTokens(rest.substring(sp + 1), vars);
        }
        else if (u.startsWith("KEYMAP")) {
            String id = (token.length() > 6) ? token.substring(7) : String("");
            id.trim();
            id.toLowerCase();
            if (!id.isEmpty()) {
                if (keymapLoad(id)) keymapSaveActive();
            }
        }
        else if (u.startsWith("SET_ACTIVE_REGISTER ")) {
            String arg = evaluateAllTokens(token.substring(20), vars);
            arg.trim();
            int idx = resolveRegisterArg(arg);
            if (idx >= 0) { activeRegister = idx; saveActiveRegister(); }
        }
        else if (u.startsWith("PLAY_REGISTER ")) {
            String arg = evaluateAllTokens(token.substring(14), vars);
            arg.trim();
            int idx = resolveRegisterArg(arg);
            if (idx >= 0) playRegister(idx);
        }
        else if (u.startsWith("SD_WRITE ") || u.startsWith("SD_APPEND ")) {
            int prefixLen = u.startsWith("SD_WRITE ") ? 9 : 10;
            String sdRest = evaluateAllTokens(token.substring(prefixLen), vars);
            sdRest.trim();
            String sdPath, sdContent;
            if (!sdRest.isEmpty() && sdRest[0] == '"') {
                int closeQ = sdRest.indexOf('"', 1);
                if (closeQ > 0) {
                    sdPath    = sdRest.substring(1, closeQ);
                    sdContent = sdRest.substring(closeQ + 1);
                    sdContent.trim();
                }
            } else {
                int sp2 = sdRest.indexOf(' ');
                if (sp2 > 0) {
                    sdPath    = sdRest.substring(0, sp2);
                    sdContent = sdRest.substring(sp2 + 1);
                    sdContent.trim();
                } else {
                    sdPath = sdRest;
                }
            }
            if (!sdContent.isEmpty() && sdContent[0] == '"' &&
                sdContent[sdContent.length()-1] == '"' && sdContent.length() > 1)
                sdContent = sdContent.substring(1, sdContent.length() - 1);
            if (!sdPath.isEmpty()) {
                if (u.startsWith("SD_WRITE ")) sdWriteFile(sdPath, sdContent);
                else                           sdAppendFile(sdPath, sdContent);
            }
        }
        else if (u.startsWith("SD_EXEC ")) {
            String sdExecPath = evaluateAllTokens(token.substring(8), vars);
            sdExecPath.trim();
            if (!sdExecPath.isEmpty() && sdExecPath[0] == '"' &&
                sdExecPath[sdExecPath.length()-1] == '"')
                sdExecPath = sdExecPath.substring(1, sdExecPath.length() - 1);
            kpsExecFile(sdExecPath, vars);
        }
        else if (u.startsWith("IF ")) {
            String condition = token.substring(3);
            condition.trim();

            int endIfPos = findMatchingEndIf(text, endPos + 1);
            if (endIfPos == -1) { pos = endPos + 1; continue; }

            int elsePos = findMatchingElse(text, endPos + 1, endIfPos);

            if (evaluateCondition(condition, vars)) {
                String trueBody = text.substring(endPos + 1, (elsePos != -1) ? elsePos : endIfPos);
                parseAndSendText(trueBody, vars);
            } else if (elsePos != -1) {
                parseAndSendText(text.substring(elsePos + 6, endIfPos), vars);
            }

            pos = endIfPos + 7;
            continue;
        }
        else if (u == "ELSE" || u == "ENDIF") { /* skip bare occurrences */ }

        checkParseInterrupt();
        pos = endPos + 1;
    }

    if (currentSegment.length() > 0 && !g_parserAbort) {
        sendPlainText(evaluateAllTokens(currentSegment, vars));
    }

    if (!g_parserAbort) {
        hidReleaseAll();
        delay(g_keyReleaseDelay);
    }
}

void parseAndSendText(const String& text) {
    std::map<String, String> vars;
    parseAndSendText(text, vars);
}

void putTokenString(const String& text) {
    g_parserAbort = false;
    isHalted      = false;
    std::map<String, String> vars;
    parseAndSendText(text, vars);
}
