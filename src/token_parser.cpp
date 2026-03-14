#include "token_parser.h"
#include "hid.h"
#include "connection.h"
#include "keymap.h"
#include "storage.h"
#include "credential_store.h"

#ifdef BOARD_M5STACK_CARDPUTER
#include <M5Cardputer.h>
#endif

// ---- Interrupt check ----
// Called at every delay/yield point inside the parser. Feeds the watchdog and
// checks whether BtnA or ESC/backtick has been pressed; if so, halts execution.
static void checkParseInterrupt() {
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

static String evaluateAllTokens(String input, std::map<String, String>& vars);

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

static String evaluateAllTokens(String input, std::map<String, String>& vars) {
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
        } else if (upperToken.startsWith("RAND ")) {
            int argStart = 5;
            String a, b;
            int space = token.indexOf(' ', argStart);
            if (space != -1) {
                a = token.substring(argStart, space);
                b = token.substring(space + 1);
                replacement = String(random(a.toInt(), b.toInt() + 1));
                resolved    = true;
            }
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
            String label = token.substring(10);
            label.trim();
            replacement = credStoreLocked ? "" : credStoreGet(label);
            resolved    = true;
        }

        if (resolved) {
            input   = input.substring(0, tokenStart) + replacement + input.substring(tokenEnd + 1);
            changed = true;
        }
    }

    return processEscapeSequences(input);
}

static bool isControlToken(const String& token) {
    String u = token;
    u.toUpperCase();

    bool isFKey = u.startsWith("F") && u.length() <= 3 && u.substring(1).toInt() >= 1 && u.substring(1).toInt() <= 12;

    return (u == "ENTER" || u == "TAB" || u == "ESC" || u == "ESCAPE" || u == "SPACE" ||
            u == "BACKSPACE" || u == "DELETE" || u == "LEFT" || u == "RIGHT" ||
            u == "UP" || u == "DOWN" || u == "INSERT" || u == "HOME" || u == "END" ||
            u == "PAGEUP" || u == "PAGEDOWN" || u == "PRINTSCREEN" || u == "SYSRQ" || isFKey ||
            u == "BLUETOOTH_ENABLE" || u == "BLUETOOTH_DISABLE" ||
            u == "USB_ENABLE" || u == "USB_DISABLE" ||
            u == "HALT" || u == "RESUME" || u == "SINKPROX" ||
            u == "ENDLOOP" || u == "ENDFOR" || u == "ENDWHILE" ||
            u == "ELSE" || u == "ENDIF" || u == "BREAK" ||
            u.startsWith("SLEEP ")   || u.startsWith("CHORD ")   || u.startsWith("HID ")    ||
            u.startsWith("WIFI ")    || u.startsWith("SETMOUSE ") || u.startsWith("MOVEMOUSE ") ||
            u.startsWith("MOUSECLICK") || u.startsWith("MOUSEPRESS") ||
            u.startsWith("MOUSERELEASE") || u.startsWith("MOUSEDOUBLECLICK") ||
            u.startsWith("LOOP")    || u.startsWith("FOR ")  || u.startsWith("WHILE ") ||
            u.startsWith("BREAK ")  || u.startsWith("SCHEDULE ") ||
            u.startsWith("SET ")    || u.startsWith("IF ")   || u.startsWith("KEYMAP"));
}

// ---- Parser ----

void parseAndSendText(const String& text, std::map<String, String>& vars) {
    if (g_parserAbort) return;

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

        if      (u == "ENTER")     sendSpecialKey(KEY_RETURN);
        else if (u == "TAB")       sendSpecialKey(KEY_TAB);
        else if (u == "ESC" || u == "ESCAPE") sendSpecialKey(KEY_ESC);
        else if (u == "SPACE")     sendSpecialKey(KEY_SPACE);
        else if (u == "BACKSPACE") sendSpecialKey(KEY_BACKSPACE);
        else if (u == "DELETE")    sendSpecialKey(KEY_DELETE);
        else if (u == "LEFT")      sendSpecialKey(KEY_LEFT_ARROW);
        else if (u == "RIGHT")     sendSpecialKey(KEY_RIGHT_ARROW);
        else if (u == "UP")        sendSpecialKey(KEY_UP_ARROW);
        else if (u == "DOWN")      sendSpecialKey(KEY_DOWN_ARROW);
        else if (u == "INSERT")    sendSpecialKey(KEY_INSERT);
        else if (u == "HOME")      sendSpecialKey(KEY_HOME);
        else if (u == "END")       sendSpecialKey(KEY_END);
        else if (u == "PAGEUP")    sendSpecialKey(KEY_PAGE_UP);
        else if (u == "PAGEDOWN")  sendSpecialKey(KEY_PAGE_DOWN);
        else if (u == "PRINTSCREEN" || u == "SYSRQ") sendSpecialKey(KEY_PRINTSCREEN);
        else if (u.startsWith("F") && u.length() <= 3) {
            int n = u.substring(1).toInt();
            if (n >= 1 && n <= 12) sendSpecialKey(KEY_F1 + (n - 1));
        }
        else if (u == "BLUETOOTH_ENABLE")  enableBluetooth();
        else if (u == "BLUETOOTH_DISABLE") disableBluetooth();
        else if (u == "USB_ENABLE")        enableUSB();
        else if (u == "USB_DISABLE")       disableUSB();
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
        delay(KEY_RELEASE_DELAY);
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
