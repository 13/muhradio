#pragma once
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "bresser.h"

// Bresser 7-in-1 decoder — pure logic, unit-tested in test/test_bresser.
// Payload: 26 bytes whitened with XOR 0xAA, LFSR-16 digest
// (gen=0x8810, key=0xba95, final_xor=0x6df1) over de-whitened bytes 2..24.

static constexpr int BRESSER_PAYLOAD_LEN = 26;

static uint16_t lfsr16(const uint8_t* msg, unsigned n, uint16_t gen, uint16_t key) {
  uint16_t sum = 0;
  for (unsigned k = 0; k < n; ++k) {
    uint8_t d = msg[k];
    for (int i = 7; i >= 0; --i) {
      if ((d >> i) & 1) sum ^= key;
      key = (key & 1) ? ((key >> 1) ^ gen) : (key >> 1);
    }
  }
  return sum;
}

// raw       — CC1101 FIFO bytes; raw[0] is the first whitened payload byte
// rssi      — signal strength in dBm
// topicBase — MQTT prefix, e.g. "muh/bresser"
// Returns true and fills `out` on success.
static bool decode7in1(const uint8_t* raw, int rssi, time_t ts,
                       const char* nodeId, const char* topicBase,
                       BresserPacket& out) {
  // De-whiten 26-byte payload
  const uint8_t* msg = raw;
  uint8_t w[BRESSER_PAYLOAD_LEN];
  for (int i = 0; i < BRESSER_PAYLOAD_LEN; i++) w[i] = msg[i] ^ 0xAA;

  // LFSR-16 integrity check
  uint16_t chk    = ((uint16_t)w[0] << 8) | w[1];
  uint16_t digest = lfsr16(&w[2], 23, 0x8810, 0xba95);
  if ((chk ^ digest) != 0x6df1) {
#if defined(DEBUG) && defined(ARDUINO)
    Serial.printf("> [Bresser] digest %04X^%04X=%04X (want 6DF1)\n",
                  chk, digest, chk ^ digest);
#endif
    return false;
  }

  // s_type is extracted from the RAW (non-de-whitened) byte
  uint8_t s_type = msg[6] >> 4;
  if (s_type != 1) {
    // Only handle SENSOR_TYPE_WEATHER1; skip CO2, VOC, PM subtypes
#if defined(DEBUG) && defined(ARDUINO)
    Serial.printf("> [Bresser] s_type=%u (skip)\n", s_type);
#endif
    return false;
  }

  uint16_t id      = ((uint16_t)w[2] << 8) | w[3];
  uint8_t  chan    = msg[6] & 0x07;
  bool     bat_ok  = ((w[15] & 0x06) != 0x06);

  int wdir     = (w[4] >> 4)*100 + (w[4] & 0x0f)*10 + (w[5] >> 4);
  int wgst     = (w[7] >> 4)*100 + (w[7] & 0x0f)*10 + (w[8] >> 4);
  int wavg     = (w[8] & 0x0f)*100 + (w[9] >> 4)*10 + (w[9] & 0x0f);
  int rain_raw = (w[10] >> 4)*100000 + (w[10] & 0x0f)*10000
               + (w[11] >> 4)*1000   + (w[11] & 0x0f)*100
               + (w[12] >> 4)*10     + (w[12] & 0x0f);
  int temp_raw = (w[14] >> 4)*100 + (w[14] & 0x0f)*10 + (w[15] >> 4);
  int humidity = (w[16] >> 4)*10  + (w[16] & 0x0f);
  int lux_raw  = (w[17] >> 4)*100000 + (w[17] & 0x0f)*10000
               + (w[18] >> 4)*1000   + (w[18] & 0x0f)*100
               + (w[19] >> 4)*10     + (w[19] & 0x0f);
  int uv_raw   = (w[20] >> 4)*100 + (w[20] & 0x0f)*10 + (w[21] >> 4);

  float temp_c = (temp_raw > 600) ? (temp_raw - 1000) * 0.1f : temp_raw * 0.1f;

  snprintf(out.topic, sizeof(out.topic), "%s/%04X/json", topicBase, id);
  snprintf(out.json, sizeof(out.json),
    "{\"id\":\"%04X\",\"ch\":%u,\"bat\":%s,"
    "\"temp\":%.1f,\"hum\":%d,"
    "\"wind\":%.1f,\"gust\":%.1f,\"dir\":%d,"
    "\"rain\":%.1f,\"uv\":%.1f,\"lux\":%.1f,"
    "\"rssi\":%d,\"node\":\"%s\",\"ts\":%ld}",
    id, chan, bat_ok ? "1" : "0",
    temp_c, humidity,
    wavg * 0.1f, wgst * 0.1f, wdir,
    rain_raw * 0.1f, uv_raw * 0.1f, lux_raw * 0.001f,
    rssi, nodeId, (long)ts);

  out.valid = true;
  return true;
}
