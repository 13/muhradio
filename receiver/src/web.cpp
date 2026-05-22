#include "web.h"
#include "config.h"
#include "conf.h"
#include <version.h>
#include <Arduino.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include "ota8266.h"
#else
  #include <WiFi.h>
#endif
#include <FS.h>
#define SPIFFS LittleFS
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#ifdef ESP8266
  #include <Updater.h>
  #define U_FS_CMD U_FS
#else
  #include <Update.h>
  #define U_FS_CMD U_SPIFFS
#endif

// ── Module-private state ───────────────────────────────────────────────────────
static AsyncWebServer _server(80);
static AsyncWebSocket _ws("/ws");
static uint8_t        _clients      = 0;
static Status*        _status       = nullptr;
static bool           _pendingReboot = false;

// 2 KB static buffer — serialised once per notify call, never on heap.
static char _wsBuf[2048];

// ── OTA bundle state ───────────────────────────────────────────────────────────
// Bundle format (produced by `pio run -t otabundle`):
//   [magic:4 "MRBF"][fw_size:4 LE][fs_size:4 LE][firmware][littlefs]
// Plain firmware.bin / littlefs.bin are also accepted (legacy single-file path).
static constexpr uint32_t OTA_MAGIC = 0x4642524D; // 'M','R','B','F' LE

struct OtaState {
  enum Phase : uint8_t { DETECT, FW, FS, SINGLE } phase;
  uint8_t  hdr[12];
  uint8_t  hdrGot;
  uint32_t fwSize, fsSize, fwWrote;
};
static OtaState _ota;

// ── Helpers ────────────────────────────────────────────────────────────────────
static const char* _serialize(Status& s, time_t ts) {
  s.rssi      = WiFi.RSSI();
  s.memfree   = ESP.getFreeHeap();
  s.timestamp = ts;
#if defined(ESP32)
  s.memfrag = (s.memfree > 0)
    ? (uint8_t)((100 * (s.memfree - ESP.getMaxAllocHeap())) / s.memfree)
    : 0;
#elif defined(ESP8266)
  s.memfrag = ESP.getHeapFragmentation();
#endif
  s.toJson(_wsBuf, sizeof(_wsBuf));
  return _wsBuf;
}

static void _onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("> [WS] #%u connected from %s\n",
        client->id(), client->remoteIP().toString().c_str());
      _clients++;
      if (_status) _ws.textAll(_serialize(*_status, _status->timestamp));
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("> [WS] #%u disconnected\n", client->id());
      if (_clients) _clients--;
      break;
    case WS_EVT_DATA:
      // Any message from client triggers a status refresh
      if (_status) _ws.textAll(_serialize(*_status, _status->timestamp));
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// ── Public API ─────────────────────────────────────────────────────────────────
void Web::begin(Status& s) {
  _status = &s;

  if (!LittleFS.begin()) {
    Serial.println(F("> [LittleFS] ERR — rebooting"));
    ESP.restart();
  }

  _ws.onEvent(_onWsEvent);
  _server.addHandler(&_ws);

  _server.on("/ip", HTTP_GET, [](AsyncWebServerRequest* r)
    { r->send(200, "text/plain", _status ? _status->ip : ""); });
  _server.on("/ping", HTTP_GET, [](AsyncWebServerRequest* r)
    { r->send(200, "text/plain", "pong"); });
  _server.on("/json", HTTP_GET, [](AsyncWebServerRequest* r)
    { r->send(200, "application/json",
        _status ? _serialize(*_status, _status->timestamp) : "{}"); });
  _server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* r) {
    AsyncWebServerResponse* resp = r->beginResponse(200, "application/json",
      "{\"reboot\":true,\"message\":\"Rebooting...\"}");
    resp->addHeader("Connection", "close");
    r->send(resp);
    Serial.println(F("> [HTTP] Rebooting..."));
    ESP.restart();
  });

  // OTA update handler — accepts plain firmware.bin, plain littlefs.bin,
  // or a combined bundle (magic header + firmware + filesystem in one file).
  _server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest* r) {
      AsyncWebServerResponse* resp;
      if (!Update.hasError())
        resp = r->beginResponse(200, "application/json",
          "{\"success\":true,\"message\":\"Updated!\"}");
      else
        resp = r->beginResponse(500, "application/json",
          "{\"success\":false,\"message\":\"Update failed\"}");
      resp->addHeader("Connection", "close");
      r->send(resp);
    },
    [](AsyncWebServerRequest*, String filename,
       size_t index, uint8_t* data, size_t len, bool final) {
      if (!index) {
        Serial.printf("> [OTA] %s\n", filename.c_str());
        _ota = OtaState{OtaState::DETECT, {}, 0, 0, 0, 0};
      }

      const uint8_t* p    = data;
      size_t         left = len;

      while (left > 0) {
        switch (_ota.phase) {

          case OtaState::DETECT: {
            // Buffer the first 12 bytes to probe for the bundle magic.
            size_t need = 12 - _ota.hdrGot;
            size_t take = min(need, left);
            memcpy(_ota.hdr + _ota.hdrGot, p, take);
            _ota.hdrGot += take;
            p    += take;
            left -= take;
            if (_ota.hdrGot < 12) break;  // need more data

            uint32_t magic;
            memcpy(&magic, _ota.hdr, 4);

            if (magic == OTA_MAGIC) {
              memcpy(&_ota.fwSize, _ota.hdr + 4, 4);
              memcpy(&_ota.fsSize, _ota.hdr + 8, 4);
              Serial.printf("> [OTA] bundle  fw=%u  fs=%u\n", _ota.fwSize, _ota.fsSize);
              if (!Update.begin(_ota.fwSize, U_FLASH)) Update.printError(Serial);
              _ota.phase = OtaState::FW;
            } else {
              // Not a bundle — determine type from filename, write buffered header.
              uint32_t sz;
              int      cmd;
              if (filename.indexOf("littlefs") > -1) {
#ifdef ESP8266
                FSInfo fsinfo; LittleFS.info(fsinfo);
                sz  = fsinfo.totalBytes;
#else
                sz  = LittleFS.totalBytes();
#endif
                cmd = U_FS_CMD;
              } else {
                sz  = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                cmd = U_FLASH;
              }
              if (!Update.begin(sz, cmd)) Update.printError(Serial);
              if (Update.write(_ota.hdr, 12) != 12) Update.printError(Serial);
              _ota.phase = OtaState::SINGLE;
            }
            break;
          }

          case OtaState::FW: {
            uint32_t fw_left = _ota.fwSize - _ota.fwWrote;
            size_t   take    = min((size_t)fw_left, left);
            if (Update.write((uint8_t*)p, take) != take) Update.printError(Serial);
            _ota.fwWrote += take;
            p    += take;
            left -= take;

            if (_ota.fwWrote == _ota.fwSize) {
              if (!Update.end(true)) { Update.printError(Serial); return; }
              Serial.println(F("> [OTA] firmware OK — flashing filesystem"));
              if (!Update.begin(_ota.fsSize, U_FS_CMD)) Update.printError(Serial);
              _ota.phase = OtaState::FS;
            }
            break;
          }

          case OtaState::FS:
          case OtaState::SINGLE:
            if (Update.write((uint8_t*)p, left) != left) Update.printError(Serial);
            left = 0;
            break;
        }
        
        // Feed watchdog and allow WiFi stack to process on ESP8266
        yield();
      }

      if (final) {
        if (!Update.end(true)) Update.printError(Serial);
        else Serial.println(F("> [OTA] OK"));
      }
    }
  );

  // Settings — GET returns current config JSON, POST updates + reboots.
  _server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* r) {
    char buf[750];
    snprintf(buf, sizeof(buf),
      "{\"wifi_ssid\":\"%s\",\"wifi_pass\":\"%s\","
      "\"mqtt_server\":\"%s\",\"mqtt_port\":%u,"
      "\"mqtt_user\":\"%s\",\"mqtt_pass\":\"%s\","
      "\"desc\":\"%s\",\"tz_offset\":%d,\"dst_mode\":%u,"
      "\"ntp1\":\"%s\",\"ntp2\":\"%s\",\"ntp3\":\"%s\"}",
      Cfg::g.wifi_ssid, Cfg::g.wifi_pass,
      Cfg::g.mqtt_server, Cfg::g.mqtt_port,
      Cfg::g.mqtt_user,  Cfg::g.mqtt_pass,
      Cfg::g.desc, (int)Cfg::g.tz_offset, (unsigned)Cfg::g.dst_mode,
      Cfg::g.ntp1, Cfg::g.ntp2, Cfg::g.ntp3);
    r->send(200, "application/json", buf);
  });

  _server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest* r) {
    auto get = [&](const char* name, char* dst, size_t n) {
      if (r->hasParam(name, true))
        strlcpy(dst, r->getParam(name, true)->value().c_str(), n);
    };
    get("wifi_ssid",   Cfg::g.wifi_ssid,   sizeof(Cfg::g.wifi_ssid));
    get("wifi_pass",   Cfg::g.wifi_pass,   sizeof(Cfg::g.wifi_pass));
    get("mqtt_server", Cfg::g.mqtt_server, sizeof(Cfg::g.mqtt_server));
    get("mqtt_user",   Cfg::g.mqtt_user,   sizeof(Cfg::g.mqtt_user));
    get("mqtt_pass",   Cfg::g.mqtt_pass,   sizeof(Cfg::g.mqtt_pass));
    get("desc",        Cfg::g.desc,        sizeof(Cfg::g.desc));
    get("ntp1",        Cfg::g.ntp1,        sizeof(Cfg::g.ntp1));
    get("ntp2",        Cfg::g.ntp2,        sizeof(Cfg::g.ntp2));
    get("ntp3",        Cfg::g.ntp3,        sizeof(Cfg::g.ntp3));
    if (r->hasParam("mqtt_port", true))
      Cfg::g.mqtt_port  = (uint16_t)r->getParam("mqtt_port",  true)->value().toInt();
    if (r->hasParam("tz_offset", true))
      Cfg::g.tz_offset  = (int16_t) r->getParam("tz_offset",  true)->value().toInt();
    if (r->hasParam("dst_mode", true))
      Cfg::g.dst_mode   = (uint8_t) r->getParam("dst_mode",   true)->value().toInt();
    bool ok = Cfg::save();
    r->send(200, "application/json",
      ok ? "{\"success\":true}" : "{\"success\":false,\"message\":\"Save failed\"}");
    if (ok) _pendingReboot = true;
  });

  // Static files from LittleFS — registered last so API routes take priority.
  // Any file placed in data/ is served automatically; no route changes needed.
  _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  _server.begin();
  Serial.println(F("> [HTTP] Started"));

#ifdef ESP8266
  ota8266Begin(nullptr);
#endif
}

void Web::loop() {
  _ws.cleanupClients();
#ifdef ESP8266
  ota8266Handle();
#endif
  if (_pendingReboot) {
    delay(100);
    ESP.restart();
  }
}

void Web::notify(Status& s, time_t ts) {
  if (_clients > 0)
    _ws.textAll(_serialize(s, ts));
}
