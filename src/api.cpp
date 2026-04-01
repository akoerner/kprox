#include "api.h"
#include "hid.h"
#include "led.h"
#include "storage.h"
#include "registers.h"
#include "connection.h"
#include "token_parser.h"
#include "mtls.h"
#include "crypto_utils.h"
#include "keymap.h"
#include "credential_store.h"
#include "scheduled_tasks.h"
#include "totp.h"
#include "sd_utils.h"
#include "kps_parser.h"
#include <inttypes.h>
#ifdef BOARD_M5STACK_CARDPUTER
#include "cardputer/ui_manager.h"
#ifdef BOARD_M5STACK_CARDPUTER
#include <SD.h>
#endif
#endif

String currentNonce = "";


// ---- Utilities ----

static void addCorsHeaders() {
    server.sendHeader("Access-Control-Allow-Origin",   "*");
    server.sendHeader("Access-Control-Allow-Methods",  "GET, POST, PUT, DELETE, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers",  "Content-Type, Accept, Cache-Control, X-Auth, X-Encrypted");
    server.sendHeader("Access-Control-Expose-Headers", "X-Encrypted");
    server.sendHeader("Access-Control-Max-Age",        "86400");
}


static void sendEncrypted(int code, const String& json) {
    String enc = encryptResponse(json);
    if (enc.isEmpty()) {
        server.send(500, "application/json", "{\"error\":\"Encryption failed\"}");
        return;
    }
    server.sendHeader("X-Encrypted", "1");
    server.send(code, "text/plain", enc);
}

static bool checkApiKey() {
    if (!server.hasHeader("X-Auth")) {
        server.send(401, "application/json", "{\"error\":\"Missing X-Auth header\"}");
        server.client().stop();
        return false;
    }
    if (currentNonce.isEmpty() || !verifyHMAC(currentNonce, server.header("X-Auth"))) {
        currentNonce = generateNonce();  // rotate even on failure to prevent probing
        server.send(401, "application/json", "{\"error\":\"Invalid auth\"}");
        server.client().stop();
        return false;
    }
    currentNonce = generateNonce();  // single-use: rotate after successful auth
    return true;
}

static bool canProceed() {
    if (requestInProgress) {
        server.send(429, "application/json", "{\"error\":\"Request in progress, please wait\"}");
        server.client().stop();
        return false;
    }
    requestInProgress = true;
    return true;
}

static void requestComplete() {
    requestInProgress = false;
}

static bool parseJsonBody(JsonDocument& doc) {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"Missing body\"}");
        server.client().stop();
        return false;
    }

    String body = server.arg("plain");

    if (server.hasHeader("X-Encrypted") && server.header("X-Encrypted") == "1") {
        body = decryptRequest(body);
        if (body.isEmpty()) {
            server.send(400, "application/json", "{\"error\":\"Request decryption failed\"}");
            server.client().stop();
            return false;
        }
    }

    if (deserializeJson(doc, body)) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        server.client().stop();
        return false;
    }
    return true;
}

// ---- CORS / not-found ----

void handleOptions() {
    addCorsHeaders();
    server.send(200, "text/plain", "");
    server.client().stop();
}

static constexpr const char* SINK_FILE = "/sink.txt";

void handleFileRead(String path) {
    addCorsHeaders();
    if (path.endsWith("/")) path += "index.html";

    // Block internal data files from being served directly
    if (path.equalsIgnoreCase(SINK_FILE) ||
        path.equalsIgnoreCase("/sink.txt") ||
        path.equalsIgnoreCase("/kprox.kdbx")) {
        server.send(404, "text/plain", "404: Not Found");
        server.client().stop();
        return;
    }

    String ct = "text/plain";
    if      (path.endsWith(".html")) ct = "text/html";
    else if (path.endsWith(".css"))  ct = "text/css";
    else if (path.endsWith(".js"))   ct = "application/javascript";
    else if (path.endsWith(".png"))  ct = "image/png";
    else if (path.endsWith(".jpg"))  ct = "image/jpeg";
    else if (path.endsWith(".ico"))  ct = "image/x-icon";
    else if (path.endsWith(".json")) ct = "application/json";

    if (SPIFFS.exists(path)) {
        // Static assets: long cache for immutable files, no-cache for HTML
        if (path.endsWith(".html")) {
            server.sendHeader("Cache-Control", "no-cache, must-revalidate");
        } else {
            server.sendHeader("Cache-Control", "public, max-age=86400");
        }
        File file = SPIFFS.open(path, "r");
        server.streamFile(file, ct);
        file.close();
        server.client().stop();
    }
}

void handleDocs() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "public, max-age=3600");
    if (SPIFFS.exists("/TOKEN_REFERENCE.md")) {
        File f = SPIFFS.open("/TOKEN_REFERENCE.md", "r");
        if (f) { server.streamFile(f, "text/markdown; charset=utf-8"); f.close(); return; }
    }
    server.send(404, "text/plain", "TOKEN_REFERENCE.md not found on device");
    server.client().stop();
}

void handleApiRef() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "public, max-age=3600");
    if (SPIFFS.exists("/API_REFERENCE.md")) {
        File f = SPIFFS.open("/API_REFERENCE.md", "r");
        if (f) { server.streamFile(f, "text/markdown; charset=utf-8"); f.close(); return; }
    }
    server.send(404, "text/plain", "API_REFERENCE.md not found on device");
    server.client().stop();
}

void handleNotFound() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    String path = server.uri();
    if (path == "/api/status") { handleApiStatus(); return; }
    // Never serve internal data files via the static file fallback
    if (path.equalsIgnoreCase(SINK_FILE) ||
        path.equalsIgnoreCase("/sink.txt") ||
        path.equalsIgnoreCase("/kprox.kdbx")) {
        server.send(404, "text/plain", "404: Not Found");
        server.client().stop();
        return;
    }
    if (SPIFFS.exists(path.endsWith("/") ? path + "index.html" : path)) {
        handleFileRead(path);
        return;
    }
    server.send(404, "text/plain", "404: Not Found");
    server.client().stop();
}


static size_t sinkSize() {
    if (!SPIFFS.exists(SINK_FILE)) return 0;
    File f = SPIFFS.open(SINK_FILE, "r");
    if (!f) return 0;
    size_t sz = f.size();
    f.close();
    return sz;
}

static bool sinkAppend(const String& data) {
    // Enforce max sink size
    size_t cur = sinkSize();
    if (maxSinkSize > 0 && cur + data.length() > (size_t)maxSinkSize) return false;
    File f = SPIFFS.open(SINK_FILE, "a");
    if (!f) return false;
    f.print(data);
    f.close();
    return true;
}

// ---- Status ----

void handleApiStatus() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    bool bleConn = isBLEConnected();
    bool usbConn = isUSBConnected();

    JsonDocument doc;
    doc["connected"] = hasAnyConnection();

    JsonObject connections = doc["connections"].to<JsonObject>();
    JsonObject btObj = connections["bluetooth"].to<JsonObject>();
    btObj["enabled"]         = bluetoothEnabled;
    btObj["initialized"]     = bluetoothInitialized;
    btObj["connected"]       = bleConn;
    btObj["keyboard_enabled"]      = bleKeyboardEnabled;
    btObj["mouse_enabled"]         = bleMouseEnabled;
    btObj["intl_keyboard_enabled"] = bleIntlKeyboardEnabled;

    JsonObject wifiObj = connections["wifi"].to<JsonObject>();
    wifiObj["ssid"]      = wifiSSID;
    wifiObj["connected"] = (WiFi.status() == WL_CONNECTED);
    wifiObj["rssi"]      = WiFi.RSSI();

    JsonObject usbObj = connections["usb"].to<JsonObject>();
#ifdef BOARD_HAS_USB_HID
    usbObj["supported"]        = true;
    usbObj["enabled"]          = usbEnabled;
    usbObj["initialized"]      = usbInitialized;
    usbObj["connected"]        = usbConn;
    usbObj["keyboard_ready"]   = usbKeyboardReady;
    usbObj["mouse_ready"]      = usbMouseReady;
    usbObj["keyboard_enabled"]      = usbKeyboardEnabled;
    usbObj["mouse_enabled"]         = usbMouseEnabled;
    usbObj["intl_keyboard_enabled"] = usbIntlKeyboardEnabled;
    usbObj["fido2_enabled"]         = fido2Enabled;
#else
    usbObj["supported"] = false; usbObj["enabled"] = false;
    usbObj["initialized"] = false; usbObj["connected"] = false;
    usbObj["keyboard_ready"] = false; usbObj["mouse_ready"] = false;
    usbObj["keyboard_enabled"] = false; usbObj["mouse_enabled"] = false;
    usbObj["intl_keyboard_enabled"] = false;
    usbObj["fido2_enabled"] = false;
#endif

    doc["request_in_progress"] = requestInProgress;
    doc["halted"]              = isHalted;
    doc["active_register"]     = activeRegister;
    doc["total_registers"]     = (int)registers.size();
    doc["looping"]             = isLooping;
    doc["looping_register"]    = loopingRegister;
    doc["free_heap"]           = ESP.getFreeHeap();
    doc["min_free_heap"]       = ESP.getMinFreeHeap();
    doc["max_alloc_heap"]      = ESP.getMaxAllocHeap();
    doc["total_heap"]          = ESP.getHeapSize();
    doc["psram_found"]         = psramFound();
    doc["psram_size"]          = ESP.getPsramSize();
    doc["psram_free"]          = ESP.getFreePsram();
    doc["psram_min_free"]      = ESP.getMinFreePsram();
    doc["sketch_size"]         = ESP.getSketchSize();
    doc["free_sketch_space"]   = ESP.getFreeSketchSpace();
    doc["spiffs_total"]        = SPIFFS.totalBytes();
    doc["spiffs_used"]         = SPIFFS.usedBytes();
    doc["sd_available"]        = sdAvailable();
#ifdef BOARD_M5STACK_CARDPUTER
    if (sdAvailable()) {
        doc["sd_total"]        = SD.totalBytes();
        doc["sd_used"]         = SD.usedBytes();
    }
#endif
    doc["uptime"]              = millis();
    doc["bleDeviceName"]       = deviceName;
    doc["ip"]                  = WiFi.localIP().toString();
    doc["hostname"]            = hostname;
    doc["mdnsEnabled"]         = mdnsEnabled;
    doc["localUrl"]            = String(mtlsEnabled ? "https://" : "http://") + hostname + ".local" + (mtlsEnabled ? ":443" : "");
    doc["mtlsEnabled"]         = mtlsEnabled;
    doc["activeKeymap"]        = keymapActive();
    doc["ledEnabled"]          = ledEnabled;
    doc["credStoreLocked"]     = credStoreLocked;
    doc["credStoreCount"]      = credStoreCount();
    doc["ntp_synced"]          = totpTimeReady();
    doc["sinkSize"]            = sinkSize();
    doc["sinkMaxSize"]         = maxSinkSize;
    doc["ledColor"]["r"]       = ledColorR;
    doc["ledColor"]["g"]       = ledColorG;
    doc["ledColor"]["b"]       = ledColorB;
    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);
    const char* chipModelStr = "ESP32";
    switch (chipInfo.model) {
        case CHIP_ESP32:   chipModelStr = "ESP32";   break;
        case CHIP_ESP32S2: chipModelStr = "ESP32-S2"; break;
        case CHIP_ESP32S3: chipModelStr = "ESP32-S3"; break;
        case CHIP_ESP32C3: chipModelStr = "ESP32-C3"; break;
        case CHIP_ESP32H2: chipModelStr = "ESP32-H2"; break;
        default:           chipModelStr = "ESP32";   break;
    }
    doc["chipModel"]           = chipModelStr;
    doc["chipRevision"]        = chipInfo.revision;
    doc["chipCores"]           = chipInfo.cores;
    doc["cpuFreq"]             = ESP.getCpuFreqMHz();
    doc["flashSize"]           = ESP.getFlashChipSize();
    doc["mac"]                 = WiFi.macAddress();
#ifdef BOARD_M5STACK_CARDPUTER
    doc["boardType"] = "M5Stack Cardputer Adv";
#elif defined(BOARD_HAS_USB_HID)
    doc["boardType"] = "M5Stack ATOM S3";
#else
    doc["boardType"] = "M5Stack ATOM Lite";
#endif

    doc["bootReg"]["enabled"]    = bootRegEnabled;
    doc["bootReg"]["index"]      = bootRegIndex;
    doc["bootReg"]["limit"]      = bootRegLimit;
    doc["bootReg"]["firedCount"] = bootRegFiredCount;

    String response;
    serializeJson(doc, response);
    sendEncrypted(200, response);
    server.client().stop();
}

// ---- Registers ----

void handleRegisters() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;


    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["activeRegister"] = activeRegister;
        JsonArray arr = doc["registers"].to<JsonArray>();
        for (int i = 0; i < (int)registers.size(); i++) {
            JsonObject o = arr.add<JsonObject>();
            o["number"]  = i;
            o["content"] = registers[i];
            o["name"]    = (i < (int)registerNames.size()) ? registerNames[i] : "";
        }
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        if (doc.containsKey("activeRegister")) {
            int n = doc["activeRegister"].as<int>();
            if (n >= 0 && (size_t)n < registers.size()) {
                activeRegister = n;
                saveActiveRegister();
                blinkLED(activeRegister + 1, LED_COLOR_REG_CHANGE);
            }
        }

        if (doc.containsKey("action")) {
            String action = doc["action"].as<String>();
            if (action == "add") {
                addRegister(
                    doc.containsKey("content") ? doc["content"].as<String>() : "",
                    doc.containsKey("name")    ? doc["name"].as<String>()    : ""
                );
            } else if (action == "delete" && doc.containsKey("register")) {
                deleteRegister(doc["register"].as<int>());
            } else if (action == "deleteAll") {
                deleteAllRegisters();
            } else if (action == "reorder" && doc["order"].is<JsonArray>()) {
                std::vector<String> nr, nn;
                for (JsonVariant idx : doc["order"].as<JsonArray>()) {
                    int i = idx.as<int>();
                    if (i >= 0 && (size_t)i < registers.size()) {
                        nr.push_back(registers[i]);
                        nn.push_back((i < (int)registerNames.size()) ? registerNames[i] : "");
                    }
                }
                registers = nr; registerNames = nn; activeRegister = 0;
                saveRegisters();
            } else if (action == "setName" && doc.containsKey("register") && doc.containsKey("name")) {
                int rn = doc["register"].as<int>();
                if (rn >= 0 && (size_t)rn < registers.size()) {
                    while ((size_t)rn >= registerNames.size()) registerNames.push_back("");
                    saveRegisterName(rn, doc["name"].as<String>());
                }
            }
        }

        if (doc["registers"].is<JsonArray>()) {
            JsonArray arr = doc["registers"].as<JsonArray>();
            bool isTargeted = true;
            for (JsonVariant v : arr) {
                if (!v.is<JsonObject>() || !v.as<JsonObject>().containsKey("number")) { isTargeted = false; break; }
            }

            if (isTargeted) {
                for (JsonVariant v : arr) {
                    JsonObject o = v.as<JsonObject>();
                    int num = o["number"].as<int>();
                    if (num < 0 || (size_t)num >= registers.size()) continue;
                    if (o.containsKey("content")) { registers[num] = o["content"].as<String>(); saveRegister(num, registers[num]); }
                    if (o.containsKey("name"))    {
                        while ((size_t)num >= registerNames.size()) registerNames.push_back("");
                        saveRegisterName(num, o["name"].as<String>());
                    }
                }
            } else {
                registers.clear(); registerNames.clear(); activeRegister = 0;
                for (JsonVariant v : arr) {
                    if (v.is<JsonObject>()) {
                        JsonObject o = v.as<JsonObject>();
                        registers.push_back(o.containsKey("content") ? o["content"].as<String>() : "");
                        registerNames.push_back(o.containsKey("name") ? o["name"].as<String>() : "");
                    } else if (v.is<String>()) {
                        registers.push_back(v.as<String>()); registerNames.push_back("");
                    }
                }
                saveRegisters();
                if (doc.containsKey("activeRegister")) {
                    int n = doc["activeRegister"].as<int>();
                    if (n >= 0 && (size_t)n < registers.size()) { activeRegister = n; saveActiveRegister(); }
                }
            }
        }

        sendEncrypted(200, "{\"status\":\"ok\"}");
        requestComplete();

    } else if (server.method() == HTTP_DELETE) {
        if (!canProceed()) return;
        deleteAllRegisters();
        sendEncrypted(200, "{\"status\":\"ok\"}");
        requestComplete();
    }

    server.client().stop();
}

// ---- Send ----

void handleSendText() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;
    if (!canProceed()) return;

    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Use POST with JSON body");
        server.client().stop(); requestComplete(); return;
    }

    JsonDocument doc;
    if (!parseJsonBody(doc)) { requestComplete(); return; }

    if (doc["text"].is<const char*>()) pendingTokenStrings.push_back(doc["text"].as<String>());

    sendEncrypted(200, "{\"status\":\"ok\"}");
    server.client().stop();
    requestComplete();
}

void handleSendMouse() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;
    if (!canProceed()) return;

    JsonDocument doc;
    if (!parseJsonBody(doc)) { requestComplete(); return; }

    if (doc.containsKey("x") || doc.containsKey("y")) {
        int dx = doc["x"] | 0, dy = doc["y"] | 0;
        if (dx || dy) sendMouseMovement(dx, dy);
    }

    if (doc.containsKey("action")) {
        String action = doc["action"].as<String>();
        int button = doc["button"] | MOUSE_LEFT;
        if      (action == "click")   sendMouseClick(button);
        else if (action == "double")  sendMouseDoubleClick(button);
        else if (action == "press")   sendMousePress(button);
        else if (action == "release") sendMouseRelease(button);
    }

    sendEncrypted(200, "{\"status\":\"ok\"}");
    server.client().stop();
    requestComplete();
}

void handleSendMouseWebSocket(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type != WStype_TEXT) return;

    JsonDocument doc;
    deserializeJson(doc, payload, length);

    if (doc.containsKey("x") || doc.containsKey("y")) {
        int dx = doc["x"] | 0, dy = doc["y"] | 0;
        if (dx || dy) sendMouseMovement(dx, dy);
    }

    if (doc.containsKey("action")) {
        String action = doc["action"].as<String>();
        int button = doc["button"] | MOUSE_LEFT;
        if      (action == "click")   sendMouseClick(button);
        else if (action == "double")  sendMouseDoubleClick(button);
        else if (action == "press")   sendMousePress(button);
        else if (action == "release") sendMouseRelease(button);
    }
}

// ---- Sink ----

// POST /api/sink — no HMAC required; accepts:
//   • raw unstructured body (any Content-Type)
//   • JSON {"text":"..."} body
//   • encrypted body (X-Encrypted: 1) containing either of the above
// GET /api/sink — requires HMAC; returns size + preview
void handleSink() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");

    if (server.method() == HTTP_GET) {
        if (!checkApiKey()) return;
        if (!canProceed()) return;
        size_t sz = sinkSize();
        String preview = "";
        if (sz > 0) {
            File f = SPIFFS.open(SINK_FILE, "r");
            if (f) { preview = f.readString().substring(0, 120); f.close(); }
        }
        JsonDocument doc;
        doc["size"]    = sz;
        doc["preview"] = preview;
        doc["max_size"] = maxSinkSize;
        String out; serializeJson(doc, out);
        sendEncrypted(200, out);
        server.client().stop(); requestComplete(); return;
    }

    if (server.method() != HTTP_POST) {
        server.send(405, "application/json", "{\"error\":\"Use GET or POST\"}");
        server.client().stop(); return;
    }

    // POST: no HMAC check — sink is designed as a write-only accumulator
    if (!canProceed()) return;

    String body = server.hasArg("plain") ? server.arg("plain") : "";

    // Attempt decryption if flagged (falls through to raw if decryption fails)
    if (!body.isEmpty() && server.hasHeader("X-Encrypted") && server.header("X-Encrypted") == "1") {
        String dec = decryptRequest(body);
        if (!dec.isEmpty()) body = dec;
    }

    if (body.isEmpty()) {
        server.send(400, "application/json", "{\"error\":\"Empty body\"}");
        server.client().stop(); requestComplete(); return;
    }

    // Try to parse as JSON {"text":"..."} — fall back to raw string if not JSON
    String toAppend = body;
    JsonDocument jdoc;
    if (!deserializeJson(jdoc, body) && jdoc["text"].is<const char*>()) {
        toAppend = jdoc["text"].as<String>();
    }

    if (!sinkAppend(toAppend)) {
        server.send(413, "application/json", "{\"error\":\"Sink buffer full\"}");
        server.client().stop(); requestComplete(); return;
    }

    JsonDocument resp;
    resp["status"] = "ok";
    resp["size"]   = sinkSize();
    String out; serializeJson(resp, out);
    server.send(200, "application/json", out);
    server.client().stop();
    requestComplete();
}

// POST /api/flush — HMAC required; flushes sink to HID and clears it
void handleFlush() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;
    if (!canProceed()) return;

    String content = "";
    if (SPIFFS.exists(SINK_FILE)) {
        File f = SPIFFS.open(SINK_FILE, "r");
        if (f) { content = f.readString(); f.close(); }
        SPIFFS.remove(SINK_FILE);
    }

    if (!content.isEmpty()) pendingTokenStrings.push_back(content);

    JsonDocument doc;
    doc["status"]  = "ok";
    doc["flushed"] = content.length();
    String out; serializeJson(doc, out);
    sendEncrypted(200, out);
    server.client().stop();
    requestComplete();
}

// GET /api/sink_size — HMAC required; returns just the byte count
void handleSinkSize() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;
    if (!canProceed()) return;
    JsonDocument doc;
    doc["size"]     = sinkSize();
    doc["max_size"] = maxSinkSize;
    String out; serializeJson(doc, out);
    sendEncrypted(200, out);
    server.client().stop();
    requestComplete();
}

// POST /api/sink_delete — HMAC required; deletes sink without flushing
void handleSinkDelete() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;
    if (!canProceed()) return;
    size_t was = sinkSize();
    if (SPIFFS.exists(SINK_FILE)) SPIFFS.remove(SINK_FILE);
    JsonDocument doc;
    doc["status"]  = "ok";
    doc["deleted"] = was;
    String out; serializeJson(doc, out);
    sendEncrypted(200, out);
    server.client().stop();
    requestComplete();
}

// ---- Bluetooth ----

void handleBluetooth() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;


    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["enabled"]     = bluetoothEnabled;
        doc["initialized"] = bluetoothInitialized;
        doc["connected"]   = isBLEConnected();
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }
        if (doc.containsKey("enabled")) {
            doc["enabled"].as<bool>() ? enableBluetooth() : disableBluetooth();
        }
        sendEncrypted(200, "{\"status\":\"ok\"}");
        requestComplete();
    }

    server.client().stop();
}

// ---- USB ----

#ifdef BOARD_HAS_USB_HID
void handleUSB() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;


    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["enabled"]       = usbEnabled;
        doc["initialized"]   = usbInitialized;
        doc["connected"]     = isUSBConnected();
        doc["keyboardReady"] = usbKeyboardReady;
        doc["mouseReady"]    = usbMouseReady;
        doc["manufacturer"]  = usbManufacturer;
        doc["product"]       = usbProduct;
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }
        if (doc.containsKey("enabled"))      doc["enabled"].as<bool>() ? enableUSB() : disableUSB();
        if (doc.containsKey("manufacturer")) { usbManufacturer = doc["manufacturer"].as<String>(); saveUSBIdentitySettings(); }
        if (doc.containsKey("product"))      { usbProduct      = doc["product"].as<String>();      saveUSBIdentitySettings(); }
        sendEncrypted(200, "{\"status\":\"ok\"}");
        requestComplete();
    }

    server.client().stop();
}
#else
void handleUSB() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    server.send(501, "application/json", "{\"error\":\"USB HID not supported on this board\"}");
    server.client().stop();
}
#endif

// ---- Device identity ----

void handleDevice() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;


    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["manufacturer"] = usbManufacturer;
        doc["product"]      = usbProduct;
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        bool changed = false;
        if (doc.containsKey("manufacturer") && doc["manufacturer"].as<String>() != usbManufacturer) { usbManufacturer = doc["manufacturer"].as<String>(); changed = true; }
        if (doc.containsKey("product")      && doc["product"].as<String>()      != usbProduct)      { usbProduct      = doc["product"].as<String>();      changed = true; }

        if (changed) {
            saveUSBIdentitySettings();
#ifdef BOARD_HAS_USB_HID
            if (usbEnabled) { USB.manufacturerName(usbManufacturer.c_str()); USB.productName(usbProduct.c_str()); }
#endif
        }

        sendEncrypted(200, "{\"status\":\"ok\"}");
        requestComplete();
    }

    server.client().stop();
}

// ---- LED ----

void handleLED() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;


    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["enabled"]    = ledEnabled;
        doc["color"]["r"] = ledColorR;
        doc["color"]["g"] = ledColorG;
        doc["color"]["b"] = ledColorB;
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        if (doc.containsKey("enabled")) ledEnabled = doc["enabled"].as<bool>();
        if (doc["color"].is<JsonObject>()) {
            JsonObject c = doc["color"];
            if (c.containsKey("r")) ledColorR = c["r"].as<uint8_t>();
            if (c.containsKey("g")) ledColorG = c["g"].as<uint8_t>();
            if (c.containsKey("b")) ledColorB = c["b"].as<uint8_t>();
        }
        saveLEDSettings();
        if (ledEnabled) {
            setLED(ledColorR, ledColorG, ledColorB, 200);
            setLED(ledColorR, ledColorG, ledColorB, 200);
            setLED(ledColorR, ledColorG, ledColorB, 200);
        }
        sendEncrypted(200, "{\"status\":\"ok\"}");
        requestComplete();
    }

    server.client().stop();
}

// ---- WiFi ----

void handleWiFi() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;


    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["ssid"]      = wifiSSID;
        doc["password"]  = "********";
        doc["connected"] = (WiFi.status() == WL_CONNECTED);
        doc["ip"]        = WiFi.localIP().toString();
        doc["rssi"]      = WiFi.RSSI();
        doc["hostname"]  = WiFi.getHostname();
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        if (doc.containsKey("ssid") && doc.containsKey("password")) {
            bool ok = connectToNewWiFi(doc["ssid"].as<String>(), doc["password"].as<String>());
            JsonDocument resp;
            resp["status"]  = ok ? "connected" : "failed";
            resp["message"] = ok ? "WiFi connection successful" : "WiFi connection failed, reverted to previous settings";
            String respStr; serializeJson(resp, respStr);
            sendEncrypted(200, respStr);
        } else {
            server.send(400, "application/json", "{\"error\":\"Missing ssid or password\"}");
        }
        requestComplete();
    }

    server.client().stop();
}

// ---- Settings (aggregate) ----

void handleSettings() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;


    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["bluetooth"]["enabled"]  = bluetoothEnabled;
        doc["led"]["enabled"]        = ledEnabled;
        doc["led"]["color"]["r"]     = ledColorR;
        doc["led"]["color"]["g"]     = ledColorG;
        doc["led"]["color"]["b"]     = ledColorB;
        doc["wifi"]["ssid"]          = wifiSSID;
        doc["wifi"]["password"]      = "********";
        doc["utcOffset"]             = utcOffsetSeconds;
        doc["device"]["manufacturer"] = usbManufacturer;
        doc["device"]["product"]     = usbProduct;
        doc["device"]["hostname"]    = hostnameStr;
        doc["device"]["usb_serial"]  = usbSerialNumber;
        doc["defaultApp"]            = defaultAppIndex;
        doc["maxSinkSize"]           = maxSinkSize;
        doc["bootReg"]["enabled"]    = bootRegEnabled;
        doc["bootReg"]["index"]      = bootRegIndex;
        doc["bootReg"]["limit"]      = bootRegLimit;
        doc["bootReg"]["firedCount"] = bootRegFiredCount;
        doc["cs"]["autoLockSecs"]    = csAutoLockSecs;
        doc["cs"]["autoWipeAttempts"]= csAutoWipeAttempts;
        doc["cs"]["failedAttempts"]  = csGetFailedAttempts();
        doc["cs"]["storageLocation"] = csStorageLocation;
        doc["cs"]["sdAvailable"]     = sdAvailable();
        doc["csAutoLockSecs"]        = csAutoLockSecs;
        doc["csAutoWipeAttempts"]    = csAutoWipeAttempts;
        doc["csFailedAttempts"]      = csGetFailedAttempts();
        // App layout
        JsonArray orderArr  = doc["appOrder"].to<JsonArray>();
        JsonArray hiddenArr = doc["appHidden"].to<JsonArray>();
        for (size_t i = 0; i < appOrder.size(); i++) {
            orderArr.add(appOrder[i]);
            hiddenArr.add((i < appHidden.size()) ? appHidden[i] : false);
        }
        // Dynamic app name list (launcher=0 excluded, indices 1-based)
        JsonArray namesArr = doc["appNames"].to<JsonArray>();
#ifdef BOARD_M5STACK_CARDPUTER
        {
            const auto& regApps = Cardputer::uiManager.apps();
            for (size_t i = 1; i < regApps.size(); i++)
                namesArr.add(regApps[i]->appName());
        }
#endif
        doc["timing"]["key_press_delay"]         = g_keyPressDelay;
        doc["timing"]["key_release_delay"]       = g_keyReleaseDelay;
        doc["timing"]["between_keys_delay"]      = g_betweenKeysDelay;
        doc["timing"]["between_send_text_delay"] = g_betweenSendTextDelay;
        doc["timing"]["special_key_delay"]       = g_specialKeyDelay;
        doc["timing"]["token_delay"]             = g_tokenDelay;
#ifdef BOARD_HAS_USB_HID
        doc["usb"]["enabled"] = usbEnabled;
#endif
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        if (doc["bootReg"].is<JsonObject>()) {
            JsonObject br = doc["bootReg"];
            bool changed = false;
            if (br.containsKey("enabled"))    { bootRegEnabled    = br["enabled"].as<bool>();  changed = true; }
            if (br.containsKey("index"))      { bootRegIndex      = max(0, br["index"].as<int>()); changed = true; }
            if (br.containsKey("limit"))      { bootRegLimit      = max(0, br["limit"].as<int>()); changed = true; }
            if (br.containsKey("firedCount")) { bootRegFiredCount = max(0, br["firedCount"].as<int>()); changed = true; }
            if (changed) saveBootRegSettings();
        }

        if (doc.containsKey("utcOffset")) {
            utcOffsetSeconds = doc["utcOffset"].as<long>();
            saveUtcOffsetSettings();
            configTime(utcOffsetSeconds, 0, "pool.ntp.org", "time.nist.gov");
        }

        if (doc.containsKey("api_key")) {
            String nk = doc["api_key"].as<String>();
            if (nk.length() >= 8) { apiKey = nk; saveApiKeySettings(); }
        }

        if (doc["bluetooth"].is<JsonObject>() && doc["bluetooth"].containsKey("enabled")) {
            bool en = doc["bluetooth"]["enabled"].as<bool>();
            if (en != bluetoothEnabled) {
                bluetoothEnabled = en;
                saveBtSettings();
                if (bluetoothEnabled && !bluetoothInitialized && BLE_KEYBOARD_VALID) { BLE_KEYBOARD.begin(); BLE_MOUSE.begin(); bluetoothInitialized = true; }
                if (ledEnabled) setLED(bluetoothEnabled ? LED_COLOR_BT_ENABLE : LED_COLOR_BT_DISABLE, 500);
            }
        }
        if (doc["bluetooth"].is<JsonObject>()) {
            JsonObject bt = doc["bluetooth"];
            bool changed = false;
            if (bt.containsKey("keyboard_enabled"))      { bleKeyboardEnabled      = bt["keyboard_enabled"].as<bool>();      changed = true; }
            if (bt.containsKey("mouse_enabled"))          { bleMouseEnabled          = bt["mouse_enabled"].as<bool>();          changed = true; }
            if (bt.containsKey("intl_keyboard_enabled")) { bleIntlKeyboardEnabled   = bt["intl_keyboard_enabled"].as<bool>(); changed = true; }
            if (changed) saveBtSettings();
        }

        if (doc["led"].is<JsonObject>()) {
            JsonObject led = doc["led"];
            if (led.containsKey("enabled")) ledEnabled = led["enabled"].as<bool>();
            if (led["color"].is<JsonObject>()) {
                JsonObject c = led["color"];
                if (c.containsKey("r")) ledColorR = c["r"].as<uint8_t>();
                if (c.containsKey("g")) ledColorG = c["g"].as<uint8_t>();
                if (c.containsKey("b")) ledColorB = c["b"].as<uint8_t>();
            }
            saveLEDSettings();
            if (ledEnabled) setLED(ledColorR, ledColorG, ledColorB, 200);
        }

        if (doc["wifi"].is<JsonObject>()) {
            JsonObject wifi = doc["wifi"];
            if (wifi.containsKey("ssid") && wifi.containsKey("password"))
                connectToNewWiFi(wifi["ssid"].as<String>(), wifi["password"].as<String>());
        }

        if (doc["device"].is<JsonObject>()) {
            JsonObject device = doc["device"];
            bool changed = false;
            if (device.containsKey("manufacturer") && device["manufacturer"].as<String>() != usbManufacturer) { usbManufacturer = device["manufacturer"].as<String>(); changed = true; }
            if (device.containsKey("product")      && device["product"].as<String>()      != usbProduct)      { usbProduct      = device["product"].as<String>();      changed = true; }
            if (changed) {
                saveUSBIdentitySettings();
#ifdef BOARD_HAS_USB_HID
                if (usbEnabled) { USB.manufacturerName(usbManufacturer.c_str()); USB.productName(usbProduct.c_str()); }
#endif
            }
            if (device.containsKey("hostname") && device["hostname"].as<String>().length() > 0) {
                hostnameStr = device["hostname"].as<String>();
                hostname = hostnameStr.c_str();
                saveHostnameSettings();
                WiFi.setHostname(hostname);
            }
            if (device.containsKey("usb_serial") && device["usb_serial"].as<String>().length() > 0) {
                usbSerialNumber = device["usb_serial"].as<String>();
                saveHostnameSettings();
            }
        }

        if (doc["timing"].is<JsonObject>()) {
            JsonObject t = doc["timing"];
            bool changed = false;
            if (t.containsKey("key_press_delay"))         { g_keyPressDelay        = max(0, t["key_press_delay"].as<int>());         changed = true; }
            if (t.containsKey("key_release_delay"))       { g_keyReleaseDelay      = max(0, t["key_release_delay"].as<int>());       changed = true; }
            if (t.containsKey("between_keys_delay"))      { g_betweenKeysDelay     = max(0, t["between_keys_delay"].as<int>());      changed = true; }
            if (t.containsKey("between_send_text_delay")) { g_betweenSendTextDelay = max(0, t["between_send_text_delay"].as<int>()); changed = true; }
            if (t.containsKey("special_key_delay"))       { g_specialKeyDelay      = max(0, t["special_key_delay"].as<int>());       changed = true; }
            if (t.containsKey("token_delay"))             { g_tokenDelay           = max(0, t["token_delay"].as<int>());             changed = true; }
            if (changed) saveTimingSettings();
        }

        if (doc.containsKey("defaultApp")) {
            int da = doc["defaultApp"].as<int>();
            if (da >= 1) { defaultAppIndex = da; saveDefaultAppSettings(); }
        }

        if (doc.containsKey("maxSinkSize")) {
            int ms = doc["maxSinkSize"].as<int>();
            if (ms >= 0) { maxSinkSize = ms; saveSinkSettings(); }
        }

        if (doc["cs"].is<JsonObject>()) {
            JsonObject cs = doc["cs"];
            // resetFailedAttempts also requires store to be unlocked
            if (!credStoreLocked) {
                if (cs.containsKey("resetFailedAttempts") && cs["resetFailedAttempts"].as<bool>()) {
                    csResetFailedAttempts();
                }
                bool changed = false;
                if (cs.containsKey("autoLockSecs"))    { csAutoLockSecs     = max(0, cs["autoLockSecs"].as<int>());     changed = true; }
                if (cs.containsKey("autoWipeAttempts")) { csAutoWipeAttempts = max(0, cs["autoWipeAttempts"].as<int>()); changed = true; }
                if (changed) saveCsSecuritySettings();
            }
        }

        if (doc.containsKey("csAutoLockSecs")) {
            int v = doc["csAutoLockSecs"].as<int>();
            if (v >= 0) { csAutoLockSecs = v; saveCsSecuritySettings(); }
        }

        if (doc.containsKey("csAutoWipeAttempts")) {
            int v = doc["csAutoWipeAttempts"].as<int>();
            if (v >= 0) { csAutoWipeAttempts = v; saveCsSecuritySettings(); }
        }

        if (doc.containsKey("csResetFailedAttempts") && doc["csResetFailedAttempts"].as<bool>()) {
            csResetFailedAttempts();
        }

        if (doc["appOrder"].is<JsonArray>() && doc["appHidden"].is<JsonArray>()) {
            JsonArray orderArr  = doc["appOrder"].as<JsonArray>();
            JsonArray hiddenArr = doc["appHidden"].as<JsonArray>();
            appOrder.clear();
            appHidden.clear();
            int idx = 0;
            for (JsonVariant v : orderArr) {
                int val = v.as<int>();
                appOrder.push_back(val);
                bool h = (idx < (int)hiddenArr.size()) ? hiddenArr[idx].as<bool>() : false;
                appHidden.push_back(h);
                idx++;
            }
            // Settings app (last) is never hidden
#ifdef BOARD_M5STACK_CARDPUTER
            int numApps = (int)Cardputer::uiManager.apps().size() - 1;
#else
            int numApps = (int)appOrder.size();
#endif
            for (size_t i = 0; i < appOrder.size(); i++) {
                if (appOrder[i] == numApps) appHidden[i] = false;
            }
            saveAppLayout();
        }

#ifdef BOARD_HAS_USB_HID
        if (doc["usb"].is<JsonObject>() && doc["usb"].containsKey("enabled")) {
            bool en = doc["usb"]["enabled"].as<bool>();
            if (en != usbEnabled) {
                usbEnabled = en;
                saveUSBSettings();
                if (usbEnabled && !usbInitialized) {
                    USB.manufacturerName(usbManufacturer.c_str());
                    USB.productName(usbProduct.c_str());
                    USB.serialNumber(usbSerialNumber.c_str());
                    if (fido2Enabled) FIDO2Device.begin();
                    USB.begin();
                    if (usbKeyboardEnabled) { USBKeyboard.begin(); usbKeyboardReady = true; }
                    if (usbMouseEnabled)    { USBMouse.begin();    usbMouseReady    = true; }
                    KProxConsumer.begin();
                    usbInitialized = true;
                }
                if (ledEnabled) setLED(usbEnabled ? LED_COLOR_USB_ENABLE : LED_COLOR_USB_DISABLE, 500);
            }
        }
        if (doc["usb"].is<JsonObject>()) {
            JsonObject usb = doc["usb"];
            bool changed = false;
            if (usb.containsKey("keyboard_enabled")) {
                usbKeyboardEnabled = usb["keyboard_enabled"].as<bool>();
                usbKeyboardReady   = usbEnabled && usbInitialized && usbKeyboardEnabled;
                changed = true;
            }
            if (usb.containsKey("mouse_enabled")) {
                usbMouseEnabled = usb["mouse_enabled"].as<bool>();
                usbMouseReady   = usbEnabled && usbInitialized && usbMouseEnabled;
                changed = true;
            }
            if (usb.containsKey("intl_keyboard_enabled")) {
                usbIntlKeyboardEnabled = usb["intl_keyboard_enabled"].as<bool>();
                changed = true;
            }
            if (usb.containsKey("fido2_enabled")) {
                fido2Enabled = usb["fido2_enabled"].as<bool>();
                changed = true;
            }
            if (changed) saveUSBSettings();
        }
#endif

        sendEncrypted(200, "{\"status\":\"ok\"}");
        requestComplete();

    } else if (server.method() == HTTP_DELETE) {
        if (!canProceed()) return;
        wipeAllSettings();
        JsonDocument resp; resp["status"] = "success"; resp["message"] = "All settings deleted";
        String rs; serializeJson(resp, rs);
        sendEncrypted(200, rs);
        if (ledEnabled) blinkLED(10, LED_COLOR_REG_DELETE_ALL);
        requestComplete();
    }

    server.client().stop();
}

// ---- Wipe ----

void handleWipeSettings() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    if (server.method() == HTTP_POST) {
        wipeAllSettings();
        JsonDocument resp; resp["status"] = "success"; resp["message"] = "All settings wiped. Device restart required.";
        String rs; serializeJson(resp, rs);
        sendEncrypted(200, rs);
        if (ledEnabled) blinkLED(10, LED_COLOR_REG_DELETE_ALL);
    }
    server.client().stop();
}

void handleWipeEverything() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    if (server.method() == HTTP_POST) {
        wipeAllSettings();
        registers.clear(); registerNames.clear(); activeRegister = 0;
        saveRegisters();
        JsonDocument resp; resp["status"] = "success"; resp["message"] = "Everything wiped. Device restart required.";
        String rs; serializeJson(resp, rs);
        sendEncrypted(200, rs);
        if (ledEnabled) blinkLED(15, LED_COLOR_REG_DELETE_ALL);
    }
    server.client().stop();
}

// ---- Export / Import ----

void handleRegistersExport() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;
    server.sendHeader("Content-Disposition", "attachment; filename=\"kprox_registers.json\"");

    JsonDocument doc;
    doc["activeRegister"] = activeRegister;
    JsonArray arr = doc["registers"].to<JsonArray>();
    for (int i = 0; i < (int)registers.size(); i++) {
        JsonObject o = arr.add<JsonObject>();
        o["number"]  = i;
        o["content"] = registers[i];
        o["name"]    = (i < (int)registerNames.size()) ? registerNames[i] : "";
    }
    String resp; serializeJson(doc, resp);
    sendEncrypted(200, resp);
    server.client().stop();
}

void handleRegistersImport() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    if (!canProceed()) return;

    JsonDocument doc;
    if (!parseJsonBody(doc)) { requestComplete(); return; }

    if (!doc["registers"].is<JsonArray>()) {
        server.send(400, "application/json", "{\"error\":\"Missing registers array\"}");
        server.client().stop(); requestComplete(); return;
    }

    registers.clear(); registerNames.clear(); activeRegister = 0;

    for (JsonVariant v : doc["registers"].as<JsonArray>()) {
        if (v.is<JsonObject>()) {
            JsonObject o = v.as<JsonObject>();
            registers.push_back(o.containsKey("content") ? o["content"].as<String>() : "");
            registerNames.push_back(o.containsKey("name") ? o["name"].as<String>() : "");
        }
    }

    if (doc.containsKey("activeRegister")) {
        int n = doc["activeRegister"].as<int>();
        if (n >= 0 && (size_t)n < registers.size()) activeRegister = n;
    }

    saveRegisters();
    sendEncrypted(200, "{\"status\":\"ok\"}");
    requestComplete();
    server.client().stop();
}

// ---- OTA ----

// Auth state tracked across the multipart upload boundary
static bool _otaFlashAuthed  = false;
static bool _otaSpiffsAuthed = false;

void handleOTAUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        // Auth must have been checked by handleOTAComplete before this fires
        // on the same connection. If not yet set, abort.
        if (!_otaFlashAuthed) { Update.abort(); return; }
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!_otaFlashAuthed) return;
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
        feedWatchdog();
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!_otaFlashAuthed) return;
        if (!Update.end(true)) Update.printError(Serial);
    }
}

void handleOTAComplete() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) { _otaFlashAuthed = false; return; }
    _otaFlashAuthed = true;
    if (Update.hasError()) {
        JsonDocument doc; doc["error"] = Update.errorString();
        String resp; serializeJson(doc, resp);
        server.send(500, "application/json", resp);
    } else {
        server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Firmware OTA complete, restarting\"}");
        server.client().stop();
        delay(200);
        ESP.restart();
    }
    server.client().stop();
}

void handleSPIFFSUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!_otaSpiffsAuthed) { Update.abort(); return; }
        Serial.printf("[SPIFFS OTA] Starting: %s\n", upload.filename.c_str());
        if (SPIFFS.begin(false)) SPIFFS.end();
        size_t fsSize = UPDATE_SIZE_UNKNOWN;
        if (server.hasHeader("Content-Length")) {
            fsSize = server.header("Content-Length").toInt();
        }
        if (!Update.begin(fsSize, U_SPIFFS)) {
            Serial.printf("[SPIFFS OTA] begin failed: %s\n", Update.errorString());
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!_otaSpiffsAuthed) return;
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
        feedWatchdog();
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!_otaSpiffsAuthed) return;
        Serial.printf("[SPIFFS OTA] End, size=%u, success=%d\n", upload.totalSize, Update.end(true));
    }
}

void handleSPIFFSComplete() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) { _otaSpiffsAuthed = false; return; }
    _otaSpiffsAuthed = true;
    if (Update.hasError()) {
        JsonDocument doc; doc["error"] = Update.errorString();
        String resp; serializeJson(doc, resp);
        server.send(500, "application/json", resp);
    } else {
        server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"SPIFFS OTA complete, restarting\"}");
        server.client().stop();
        delay(200);
        ESP.restart();
    }
    server.client().stop();
}

// ---- Discovery / Network ----

void handleApiDiscovery() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    if (server.method() != HTTP_GET) { server.send(405, "text/plain", "Method not allowed"); server.client().stop(); return; }

    JsonDocument doc;
    doc["protocol_version"] = "1.0";
    doc["device_type"]      = "KProx_HID_Controller";
    doc["device_id"]        = WiFi.macAddress();
    doc["device_name"]      = String(BLUETOOTH_DEVICE_NAME);
    doc["hostname"]         = String(hostname);
    doc["ip"]               = WiFi.localIP().toString();
    doc["gateway"]          = WiFi.gatewayIP().toString();
    doc["subnet_mask"]      = WiFi.subnetMask().toString();

    JsonObject services = doc["services"].to<JsonObject>();
    services["http"] = 80;

    JsonObject caps = doc["capabilities"].to<JsonObject>();
    caps["hid"] = true; caps["keyboard"] = true; caps["mouse"] = true;
    caps["bluetooth"] = bluetoothEnabled;
#ifdef BOARD_HAS_USB_HID
    caps["usb"] = usbEnabled;
#else
    caps["usb"] = false;
#endif

    JsonObject status = doc["status"].to<JsonObject>();
    status["bluetooth_connected"] = isBLEConnected();
#ifdef BOARD_HAS_USB_HID
    status["usb_connected"] = isUSBConnected();
#endif
    status["wifi_rssi"]       = WiFi.RSSI();
    status["free_heap"]       = ESP.getFreeHeap();
    status["uptime"]          = millis() / 1000;
    status["active_register"] = activeRegister;
    status["total_registers"] = registers.size();
    status["is_halted"]       = isHalted;

    String resp; serializeJson(doc, resp);
    sendEncrypted(200, resp);
    server.client().stop();
}

void handleApiNetwork() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    if (server.method() != HTTP_GET) { server.send(405, "text/plain", "Method not allowed"); server.client().stop(); return; }

    JsonDocument doc;
    doc["wifi_connected"]   = WiFi.isConnected();
    doc["wifi_ssid"]        = WiFi.SSID();
    doc["wifi_rssi"]        = WiFi.RSSI();
    doc["ip_address"]       = WiFi.localIP().toString();
    doc["gateway"]          = WiFi.gatewayIP().toString();
    doc["subnet_mask"]      = WiFi.subnetMask().toString();
    doc["dns1"]             = WiFi.dnsIP(0).toString();
    doc["dns2"]             = WiFi.dnsIP(1).toString();
    doc["mac_address"]      = WiFi.macAddress();
    doc["hostname"]         = WiFi.getHostname();
    doc["broadcast_address"] = IPAddress(~(uint32_t)WiFi.subnetMask() | (uint32_t)WiFi.localIP()).toString();
    doc["mdns_enabled"]     = mdnsEnabled;

    String resp; serializeJson(doc, resp);
    sendEncrypted(200, resp);
    server.client().stop();
}


// ---- mTLS / Security ----

void handleMTLS() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;


    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["enabled"]         = mtlsEnabled;
        doc["has_server_cert"] = !serverCert.isEmpty();
        doc["has_server_key"]  = !serverKey.isEmpty();
        doc["has_ca_cert"]     = !caCert.isEmpty();
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;

        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        if (doc.containsKey("enabled")) {
            mtlsEnabled = doc["enabled"].as<bool>();
            saveMTLSSettings();
        }
        JsonDocument resp;
        resp["status"]  = "ok";
        resp["message"] = "mTLS settings saved. Restart required to apply.";
        String rs; serializeJson(resp, rs);
        sendEncrypted(200, rs);
        requestComplete();
    }

    server.client().stop();
}

void handleMTLSCerts() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    if (server.method() == HTTP_POST) {
        if (!canProceed()) return;

        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        String cert = doc.containsKey("server_cert") ? doc["server_cert"].as<String>() : "";
        String key  = doc.containsKey("server_key")  ? doc["server_key"].as<String>()  : "";
        String ca   = doc.containsKey("ca_cert")      ? doc["ca_cert"].as<String>()     : "";

        if (cert.isEmpty() && key.isEmpty() && ca.isEmpty()) {
            server.send(400, "application/json", "{\"error\":\"No certificate data provided\"}");
            server.client().stop(); requestComplete(); return;
        }

        saveMTLSCerts(cert, key, ca);
        JsonDocument resp;
        resp["status"]  = "ok";
        resp["message"] = "Certificates saved. Restart required to apply.";
        String rs; serializeJson(resp, rs);
        sendEncrypted(200, rs);
        requestComplete();

    } else if (server.method() == HTTP_DELETE) {
        if (!canProceed()) return;
        saveMTLSCerts("", "", "");
        mtlsEnabled = false;
        saveMTLSSettings();
        server.send(200, "application/json",
            "{\"status\":\"ok\",\"message\":\"Certificates cleared. Restart required.\"}");
        requestComplete();
    }

    server.client().stop();
}

void handleNonce() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    currentNonce = generateNonce();
    JsonDocument doc;
    doc["nonce"] = currentNonce;
    String resp; serializeJson(doc, resp);
    server.send(200, "application/json", resp);
    server.client().stop();
}

// ---- Keymap ----

void handleKeymap() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    if (server.method() == HTTP_GET) {
        // ?id=xx  — return the raw JSON content of a specific keymap
        if (server.hasArg("id")) {
            String id = server.arg("id");
            id.toLowerCase(); id.trim();
            if (id == "en") {
                // built-in English has no stored file; return empty map placeholder
                sendEncrypted(200, "{\"id\":\"en\",\"name\":\"English (built-in)\",\"map\":[]}");
            } else {
                String path = "/keymaps/" + id + ".json";
                if (SPIFFS.exists(path)) {
                    File f = SPIFFS.open(path, "r");
                    if (f) {
                        String body = f.readString();
                        f.close();
                        sendEncrypted(200, body);
                    } else {
                        server.send(500, "application/json", "{\"error\":\"read failed\"}");
                    }
                } else {
                    server.send(404, "application/json", "{\"error\":\"not found\"}");
                }
            }
            server.client().stop();
            return;
        }
        JsonDocument doc;
        doc["active"] = keymapActive();
        JsonArray arr = doc["available"].to<JsonArray>();
        for (const String& id : keymapListAvailable()) arr.add(id);
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }
        if (doc["keymap"].is<String>()) {
            String id = doc["keymap"].as<String>();
            id.toLowerCase();
            if (keymapLoad(id)) {
                keymapSaveActive();
                sendEncrypted(200, "{\"status\":\"ok\"}");
            } else {
                server.send(404, "application/json", "{\"error\":\"Keymap not found\"}");
            }
        } else {
            server.send(400, "application/json", "{\"error\":\"Missing keymap field\"}");
        }
        requestComplete();

    } else if (server.method() == HTTP_PUT) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }
        String id   = doc["id"].as<String>();
        String json = doc["json"].as<String>();
        id.toLowerCase(); id.trim();
        if (id.isEmpty() || id == "en") {
            server.send(400, "application/json", "{\"error\":\"Invalid keymap id\"}");
            requestComplete(); return;
        }
        if (json.isEmpty()) {
            server.send(400, "application/json", "{\"error\":\"Missing json field\"}");
            requestComplete(); return;
        }
        if (keymapUpload(id, json)) {
            sendEncrypted(200, "{\"status\":\"ok\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"Invalid keymap JSON or write failed\"}");
        }
        requestComplete();

    } else if (server.method() == HTTP_DELETE) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }
        String id = doc["keymap"].as<String>();
        id.toLowerCase();
        if (keymapDelete(id)) {
            sendEncrypted(200, "{\"status\":\"ok\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"Cannot delete keymap (built-in or not found)\"}");
        }
        requestComplete();
    }

    server.client().stop();
}

// ---- Credential Store ----

void handleCredStore() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["locked"] = credStoreLocked;
        doc["count"]  = credStoreCount();
        {
            bool nvsHasDb = ([]{ preferences.begin("kprox_db", true);
                                  bool h = preferences.getInt("cs_db_n", 0) > 0;
                                  preferences.end(); return h; })();
            bool sdHasDb  = sdExists();
            bool hasDb    = (csStorageLocation == "sd") ? sdHasDb : nvsHasDb;
            doc["has_db"]           = hasDb;
            doc["nvs_has_db"]       = nvsHasDb;
            doc["sd_has_db"]        = sdHasDb;
            doc["storage_location"] = csStorageLocation;
            doc["sd_available"]     = sdAvailable();
        }
        doc["failed_attempts"]  = csGetFailedAttempts();
        doc["auto_lock_secs"]   = csAutoLockSecs;
        doc["auto_wipe_at"]     = csAutoWipeAttempts;
        if (!credStoreLocked && csAutoLockSecs > 0) {
            unsigned long elapsed = (millis() - credStoreLastActivity) / 1000UL;
            doc["lock_in_secs"] = (int)max(0L, (long)csAutoLockSecs - (long)elapsed);
        } else {
            doc["lock_in_secs"] = -1;
        }
        if (!credStoreLocked) {
            JsonArray arr = doc["credentials"].to<JsonArray>();
            for (auto& lbl : credStoreListLabels()) {
                JsonObject entry = arr.add<JsonObject>();
                entry["label"]        = lbl;
                entry["has_password"] = !credStoreGet(lbl, CredField::PASSWORD).isEmpty();
                entry["has_username"] = !credStoreGet(lbl, CredField::USERNAME).isEmpty();
                entry["has_notes"]    = !credStoreGet(lbl, CredField::NOTES).isEmpty();
            }
            // Keep legacy "labels" array for backwards compat
            JsonArray lblArr = doc["labels"].to<JsonArray>();
            for (auto& lbl : credStoreListLabels()) lblArr.add(lbl);
        }
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        String action = doc["action"] | "";

        if (action == "unlock") {
            CSGateMode gate  = csGateGetMode();
            String key       = doc["key"]       | "";
            String totpCode  = doc["totp_code"] | "";

            // TOTP-only: key field is the totp code itself (backward compat)
            if (gate == CSGateMode::TOTP_ONLY) {
                // accept code from either field
                if (totpCode.isEmpty()) totpCode = key;
                if (totpCode.length() < 6) {
                    sendEncrypted(400, "{\"error\":\"6-digit TOTP code required\"}");
                    server.client().stop(); requestComplete(); return;
                }
                if (credStoreUnlockWithTOTP("", totpCode)) {
                    sendEncrypted(200, "{\"status\":\"ok\",\"locked\":false}");
                } else {
                    sendEncrypted(401, "{\"error\":\"Invalid TOTP code\"}");
                }
            } else {
                int minKeyLen = (gate == CSGateMode::TOTP) ? 4 : 8;
                if ((int)key.length() < minKeyLen) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "{\"error\":\"Key must be at least %d characters\"}", minKeyLen);
                    server.send(400, "application/json", buf);
                    server.client().stop(); requestComplete(); return;
                }
                if (gate == CSGateMode::TOTP && totpCode.length() < 6) {
                    sendEncrypted(400, "{\"error\":\"6-digit TOTP code required\"}");
                    server.client().stop(); requestComplete(); return;
                }
                if (credStoreUnlockWithTOTP(key, totpCode)) {
                    sendEncrypted(200, "{\"status\":\"ok\",\"locked\":false}");
                } else {
                    sendEncrypted(401, "{\"error\":\"Invalid credentials\"}");
                }
            }

        } else if (action == "lock") {
            credStoreLock();
            sendEncrypted(200, "{\"status\":\"ok\",\"locked\":true}");

        } else if (action == "get") {
            if (credStoreLocked) {
                sendEncrypted(403, "{\"error\":\"Store is locked\"}");
            } else {
                String label     = doc["label"] | "";
                String fieldStr  = doc["field"] | "password";
                fieldStr.toLowerCase();
                CredField field  = fieldStr == "username" ? CredField::USERNAME
                                 : fieldStr == "notes"    ? CredField::NOTES
                                 : CredField::PASSWORD;
                String value = credStoreGet(label, field);
                JsonDocument r;
                r["label"] = label;
                r["field"] = fieldStr;
                r["found"] = credStoreLabelExists(label);
                r["value"] = value;
                String resp; serializeJson(r, resp);
                sendEncrypted(200, resp);
            }

        } else if (action == "set") {
            if (credStoreLocked) {
                sendEncrypted(403, "{\"error\":\"Store is locked\"}");
            } else {
                String label    = doc["label"] | "";
                String value    = doc["value"] | "";
                String fieldStr = doc["field"] | "password";
                fieldStr.toLowerCase();
                CredField field = fieldStr == "username" ? CredField::USERNAME
                                : fieldStr == "notes"    ? CredField::NOTES
                                : CredField::PASSWORD;
                if (label.isEmpty()) {
                    server.send(400, "application/json", "{\"error\":\"Missing label\"}");
                    server.client().stop(); requestComplete(); return;
                }
                credStoreSet(label, value, field);
                sendEncrypted(200, "{\"status\":\"ok\"}");
            }

        } else if (action == "delete") {
            if (credStoreLocked) {
                sendEncrypted(403, "{\"error\":\"Store is locked\"}");
            } else {
                String label = doc["label"] | "";
                credStoreDelete(label);
                sendEncrypted(200, "{\"status\":\"ok\"}");
            }

        } else if (action == "wipe") {
            credStoreWipe();
            sendEncrypted(200, "{\"status\":\"ok\"}");

        } else if (action == "set_storage_location") {
            String loc = doc["location"] | "nvs";
            if (loc != "sd") loc = "nvs";
            if (loc != csStorageLocation) {
                if (loc == "sd" && !sdAvailable()) {
                    sendEncrypted(503, "{\"error\":\"SD card not available\"}");
                    server.client().stop(); requestComplete(); return;
                }
                if (!credStoreLocked) {
                    String oldLoc = csStorageLocation;
                    csStorageLocation = loc;
                    if (!writeKDBX(credStoreRuntimeKey)) {
                        csStorageLocation = oldLoc;
                        sendEncrypted(500, "{\"error\":\"Migration write failed\"}");
                        server.client().stop(); requestComplete(); return;
                    }
                    if (oldLoc == "sd") sdRemove();
                    else { preferences.begin("kprox_db", false); preferences.clear(); preferences.end(); }
                } else {
                    // Locked — pointer-only move. Warn if destination already has data
                    // (the web app confirms before calling, but enforce server-side too
                    //  unless the client explicitly passes force:true).
                    bool force = doc["force"] | false;
                    if (!force) {
                        bool destHasDb = (loc == "sd")
                            ? sdExists()
                            : ([]{ preferences.begin("kprox_db", true);
                                   bool h = preferences.getInt("cs_db_n", 0) > 0;
                                   preferences.end(); return h; })();
                        if (destHasDb) {
                            sendEncrypted(409, "{\"error\":\"Destination already has a database\",\"code\":\"dest_has_db\"}");
                            server.client().stop(); requestComplete(); return;
                        }
                    }
                    csStorageLocation = loc;
                }
                saveCsStorageLocation();
            }
            sendEncrypted(200, "{\"status\":\"ok\"}");

        } else if (action == "format_sd") {
            if (!sdAvailable()) {
                sendEncrypted(503, "{\"error\":\"SD card not available\"}");
                server.client().stop(); requestComplete(); return;
            }
            if (!sdFormat()) {
                sendEncrypted(500, "{\"error\":\"SD format failed\"}");
                server.client().stop(); requestComplete(); return;
            }
            if (csStorageLocation == "sd") credStoreLock();
            sendEncrypted(200, "{\"status\":\"ok\"}");

        } else {
            server.send(400, "application/json", "{\"error\":\"Unknown action\"}");
        }

        requestComplete();

    } else {
        server.send(405, "text/plain", "Method not allowed");
    }

    server.client().stop();
}

void handleCredStoreKey() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;
    if (!canProceed()) return;

    JsonDocument doc;
    if (!parseJsonBody(doc)) { requestComplete(); return; }

    String oldKey = doc["old_key"] | "";
    String newKey = doc["new_key"] | "";

    if (oldKey.isEmpty() || newKey.isEmpty()) {
        server.send(400, "application/json", "{\"error\":\"Missing old_key or new_key\"}");
        server.client().stop(); requestComplete(); return;
    }

    int minKeyLen = (csGateGetMode() == CSGateMode::TOTP) ? 4 : 8;
    if ((int)newKey.length() < minKeyLen) {
        char buf[80];
        snprintf(buf, sizeof(buf), "{\"error\":\"new_key must be at least %d characters\"}", minKeyLen);
        server.send(400, "application/json", buf);
        server.client().stop(); requestComplete(); return;
    }

    if (credStoreRekey(oldKey, newKey)) {
        sendEncrypted(200, "{\"status\":\"ok\"}");
    } else {
        sendEncrypted(401, "{\"error\":\"Invalid current key\"}");
    }

    requestComplete();
    server.client().stop();
}

// ---- Scheduled Tasks ----

void handleSchedTasks() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;

    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        JsonArray arr = doc["tasks"].to<JsonArray>();
        for (const ScheduledTask& t : scheduledTasks) {
            JsonObject o = arr.add<JsonObject>();
            o["id"]      = t.id;
            o["label"]   = t.label;
            o["year"]    = t.year;
            o["month"]   = t.month;
            o["day"]     = t.day;
            o["hour"]    = t.hour;
            o["minute"]  = t.minute;
            o["second"]  = t.second;
            o["payload"] = t.payload;
            o["enabled"] = t.enabled;
            o["repeat"]  = t.repeat;
        }
        doc["count"] = (int)scheduledTasks.size();
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);
        server.client().stop(); return;
    }

    if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        // toggle enable: { "id": N, "enabled": bool }
        if (doc.containsKey("id") && doc.containsKey("enabled")) {
            int id = doc["id"].as<int>();
            for (ScheduledTask& t : scheduledTasks) {
                if (t.id == id) { t.enabled = doc["enabled"].as<bool>(); saveScheduledTasks(); break; }
            }
            sendEncrypted(200, "{\"status\":\"ok\"}");
            requestComplete(); return;
        }

        // Add new task
        ScheduledTask t;
        t.label   = doc["label"]   | String("");
        t.year    = doc["year"]    | 0;
        t.month   = doc["month"]   | 0;
        t.day     = doc["day"]     | 0;
        t.hour    = doc["hour"]    | 0;
        t.minute  = doc["minute"]  | 0;
        t.second  = doc["second"]  | 0;
        t.payload = doc["payload"] | String("");
        t.enabled = doc["enabled"] | true;
        t.repeat  = doc["repeat"]  | false;
        t.fired   = false;

        if (t.payload.isEmpty()) {
            sendEncrypted(400, "{\"error\":\"payload required\"}");
            requestComplete(); return;
        }

        int newId = addScheduledTask(t);
        JsonDocument resp;
        resp["status"] = "ok";
        resp["id"]     = newId;
        String rs; serializeJson(resp, rs);
        sendEncrypted(200, rs);
        requestComplete(); return;
    }

    if (server.method() == HTTP_DELETE) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }
        int id = doc["id"] | -1;
        if (id < 0) {
            sendEncrypted(400, "{\"error\":\"id required\"}");
            requestComplete(); return;
        }
        bool ok = deleteScheduledTask(id);
        sendEncrypted(ok ? 200 : 404, ok ? "{\"status\":\"ok\"}" : "{\"error\":\"not found\"}");
        requestComplete(); return;
    }

    server.send(405, "text/plain", "Method not allowed");
    server.client().stop();
}

// ---- TOTP ----

void handleTOTP() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");

    if (server.method() == HTTP_OPTIONS) {
        server.send(200, "text/plain", "");
        server.client().stop();
        return;
    }

    if (!checkApiKey()) return;

    if (server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["gate_mode"]  = (int)csGateGetMode();
        doc["time_ready"] = totpTimeReady();
        doc["time_epoch"] = (long)time(nullptr);
        doc["cs_locked"]  = credStoreLocked;

        JsonArray arr = doc["accounts"].to<JsonArray>();
        if (!credStoreLocked) {
            for (auto& a : totpListAccounts()) {
                JsonObject obj = arr.add<JsonObject>();
                obj["name"]   = a.name;
                obj["digits"] = a.digits;
                obj["period"] = a.period;
                if (totpTimeReady()) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%06" PRId32,
                             (int32_t)totpCompute(a.secret, time(nullptr), a.period, a.digits));
                    obj["code"] = buf;
                    obj["seconds_remaining"] = totpSecondsRemaining(time(nullptr), a.period);
                }
            }
        }
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);
        server.client().stop(); requestComplete(); return;
    }

    if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        String action = doc["action"] | "";

        if (action == "add") {
            if (credStoreLocked) {
                sendEncrypted(403, "{\"error\":\"Unlock the credential store first — TOTP secrets are encrypted with it\"}");
                server.client().stop(); requestComplete(); return;
            }
            TOTPAccount a;
            a.name   = doc["name"]   | "";
            a.secret = doc["secret"] | "";
            a.digits = doc["digits"] | 6;
            a.period = doc["period"] | 30;
            if (a.name.isEmpty() || a.secret.isEmpty()) {
                server.send(400, "application/json", "{\"error\":\"name and secret required\"}");
                server.client().stop(); requestComplete(); return;
            }
            if (totpAddAccount(a)) {
                sendEncrypted(200, "{\"status\":\"ok\"}");
            } else {
                sendEncrypted(400, "{\"error\":\"Invalid secret (Base32, min 16 chars)\"}");
            }

        } else if (action == "delete") {
            if (credStoreLocked) {
                sendEncrypted(403, "{\"error\":\"Unlock the credential store first — TOTP accounts are encrypted with it\"}");
                server.client().stop(); requestComplete(); return;
            }
            String name = doc["name"] | "";
            totpDeleteAccount(name);
            sendEncrypted(200, "{\"status\":\"ok\"}");

        } else if (action == "set_gate") {
            int mode = doc["gate_mode"] | 0;
            if (mode < 0 || mode > 2) {
                server.send(400, "application/json", "{\"error\":\"gate_mode must be 0,1,2\"}");
                server.client().stop(); requestComplete(); return;
            }
            if (mode > 0) {
                String secret = doc["gate_secret"] | "";
                if (!secret.isEmpty()) csGateSetSecret(secret);
                if (csGateGetSecret().isEmpty()) {
                    server.send(400, "application/json", "{\"error\":\"gate_secret required\"}");
                    server.client().stop(); requestComplete(); return;
                }
            }

            CSGateMode prevMode = csGateGetMode();
            String gateSecret   = csGateGetSecret();
            bool switchingToTotpOnly   = (mode == (int)CSGateMode::TOTP_ONLY) && (prevMode != CSGateMode::TOTP_ONLY);
            bool switchingFromTotpOnly = (prevMode == CSGateMode::TOTP_ONLY) && (mode != (int)CSGateMode::TOTP_ONLY);

            if (switchingToTotpOnly) {
                // Must rekey all credentials from current symmetric key to the gate secret.
                // Store must be unlocked to do this.
                if (credStoreLocked || credStoreRuntimeKey.isEmpty()) {
                    server.send(409, "application/json",
                        "{\"error\":\"Unlock the credential store before switching to TOTP-only mode\"}");
                    server.client().stop(); requestComplete(); return;
                }
                if (!gateSecret.isEmpty() && !credStoreRekey(credStoreRuntimeKey, gateSecret)) {
                    server.send(500, "application/json", "{\"error\":\"Failed to rekey credentials\"}");
                    server.client().stop(); requestComplete(); return;
                }
            } else if (switchingFromTotpOnly) {
                String newKey = doc["new_key"] | "";
                // Target mode is key-only or key+TOTP
                int minKeyLen = (mode == (int)CSGateMode::TOTP) ? 4 : 8;
                if ((int)newKey.length() < minKeyLen) {
                    char errBuf[80];
                    snprintf(errBuf, sizeof(errBuf),
                        "{\"error\":\"new_key (min %d chars) required when leaving TOTP-only mode\"}", minKeyLen);
                    server.send(400, "application/json", errBuf);
                    server.client().stop(); requestComplete(); return;
                }
                if (credStoreLocked || credStoreRuntimeKey.isEmpty()) {
                    server.send(409, "application/json",
                        "{\"error\":\"Unlock the credential store before changing gate mode\"}");
                    server.client().stop(); requestComplete(); return;
                }
                if (!credStoreRekey(credStoreRuntimeKey, newKey)) {
                    server.send(500, "application/json", "{\"error\":\"Failed to rekey credentials\"}");
                    server.client().stop(); requestComplete(); return;
                }
            }

            csGateSetMode((CSGateMode)mode);
            sendEncrypted(200, "{\"status\":\"ok\"}");

        } else if (action == "set_cs_key") {
            String newKey = doc["new_key"] | "";
            int minKeyLen = (csGateGetMode() == CSGateMode::TOTP) ? 4 : 8;
            if ((int)newKey.length() < minKeyLen) {
                char errBuf[80];
                snprintf(errBuf, sizeof(errBuf), "{\"error\":\"new_key must be at least %d characters\"}", minKeyLen);
                server.send(400, "application/json", errBuf);
                server.client().stop(); requestComplete(); return;
            }
            if (!credStoreLocked && !credStoreRuntimeKey.isEmpty()) {
                if (credStoreRekey(credStoreRuntimeKey, newKey)) {
                    sendEncrypted(200, "{\"status\":\"ok\",\"note\":\"credentials rekeyed\"}");
                } else {
                    sendEncrypted(500, "{\"error\":\"Rekey failed\"}");
                }
            } else {
                // Store locked — clear keycheck; new key will be the initialisation key
                preferences.begin("kprox_cs", false);
                preferences.putString("cs_kc", "");
                preferences.end();
                sendEncrypted(200, "{\"status\":\"ok\",\"note\":\"keycheck cleared — new key takes effect on next unlock\"}");
            }

        } else if (action == "wipe") {
            totpWipe();
            sendEncrypted(200, "{\"status\":\"ok\"}");

        } else {
            server.send(400, "application/json", "{\"error\":\"Unknown action\"}");
        }
        server.client().stop();
        requestComplete(); return;
    }

    server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    server.client().stop();
}

// ---- Route setup ----

void handleKpsRef() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "public, max-age=3600");
    // Prefer SD card copy so users can update without reflashing
    if (sdAvailable()) {
        String md = sdReadFile("/KEYPROX_SCRIPT_REFERENCE.md");
        if (!md.isEmpty()) {
            server.send(200, "text/markdown; charset=utf-8", md);
            server.client().stop(); return;
        }
    }
    if (SPIFFS.exists("/KEYPROX_SCRIPT_REFERENCE.md")) {
        File f = SPIFFS.open("/KEYPROX_SCRIPT_REFERENCE.md", "r");
        if (f) { server.streamFile(f, "text/markdown; charset=utf-8"); f.close(); return; }
    }
    // Fallback: inline a minimal stub so the UI isn't empty first flash
    server.send(200, "text/markdown; charset=utf-8",
        "# KProx Script Reference\n\nFlash the SPIFFS image (run `pio run -t buildfs -t uploadfs`) to load the full reference, or place KEYPROX_SCRIPT_REFERENCE.md on the SD card.");
    server.client().stop();
}

void handleSdApi() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (server.method() == HTTP_OPTIONS) { server.send(200); server.client().stop(); return; }
    if (!checkApiKey()) return;

    if (!sdAvailable()) {
        server.send(503, "application/json", "{\"error\":\"SD card not available\"}");
        server.client().stop(); return;
    }

    if (server.method() == HTTP_GET) {
        String path   = server.arg("path");
        String action = server.arg("action");
        if (path.isEmpty()) path = "/";

        if (action == "read") {
            String content = sdReadFile(path);
            JsonDocument doc;
            doc["path"]    = path;
            doc["content"] = content;
            doc["size"]    = content.length();
            String resp; serializeJson(doc, resp);
            sendEncrypted(200, resp);
        } else {
            // List directory
            String listing = sdListDir(path);
            server.send(200, "application/json", listing);
        }
        server.client().stop(); return;
    }

    if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }
        String action  = doc["action"] | "";
        String path    = doc["path"]   | "";
        String content = doc["content"]| "";

        if (path.isEmpty()) {
            server.send(400, "application/json", "{\"error\":\"path required\"}");
            server.client().stop(); requestComplete(); return;
        }

        if (action == "write") {
            bool ok = sdWriteFile(path, content);
            sendEncrypted(ok ? 200 : 500, ok ? "{\"status\":\"ok\"}" : "{\"error\":\"write failed\"}");
        } else if (action == "append") {
            bool ok = sdAppendFile(path, content);
            sendEncrypted(ok ? 200 : 500, ok ? "{\"status\":\"ok\"}" : "{\"error\":\"append failed\"}");
        } else if (action == "delete") {
            bool ok = sdDeleteFile(path);
            sendEncrypted(ok ? 200 : 500, ok ? "{\"status\":\"ok\"}" : "{\"error\":\"delete failed\"}");
        } else if (action == "mkdir") {
            bool ok = sdMkdir(path);
            sendEncrypted(ok ? 200 : 500, ok ? "{\"status\":\"ok\"}" : "{\"error\":\"mkdir failed\"}");
        } else if (action == "rename") {
            String newPath = doc["new_path"] | "";
            if (newPath.isEmpty()) {
                server.send(400, "application/json", "{\"error\":\"new_path required\"}");
                server.client().stop(); requestComplete(); return;
            }
            String src = sdReadFile(path);
            bool ok = sdWriteFile(newPath, src) && sdDeleteFile(path);
            sendEncrypted(ok ? 200 : 500, ok ? "{\"status\":\"ok\"}" : "{\"error\":\"rename failed\"}");
        } else if (action == "exec") {
            kpsExecFile(path);
            sendEncrypted(200, "{\"status\":\"ok\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"unknown action\"}");
        }
        server.client().stop(); requestComplete(); return;
    }
    server.send(405, "text/plain", "Method not allowed");
    server.client().stop();
}

void setupRoutes() {
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        if (SPIFFS.exists("/index.html")) {
            handleFileRead("/index.html");
        } else {
            server.send(404, "text/plain", "index.html not found");
            server.client().stop();
        }
    });

    server.on("/api/docs",               HTTP_GET,     handleDocs);
    server.on("/api/kpsref",             HTTP_GET,     handleKpsRef);
    server.on("/api/sd",                 HTTP_GET,     handleSdApi);
    server.on("/api/sd",                 HTTP_POST,    handleSdApi);
    server.on("/api/apiref",             HTTP_GET,     handleApiRef);
    server.on("/api/nonce",              HTTP_GET,     handleNonce);
    server.on("/api/status",             HTTP_GET,     handleApiStatus);
    server.on("/api/settings",           HTTP_GET,     handleSettings);
    server.on("/api/settings",           HTTP_POST,    handleSettings);
    server.on("/api/settings",           HTTP_DELETE,  handleSettings);
    server.on("/api/registers",          HTTP_GET,     handleRegisters);
    server.on("/api/registers",          HTTP_POST,    handleRegisters);
    server.on("/api/registers",          HTTP_DELETE,  handleRegisters);
    server.on("/api/led",                HTTP_GET,     handleLED);
    server.on("/api/led",                HTTP_POST,    handleLED);
    server.on("/api/bluetooth",          HTTP_GET,     handleBluetooth);
    server.on("/api/bluetooth",          HTTP_POST,    handleBluetooth);
    server.on("/api/usb",                HTTP_GET,     handleUSB);
    server.on("/api/usb",                HTTP_POST,    handleUSB);
    server.on("/api/device",             HTTP_GET,     handleDevice);
    server.on("/api/device",             HTTP_POST,    handleDevice);
    server.on("/api/wifi",               HTTP_GET,     handleWiFi);
    server.on("/api/wifi",               HTTP_POST,    handleWiFi);
    server.on("/api/wipe-settings",      HTTP_POST,    handleWipeSettings);
    server.on("/api/wipe-everything",    HTTP_POST,    handleWipeEverything);
    server.on("/api/registers/export",   HTTP_GET,     handleRegistersExport);
    server.on("/api/registers/import",   HTTP_POST,    handleRegistersImport);
    server.on("/api/ota",                HTTP_POST,    handleOTAComplete,    handleOTAUpload);
    server.on("/api/ota/spiffs",          HTTP_POST,    handleSPIFFSComplete, handleSPIFFSUpload);
    server.on("/api/mtls",               HTTP_GET,     handleMTLS);
    server.on("/api/mtls",               HTTP_POST,    handleMTLS);
    server.on("/api/mtls/certs",         HTTP_POST,    handleMTLSCerts);
    server.on("/api/mtls/certs",         HTTP_DELETE,  handleMTLSCerts);
    server.on("/api/discovery",          HTTP_GET,     handleApiDiscovery);
    server.on("/api/network",            HTTP_GET,     handleApiNetwork);
    server.on("/api/keymap",             HTTP_GET,     handleKeymap);
    server.on("/api/keymap",             HTTP_POST,    handleKeymap);
    server.on("/api/keymap",             HTTP_PUT,     handleKeymap);
    server.on("/api/keymap",             HTTP_DELETE,  handleKeymap);
    server.on("/api/credstore",          HTTP_GET,     handleCredStore);
    server.on("/api/credstore",          HTTP_POST,    handleCredStore);
    server.on("/api/credstore/rekey",    HTTP_POST,    handleCredStoreKey);
    server.on("/api/totp",               HTTP_ANY,     handleTOTP);
    server.on("/api/schedtasks",         HTTP_GET,     handleSchedTasks);
    server.on("/api/schedtasks",         HTTP_POST,    handleSchedTasks);
    server.on("/api/schedtasks",         HTTP_DELETE,  handleSchedTasks);
    server.on("/send/text",              HTTP_POST,    handleSendText);
    server.on("/send/mouse",             HTTP_POST,    handleSendMouse);
    server.on("/api/sink",               HTTP_GET,     handleSink);
    server.on("/api/sink",               HTTP_POST,    handleSink);
    server.on("/api/sink_size",          HTTP_GET,     handleSinkSize);
    server.on("/api/flush",              HTTP_POST,    handleFlush);
    server.on("/api/sink_delete",        HTTP_POST,    handleSinkDelete);

    const char* opts[] = {
        "/send/text", "/send/mouse", "/api/sink", "/api/sink_size", "/api/sink_delete", "/api/flush", "/api/status", "/api/settings",
        "/api/registers", "/api/led", "/api/bluetooth", "/api/usb",
        "/api/device", "/api/wifi", "/api/wipe-settings", "/api/wipe-everything",
        "/api/discovery", "/api/network", "/api/registers/export",
        "/api/registers/import", "/api/ota", "/api/ota/spiffs",
        "/api/mtls", "/api/mtls/certs",
        "/api/nonce", "/api/keymap", "/api/docs", "/api/apiref", "/api/kpsref", "/api/sd",
        "/api/credstore", "/api/credstore/rekey",
        "/api/totp",
        "/api/schedtasks"
    };
    for (const char* path : opts) server.on(path, HTTP_OPTIONS, handleOptions);

    server.onNotFound(handleNotFound);

    const char* headerKeys[] = {"X-Auth", "X-Encrypted", "Content-Length"};
    server.collectHeaders(headerKeys, 3);
}
