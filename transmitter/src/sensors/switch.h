#pragma once
#include <Arduino.h>
#include "../packet.h"

// Contact settle time after a wake. A rocker bounces ~10-50 ms; a reed switch
// bounces ~1-10 ms but can chatter as the magnet crosses its threshold.
#ifndef SWITCH_DEBOUNCE_MS
#define SWITCH_DEBOUNCE_MS 30
#endif

namespace Switch {
  // Set by the pin-change ISR, cleared under cli() in pending().
  static volatile bool _changed = true;
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
    // Pin change, not attachInterrupt(): power-down stops the I/O clock, so
    // INT0/INT1 wake on LOW level only (datasheet table 12-1 note 3) and an
    // edge-triggered CHANGE never comes back. PCINT is detected asynchronously
    // and fires on both edges — and it frees the switch from D2/D3.
    *digitalPinToPCMSK(SENSOR_PIN_SWITCH) |= bit(digitalPinToPCMSKbit(SENSOR_PIN_SWITCH));
    PCIFR |= bit(digitalPinToPCICRbit(SENSOR_PIN_SWITCH)); // drop a stale flag
    PCICR |= bit(digitalPinToPCICRbit(SENSOR_PIN_SWITCH)); // enable the group
  }

  // True if this wake is worth a packet: debounces, then drops chatter that
  // settled back on the position we already reported.
  inline bool pending() {
    uint8_t sreg = SREG;
    cli();
    bool changed = _changed;
    _changed = false;
    SREG = sreg;
    if (!changed) return false;

    delay(SWITCH_DEBOUNCE_MS); // Timer0 is left running (see main.cpp)

    uint8_t s = digitalRead(SENSOR_PIN_SWITCH);
    // Bounce edges fired during the delay; drop them so they don't queue another
    // wake for the transition we are about to report.
    sreg = SREG;
    cli();
    _changed = false;
    SREG = sreg;

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

// SENSOR_PIN_SWITCH is a build flag, so the port's PCINT group isn't known at
// preprocessing time — claim all three vectors and let the unused two cost a
// few bytes of flash. Nothing else in the firmware uses pin change interrupts.
ISR(PCINT0_vect) { Switch::_changed = true; }
ISR(PCINT1_vect) { Switch::_changed = true; }
ISR(PCINT2_vect) { Switch::_changed = true; }
