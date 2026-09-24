// Watchdog sleep planner: how a millisecond wait is split into the ATmega328P's
// WDT power-down periods. Run: pio test -e native_test
#include <unity.h>
#include "../../src/sleep_plan.h"

void setUp() {}
void tearDown() {}

static uint32_t sum(const uint16_t* p, uint8_t n) {
  uint32_t s = 0;
  for (uint8_t i = 0; i < n; i++) s += p[i];
  return s;
}

static bool valid_period(uint16_t ms) {
  for (uint8_t i = 0; i < SLEEP_PLAN_PERIODS; i++)
    if (SLEEP_PLAN_PERIOD_MS[i] == ms) return true;
  return false;
}

static void test_zero_sleeps_nothing() {
  uint16_t plan[SLEEP_PLAN_MAX];
  TEST_ASSERT_EQUAL_UINT8(0, sleepPlan(0, plan, SLEEP_PLAN_MAX));
}

static void test_exact_period() {
  uint16_t plan[SLEEP_PLAN_MAX];
  uint8_t n = sleepPlan(500, plan, SLEEP_PLAN_MAX);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_UINT16(500, plan[0]);
}

static void test_bme680_wait_rounds_up_within_one_step() {
  // 190 ms heater+TPH period with the 25 % margin the caller adds -> 237 ms
  uint16_t plan[SLEEP_PLAN_MAX];
  uint8_t n = sleepPlan(237, plan, SLEEP_PLAN_MAX);
  uint32_t total = sum(plan, n);
  TEST_ASSERT_TRUE(total >= 237);
  TEST_ASSERT_TRUE(total < 237 + 15);
  for (uint8_t i = 0; i < n; i++) TEST_ASSERT_TRUE(valid_period(plan[i]));
}

static void test_ds18b20_870ms_matches_hand_written_steps() {
  uint16_t plan[SLEEP_PLAN_MAX];
  uint8_t n = sleepPlan(870, plan, SLEEP_PLAN_MAX);
  TEST_ASSERT_EQUAL_UINT8(3, n);
  TEST_ASSERT_EQUAL_UINT16(500, plan[0]);
  TEST_ASSERT_EQUAL_UINT16(250, plan[1]);
  TEST_ASSERT_EQUAL_UINT16(120, plan[2]);
}

static void test_never_undersleeps_and_never_oversleeps_by_a_step() {
  uint16_t plan[SLEEP_PLAN_MAX];
  for (uint32_t ms = 1; ms <= 20000; ms += 7) {
    uint8_t n = sleepPlan((uint16_t)ms, plan, SLEEP_PLAN_MAX);
    uint32_t total = sum(plan, n);
    TEST_ASSERT_TRUE(total >= ms);
    TEST_ASSERT_TRUE(total < ms + 15);
    for (uint8_t i = 1; i < n; i++) TEST_ASSERT_TRUE(plan[i] <= plan[i - 1]); // longest first
  }
}

static void test_respects_output_capacity() {
  uint16_t plan[2];
  uint8_t n = sleepPlan(65535, plan, 2);
  TEST_ASSERT_EQUAL_UINT8(2, n);
  TEST_ASSERT_EQUAL_UINT16(8000, plan[0]);
  TEST_ASSERT_EQUAL_UINT16(8000, plan[1]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_zero_sleeps_nothing);
  RUN_TEST(test_exact_period);
  RUN_TEST(test_bme680_wait_rounds_up_within_one_step);
  RUN_TEST(test_ds18b20_870ms_matches_hand_written_steps);
  RUN_TEST(test_never_undersleeps_and_never_oversleeps_by_a_step);
  RUN_TEST(test_respects_output_capacity);
  return UNITY_END();
}
