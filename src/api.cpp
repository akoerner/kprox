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

void handleFileRead(String path) {
    addCorsHeaders();
    if (path.endsWith("/")) path += "index.html";

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
        if (f) {
            server.streamFile(f, "text/markdown; charset=utf-8");
            f.close();
            return;
        }
    }
    server.send(404, "text/plain", "TOKEN_REFERENCE.md not found on device");
    server.client().stop();
}

void handleNotFound() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    String path = server.uri();
    if (path == "/api/status") { handleApiStatus(); return; }
    if (SPIFFS.exists(path.endsWith("/") ? path + "index.html" : path)) {
        handleFileRead(path);
        return;
    }
    server.send(404, "text/plain", "404: Not Found");
    server.client().stop();
}

static constexpr const char* SINK_FILE = "/sink.txt";

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
    btObj["keyboard_enabled"] = bleKeyboardEnabled;
    btObj["mouse_enabled"]   = bleMouseEnabled;

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
    usbObj["keyboard_enabled"] = usbKeyboardEnabled;
    usbObj["mouse_enabled"]    = usbMouseEnabled;
    usbObj["fido2_enabled"]    = fido2Enabled;
#else
    usbObj["supported"] = false; usbObj["enabled"] = false;
    usbObj["initialized"] = false; usbObj["connected"] = false;
    usbObj["keyboard_ready"] = false; usbObj["mouse_ready"] = false;
    usbObj["keyboard_enabled"] = false; usbObj["mouse_enabled"] = false;
    usbObj["fido2_enabled"] = false;
#endif

    doc["request_in_progress"] = requestInProgress;
    doc["halted"]              = isHalted;
    doc["active_register"]     = activeRegister;
    doc["total_registers"]     = (int)registers.size();
    doc["looping"]             = isLooping;
    doc["looping_register"]    = loopingRegister;
    doc["free_heap"]           = ESP.getFreeHeap();
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
#ifdef BOARD_HAS_USB_HID
    doc["boardType"] = "M5Stack ATOM S3";
#else
    doc["boardType"] = "M5Stack ATOM Lite";
#endif

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
#ifdef BOARD_HAS_USB_HID
        doc["usb"]["enabled"] = usbEnabled;
#endif
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

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
            if (bt.containsKey("keyboard_enabled")) { bleKeyboardEnabled = bt["keyboard_enabled"].as<bool>(); changed = true; }
            if (bt.containsKey("mouse_enabled"))    { bleMouseEnabled    = bt["mouse_enabled"].as<bool>();    changed = true; }
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
                    USB.serialNumber(USB_SERIAL_NUMBER);
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

void handleOTAUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
        feedWatchdog();
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) Update.printError(Serial);
    }
}

void handleOTAComplete() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;
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
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
        feedWatchdog();
    } else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("[SPIFFS OTA] End, size=%u, success=%d\n", upload.totalSize, Update.end(true));
    }
}

void handleSPIFFSComplete() {
    addCorsHeaders();
    server.sendHeader("Connection", "close");
    if (!checkApiKey()) return;
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
        if (!credStoreLocked) {
            JsonArray arr = doc["labels"].to<JsonArray>();
            for (auto& lbl : credStoreListLabels()) arr.add(lbl);
        }
        String resp; serializeJson(doc, resp);
        sendEncrypted(200, resp);

    } else if (server.method() == HTTP_POST) {
        if (!canProceed()) return;
        JsonDocument doc;
        if (!parseJsonBody(doc)) { requestComplete(); return; }

        String action = doc["action"] | "";

        if (action == "unlock") {
            String key = doc["key"] | "";
            if (key.isEmpty()) {
                server.send(400, "application/json", "{\"error\":\"Missing key\"}");
                server.client().stop(); requestComplete(); return;
            }
            if (credStoreUnlock(key)) {
                sendEncrypted(200, "{\"status\":\"ok\",\"locked\":false}");
            } else {
                sendEncrypted(401, "{\"error\":\"Invalid key\"}");
            }

        } else if (action == "lock") {
            credStoreLock();
            sendEncrypted(200, "{\"status\":\"ok\",\"locked\":true}");

        } else if (action == "get") {
            if (credStoreLocked) {
                sendEncrypted(403, "{\"error\":\"Store is locked\"}");
            } else {
                String label = doc["label"] | "";
                String value = credStoreGet(label);
                JsonDocument r;
                r["label"] = label;
                r["found"] = credStoreLabelExists(label);
                r["value"] = value;
                String resp; serializeJson(r, resp);
                sendEncrypted(200, resp);
            }

        } else if (action == "set") {
            if (credStoreLocked) {
                sendEncrypted(403, "{\"error\":\"Store is locked\"}");
            } else {
                String label = doc["label"] | "";
                String value = doc["value"] | "";
                if (label.isEmpty()) {
                    server.send(400, "application/json", "{\"error\":\"Missing label\"}");
                    server.client().stop(); requestComplete(); return;
                }
                credStoreSet(label, value);
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

    if (newKey.length() < 8) {
        server.send(400, "application/json", "{\"error\":\"new_key must be at least 8 characters\"}");
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

// ---- Route setup ----

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
        "/api/nonce", "/api/keymap",
        "/api/credstore", "/api/credstore/rekey"
    };
    for (const char* path : opts) server.on(path, HTTP_OPTIONS, handleOptions);

    server.onNotFound(handleNotFound);

    const char* headerKeys[] = {"X-Auth", "X-Encrypted", "Content-Length"};
    server.collectHeaders(headerKeys, 3);
}
