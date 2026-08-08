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

    auto kvs = [&](const char* k, const char* v) {
      n += snprintf(buf + n, cap - n, "\"%s\":\"", k);
      if (n > cap - 1) n = cap - 1;
      n = jsonAppendEscaped(buf, n, cap, v);
      n += snprintf(buf + n, cap - n, "\",");
      if (n > cap - 1) n = cap - 1;
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
      if (n >= cap - 4) break;
      if (i > 0) buf[n++] = ',';
      buf[n++] = '"';
      n = jsonAppendEscaped(buf, n, cap - 2, packets[i]); // keep room for "]}"
      if (n < cap) buf[n++] = '"';
    }
    if (n < cap - 2) { buf[n++] = ']'; buf[n++] = '}'; }
    buf[n] = '\0';
  }
};
