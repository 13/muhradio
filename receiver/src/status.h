#pragma once
#include <Arduino.h>

static constexpr int MAX_PACKETS = 5;

struct Status {
  int    uptime;       // minutes since boot
  int    rssi;         // WiFi RSSI dBm
  int    memfree;      // free heap bytes
  int    memfrag;      // heap fragmentation %
  char   ssid[33];
  char   ip[16];
  char   mac[18];
  char   cpu[48];
  char   hostname[24];
  char   desc[32];
  char   resetreason[32];
  char   version[64];
  time_t boottime;
  time_t timestamp;
  char   packets[MAX_PACKETS][256]; // last N received LoRa packets (JSON strings)

  void addPacket(const char* p) {
    for (int i = MAX_PACKETS - 1; i > 0; --i)
      memcpy(packets[i], packets[i - 1], sizeof(packets[0]));
    strlcpy(packets[0], p, sizeof(packets[0]));
  }

  // Writes the full status JSON into buf.
  // Packets are embedded as JSON strings so the frontend can JSON.parse() each one.
  void toJson(char* buf, size_t cap) const {
    int n = snprintf(buf, cap,
      "{\"uptime\":%d,\"rssi\":%d,\"memfree\":%d,\"memfrag\":%d,"
      "\"ssid\":\"%s\",\"ip\":\"%s\",\"mac\":\"%s\","
      "\"cpu\":\"%s\",\"hostname\":\"%s\",\"desc\":\"%s\","
      "\"resetreason\":\"%s\",\"version\":\"%s\","
      "\"boottime\":%ld,\"timestamp\":%ld,\"packets\":[",
      uptime, rssi, memfree, memfrag,
      ssid, ip, mac, cpu, hostname, desc,
      resetreason, version,
      (long)boottime, (long)timestamp);

    for (int i = 0; i < MAX_PACKETS; i++) {
      if (n >= (int)cap - 4) break;
      if (i > 0) buf[n++] = ',';
      if (!packets[i][0]) {
        buf[n++] = '"'; buf[n++] = '"';
        continue;
      }
      buf[n++] = '"';
      for (const char* s = packets[i]; *s && n < (int)cap - 3; s++) {
        if (*s == '"' || *s == '\\') buf[n++] = '\\';
        buf[n++] = *s;
      }
      buf[n++] = '"';
    }
    if (n < (int)cap - 2) { buf[n++] = ']'; buf[n++] = '}'; buf[n] = '\0'; }
  }
};
