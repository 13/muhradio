#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <time.h>

struct BresserPacket {
  char json[256];
  char topic[64];
  bool valid;
};

#ifdef USE_BRESSER
namespace Bresser {
  void         init();
  bool         pending();
  BresserPacket decode(time_t ts, const char* nodeId);
}
#endif
