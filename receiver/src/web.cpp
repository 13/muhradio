#include "web.h"
#include "config.h"
#include "conf.h"
#include "net.h"
#include "nodetable.h"
#include <version.h>

extern NodeTable g_nodeTable; // defined in main.cpp
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

#include "jsonbuilder.h" // jsonAppendEscaped

// ── OTA bundle state ───────────────────────────────────────────────────────────
// Bundle format (produced by `pio run -t otabundle`):
//   [magic:4 "MRBF"][fw_size:4 LE][fs_size:4 LE][firmware][littlefs]
// Plain firmware.bin / littlefs.bin are also accepted (legacy single-file path).
static constexpr uint32_t OTA_MAGIC = 0x4642524D; // 'M','R','B','F' LE

struct OtaState {
  enum Phase : uint8_t { DETECT, FW, FS, SINGLE, DONE, ABORT } phase;
  uint8_t  hdr[12];
  uint8_t  hdrGot;
  uint32_t fwSize, fsSize, fwWrote;
};
static OtaState _ota;

// ── Auth ───────────────────────────────────────────────────────────────────────
// Basic auth on mutating endpoints, active only when WEB_PASS is set.
static bool _auth(AsyncWebServerRequest* r) {
  if (!WEB_PASS[0]) return true;
  if (r->authenticate(WEB_USER, WEB_PASS)) return true;
  r->requestAuthentication();
  return false;
}

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
  _server.on("/nodes", HTTP_GET, [](AsyncWebServerRequest* r) {
    char buf[2048];
    g_nodeTable.toJson(buf, sizeof(buf), (uint32_t)Net::nowUtc(), Net::nodeId);
    r->send(200, "application/json", buf);
  });
  // GET kept for the update.html link; restart deferred to loop() so the
  // response actually reaches the client.
  _server.on("/reboot", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!_auth(r)) return;
    AsyncWebServerResponse* resp = r->beginResponse(200, "application/json",
      "{\"reboot\":true,\"message\":\"Rebooting...\"}");
    resp->addHeader("Connection", "close");
    r->send(resp);
    Serial.println(F("> [HTTP] Rebooting..."));
    _pendingReboot = true;
  });

  // OTA update handler — accepts plain firmware.bin, plain littlefs.bin,
  // or a combined bundle (magic header + firmware + filesystem in one file).
  _server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest* r) {
      if (!_auth(r)) return;
      AsyncWebServerResponse* resp;
      if (_ota.phase != OtaState::ABORT && !Update.hasError())
        resp = r->beginResponse(200, "application/json",
          "{\"success\":true,\"message\":\"Updated!\"}");
      else
        resp = r->beginResponse(500, "application/json",
          "{\"success\":false,\"message\":\"Update failed\"}");
      resp->addHeader("Connection", "close");
      r->send(resp);
    },
    [](AsyncWebServerRequest* r, String filename,
       size_t index, uint8_t* data, size_t len, bool final) {
      if (!index) {
        Serial.printf("> [OTA] %s\n", filename.c_str());
        _ota = OtaState{OtaState::DETECT, {}, 0, 0, 0, 0};
        if (WEB_PASS[0] && !r->authenticate(WEB_USER, WEB_PASS)) {
          Serial.println(F("> [OTA] unauthorized — dropping upload"));
          _ota.phase = OtaState::ABORT;
        }
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
              // Header comes from the upload — sanity-check before trusting it
              if (_ota.fwSize == 0 || _ota.fwSize > 0x1000000UL ||
                  _ota.fsSize > 0x1000000UL) {
                Serial.println(F("> [OTA] implausible bundle sizes — abort"));
                _ota.phase = OtaState::ABORT;
                break;
              }
              if (!Update.begin(_ota.fwSize, U_FLASH)) {
                Update.printError(Serial);
                _ota.phase = OtaState::ABORT;
                break;
              }
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
              if (!Update.begin(sz, cmd)) {
                Update.printError(Serial);
                _ota.phase = OtaState::ABORT;
                break;
              }
              if (Update.write(_ota.hdr, 12) != 12) {
                Update.printError(Serial);
                _ota.phase = OtaState::ABORT;
                break;
              }
              _ota.phase = OtaState::SINGLE;
            }
            break;
          }

          case OtaState::FW: {
            uint32_t fw_left = _ota.fwSize - _ota.fwWrote;
            size_t   take    = min((size_t)fw_left, left);
            if (Update.write((uint8_t*)p, take) != take) {
              Update.printError(Serial);
              _ota.phase = OtaState::ABORT;
              break;
            }
            _ota.fwWrote += take;
            p    += take;
            left -= take;

            if (_ota.fwWrote == _ota.fwSize) {
              if (!Update.end(true)) {
                Update.printError(Serial);
                _ota.phase = OtaState::ABORT;
                break;
              }
              if (_ota.fsSize == 0) {
                Serial.println(F("> [OTA] firmware OK — no filesystem in bundle"));
                _ota.phase = OtaState::DONE;
                break;
              }
              Serial.println(F("> [OTA] firmware OK — flashing filesystem"));
              if (!Update.begin(_ota.fsSize, U_FS_CMD)) {
                Update.printError(Serial);
                _ota.phase = OtaState::ABORT;
                break;
              }
              _ota.phase = OtaState::FS;
            }
            break;
          }

          case OtaState::FS:
          case OtaState::SINGLE:
            if (Update.write((uint8_t*)p, left) != left) {
              Update.printError(Serial);
              _ota.phase = OtaState::ABORT;
              break;
            }
            left = 0;
            break;

          case OtaState::DONE:
          case OtaState::ABORT:
            left = 0; // swallow the rest of the upload, nothing gets written
            break;
        }

        // Feed watchdog and allow WiFi stack to process on ESP8266
        yield();
      }

      if (final) {
        if (_ota.phase == OtaState::FS || _ota.phase == OtaState::SINGLE) {
          if (!Update.end(true)) {
            Update.printError(Serial);
            _ota.phase = OtaState::ABORT;
          } else {
            Serial.println(F("> [OTA] OK"));
          }
        }
      }
    }
  );

  // Settings — GET returns current config JSON, POST updates + reboots.
  _server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!_auth(r)) return;
    char buf[900];
    size_t n = 0;
    auto ap = [&](const char* s) { n = jsonAppendEscaped(buf, n, sizeof(buf), s); };
    auto as = [&](const char* key, const char* val) {
      n += snprintf(buf + n, sizeof(buf) - n, "\"%s\":\"", key);
      ap(val);
      n += snprintf(buf + n, sizeof(buf) - n, "\",");
    };
    // Secrets never leave the device: "***" = set, "" = unset. The POST
    // handler ignores the literal "***" so the settings form round-trips.
    buf[n++] = '{';
    as("wifi_ssid",   Cfg::g.wifi_ssid);
    as("wifi_pass",   Cfg::g.wifi_pass[0] ? "***" : "");
    as("mqtt_server", Cfg::g.mqtt_server);
    n += snprintf(buf + n, sizeof(buf) - n, "\"mqtt_port\":%u,", Cfg::g.mqtt_port);
    as("mqtt_user",   Cfg::g.mqtt_user);
    as("mqtt_pass",   Cfg::g.mqtt_pass[0] ? "***" : "");
    as("desc",        Cfg::g.desc);
    n += snprintf(buf + n, sizeof(buf) - n, "\"tz_offset\":%d,\"dst_mode\":%u,",
                  (int)Cfg::g.tz_offset, (unsigned)Cfg::g.dst_mode);
    as("ntp1", Cfg::g.ntp1);
    as("ntp2", Cfg::g.ntp2);
    // last key: no trailing comma
    n += snprintf(buf + n, sizeof(buf) - n, "\"ntp3\":\"");
    ap(Cfg::g.ntp3);
    n += snprintf(buf + n, sizeof(buf) - n, "\"}");
    r->send(200, "application/json", buf);
  });

  _server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!_auth(r)) return;
    auto get = [&](const char* name, char* dst, size_t n) {
      if (r->hasParam(name, true))
        strlcpy(dst, r->getParam(name, true)->value().c_str(), n);
    };
    // "***" is the mask from GET — means "unchanged". Empty clears.
    auto getSecret = [&](const char* name, char* dst, size_t n) {
      if (!r->hasParam(name, true)) return;
      const String& v = r->getParam(name, true)->value();
      if (v == "***") return;
      strlcpy(dst, v.c_str(), n);
    };
    // Never overwrite with an empty value — a device with no SSID can't be
    // reached again without USB. Defends against a settings form that failed
    // to load (e.g. cancelled auth prompt) posting blanks.
    auto getNonEmpty = [&](const char* name, char* dst, size_t n) {
      if (!r->hasParam(name, true)) return;
      const String& v = r->getParam(name, true)->value();
      if (v.length() == 0) return;
      strlcpy(dst, v.c_str(), n);
    };
    getNonEmpty("wifi_ssid", Cfg::g.wifi_ssid, sizeof(Cfg::g.wifi_ssid));
    getSecret("wifi_pass", Cfg::g.wifi_pass, sizeof(Cfg::g.wifi_pass));
    getNonEmpty("mqtt_server", Cfg::g.mqtt_server, sizeof(Cfg::g.mqtt_server));
    get("mqtt_user",   Cfg::g.mqtt_user,   sizeof(Cfg::g.mqtt_user));
    getSecret("mqtt_pass", Cfg::g.mqtt_pass, sizeof(Cfg::g.mqtt_pass));
    get("desc",        Cfg::g.desc,        sizeof(Cfg::g.desc));
    get("ntp1",        Cfg::g.ntp1,        sizeof(Cfg::g.ntp1));
    get("ntp2",        Cfg::g.ntp2,        sizeof(Cfg::g.ntp2));
    get("ntp3",        Cfg::g.ntp3,        sizeof(Cfg::g.ntp3));
    // Numeric fields: empty string toInt()s to 0 — only accept non-empty
    auto getInt = [&](const char* name, long& out) {
      if (!r->hasParam(name, true)) return false;
      const String& v = r->getParam(name, true)->value();
      if (v.length() == 0) return false;
      out = v.toInt();
      return true;
    };
    long v;
    if (getInt("mqtt_port", v)) Cfg::g.mqtt_port = (uint16_t)v;
    if (getInt("tz_offset", v)) Cfg::g.tz_offset = (int16_t)v;
    if (getInt("dst_mode",  v)) Cfg::g.dst_mode  = (uint8_t)v;
    bool ok = Cfg::save();
    r->send(200, "application/json",
      ok ? "{\"success\":true}" : "{\"success\":false,\"message\":\"Save failed\"}");
    if (ok) _pendingReboot = true;
  });

  _server.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!_auth(r)) return;
    LittleFS.remove("/config.json");
    r->send(200, "application/json", "{\"success\":true}");
    _pendingReboot = true;
  });

  // Config backup/restore. Contains plaintext secrets, so both endpoints
  // require WEB_PASS to be configured — refused entirely on open devices.
  _server.on("/api/config/export", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!WEB_PASS[0]) {
      r->send(403, "application/json",
        "{\"success\":false,\"message\":\"set WEB_PASS to enable export\"}");
      return;
    }
    if (!_auth(r)) return;
    if (!LittleFS.exists("/config.json")) {
      r->send(404, "application/json",
        "{\"success\":false,\"message\":\"no saved config\"}");
      return;
    }
    AsyncWebServerResponse* resp =
      r->beginResponse(LittleFS, "/config.json", "application/json");
    resp->addHeader("Content-Disposition", "attachment; filename=config.json");
    r->send(resp);
  });

  _server.on("/api/config/import", HTTP_POST,
    [](AsyncWebServerRequest* r) {
      if (!WEB_PASS[0]) {
        r->send(403, "application/json",
          "{\"success\":false,\"message\":\"set WEB_PASS to enable import\"}");
        return;
      }
      if (!_auth(r)) return;
      bool ok = r->_tempObject != nullptr;
      if (ok) {
        File f = LittleFS.open("/config.json", "w");
        ok = f && f.print((const char*)r->_tempObject) > 0;
        if (f) f.close();
      }
      r->send(ok ? 200 : 400, "application/json",
        ok ? "{\"success\":true,\"message\":\"Config imported — rebooting\"}"
           : "{\"success\":false,\"message\":\"invalid or missing body\"}");
      if (ok) _pendingReboot = true;
    },
    nullptr,
    [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
      // Accumulate the raw body; validated and written in the request handler.
      if (total == 0 || total > 2048) return;
      if (index == 0) {
        r->_tempObject = calloc(1, total + 1);
        // Reject anything that doesn't even start like a JSON object
        if (r->_tempObject && (len == 0 || data[0] != '{')) {
          free(r->_tempObject);
          r->_tempObject = nullptr;
        }
      }
      if (r->_tempObject)
        memcpy((uint8_t*)r->_tempObject + index, data, len);
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
