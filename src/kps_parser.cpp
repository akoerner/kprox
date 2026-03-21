// ============================================================
// KProx Script (.kps) interpreter
// ============================================================
// A line-oriented scripting language that compiles all the hardware
// capability of the token parser into a readable, programming-language-
// style syntax. Every token in the token reference still works inside
// quoted strings and expressions via {TOKEN} syntax. Variables are
// accessed as ${varname}.
//
// Language reference: see TOKEN_REFERENCE.md § KProx Script
// ============================================================

#include "kps_parser.h"
#include "token_parser.h"
#include "hid.h"
#include "sd_utils.h"
#include "registers.h"
#include "credential_store.h"
#include <vector>

// ---- Recursion / abort guard ----

static constexpr int KPS_MAX_DEPTH  = 16;
static constexpr int KPS_MAX_LINES  = 4096;
static bool          g_kpsBreak     = false;
static bool          g_kpsReturn    = false;

// ---- Argument parser ----
// Splits a raw args string into a list of tokens, respecting double-quoted
// strings (with backslash escapes) and bare words.

static std::vector<String> kpsArgs(const String& line) {
    std::vector<String> out;
    int i = 0;
    int n = (int)line.length();
    while (i < n) {
        while (i < n && line[i] == ' ') i++;
        if (i >= n) break;
        String tok;
        if (line[i] == '"') {
            i++;
            while (i < n && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < n) {
                    char nx = line[i + 1];
                    if      (nx == '"')  { tok += '"';  i += 2; }
                    else if (nx == 'n')  { tok += '\n'; i += 2; }
                    else if (nx == 't')  { tok += '\t'; i += 2; }
                    else if (nx == '\\') { tok += '\\'; i += 2; }
                    else                 { tok += line[i++]; }
                } else {
                    tok += line[i++];
                }
            }
            if (i < n) i++; // closing quote
        } else {
            while (i < n && line[i] != ' ') tok += line[i++];
        }
        out.push_back(tok);
    }
    return out;
}

// ---- Expression evaluator ----
// Resolves ${var} substitution then evaluates {TOKEN} inline tokens via
// the existing token parser evaluator.

static String kpsEval(const String& expr, std::map<String, String>& vars) {
    String s = expr;
    // Strip surrounding double quotes if present
    if (s.length() >= 2 && s[0] == '"' && s[s.length() - 1] == '"')
        s = s.substring(1, s.length() - 1);
    // ${varname} → value (longest-match first via sorted iteration isn't easy
    // on Arduino String, so just do repeated passes — safe for typical sizes)
    bool changed = true;
    for (int pass = 0; pass < 32 && changed; pass++) {
        changed = false;
        for (auto& kv : vars) {
            String ph = "${" + kv.first + "}";
            int idx;
            while ((idx = s.indexOf(ph)) >= 0) {
                s = s.substring(0, idx) + kv.second + s.substring(idx + ph.length());
                changed = true;
            }
        }
    }
    // Delegate remaining {TOKEN} sequences to the token parser evaluator
    return evaluateAllTokens(s, vars);
}

// ---- Condition evaluator ----

static bool kpsEvalCond(const String& cond, std::map<String, String>& vars) {
    static const char* OPS[] = { "==", "!=", "<=", ">=", "<", ">", nullptr };
    for (int oi = 0; OPS[oi]; oi++) {
        int pos = cond.indexOf(OPS[oi]);
        if (pos < 0) continue;
        String lhs = cond.substring(0, pos);           lhs.trim();
        String rhs = cond.substring(pos + strlen(OPS[oi])); rhs.trim();
        String lv = kpsEval(lhs, vars);
        String rv = kpsEval(rhs, vars);
        // Attempt numeric comparison
        bool lOk = !lv.isEmpty(), rOk = !rv.isEmpty();
        for (char c : lv) if (!isDigit(c) && c != '-' && c != '.') { lOk = false; break; }
        for (char c : rv) if (!isDigit(c) && c != '-' && c != '.') { rOk = false; break; }
        if (lOk && rOk) {
            float l = lv.toFloat(), r = rv.toFloat();
            if (strcmp(OPS[oi], "==") == 0) return l == r;
            if (strcmp(OPS[oi], "!=") == 0) return l != r;
            if (strcmp(OPS[oi], "<")  == 0) return l <  r;
            if (strcmp(OPS[oi], ">")  == 0) return l >  r;
            if (strcmp(OPS[oi], "<=") == 0) return l <= r;
            if (strcmp(OPS[oi], ">=") == 0) return l >= r;
        } else {
            if (strcmp(OPS[oi], "==") == 0) return lv == rv;
            if (strcmp(OPS[oi], "!=") == 0) return lv != rv;
            if (strcmp(OPS[oi], "<")  == 0) return lv <  rv;
            if (strcmp(OPS[oi], ">")  == 0) return lv >  rv;
            if (strcmp(OPS[oi], "<=") == 0) return lv <= rv;
            if (strcmp(OPS[oi], ">=") == 0) return lv >= rv;
        }
    }
    // No operator — truthy if the evaluated expression is non-empty and not "0"
    String val = kpsEval(cond, vars);
    return !val.isEmpty() && val != "0" && val != "false";
}

// ---- Block boundary finder ----
// Finds the matching end-keyword for a block opener at lines[from], handling
// nesting of any recognised block openers.

static int kpsFindEnd(const std::vector<String>& lines, int from, const char* endKw) {
    int depth = 1;
    static const char* OPENS[]  = { "if", "for", "while", nullptr };
    static const char* CLOSES[] = { "endif", "endfor", "endwhile", nullptr };
    for (int i = from + 1; i < (int)lines.size() && i < KPS_MAX_LINES; i++) {
        String l = lines[i]; l.trim();
        if (l.isEmpty() || l[0] == '#') continue;
        int sp = l.indexOf(' ');
        String cmd = (sp > 0 ? l.substring(0, sp) : l);
        cmd.toLowerCase();
        for (int k = 0; OPENS[k]; k++)  if (cmd == OPENS[k])  { depth++; break; }
        for (int k = 0; CLOSES[k]; k++) if (cmd == CLOSES[k]) { depth--; break; }
        if (depth == 0 && cmd == endKw) return i;
        if (depth < 0) break;
    }
    return -1;
}

// Find elif/else/endif at the same nesting level within [from+1, endifIdx)
static std::vector<int> kpsFindBranches(const std::vector<String>& lines,
                                         int from, int endifIdx) {
    std::vector<int> out;
    int depth = 0;
    for (int i = from + 1; i < endifIdx; i++) {
        String l = lines[i]; l.trim();
        if (l.isEmpty() || l[0] == '#') continue;
        int sp = l.indexOf(' ');
        String cmd = (sp > 0 ? l.substring(0, sp) : l);
        cmd.toLowerCase();
        if (cmd == "if" || cmd == "for" || cmd == "while") { depth++; continue; }
        if (cmd == "endif" || cmd == "endfor" || cmd == "endwhile") { depth--; continue; }
        if (depth == 0 && (cmd == "elif" || cmd == "else")) out.push_back(i);
    }
    return out;
}

// ---- Forward declaration ----
static void kpsExecBlock(const std::vector<String>& lines, int start, int end,
                         std::map<String, String>& vars, int depth);

// ---- Statement dispatcher ----

static void kpsExecBlock(const std::vector<String>& lines, int start, int end,
                         std::map<String, String>& vars, int depth) {
    if (depth > KPS_MAX_DEPTH || g_parserAbort) return;

    int i = start;
    while (i < end && !g_parserAbort && !g_kpsBreak && !g_kpsReturn) {
        String line = lines[i]; line.trim();

        if (line.isEmpty() || line[0] == '#') { i++; continue; }

        // Split command from arguments
        int sp = line.indexOf(' ');
        String cmd  = (sp > 0) ? line.substring(0, sp) : line;
        String rest = (sp > 0) ? line.substring(sp + 1) : String("");
        rest.trim();
        cmd.toLowerCase();

        // ---- Output ----
        if (cmd == "echo" || cmd == "type") {
            parseAndSendText(kpsEval(rest, vars), vars);

        // ---- Variables ----
        } else if (cmd == "set") {
            auto av = kpsArgs(rest);
            if (av.size() >= 2) {
                // Rejoin everything after the name as the value expression
                String valExpr = rest.substring(av[0].length());
                valExpr.trim();
                vars[av[0]] = kpsEval(valExpr, vars);
            } else if (av.size() == 1) {
                vars[av[0]] = "";
            }

        // ---- Unset variable ----
        } else if (cmd == "unset") {
            String unsetKey = rest;
            unsetKey.trim();
            vars.erase(unsetKey);

        // ---- Sleep ----
        } else if (cmd == "sleep") {
            int ms = kpsEval(rest, vars).toInt();
            if (ms > 0) {
                unsigned long t = millis();
                while (millis() - t < (unsigned long)ms && !g_parserAbort) {
                    server.handleClient();
                    delay(10);
                    checkParseInterrupt();
                }
            }

        // ---- System ----
        } else if (cmd == "halt")   { haltAllOperations(); return;
        } else if (cmd == "resume") { resumeOperations();
        } else if (cmd == "break")  { g_kpsBreak  = true; return;
        } else if (cmd == "return") { g_kpsReturn = true; return;

        // ---- HID helpers ----
        } else if (cmd == "key") {
            String k = kpsEval(rest, vars); k.toUpperCase();
            parseAndSendText("{" + k + "}", vars);
        } else if (cmd == "chord") {
            String k = kpsEval(rest, vars);
            parseAndSendText("{CHORD " + k + "}", vars);

        // ---- Run raw token string ----
        } else if (cmd == "run") {
            parseAndSendText(kpsEval(rest, vars), vars);

        // ---- SD file operations ----
        } else if (cmd == "sd_write" || cmd == "sd_append") {
            auto av = kpsArgs(rest);
            if (av.size() >= 2) {
                String path    = kpsEval(av[0], vars);
                // Rejoin remaining args as the content
                String content = rest.substring(av[0].length() + (rest[av[0].length()] == '"' ? 2 : 1));
                content.trim();
                content = kpsEval(content, vars);
                if (cmd == "sd_write") sdWriteFile(path, content);
                else                   sdAppendFile(path, content);
            }

        // ---- Include another KPS file ----
        } else if (cmd == "include") {
            String path = kpsEval(rest, vars);
            kpsExecFile(path, vars);
            g_kpsReturn = false; // return in included file doesn't bubble up

        // ---- WiFi ----
        } else if (cmd == "wifi_connect") {
            auto av = kpsArgs(rest);
            if (av.size() >= 2) {
                String ssid = kpsEval(av[0], vars);
                String pass = kpsEval(av[1], vars);
                parseAndSendText("{WIFI " + ssid + " " + pass + "}", vars);
            }

        // ---- Register operations ----
        } else if (cmd == "play_register") {
            String arg = kpsEval(rest, vars);
            int idx = resolveRegisterArg(arg);
            if (idx >= 0) playRegister(idx);

        } else if (cmd == "set_active_register") {
            String arg = kpsEval(rest, vars);
            int idx = resolveRegisterArg(arg);
            if (idx >= 0) { activeRegister = idx; saveActiveRegister(); }

        // ====================================================
        // ---- Block structures ----
        // ====================================================

        // ---- if / elif / else / endif ----
        } else if (cmd == "if") {
            int endifIdx = kpsFindEnd(lines, i, "endif");
            if (endifIdx < 0) return; // malformed

            std::vector<int> branches = kpsFindBranches(lines, i, endifIdx);

            bool taken = false;
            int  blockStart = i;
            String blockCond = rest;

            // Walk through if → elif → else chains
            for (int bi = 0; bi <= (int)branches.size(); bi++) {
                int blockEnd = (bi < (int)branches.size()) ? branches[bi] : endifIdx;

                if (!taken) {
                    // Get the condition for this branch
                    bool isElse = false;
                    if (bi > 0) {
                        String bl = lines[branches[bi - 1]]; bl.trim();
                        int bsp = bl.indexOf(' ');
                        String bc  = bsp > 0 ? bl.substring(0, bsp) : bl; bc.toLowerCase();
                        String bcond = bsp > 0 ? bl.substring(bsp + 1) : String("");
                        bcond.trim();
                        if (bc == "else") { isElse = true; blockCond = ""; }
                        else              { blockCond = bcond; }
                        blockStart = branches[bi - 1];
                    }
                    bool cond = isElse ? true : kpsEvalCond(blockCond, vars);
                    if (cond) {
                        taken = true;
                        kpsExecBlock(lines, blockStart + 1, blockEnd, vars, depth + 1);
                        if (g_kpsBreak || g_kpsReturn || g_parserAbort) { i = endifIdx + 1; return; }
                    }
                }
            }
            i = endifIdx + 1;
            continue;

        // ---- for var start step end / endfor ----
        } else if (cmd == "for") {
            int endforIdx = kpsFindEnd(lines, i, "endfor");
            if (endforIdx < 0) { i++; continue; }

            auto av = kpsArgs(rest);
            if (av.size() == 4) {
                String var   = av[0];
                long   cur   = kpsEval(av[1], vars).toInt();
                long   step  = kpsEval(av[2], vars).toInt();
                long   limit = kpsEval(av[3], vars).toInt();
                if (step == 0) step = 1;

                while (!g_parserAbort && !g_kpsReturn) {
                    if (step > 0 && cur > limit) break;
                    if (step < 0 && cur < limit) break;
                    vars[var] = String(cur);
                    kpsExecBlock(lines, i + 1, endforIdx, vars, depth + 1);
                    if (g_kpsBreak) { g_kpsBreak = false; break; }
                    if (g_kpsReturn || g_parserAbort) { i = endforIdx + 1; return; }
                    cur += step;
                    feedWatchdog();
                }
                vars.erase(var);
            }
            i = endforIdx + 1;
            continue;

        // ---- while condition / endwhile ----
        } else if (cmd == "while") {
            int endwhileIdx = kpsFindEnd(lines, i, "endwhile");
            if (endwhileIdx < 0) { i++; continue; }

            while (!g_parserAbort && !g_kpsReturn && kpsEvalCond(rest, vars)) {
                kpsExecBlock(lines, i + 1, endwhileIdx, vars, depth + 1);
                if (g_kpsBreak) { g_kpsBreak = false; break; }
                if (g_kpsReturn || g_parserAbort) { i = endwhileIdx + 1; return; }
                feedWatchdog();
            }
            i = endwhileIdx + 1;
            continue;

        // ---- loop (infinite or timed) / endloop ----
        } else if (cmd == "loop") {
            int endloopIdx = kpsFindEnd(lines, i, "endloop");
            if (endloopIdx < 0) { i++; continue; }

            unsigned long dur = rest.isEmpty() ? 0 : (unsigned long)kpsEval(rest, vars).toInt();
            unsigned long t0  = millis();

            while (!g_parserAbort && !g_kpsReturn) {
                if (dur > 0 && millis() - t0 >= dur) break;
                kpsExecBlock(lines, i + 1, endloopIdx, vars, depth + 1);
                if (g_kpsBreak) { g_kpsBreak = false; break; }
                if (g_kpsReturn || g_parserAbort) { i = endloopIdx + 1; return; }
                feedWatchdog();
            }
            i = endloopIdx + 1;
            continue;

        // ---- Ignore elif/else/endif/endfor/endwhile when encountered at top level ----
        } else if (cmd == "elif" || cmd == "else" || cmd == "endif" ||
                   cmd == "endfor" || cmd == "endwhile" || cmd == "endloop") {
            // Handled by their opener — skip if encountered bare
        }

        i++;
    }
}

// ---- Tokenise script into lines ----

static std::vector<String> kpsSplit(const String& script) {
    std::vector<String> lines;
    int start = 0;
    for (int i = 0; i <= (int)script.length(); i++) {
        if (i == (int)script.length() || script[i] == '\n') {
            String line = script.substring(start, i);
            // Strip CR
            if (!line.isEmpty() && line[line.length() - 1] == '\r')
                line.remove(line.length() - 1);
            lines.push_back(line);
            start = i + 1;
            if ((int)lines.size() >= KPS_MAX_LINES) break;
        }
    }
    return lines;
}

// ---- Public API ----

void kpsExec(const String& script, std::map<String, String>& vars) {
    g_parserAbort = false;
    isHalted      = false;
    g_kpsBreak    = false;
    g_kpsReturn   = false;
    auto lines = kpsSplit(script);
    kpsExecBlock(lines, 0, (int)lines.size(), vars, 0);
    g_kpsBreak  = false;
    g_kpsReturn = false;
}

void kpsExec(const String& script) {
    std::map<String, String> vars;
    kpsExec(script, vars);
}

void kpsExecFile(const String& path, std::map<String, String>& vars) {
    String script = sdReadFile(path);
    if (script.isEmpty()) return;
    kpsExec(script, vars);
}

void kpsExecFile(const String& path) {
    std::map<String, String> vars;
    kpsExecFile(path, vars);
}
