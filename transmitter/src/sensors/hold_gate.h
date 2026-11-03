#pragma once
#include <stdint.h>

// Hold-to-confirm for a button press (garage door): the press only counts once
// the button has been held for holdMs. Pure logic (no AVR registers, the pin,
// clock and wait are passed in) so it is unit-tested natively (test_hold).
//
// Releases shorter than gapMs are contact chatter, not a release: a worn
// contact flickering under the finger must not abort the hold. The hold only
// completes while the button is down, so letting go just before holdMs inside
// a gap doesn't count.
struct HoldGate {
  uint16_t holdMs;
  uint16_t gapMs;

  // pressed(): current debounced-or-raw pin state, true = down
  // now():     milliseconds (millis())
  // tick(el):  called between samples with the elapsed hold time; waits a
  //            little (and drives feedback). It must let now() advance.
  // Returns true if held for holdMs, false if released for longer than gapMs.
  template <class Pressed, class Now, class Tick>
  bool wait(Pressed pressed, Now now, Tick tick) const {
    const uint32_t t0 = now();
    uint32_t lastDown = t0;
    for (;;) {
      const uint32_t t = now();
      const bool down = pressed();
      if (down) lastDown = t;
      else if ((uint32_t)(t - lastDown) > gapMs) return false;
      if (down && (uint32_t)(t - t0) >= holdMs) return true;
      tick((uint32_t)(t - t0));
    }
  }
};
