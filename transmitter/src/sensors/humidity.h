#pragma once
#include <Arduino.h>
#include "../packet.h"

// Relative-humidity conversion (Si7021) and compensation (BME680) can yield
// slightly <0% or >100% at the extremes — clamp those; values further out are
// read errors and are skipped. Returns false when the value was skipped.
inline bool addHumidity(Packet& pkt, Field f, float h) {
  if (isnan(h) || h <= -5.0f || h >= 105.0f) return false;
  h = constrain(h, 0.0f, 100.0f);
  pkt.addI16(f, (int16_t)round(h * 10.0f));
  return true;
}
