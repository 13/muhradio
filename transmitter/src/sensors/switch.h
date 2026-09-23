#pragma once
#include <Arduino.h>
#include "../packet.h"
#include "wake.h"

// Contact settle time after a wake. A rocker bounces ~10-50 ms; a reed switch
// bounces ~1-10 ms but can chatter as the magnet crosses its threshold.
#ifndef SWITCH_DEBOUNCE_MS
#define SWITCH_DEBOUNCE_MS 30
#endif

namespace Switch {
  // Level we last transmitted; -1 = nothing reported yet, so the first loop()
  // after power-on always announces the current position. AVR power-down keeps
  // SRAM alive, so this survives every wake without touching EEPROM.
  static int8_t  _last  = -1;
  static uint8_t _level = 0;

  inline void setup() {
    pinMode(SENSOR_PIN_SWITCH, INPUT_PULLUP);
#ifdef VERBOSE
    Serial.print(F("Switch: "));
    Serial.println(digitalRead(SENSOR_PIN_SWITCH) == HIGH ? F("HIGH") : F("LOW"));
#endif
    Wake::attach(SENSOR_PIN_SWITCH); // both edges, any pin (see wake.h)
  }

  // True if this wake is worth a packet: debounces, then drops chatter that
  // settled back on the position we already reported.
  inline bool pending() {
    if (!Wake::take()) return false;
    uint8_t s = Wake::settle(SENSOR_PIN_SWITCH, SWITCH_DEBOUNCE_MS);

    // INPUT_PULLUP: LOW = contact closed → 1
#ifdef SWITCH_INVERT
    _level = (s == HIGH) ? 1 : 0;
#else
    _level = (s == HIGH) ? 0 : 1;
#endif

    if ((int8_t)_level == _last) {
#ifdef VERBOSE
      Serial.print(F("Switch: unchanged (")); Serial.print(_level); Serial.println(')');
#endif
      return false;
    }
    _last = (int8_t)_level;
    return true;
  }

  inline void read(Packet& pkt) {
    pkt.addU8(Field::SWITCH, _level);
#ifdef VERBOSE
    Serial.print(F("Switch: ")); Serial.println(_level);
#endif
  }
}

