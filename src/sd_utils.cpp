#include "sd_utils.h"
#include "credential_store.h"
#include <SD.h>

bool sdMkdirP(const String& filePath) {
    if (!sdMount()) return false;
    // Strip filename — we only need to create the directory portion
    int last = filePath.lastIndexOf('/');
    if (last <= 0) return true; // root or no directory component
    String dir = filePath.substring(0, last);
    if (SD.exists(dir)) return true;

    // Walk forward creating each path component
    for (int i = 1; i <= (int)dir.length(); i++) {
        if (i == (int)dir.length() || dir[i] == '/') {
            String part = dir.substring(0, i);
            if (!SD.exists(part)) {
                if (!SD.mkdir(part)) return false;
            }
        }
    }
    return true;
}

String sdReadFile(const String& path) {
    if (!sdMount()) return "";
    File f = SD.open(path, FILE_READ);
    if (!f) return "";
    String out;
    out.reserve(f.size());
    while (f.available()) out += (char)f.read();
    f.close();
    return out;
}

bool sdWriteFile(const String& path, const String& content) {
    if (!sdMount()) return false;
    if (!sdMkdirP(path)) return false;
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    size_t written = f.print(content);
    f.close();
    return written == (size_t)content.length();
}

bool sdAppendFile(const String& path, const String& content) {
    if (!sdMount()) return false;
    if (!sdMkdirP(path)) return false;
    File f = SD.open(path, FILE_APPEND);
    if (!f) return false;
    size_t written = f.print(content);
    f.close();
    return written == (size_t)content.length();
}

bool sdDeleteFile(const String& path) {
    if (!sdMount()) return false;
    return SD.remove(path);
}

bool sdMkdir(const String& path) {
    if (!sdMount()) return false;
    return SD.mkdir(path);
}

String sdListDir(const String& path) {
    if (!sdMount()) return "[]";
    File dir = SD.open(path.isEmpty() ? "/" : path);
    if (!dir || !dir.isDirectory()) return "[]";

    String json = "[";
    bool first = true;
    File entry = dir.openNextFile();
    while (entry) {
        if (!first) json += ",";
        first = false;
        String name = String(entry.name());
        // entry.name() on ESP32 SD returns the full path; strip to basename
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0) name = name.substring(lastSlash + 1);
        // Escape any quotes in name
        name.replace("\"", "\\\"");
        if (entry.isDirectory()) {
            json += "{\"name\":\"" + name + "\",\"type\":\"dir\"}";
        } else {
            json += "{\"name\":\"" + name + "\",\"type\":\"file\",\"size\":" + String(entry.size()) + "}";
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
    json += "]";
    return json;
}
