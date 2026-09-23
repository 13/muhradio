#pragma once

// Decides whether a wake from a button/PIR/radar input is worth a packet.
// Pure logic (no AVR registers) so it is unit-tested natively (test_wake).
//
// Transmits on the first wake after power-on (VCC announce, as before) and on
// a debounced transition into the active level — press, motion start. The
// inactive edge (release, motion end) and bounce that settles on the level
// already seen go straight back to sleep. AVR power-down keeps SRAM alive, so
// the state survives every sleep.
struct WakeEdge {
  bool active = false;
  bool boot   = true;

  // a: the debounced pin level is the active one
  bool update(bool a) {
    bool rise = a && !active;
    active = a;
    if (boot) { boot = false; return true; }
    return rise;
  }
};
