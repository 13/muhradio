// Hold-to-confirm logic for garage-door button nodes (BUTTON_HOLD_MS).
// Run: pio test -e native_test
#include <unity.h>
#include "../../src/sensors/hold_gate.h"

void setUp() {}
void tearDown() {}

// Simulated clock: tick() advances 10 ms, like the firmware's delay(10).
// Each test describes the button as "down" over time windows.
struct Sim {
  uint32_t t = 0;
  uint32_t ticks = 0;
  bool (*down)(uint32_t);
  bool run(uint16_t hold = 1000, uint16_t gap = 30) {
    const HoldGate g{hold, gap};
    return g.wait([this] { return down(t); },
                  [this] { return t; },
                  [this](uint32_t) { t += 10; ticks++; });
  }
};

static void test_released_early_aborts() {
  Sim s{0, 0, [](uint32_t t) { return t < 500; }};
  TEST_ASSERT_FALSE(s.run());
  TEST_ASSERT_TRUE(s.t < 600);  // gives up shortly after the release
}

static void test_full_hold_fires() {
  Sim s{0, 0, [](uint32_t) { return true; }};
  TEST_ASSERT_TRUE(s.run());
  TEST_ASSERT_EQUAL_UINT32(1000, s.t); // fires at the hold time, not on release
}

static void test_short_dropout_tolerated() {
  Sim s{0, 0, [](uint32_t t) { return !(t >= 400 && t < 420); }}; // 20 ms gap
  TEST_ASSERT_TRUE(s.run());
}

static void test_long_dropout_aborts() {
  Sim s{0, 0, [](uint32_t t) { return !(t >= 400 && t < 450); }}; // 50 ms gap
  TEST_ASSERT_FALSE(s.run());
}

static void test_released_inside_gap_at_deadline_aborts() {
  // Let go at 990 ms: the deadline falls inside the gap window, but the
  // button never comes back, so it must not fire.
  Sim s{0, 0, [](uint32_t t) { return t < 990; }};
  TEST_ASSERT_FALSE(s.run());
}

static void test_dropout_across_deadline_fires_on_return() {
  // Chatter across the deadline: fires once the contact is back.
  Sim s{0, 0, [](uint32_t t) { return !(t >= 990 && t < 1010); }};
  TEST_ASSERT_TRUE(s.run());
  TEST_ASSERT_EQUAL_UINT32(1010, s.t);
}

static void test_zero_gap_any_release_aborts() {
  Sim s{0, 0, [](uint32_t t) { return t != 500; }};
  TEST_ASSERT_FALSE(s.run(1000, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_released_early_aborts);
  RUN_TEST(test_full_hold_fires);
  RUN_TEST(test_short_dropout_tolerated);
  RUN_TEST(test_long_dropout_aborts);
  RUN_TEST(test_released_inside_gap_at_deadline_aborts);
  RUN_TEST(test_dropout_across_deadline_fires_on_return);
  RUN_TEST(test_zero_gap_any_release_aborts);
  return UNITY_END();
}
