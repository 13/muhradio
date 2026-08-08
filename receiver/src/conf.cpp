#include "conf.h"
#include "config.h"
#include "jsonbuilder.h"
#include <LittleFS.h>
#ifdef ESP32
  #include <sys/stat.h>
#endif

namespace Cfg { Conf g; }

// Parsing lives in confjson.h (pure, unit-tested in test/test_conf).
#include "confjson.h"

static bool _getU16(const char* j, const char* key, uint16_t& out) {
  long v;
  if (!jsonGetLong(j, key, v)) return false;
  out = (uint16_t)v;
  return true;
}

static bool _getI16(const char* j, const char* key, int16_t& out) {
  long v;
  if (!jsonGetLong(j, key, v)) return false;
  out = (int16_t)v;
  return true;
}

static bool _getU8(const char* j, const char* key, uint8_t& out) {
  long v;
  if (!jsonGetLong(j, key, v)) return false;
  out = (uint8_t)v;
  return true;
}

void Cfg::load() {
  // Compile-time defaults (from config.h / pio_secrets.py build flags)
  strlcpy(g.wifi_ssid,   WIFI_SSID,          sizeof(g.wifi_ssid));
  strlcpy(g.wifi_pass,   WIFI_PASS,          sizeof(g.wifi_pass));
  strlcpy(g.mqtt_server, MQTT_SERVER,        sizeof(g.mqtt_server));
  g.mqtt_port = MQTT_PORT;
  strlcpy(g.mqtt_user,   MQTT_USER,          sizeof(g.mqtt_user));
  strlcpy(g.mqtt_pass,   MQTT_PASS,          sizeof(g.mqtt_pass));
  strlcpy(g.desc,        DEVICE_DESCRIPTION, sizeof(g.desc));
  g.tz_offset  = TZ_OFFSET;
  g.dst_mode   = TZ_DST_MODE;
  g.node_stats = NODE_STATS;
  strlcpy(g.ntp1, NTP1, sizeof(g.ntp1));
  strlcpy(g.ntp2, NTP2, sizeof(g.ntp2));
  strlcpy(g.ntp3, NTP3, sizeof(g.ntp3));

  // Override with persisted config if it exists
#ifdef ESP8266
  if (!LittleFS.begin()) return;      // ESP8266 LittleFS.begin() takes no args
#else
  if (!LittleFS.begin(false)) return; // filesystem not formatted yet — use defaults
#endif

#ifdef ESP32
  struct stat _st;
  if (stat("/littlefs/config.json", &_st) != 0) return;
#endif
  File f = LittleFS.open("/config.json", "r");
  if (!f) return;
  String json = f.readString();
  f.close();

  // Schema version: absent = 0 (pre-versioning). Newer-than-known configs
  // still load field-by-field, but leave a trace in the log.
  long ver = 0;
  jsonGetLong(json.c_str(), "cfg_ver", ver);
  if (ver > CFG_VER)
    Serial.printf("> [Cfg] config.json cfg_ver=%ld newer than firmware (%d)\n",
                  ver, CFG_VER);

  jsonGetStr(json.c_str(), "wifi_ssid",   g.wifi_ssid,   sizeof(g.wifi_ssid));
  jsonGetStr(json.c_str(), "wifi_pass",   g.wifi_pass,   sizeof(g.wifi_pass));
  jsonGetStr(json.c_str(), "mqtt_server", g.mqtt_server, sizeof(g.mqtt_server));
  _getU16(json.c_str(), "mqtt_port",   g.mqtt_port);
  jsonGetStr(json.c_str(), "mqtt_user",   g.mqtt_user,   sizeof(g.mqtt_user));
  jsonGetStr(json.c_str(), "mqtt_pass",   g.mqtt_pass,   sizeof(g.mqtt_pass));
  jsonGetStr(json.c_str(), "desc",        g.desc,        sizeof(g.desc));
  _getI16(json.c_str(), "tz_offset",  g.tz_offset);
  _getU8 (json.c_str(), "dst_mode",   g.dst_mode);
  _getU8 (json.c_str(), "node_stats", g.node_stats);
  jsonGetStr(json.c_str(), "ntp1",       g.ntp1,        sizeof(g.ntp1));
  jsonGetStr(json.c_str(), "ntp2",       g.ntp2,        sizeof(g.ntp2));
  jsonGetStr(json.c_str(), "ntp3",       g.ntp3,        sizeof(g.ntp3));
  Serial.println(F("> [Cfg] Loaded /config.json"));
}

bool Cfg::save() {
  // Shared JSON escaper (jsonbuilder.h) — also handles control chars/UTF-8
  auto esc = [](const char* src, char* dst, size_t n) {
    jsonAppendEscaped(dst, 0, n, src);
  };

  char ws[128], wp[128], ms[128], mu[64], mp[128], ds[64];
  char n1[128], n2[128], n3[128];
  esc(g.wifi_ssid,   ws, sizeof(ws));
  esc(g.wifi_pass,   wp, sizeof(wp));
  esc(g.mqtt_server, ms, sizeof(ms));
  esc(g.mqtt_user,   mu, sizeof(mu));
  esc(g.mqtt_pass,   mp, sizeof(mp));
  esc(g.desc,        ds, sizeof(ds));
  esc(g.ntp1,        n1, sizeof(n1));
  esc(g.ntp2,        n2, sizeof(n2));
  esc(g.ntp3,        n3, sizeof(n3));

  // Worst case (all fields at max escaped length) is ~1215 bytes
  char buf[1280];
  int len = snprintf(buf, sizeof(buf),
    "{\"cfg_ver\":%d,"
    "\"wifi_ssid\":\"%s\",\"wifi_pass\":\"%s\","
    "\"mqtt_server\":\"%s\",\"mqtt_port\":%u,"
    "\"mqtt_user\":\"%s\",\"mqtt_pass\":\"%s\","
    "\"desc\":\"%s\",\"tz_offset\":%d,\"dst_mode\":%u,\"node_stats\":%u,"
    "\"ntp1\":\"%s\",\"ntp2\":\"%s\",\"ntp3\":\"%s\"}",
    CFG_VER,
    ws, wp, ms, g.mqtt_port, mu, mp, ds, (int)g.tz_offset, (unsigned)g.dst_mode,
    (unsigned)g.node_stats, n1, n2, n3);
  if (len < 0 || len >= (int)sizeof(buf)) {
    Serial.println(F("> [Cfg] config too large — not saved"));
    return false; // never write a truncated (invalid) config.json
  }

  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;
  f.print(buf);
  f.close();
  Serial.println(F("> [Cfg] Saved /config.json"));
  return true;
}
