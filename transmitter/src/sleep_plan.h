#pragma once
#include <stdint.h>

// Splits a millisecond wait into the ATmega328P watchdog power-down periods
// (LowPower's SLEEP_15MS ... SLEEP_8S), longest first, rounded UP to the next
// 15 ms so the plan never undersleeps. Pure: no Arduino dependency, unit
// tested natively (test/test_sleep_plan). power.h maps the values to
// LowPower's period_t and runs them.
static const uint16_t SLEEP_PLAN_PERIOD_MS[] = {8000, 4000, 2000, 1000, 500, 250, 120, 60, 30, 15};
static const uint8_t  SLEEP_PLAN_PERIODS     = sizeof(SLEEP_PLAN_PERIOD_MS) / sizeof(SLEEP_PLAN_PERIOD_MS[0]);
// 65535 ms = 8 x 8000 + 1000 + 500 + 120 + 15 (+ tail step): 12 entries at most.
static const uint8_t  SLEEP_PLAN_MAX         = 16;

// Fills out[0..max) with periods (ms) that sum to >= ms and < ms + 15.
// Returns the number of entries written; stops early when out is full.
inline uint8_t sleepPlan(uint16_t ms, uint16_t* out, uint8_t max) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SLEEP_PLAN_PERIODS && n < max; i++) {
    while (ms >= SLEEP_PLAN_PERIOD_MS[i] && n < max) {
      out[n++] = SLEEP_PLAN_PERIOD_MS[i];
      ms -= SLEEP_PLAN_PERIOD_MS[i];
    }
  }
  if (ms > 0 && n < max) out[n++] = 15;  // round the remainder up
  return n;
}
