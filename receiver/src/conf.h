#pragma once
#include <Arduino.h>

struct Conf {
  char     wifi_ssid  [64];
  char     wifi_pass  [64];
  char     mqtt_server[64];
  uint16_t mqtt_port;
  char     mqtt_user  [32];
  char     mqtt_pass  [64];
  char     desc       [64];
  int16_t  tz_offset;   // UTC offset in minutes, e.g. 60 = UTC+1
  uint8_t  dst_mode;    // 0=standard only, 1=summer only, 2=auto EU rules
};

namespace Cfg {
  extern Conf g;
  // Call before Net::begin(). Mounts LittleFS, loads /config.json, falls back to config.h defaults.
  void load();
  // Persists current Cfg::g to /config.json on LittleFS.
  bool save();
}
