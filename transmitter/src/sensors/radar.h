#pragma once
#include <Arduino.h>
#include "../packet.h"

namespace Radar {
  static volatile bool _detected = false;
  static volatile bool _state    = false;

  static void _isr() {
    _state    = digitalRead(SENSOR_PIN_RADAR);
    _detected = true;
  }

  inline void setup() {
    pinMode(SENSOR_PIN_RADAR, INPUT);
#ifdef VERBOSE
    Serial.print(F("Radar: "));
    Serial.println(digitalRead(SENSOR_PIN_RADAR) == HIGH ? F("HIGH") : F("LOW"));
#endif
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN_RADAR), _isr, RISING);
  }

  inline void read(Packet& pkt) {
    if (!_detected) return;
    bool s = _state;
    _detected = false;
    pkt.addU8(Field::RADAR, s ? 1 : 0);
#ifdef VERBOSE
    Serial.print(F("Radar: ")); Serial.println(s ? 1 : 0);
#endif
  }
}
