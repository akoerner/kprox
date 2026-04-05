#include "sd_card.h"

#ifdef BOARD_M5STACK_CARDPUTER
static constexpr int SD_CS_PIN   = 12;
static constexpr int SD_SCK_PIN  = 40;
static constexpr int SD_MOSI_PIN = 14;
static constexpr int SD_MISO_PIN = 39;
#else
static constexpr int SD_CS_PIN   = SS;
static constexpr int SD_SCK_PIN  = SCK;
static constexpr int SD_MOSI_PIN = MOSI;
static constexpr int SD_MISO_PIN = MISO;
#endif

static bool sdMounted = false;

// Called once from setup() after M5Cardputer.begin().
// Uses the global SPI (FSPI/SPI2 on ESP32-S3) — M5Cardputer 1.1.x
// pre-claims SPI3/HSPI, so FSPI must be used and initialised eagerly.
void sdInit() {
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    sdMounted = SD.begin(SD_CS_PIN, SPI, 25000000);
}

bool sdMount() {
    if (sdMounted) return true;
    sdMounted = SD.begin(SD_CS_PIN, SPI, 25000000);
    return sdMounted;
}

void sdUnmount() {
    sdMounted = false;
}

bool sdAvailable() {
    return sdMount();
}


bool sdMkdirP(const String& filePath) {
    if (!sdMount()) return false;
    int last = filePath.lastIndexOf('/');
    if (last <= 0) return true;
    String dir = filePath.substring(0, last);
    if (SD.exists(dir)) return true;
    for (int i = 1; i <= (int)dir.length(); i++) {
        if (i == (int)dir.length() || dir[i] == '/') {
            String part = dir.substring(0, i);
            if (!SD.exists(part) && !SD.mkdir(part)) return false;
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
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0) name = name.substring(lastSlash + 1);
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

String sdLsText(const String& path) {
    if (!sdMount()) return "";
    File dir = SD.open(path.isEmpty() ? "/" : path);
    if (!dir || !dir.isDirectory()) return "";
    String out;
    File entry = dir.openNextFile();
    while (entry) {
        String name = String(entry.name());
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0) name = name.substring(lastSlash + 1);
        if (entry.isDirectory()) name += "/";
        if (!out.isEmpty()) out += "\n";
        out += name;
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
    return out;
}
