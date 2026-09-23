#pragma once
#include <Arduino.h>
#include "../packet.h"
#include "wake.h"

#ifndef RADAR_SETTLE_MS
#define RADAR_SETTLE_MS 0
#endif

namespace Radar {
  static WakeEdge _edge;

  inline void setup() {
    pinMode(SENSOR_PIN_RADAR, INPUT);
#ifdef VERBOSE
    Serial.print(F("Radar: "));
    Serial.println(digitalRead(SENSOR_PIN_RADAR) == HIGH ? F("HIGH") : F("LOW"));
#endif
    Wake::attach(SENSOR_PIN_RADAR);
  }

  // HIGH = presence. Only presence start transmits; the falling edge is ignored.
  inline bool pending() {
    return Wake::pending(_edge, SENSOR_PIN_RADAR, HIGH, RADAR_SETTLE_MS);
  }

  inline void read(Packet& pkt) {
    if (!_edge.active) return; // boot announce without presence: VCC only
    pkt.addU8(Field::RADAR, 1);
#ifdef VERBOSE
    Serial.println(F("Radar: 1"));
#endif
  }
}
