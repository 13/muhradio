#pragma once
#include <Arduino.h>

struct RxPacket {
  uint8_t buf[80];
  uint8_t len;
  int     rssi;
  float   snr;
};

struct DecodedPacket {
  char     json[256];
  char     topic[64];
  bool     valid;
  bool     retained;
  uint16_t uid;    // sender node id (for the health table)
  int16_t  rssi;
  uint8_t  vcc10;  // battery V*10 if the packet carried VCC, else 0
};

namespace Radio {
  void          init();
  bool          pending();
  RxPacket      take();
  DecodedPacket decode(const RxPacket& pkt, time_t ts, const char* nodeId);
}
