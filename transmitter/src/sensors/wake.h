#pragma once
#include <Arduino.h>
#include "wake_edge.h"

// Pin-change wake shared by the interrupt-driven sensors (button, PIR, radar,
// switch). Not attachInterrupt(): power-down stops the I/O clock, so INT0/INT1
// wake on LOW level only (datasheet table 12-1 note 3) and an edge-triggered
// wake never comes back. PCINT is detected asynchronously, fires on both edges
// and works on any digital pin.
namespace Wake {
  // Set by the ISR; starts true so the first loop() after power-on samples
  // the pin.
  static volatile bool _flag = true;

  inline void attach(uint8_t pin) {
    *digitalPinToPCMSK(pin) |= bit(digitalPinToPCMSKbit(pin));
    PCIFR |= bit(digitalPinToPCICRbit(pin)); // drop a stale flag
    PCICR |= bit(digitalPinToPCICRbit(pin)); // enable the group
  }

  // Read and clear the flag atomically.
  inline bool take() {
    uint8_t sreg = SREG;
    cli();
    bool f = _flag;
    _flag = false;
    SREG = sreg;
    return f;
  }

  // Level after a settle delay. Bounce edges fired during the delay are
  // dropped so they don't queue another wake for the same transition.
  inline uint8_t settle(uint8_t pin, uint16_t ms) {
    delay(ms); // Timer0 is left running (see main.cpp)
    uint8_t s = digitalRead(pin);
    take();
    return s;
  }

  // One-line pending() for button/PIR/radar: true if this wake should transmit.
  inline bool pending(WakeEdge& e, uint8_t pin, uint8_t activeLevel, uint16_t ms) {
    if (!take()) return false;
    return e.update(settle(pin, ms) == activeLevel);
  }
}

// The sensor pin is a build flag, so its port's PCINT group isn't known at
// preprocessing time — claim all three vectors and let the unused two cost a
// few bytes of flash. Nothing else in the firmware uses pin change interrupts.
ISR(PCINT0_vect) { Wake::_flag = true; }
ISR(PCINT1_vect) { Wake::_flag = true; }
ISR(PCINT2_vect) { Wake::_flag = true; }
