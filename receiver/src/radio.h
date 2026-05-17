#pragma once
#include <Arduino.h>

struct RxPacket {
  uint8_t buf[80];
  uint8_t len;
  int     rssi;
  float   snr;
};

struct DecodedPacket {
  char json[256];
  char topic[64];
  bool valid;
};

namespace Radio {
  void          init();
  bool          pending();
  RxPacket      take();
  DecodedPacket decode(const RxPacket& pkt, time_t ts, const char* nodeId);
}
