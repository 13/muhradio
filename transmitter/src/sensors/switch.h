#pragma once
#include <Arduino.h>
#include "../packet.h"

namespace Switch {
  static volatile bool _changed = true;
  static volatile bool _state   = false;

  static void _isr() {
    _state   = digitalRead(SENSOR_PIN_SWITCH);
    _changed = true;
  }

  inline void setup() {
    pinMode(SENSOR_PIN_SWITCH, INPUT_PULLUP);
#ifdef VERBOSE
    Serial.print(F("Switch: "));
    Serial.println(digitalRead(SENSOR_PIN_SWITCH) == HIGH ? F("HIGH") : F("LOW"));
#endif
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN_SWITCH), _isr, CHANGE);
  }

  inline void read(Packet& pkt) {
    if (!_changed) return;
    bool s = _state;
    _changed = false;
    pkt.addU8(Field::SWITCH, s ? 0 : 1);
#ifdef VERBOSE
    Serial.print(F("Switch: ")); Serial.println(s ? 0 : 1);
#endif
  }
}
