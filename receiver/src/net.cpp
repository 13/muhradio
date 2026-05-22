#include "net.h"
#include "config.h"
#include "conf.h"
#include <version.h>
#include <time.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
#else
  #include <WiFi.h>
  #include <ESPmDNS.h>
#endif
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>

// ── Module-private state ───────────────────────────────────────────────────────
static WiFiClient   _wifiClient;
static PubSubClient _mqtt(_wifiClient);
static WiFiUDP      _ntpUDP;
static NTPClient    _ntp(_ntpUDP, "", 0);

static unsigned long _reconnectAt = 0;
static unsigned long _markAt      = 0;
static uint32_t      _uptime      = 0; // minutes

// EU DST rule: active from last Sunday of March 01:00 UTC
//              to last Sunday of October 01:00 UTC.
// Formula for last Sunday of month m (3=March, 10=Oct) in year y:
//   31 - (5*y/4 + bias) % 7   where bias = 4 for March, 1 for October.
static bool _euDstActive(time_t utc) {
  struct tm t;
  gmtime_r(&utc, &t);
  int y = t.tm_year + 1900;
  int m = t.tm_mon + 1; // 1-12
  if (m <  3 || m > 10) return false; // Nov-Feb: always standard
  if (m >  3 && m <  10) return true;  // Apr-Sep: always DST
  int lastSun = 31 - (5 * y / 4 + (m == 3 ? 4 : 1)) % 7;
  if (t.tm_mday < lastSun) return m == 10; // before transition day
  if (t.tm_mday > lastSun) return m == 3;  // after  transition day
  return (m == 3) ? (t.tm_hour >= 1)       // Mar: DST starts at 01:00 UTC
                  : (t.tm_hour <  1);      // Oct: DST ends   at 01:00 UTC
}

// Returns total local offset in seconds for the given UTC epoch.
static long _localOffset(time_t utc) {
  long base = (long)Cfg::g.tz_offset * 60L;
  switch (Cfg::g.dst_mode) {
    case 1:  return base + 3600;                              // always summer
    case 2:  return base + (_euDstActive(utc) ? 3600L : 0L); // auto EU
    default: return base;                                     // always standard
  }
}

// Try NTP servers in order until one succeeds.
static bool _ntpUpdate() {
  const char* servers[] = { Cfg::g.ntp1, Cfg::g.ntp2, Cfg::g.ntp3 };
  for (const char* srv : servers) {
    if (!srv || !srv[0]) continue;
    _ntp.setPoolServerName(srv);
    if (_ntp.forceUpdate()) return true;
  }
  return false;
}

// ── Exported symbols ───────────────────────────────────────────────────────────
namespace Net {
  char hostname[24] = {0};
  char nodeId[5]    = {0};
}

// ── Helpers ────────────────────────────────────────────────────────────────────
static void _connectWiFi() {
  //WiFi.persistent(false);   // don't read/write credentials from NVS
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.hostname(Net::hostname);
  WiFi.begin(Cfg::g.wifi_ssid, Cfg::g.wifi_pass);
  Serial.print(F("> [WiFi] Connecting"));
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    Serial.print('.');
    delay(1000);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F(" OK — "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F(" timeout"));
  }
}

static void _connectMqtt() {
  char base[80], lwt[88], ip[84], ver[92];
  snprintf(base, sizeof(base), "%s/%s",      MQTT_TOPIC_LWT, Net::hostname);
  snprintf(lwt,  sizeof(lwt),  "%s/LWT",     base);
  snprintf(ip,   sizeof(ip),   "%s/IP",      base);
  snprintf(ver,  sizeof(ver),  "%s/VERSION", base);

  Serial.print(F("> [MQTT] Connecting..."));
  const char* mu = Cfg::g.mqtt_user[0] ? Cfg::g.mqtt_user : nullptr;
  const char* mp = Cfg::g.mqtt_pass[0] ? Cfg::g.mqtt_pass : nullptr;
  if (_mqtt.connect(Net::hostname, mu, mp, lwt, 1, true, "offline")) {
    Serial.println(F(" OK"));
    char ipStr[16];
    strlcpy(ipStr, WiFi.localIP().toString().c_str(), sizeof(ipStr));
    _mqtt.publish(lwt, "online", true);
    _mqtt.publish(ip,  ipStr,    true);
    _mqtt.publish(ver, VERSION,  true);
  } else {
    Serial.print(F(" ERR rc="));
    Serial.println(_mqtt.state());
  }
}

#ifdef VERBOSE
static void _printUptime(uint32_t mins) {
  Serial.print(F("> [MARK] "));
  if (mins >= 60) {
    uint32_t h = mins / 60, m = mins % 60;
    if (h >= 24) { Serial.print(h / 24); Serial.print(F("d ")); h %= 24; }
    Serial.print(h); Serial.print(F("h "));
    Serial.print(m); Serial.println(F("m"));
  } else {
    Serial.print(mins); Serial.println(F("m"));
  }
}
#endif

static void _updateStatus(Status& s) {
  strlcpy(s.ip,       WiFi.localIP().toString().c_str(), sizeof(s.ip));
  strlcpy(s.mac,      WiFi.macAddress().c_str(),         sizeof(s.mac));
  strlcpy(s.ssid,     WiFi.SSID().c_str(),               sizeof(s.ssid));
  strlcpy(s.hostname, Net::hostname,                     sizeof(s.hostname));
  strlcpy(s.version,  VERSION,                           sizeof(s.version));
  strlcpy(s.desc,     Cfg::g.desc,                       sizeof(s.desc));
  s.rssi    = WiFi.RSSI();
  s.memfree = ESP.getFreeHeap();
  s.uptime  = _uptime;
#if defined(ESP32)
  snprintf(s.resetreason, sizeof(s.resetreason), "%d", esp_reset_reason());
  snprintf(s.cpu, sizeof(s.cpu), "%s(v%d) CPU%d",
    ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
  s.memfrag = (s.memfree > 0)
    ? (uint8_t)((100 * (s.memfree - ESP.getMaxAllocHeap())) / s.memfree)
    : 0;
#elif defined(ESP8266)
  strlcpy(s.resetreason, ESP.getResetReason().c_str(), sizeof(s.resetreason));
  snprintf(s.cpu, sizeof(s.cpu), "%u", ESP.getChipId());
  s.memfrag = ESP.getHeapFragmentation();
#endif
}

// ── Public API ─────────────────────────────────────────────────────────────────

void Net::begin(Status& s) {
  // Derive hostname and nodeId from MAC (works before WiFi.begin())
  String mac = WiFi.macAddress();
  mac = mac.substring(12);
  mac.replace(":", "");
  strlcpy(Net::nodeId,   mac.c_str(), sizeof(Net::nodeId));
#ifdef ESP8266
  snprintf(Net::hostname, sizeof(Net::hostname), "esp8266-%s", Net::nodeId);
#else
  snprintf(Net::hostname, sizeof(Net::hostname), "esp32-%s",   Net::nodeId);
#endif

  Serial.print(F("> NodeID: ")); Serial.println(Net::nodeId);

  _connectWiFi();

  if (WiFi.status() != WL_CONNECTED) {
#ifdef REQUIRES_INTERNET
    Serial.println(F("> [Net] No WiFi — rebooting"));
    ESP.restart();
#endif
    return;
  }

  if (!MDNS.begin(Net::hostname)) {
    Serial.println(F("> [mDNS] ERR"));
  } else {
    Serial.print(F("> [mDNS] http://"));
    Serial.print(Net::hostname);
    Serial.println(F(".local"));
    MDNS.addService("http", "tcp", 80);
  }

  ArduinoOTA.setHostname(Net::hostname);
  ArduinoOTA.onStart([]()  { Serial.println(F("> [OTA] Start")); });
  ArduinoOTA.onEnd([]()    { Serial.println(F("> [OTA] Done — rebooting")); });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.print(F("> [OTA] ERR ")); Serial.println(e);
  });
  ArduinoOTA.begin();
  Serial.println(F("> [OTA]  espota ready"));

  _mqtt.setServer(Cfg::g.mqtt_server, Cfg::g.mqtt_port);
  _mqtt.setBufferSize(512);
  _connectMqtt();

  _ntp.begin();
  _ntpUpdate();

  _updateStatus(s);
  s.boottime = _ntp.getEpochTime(); // raw UTC — JS uptime = Date.now()/1000 - boottime
  _markAt = millis();
}

bool Net::loop(Status& s) {
  ArduinoOTA.handle();
#ifdef ESP8266
  MDNS.update();
#endif
  // MQTT keepalive
  if (WiFi.status() == WL_CONNECTED) {
    if (!_mqtt.connected()) {
      unsigned long now = millis();
      if (now - _reconnectAt >= 5000) {
        _reconnectAt = now;
        _connectMqtt();
      }
    } else {
      _mqtt.loop();
    }
  }

  // 1-minute mark
  if (millis() - _markAt < 60000UL) return false;
  _markAt += 60000UL;
  _uptime++;

#ifdef VERBOSE
  _printUptime(_uptime);
#endif

  if (WiFi.status() != WL_CONNECTED) {
#ifdef REQUIRES_INTERNET
    if (_uptime % 5 == 0) {
      Serial.println(F("> [WiFi] Lost — rebooting"));
      ESP.restart();
    }
#endif
    return false;
  }

  _connectMqtt();
  _ntpUpdate();
  _updateStatus(s);
  if (s.boottime == 0) {
    time_t t = _ntp.getEpochTime();
    if (t > 1577836800) s.boottime = t - (time_t)(_uptime * 60);
  }
  return true; // signal main to push WS status
}

bool Net::publish(const char* topic, const char* payload, bool retained) {
  if (!_mqtt.connected()) _connectMqtt();
  if (!_mqtt.connected()) return false;
  bool ok = _mqtt.publish(topic, payload, retained);
#ifdef VERBOSE
  Serial.println(ok ? F("> [MQTT] Published") : F("> [MQTT] Publish failed"));
#endif
  return ok;
}

time_t Net::now() {
  time_t utc = _ntp.getEpochTime();
  return utc + _localOffset(utc);
}

time_t Net::nowUtc() {
  return _ntp.getEpochTime();
}
