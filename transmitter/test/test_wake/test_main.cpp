// Wake decision logic for button/PIR/radar: which wakes are worth a packet.
// Run: pio test -e native_test
#include <unity.h>
#include "../../src/sensors/wake_edge.h"

void setUp() {}
void tearDown() {}

static void test_boot_always_announces() {
  WakeEdge e;
  TEST_ASSERT_TRUE(e.update(false)); // first wake after power-on: VCC announce
  WakeEdge p;
  TEST_ASSERT_TRUE(p.update(true));  // pressed at boot: announce too
}

static void test_press_reported_release_not() {
  WakeEdge e;
  e.update(false);                   // boot
  TEST_ASSERT_TRUE (e.update(true));  // press
  TEST_ASSERT_FALSE(e.update(false)); // release: straight back to sleep
  TEST_ASSERT_TRUE (e.update(true));  // next press
}

static void test_bounce_while_held_not_repeated() {
  WakeEdge e;
  e.update(false);
  TEST_ASSERT_TRUE (e.update(true));
  TEST_ASSERT_FALSE(e.update(true)); // bounce edge settles on the same level
  TEST_ASSERT_FALSE(e.update(true));
}

static void test_active_at_boot_needs_release_first() {
  WakeEdge e;
  e.update(true);                    // held during power-on
  TEST_ASSERT_FALSE(e.update(true));
  TEST_ASSERT_FALSE(e.update(false));
  TEST_ASSERT_TRUE (e.update(true));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_always_announces);
  RUN_TEST(test_press_reported_release_not);
  RUN_TEST(test_bounce_while_held_not_repeated);
  RUN_TEST(test_active_at_boot_needs_release_first);
  return UNITY_END();
}
