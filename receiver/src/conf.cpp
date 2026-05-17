#include "conf.h"
#include "config.h"
#include <LittleFS.h>
#ifdef ESP32
  #include <sys/stat.h>
#endif

namespace Cfg { Conf g; }

// Simple flat-JSON string extractor; handles \" and \\ escapes.
static bool _getStr(const char* j, const char* key, char* out, size_t n) {
  char pat[80];
  snprintf(pat, sizeof(pat), "\"%s\":\"", key);
  const char* p = strstr(j, pat);
  if (!p) return false;
  p += strlen(pat);
  size_t i = 0;
  for (; *p && i + 1 < n; p++) {
    if (*p == '\\' && *(p + 1)) { p++; out[i++] = *p; continue; }
    if (*p == '"') break;
    out[i++] = *p;
  }
  out[i] = '\0';
  return true;
}

static bool _getU16(const char* j, const char* key, uint16_t& out) {
  char pat[80];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(j, pat);
  if (!p) return false;
  out = (uint16_t)atoi(p + strlen(pat));
  return true;
}

static bool _getI16(const char* j, const char* key, int16_t& out) {
  char pat[80];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(j, pat);
  if (!p) return false;
  out = (int16_t)atoi(p + strlen(pat));
  return true;
}

static bool _getU8(const char* j, const char* key, uint8_t& out) {
  char pat[80];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(j, pat);
  if (!p) return false;
  out = (uint8_t)atoi(p + strlen(pat));
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
  g.tz_offset = TZ_OFFSET;
  g.dst_mode  = TZ_DST_MODE;

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

  _getStr(json.c_str(), "wifi_ssid",   g.wifi_ssid,   sizeof(g.wifi_ssid));
  _getStr(json.c_str(), "wifi_pass",   g.wifi_pass,   sizeof(g.wifi_pass));
  _getStr(json.c_str(), "mqtt_server", g.mqtt_server, sizeof(g.mqtt_server));
  _getU16(json.c_str(), "mqtt_port",   g.mqtt_port);
  _getStr(json.c_str(), "mqtt_user",   g.mqtt_user,   sizeof(g.mqtt_user));
  _getStr(json.c_str(), "mqtt_pass",   g.mqtt_pass,   sizeof(g.mqtt_pass));
  _getStr(json.c_str(), "desc",        g.desc,        sizeof(g.desc));
  _getI16(json.c_str(), "tz_offset",  g.tz_offset);
  _getU8 (json.c_str(), "dst_mode",   g.dst_mode);
  Serial.println(F("> [Cfg] Loaded /config.json"));
}

bool Cfg::save() {
  // Escape " and \ for JSON safety
  auto esc = [](const char* src, char* dst, size_t n) {
    size_t i = 0;
    for (; *src && i + 2 < n; src++) {
      if (*src == '"' || *src == '\\') dst[i++] = '\\';
      dst[i++] = *src;
    }
    dst[i] = '\0';
  };

  char ws[128], wp[128], ms[128], mu[64], mp[128], ds[64];
  esc(g.wifi_ssid,   ws, sizeof(ws));
  esc(g.wifi_pass,   wp, sizeof(wp));
  esc(g.mqtt_server, ms, sizeof(ms));
  esc(g.mqtt_user,   mu, sizeof(mu));
  esc(g.mqtt_pass,   mp, sizeof(mp));
  esc(g.desc,        ds, sizeof(ds));

  char buf[800];
  snprintf(buf, sizeof(buf),
    "{\"wifi_ssid\":\"%s\",\"wifi_pass\":\"%s\","
    "\"mqtt_server\":\"%s\",\"mqtt_port\":%u,"
    "\"mqtt_user\":\"%s\",\"mqtt_pass\":\"%s\","
    "\"desc\":\"%s\",\"tz_offset\":%d,\"dst_mode\":%u}",
    ws, wp, ms, g.mqtt_port, mu, mp, ds, (int)g.tz_offset, (unsigned)g.dst_mode);

  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;
  f.print(buf);
  f.close();
  Serial.println(F("> [Cfg] Saved /config.json"));
  return true;
}
