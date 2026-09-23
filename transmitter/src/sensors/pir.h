#pragma once
#include <Arduino.h>
#include "../packet.h"
#include "wake.h"

// PIR output is a clean logic level; no debounce needed by default.
#ifndef PIR_SETTLE_MS
#define PIR_SETTLE_MS 0
#endif

namespace PIR {
  static WakeEdge _edge;

  inline void setup() {
    pinMode(SENSOR_PIN_PIR, INPUT);
#ifdef VERBOSE
    Serial.print(F("PIR: "));
    Serial.println(digitalRead(SENSOR_PIN_PIR) == HIGH ? F("HIGH") : F("LOW"));
#endif
    Wake::attach(SENSOR_PIN_PIR);
  }

  // HIGH = motion. Only motion start transmits; the falling edge is ignored.
  inline bool pending() {
    return Wake::pending(_edge, SENSOR_PIN_PIR, HIGH, PIR_SETTLE_MS);
  }

  inline void read(Packet& pkt) {
    if (!_edge.active) return; // boot announce without motion: VCC only
    pkt.addU8(Field::PIR, 1);
#ifdef VERBOSE
    Serial.println(F("PIR: 1"));
#endif
  }
}
