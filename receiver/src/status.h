#pragma once
#include <Arduino.h>
#include "jsonbuilder.h"

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
  // String fields go through the shared escaper — a quote in desc/ssid must
  // not break the websocket JSON.
  void toJson(char* buf, size_t cap) const {
    size_t n = snprintf(buf, cap,
      "{\"uptime\":%d,\"rssi\":%d,\"memfree\":%d,\"memfrag\":%d,",
      uptime, rssi, memfree, memfrag);
    if (n > cap - 1) n = cap - 1;

    // Drop a field whole when it doesn't fit — output must stay valid JSON
    auto kvs = [&](const char* k, const char* v) {
      size_t save = n;
      n += snprintf(buf + n, cap - n, "\"%s\":\"", k);
      if (n > cap - 1) n = cap - 1;
      bool tr = false;
      n = jsonAppendEscaped(buf, n, cap, v, &tr);
      if (tr || n + 4 > cap) { n = save; buf[n] = '\0'; return; }
      buf[n++] = '"'; buf[n++] = ',';
    };
    kvs("ssid",        ssid);
    kvs("ip",          ip);
    kvs("mac",         mac);
    kvs("cpu",         cpu);
    kvs("hostname",    hostname);
    kvs("desc",        desc);
    kvs("resetreason", resetreason);
    kvs("version",     version);

    n += snprintf(buf + n, cap - n, "\"boottime\":%ld,\"timestamp\":%ld,\"packets\":[",
                  (long)boottime, (long)timestamp);
    if (n > cap - 1) n = cap - 1;

    for (int i = 0; i < MAX_PACKETS; i++) {
      size_t save = n;
      if (n + 6 > cap) break;
      if (i > 0) buf[n++] = ',';
      buf[n++] = '"';
      bool tr = false;
      n = jsonAppendEscaped(buf, n, cap - 3, packets[i], &tr); // room for "]}\0
      if (tr || n + 4 > cap) { n = save; break; } // drop partial element whole
      buf[n++] = '"';
    }
    if (n > cap - 3) n = cap - 3; // unreachable unless cap is tiny
    buf[n++] = ']'; buf[n++] = '}'; buf[n] = '\0';
  }
};
