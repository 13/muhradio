#pragma once
#include <Arduino.h>
#include "../packet.h"
#include "../power.h"

namespace PIR {
  static volatile bool _detected = false;
  static volatile bool _state    = false;

  static void _isr() {
    _state    = digitalRead(SENSOR_PIN_PIR);
    _detected = true;
  }

  inline void setup() {
    pinMode(SENSOR_PIN_PIR, INPUT);
#ifdef VERBOSE
    Serial.print(F("PIR: "));
    Serial.println(digitalRead(SENSOR_PIN_PIR) == HIGH ? F("HIGH") : F("LOW"));
#endif
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN_PIR), _isr, RISING);
    Power::sleepDeep(); // block until first motion event
  }

  inline void read(Packet& pkt) {
    if (!_detected) return;
    bool s = _state;
    _detected = false;
    pkt.addU8(Field::PIR, s ? 1 : 0);
#ifdef VERBOSE
    Serial.print(F("PIR: ")); Serial.println(s ? 1 : 0);
#endif
  }
}
