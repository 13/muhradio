#pragma once
#ifdef USE_BRESSER
#include <Arduino.h>

struct BresserPacket {
  char json[256];
  char topic[64];
  bool valid;
};

namespace Bresser {
  void         init();
  bool         pending();
  BresserPacket decode(time_t ts, const char* nodeId);
}
#endif
