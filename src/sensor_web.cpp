#include "sensor_web.h"
#include "serial_log.h"
#include "web_ui.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <LittleFS.h>
#include <time.h>
#include <cmath>

// mapview.png 실제 크기(px). data/mapview.png 교체 시 함께 갱신 필요함.
constexpr uint16_t mapImageWidth = 983;
constexpr uint16_t mapImageHeight = 739;

#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID "SOL_2"
#endif
#ifndef DEFAULT_WIFI_PASSWORD
#define DEFAULT_WIFI_PASSWORD "qwer1234#"
#endif
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0.0"
#endif

namespace {
WebServer server(80);
Preferences prefs;
bool nvsReady = false;
struct WifiConfig {
    uint32_t magic;
    char ssid[33];
    char password[64];
};
constexpr uint32_t wifiConfigMagic = 0x57494631;
String ssid, password, deviceName, versionUrl, binaryUrl, ca;
String updateStatus = "자동업데이트 비활성화됨";
float alpha = 0.35f;
uint32_t rangeMm = 6000, staleMs = 2000, intervalMinutes = 60;
bool mapReady = false;
float mapOriginX = mapImageWidth / 2.0f, mapOriginY = mapImageHeight - 20.0f;
float mapHeadingDeg = 0.0f, mapScaleMmPerPx = 10.0f;
bool automatic = false, apMode = false, connecting = false;
bool webActive = false;
IPAddress webAddress;
bool checkPending = false, uploading = false, uploadOK = false;
uint8_t attempts = 0;
uint32_t attemptAt = 0, lastCheckAt = 0, restartAt = 0, reconnectAt = 0;
uint32_t lastNetworkReportAt = 0;
uint32_t sensorBytes = 0, sensorFrames = 0, frameAt = 0;
SensorTarget targets[3] = {};
float filteredX[3] = {}, filteredY[3] = {};
bool previousPresent[3] = {};

String quote(const String& value) {
    String out = "\"";
    for (size_t i = 0; i < value.length(); ++i) {
        const unsigned char c = value[i];
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) { char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped; }
        else out += char(c);
    }
    return out + '"';
}

void reply(int code, const String& message) {
    server.send(code, "application/json; charset=utf-8", "{\"message\":" + quote(message) + "}");
}

bool validNumber(const String& text, double low, double high, double& value) {
    if (text.isEmpty()) return false;
    char* end = nullptr;
    value = strtod(text.c_str(), &end);
    return *end == '\0' && std::isfinite(value) && value >= low && value <= high;
}

bool validUrl(const String& url) {
    return url.length() <= 512 && (url.startsWith("http://") || url.startsWith("https://")) &&
           url.indexOf(' ') < 0 && url.indexOf('\n') < 0 && url.indexOf('\r') < 0;
}

bool validWifi(const String& name, const String& pass) {
    return !name.isEmpty() && name.length() <= 32 &&
           (pass.isEmpty() || (pass.length() >= 8 && pass.length() <= 63));
}

bool loadWifi(String& name, String& pass) {
    WifiConfig config = {};
    if (!nvsReady || prefs.getBytesLength("wifiCfg") != sizeof(config) ||
        prefs.getBytes("wifiCfg", &config, sizeof(config)) != sizeof(config) ||
        config.magic != wifiConfigMagic || config.ssid[32] != '\0' || config.password[63] != '\0') return false;
    name = config.ssid;
    pass = config.password;
    return validWifi(name, pass);
}

bool storeWifi(const String& name, const String& pass) {
    if (!nvsReady || !validWifi(name, pass)) return false;
    // SSID와 비밀번호를 단일 NVS 항목에 저장하여 서로 다른 설정의 혼합 방지함.
    WifiConfig config = {};
    config.magic = wifiConfigMagic;
    name.toCharArray(config.ssid, sizeof(config.ssid));
    pass.toCharArray(config.password, sizeof(config.password));
    if (prefs.putBytes("wifiCfg", &config, sizeof(config)) != sizeof(config)) return false;
    String savedName, savedPass;
    return loadWifi(savedName, savedPass) && savedName == name && savedPass == pass;
}

void initializeWifi() {
    if (loadWifi(ssid, password)) {
        serialLog::print(cli::fg::GREEN, "NVS", "Wi-Fi 저장값 사용함");
        return;
    }
    // 기존 개별 키가 모두 있고 유효하면 값을 유지하며 새 저장 구조로 이관함.
    const bool legacy = nvsReady && prefs.isKey("ssid") && prefs.isKey("pass");
    if (legacy) {
        ssid = prefs.getString("ssid", "");
        password = prefs.getString("pass", "");
    }
    const bool migrated = legacy && validWifi(ssid, password);
    if (!migrated) {
        ssid = DEFAULT_WIFI_SSID;
        password = DEFAULT_WIFI_PASSWORD;
    }
    if (storeWifi(ssid, password)) {
        serialLog::print(cli::fg::GREEN, "NVS", migrated ? "기존 Wi-Fi 값 이관 완료됨" : "코드 기본값으로 Wi-Fi 초기화됨");
    } else {
        serialLog::print(cli::fg::RED, "ERROR", "NVS 저장 실패함. 현재 부팅에서만 Wi-Fi 설정 사용함");
    }
}

void startAttempt() {
    ++attempts;
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    attemptAt = millis();
    connecting = true;
    serialLog::print(cli::fg::YELLOW, "WIFI", "연결 시도함: %u / 5", attempts);
}

void startNetwork() {
    server.stop();
    webActive = false;
    apMode = false;
    attempts = 0;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(false);
    startAttempt();
}

void fallbackAP() {
    connecting = false;
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    apMode = WiFi.softAP("ESP32-Sensor");
    serialLog::print(apMode ? cli::fg::GREEN : cli::fg::RED, "AP", "공개 AP %s: http://%s", apMode ? "시작됨" : "시작 실패함",
                  WiFi.softAPIP().toString().c_str());
}

void networkTick() {
    if (apMode) return;
    if (WiFi.status() == WL_CONNECTED) {
        if (connecting) {
            connecting = false;
            configTime(0, 0, "pool.ntp.org", "time.google.com");
            serialLog::print(cli::fg::GREEN, "WIFI", "연결됨: http://%s", WiFi.localIP().toString().c_str());
        }
        return;
    }
    if (!connecting) { attempts = 0; startAttempt(); }
    else if (millis() - attemptAt >= 10000) {
        if (attempts < 5) startAttempt();
        else fallbackAP();
    }
}

void reportNetwork() {
    if (millis() - lastNetworkReportAt < 10000) return;
    lastNetworkReportAt = millis();
    if (apMode) {
        serialLog::print(cli::fg::BRIGHT_CYAN, "NET", "AP ESP32-Sensor | IP=%s | 웹UI=http://%s",
                      WiFi.softAPIP().toString().c_str(),
                      WiFi.softAPIP().toString().c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        serialLog::print(cli::fg::BRIGHT_CYAN, "NET", "Wi-Fi | IP=%s | 웹UI=http://%s",
                      WiFi.localIP().toString().c_str(),
                      WiFi.localIP().toString().c_str());
    }
}

void webNetworkTick() {
    const bool ready = apMode || WiFi.status() == WL_CONNECTED;
    const IPAddress address = apMode ? WiFi.softAPIP() : WiFi.localIP();
    if (!ready || address == IPAddress(0, 0, 0, 0)) {
        if (webActive) server.stop();
        webActive = false;
        return;
    }
    if (!webActive || address != webAddress) {
        server.begin();
        webActive = true;
        webAddress = address;
        serialLog::print(cli::fg::GREEN, "WEB", "HTTP 서버 시작 요청됨: http://%s:80", address.toString().c_str());
    }
}

bool fresh() { return sensorFrames && millis() - frameAt <= staleMs; }

void status() {
    const bool connected = WiFi.status() == WL_CONNECTED;
    String json;
    json.reserve(1600);
    json = "{\"name\":" + quote(deviceName) + ",\"version\":" + quote(FIRMWARE_VERSION);
    json += ",\"network\":" + quote(apMode ? "AP" : connected ? "Wi-Fi" : "연결 시도 중임");
    json += ",\"ip\":" + quote((apMode ? WiFi.softAPIP() : WiFi.localIP()).toString());
    json += ",\"rssi\":" + (connected ? String(WiFi.RSSI()) : String("null"));
    json += ",\"attempts\":" + String(attempts) + ",\"uptime\":" + String(millis() / 1000);
    json += ",\"heap\":" + String(ESP.getFreeHeap()) + ",\"bytes\":" + String(sensorBytes);
    json += ",\"frames\":" + String(sensorFrames) + ",\"fresh\":" + String(fresh() ? "true" : "false");
    json += ",\"age\":" + (sensorFrames ? String(millis() - frameAt) : String("null"));
    json += ",\"range\":" + String(rangeMm) + ",\"update\":" + quote(updateStatus) + ",\"targets\":[";
    for (size_t i = 0; i < 3; ++i) {
        if (i) json += ',';
        const auto& t = targets[i];
        const bool visible = fresh() && t.present && hypotf(filteredX[i], filteredY[i]) <= rangeMm;
        json += "{\"present\":" + String(visible ? "true" : "false") + ",\"x\":" + String(t.x);
        json += ",\"y\":" + String(t.y) + ",\"fx\":" + String(filteredX[i], 1);
        json += ",\"fy\":" + String(filteredY[i], 1) + ",\"speed\":" + String(t.speed);
        json += ",\"resolution\":" + String(t.resolution) + '}';
    }
    server.send(200, "application/json; charset=utf-8", json + "]}");
}

void settings() {
    String json = "{\"ssid\":" + quote(ssid) + ",\"device\":{\"name\":" + quote(deviceName);
    json += ",\"alpha\":" + String(alpha, 2) + ",\"range\":" + String(rangeMm) + ",\"stale\":" + String(staleMs) + '}';
    json += ",\"auto\":{\"enabled\":" + String(automatic ? "true" : "false");
    json += ",\"versionUrl\":" + quote(versionUrl) + ",\"binaryUrl\":" + quote(binaryUrl);
    json += ",\"interval\":" + String(intervalMinutes) + ",\"ca\":" + quote(ca) + "}";
    json += ",\"map\":{\"ready\":" + String(mapReady ? "true" : "false");
    json += ",\"x\":" + String(mapOriginX, 1) + ",\"y\":" + String(mapOriginY, 1);
    json += ",\"heading\":" + String(mapHeadingDeg, 1) + ",\"scale\":" + String(mapScaleMmPerPx, 3);
    json += ",\"width\":" + String(mapImageWidth) + ",\"height\":" + String(mapImageHeight) + "}}";
    server.send(200, "application/json; charset=utf-8", json);
}

void saveMap() {
    double x, y, heading, scale;
    if (!validNumber(server.arg("x"), 0, mapImageWidth, x) ||
        !validNumber(server.arg("y"), 0, mapImageHeight, y) ||
        !validNumber(server.arg("heading"), 0, 359.9, heading) ||
        !validNumber(server.arg("scale"), 0.5, 500, scale)) return reply(400, "지도 보정값 범위 확인 필요함");
    mapOriginX = x; mapOriginY = y; mapHeadingDeg = heading; mapScaleMmPerPx = scale; mapReady = true;
    prefs.putFloat("mapX", mapOriginX); prefs.putFloat("mapY", mapOriginY);
    prefs.putFloat("mapHead", mapHeadingDeg); prefs.putFloat("mapScale", mapScaleMmPerPx);
    prefs.putBool("mapReady", mapReady);
    reply(200, "지도 보정값 저장됨");
}

void saveDevice() {
    double a, r, s;
    String name = server.arg("name"); name.trim();
    if (name.isEmpty() || name.length() > 96 ||
        !validNumber(server.arg("alpha"), .05, 1, a) ||
        !validNumber(server.arg("range"), 500, 6000, r) ||
        !validNumber(server.arg("stale"), 300, 10000, s)) return reply(400, "장치 설정 범위 확인 필요함");
    deviceName = name; alpha = a; rangeMm = r; staleMs = s;
    prefs.putString("name", deviceName); prefs.putFloat("alpha", alpha);
    prefs.putUInt("range", rangeMm); prefs.putUInt("stale", staleMs);
    for (size_t i = 0; i < 3; ++i) previousPresent[i] = false;
    reply(200, "장치 설정 저장됨");
}

void saveWifi() {
    String newSsid = server.arg("ssid"), newPass = server.arg("password");
    if (server.arg("open") == "1") newPass = "";
    else if (newPass.isEmpty()) newPass = password;
    if (!validWifi(newSsid, newPass))
        return reply(400, "SSID 또는 비밀번호 길이 확인 필요함");
    if (!storeWifi(newSsid, newPass)) {
        serialLog::print(cli::fg::RED, "ERROR", "Wi-Fi NVS 저장 검증 실패함");
        return reply(500, "NVS 저장 실패함. 재연결하지 않음");
    }
    ssid = newSsid; password = newPass;
    serialLog::print(cli::fg::GREEN, "NVS", "Wi-Fi 저장 및 읽기 검증 완료됨");
    reconnectAt = millis() + 1500;
    reply(200, "Wi-Fi 저장됨. 재연결 시작 예정임");
}

void saveAuto() {
    double interval;
    const String v = server.arg("versionUrl"), b = server.arg("binaryUrl"), cert = server.arg("ca");
    const bool enabled = server.arg("enabled") == "1";
    if (!validNumber(server.arg("interval"), 5, 10080, interval) ||
        (!v.isEmpty() && !validUrl(v)) || (!b.isEmpty() && !validUrl(b)) ||
        (enabled && (v.isEmpty() || b.isEmpty())) || cert.length() > 6000 ||
        ((v.startsWith("https://") || b.startsWith("https://")) && cert.isEmpty()))
        return reply(400, "업데이트 URL·주기·HTTPS CA 확인 필요함");
    versionUrl = v; binaryUrl = b; ca = cert; intervalMinutes = interval; automatic = enabled;
    prefs.putString("vurl", v); prefs.putString("burl", b); prefs.putString("ca", ca);
    prefs.putUInt("interval", intervalMinutes); prefs.putBool("auto", automatic);
    lastCheckAt = millis();
    updateStatus = automatic ? "다음 주기에서 버전 확인 예정임" : "자동업데이트 비활성화됨";
    reply(200, "자동업데이트 설정 저장됨");
}

bool versionCode(String text, uint32_t& code) {
    text.trim();
    unsigned major, minor, patch; char tail;
    if (sscanf(text.c_str(), "%u.%u.%u%c", &major, &minor, &patch, &tail) != 3 ||
        major > 999 || minor > 999 || patch > 999) return false;
    code = major * 1000000UL + minor * 1000UL + patch;
    return true;
}

void checkUpdate() {
    lastCheckAt = millis();
    if (WiFi.status() != WL_CONNECTED) { updateStatus = "Wi-Fi 연결 후 업데이트 가능함"; return; }
    if (!validUrl(versionUrl) || !validUrl(binaryUrl)) { updateStatus = "업데이트 URL 설정 필요함"; return; }
    if ((versionUrl.startsWith("https://") || binaryUrl.startsWith("https://")) &&
        (ca.isEmpty() || time(nullptr) < 1700000000)) { updateStatus = "HTTPS CA·시간 동기화 확인 필요함"; return; }
    WiFiClient plain;
    WiFiClientSecure secure;
    secure.setCACert(ca.c_str());
    secure.setHandshakeTimeout(10);
    HTTPClient http;
    http.setTimeout(5000);
    WiFiClient& versionClient = versionUrl.startsWith("https://") ? static_cast<WiFiClient&>(secure) : plain;
    if (!http.begin(versionClient, versionUrl)) { updateStatus = "버전 URL 연결 실패함"; return; }
    const int result = http.GET();
    String remoteVersion;
    // 버전 문서는 짧은 텍스트만 허용하여 과도한 메모리 사용 방지함.
    if (result == 200 && http.getSize() > 0 && http.getSize() <= 64) remoteVersion = http.getString();
    http.end();
    uint32_t remote, current;
    if (!versionCode(remoteVersion, remote) || !versionCode(FIRMWARE_VERSION, current)) {
        updateStatus = "버전 응답 확인 실패함: Content-Length 및 x.y.z 형식 필요함"; return;
    }
    if (remote <= current) { updateStatus = "현재 펌웨어가 최신임"; return; }
    updateStatus = "최신 펌웨어 설치 중임";
    HTTPUpdate updater(10000);
    updater.rebootOnUpdate(false);
    WiFiClient& binaryClient = binaryUrl.startsWith("https://") ? static_cast<WiFiClient&>(secure) : plain;
    const auto outcome = updater.update(binaryClient, binaryUrl, FIRMWARE_VERSION);
    if (outcome == HTTP_UPDATE_OK) {
        updateStatus = "업데이트 완료됨. 재부팅 예정임";
        restartAt = millis() + 1500;
    } else if (outcome == HTTP_UPDATE_NO_UPDATES) updateStatus = "배포 서버에 새 펌웨어 없음";
    else updateStatus = "업데이트 실패함: " + updater.getLastErrorString();
}

void handleUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        uploadOK = false;
        if (uploading || restartAt || reconnectAt || !upload.filename.endsWith(".bin")) {
            updateStatus = "업로드 시작 불가함. .bin 파일 및 장치 상태 확인 필요함";
            return;
        }
        uploading = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        updateStatus = uploading ? "펌웨어 수신 중임" : "업데이트 시작 실패함: " + String(Update.errorString());
    } else if (upload.status == UPLOAD_FILE_WRITE && uploading) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            updateStatus = "펌웨어 쓰기 실패함: " + String(Update.errorString());
            Update.abort(); uploading = false;
        }
    } else if (upload.status == UPLOAD_FILE_END && uploading) {
        uploadOK = Update.end(true);
        uploading = false;
        updateStatus = uploadOK ? "업데이트 완료됨. 재부팅 예정임" : "잘못된 펌웨어임: " + String(Update.errorString());
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (uploading) Update.abort();
        uploading = false; uploadOK = false; updateStatus = "업로드 중단됨";
    }
}
}

void webBegin() {
    nvsReady = prefs.begin("sensor", false);
    if (!nvsReady) serialLog::print(cli::fg::RED, "ERROR", "NVS 열기 실패함");
    initializeWifi();
    deviceName = prefs.getString("name", "ESP32 Sensor");
    alpha = prefs.getFloat("alpha", .35f);
    rangeMm = prefs.getUInt("range", 6000); staleMs = prefs.getUInt("stale", 2000);
    versionUrl = prefs.getString("vurl", ""); binaryUrl = prefs.getString("burl", ""); ca = prefs.getString("ca", "");
    automatic = prefs.getBool("auto", false); intervalMinutes = prefs.getUInt("interval", 60);
    mapReady = prefs.getBool("mapReady", false);
    mapOriginX = prefs.getFloat("mapX", mapOriginX); mapOriginY = prefs.getFloat("mapY", mapOriginY);
    mapHeadingDeg = prefs.getFloat("mapHead", mapHeadingDeg); mapScaleMmPerPx = prefs.getFloat("mapScale", mapScaleMmPerPx);
    if (automatic) updateStatus = "자동 버전 확인 대기 중임";
    if (!LittleFS.begin(true)) serialLog::print(cli::fg::RED, "ERROR", "LittleFS 마운트 실패함: 지도 이미지 제공 불가함");
    WiFi.persistent(false);
    startNetwork();
    server.on("/", HTTP_GET, [] {
        serialLog::print(cli::fg::CYAN, "WEB", "대시보드 요청 수신됨");
        server.sendHeader("Cache-Control", "no-store");
        server.send_P(200, "text/html; charset=utf-8", WEB_UI);
    });
    server.on("/api/status", HTTP_GET, status);
    server.on("/api/settings", HTTP_GET, settings);
    server.on("/mapview.png", HTTP_GET, [] {
        File file = LittleFS.open("/mapview.png", "r");
        if (!file) return reply(404, "지도 이미지 없음: data/mapview.png 업로드 필요함");
        server.sendHeader("Cache-Control", "max-age=86400");
        server.streamFile(file, "image/png");
        file.close();
    });
    server.on("/api/map", HTTP_POST, saveMap);
    server.on("/api/device", HTTP_POST, saveDevice);
    server.on("/api/wifi", HTTP_POST, saveWifi);
    server.on("/api/auto", HTTP_POST, saveAuto);
    server.on("/api/restart", HTTP_POST, [] { if (uploading) return reply(409, "업데이트 진행 중임"); reply(200, "재부팅 예정임"); restartAt = millis() + 1500; });
    server.on("/api/update/check", HTTP_POST, [] {
        if (uploading || restartAt || reconnectAt || checkPending) return reply(409, "다른 작업 진행 중임");
        if (WiFi.status() != WL_CONNECTED) return reply(409, "Wi-Fi 연결 필요함");
        if (!validUrl(versionUrl) || !validUrl(binaryUrl)) return reply(400, "업데이트 URL 설정 필요함");
        checkPending = true; reply(202, "버전 확인 및 설치 요청됨");
    });
    server.on("/api/update/upload", HTTP_POST, [] {
        reply(uploadOK ? 200 : 400, updateStatus);
        if (uploadOK) restartAt = millis() + 1500;
    }, handleUpload);
    server.onNotFound([] { reply(404, "경로 없음"); });
}

void webTick() {
    if (webActive) server.handleClient();
    if (restartAt && static_cast<int32_t>(millis() - restartAt) >= 0) ESP.restart();
    if (reconnectAt && static_cast<int32_t>(millis() - reconnectAt) >= 0 && !uploading) {
        reconnectAt = 0; startNetwork();
    }
    networkTick();
    webNetworkTick();
    reportNetwork();
    if (!uploading && !restartAt && !reconnectAt &&
        (checkPending || (automatic && WiFi.status() == WL_CONNECTED &&
         millis() - lastCheckAt >= intervalMinutes * 60000UL))) {
        checkPending = false;
        checkUpdate();
    }
}

void webSensorBytes(uint32_t bytes) { sensorBytes = bytes; }

void webSensorFrame(const SensorTarget* next, uint32_t bytes, uint32_t frames) {
    const bool expired = !fresh();
    sensorBytes = bytes; sensorFrames = frames; frameAt = millis();
    for (size_t i = 0; i < 3; ++i) {
        targets[i] = next[i];
        if (expired || !previousPresent[i]) { filteredX[i] = next[i].x; filteredY[i] = next[i].y; }
        else { filteredX[i] += alpha * (next[i].x - filteredX[i]); filteredY[i] += alpha * (next[i].y - filteredY[i]); }
        previousPresent[i] = next[i].present;
    }
}
