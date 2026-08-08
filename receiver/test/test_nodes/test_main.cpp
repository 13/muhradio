// NodeTable unit tests. Run: pio test -e native_test
#include <unity.h>
#include <string.h>
#include "../../src/nodetable.h"

static NodeTable t;
static char buf[4096];

void setUp() { t = NodeTable{}; }
void tearDown() {}

static void test_update_and_get() {
  t.update(35, 1000, -80, 31);
  t.update(35, 1060, -78, 30);
  const NodeEntry* e = t.get(35);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_UINT32(2, e->count);
  TEST_ASSERT_EQUAL_UINT32(1060, e->lastSeen);
  TEST_ASSERT_EQUAL_INT16(-78, e->rssi);
  TEST_ASSERT_EQUAL_UINT8(30, e->vcc10);
  TEST_ASSERT_EQUAL_UINT8(1, t.size());
}

static void test_zero_vcc_keeps_previous() {
  t.update(35, 1000, -80, 31);
  t.update(35, 1060, -80, 0); // packet without VCC field
  TEST_ASSERT_EQUAL_UINT8(31, t.get(35)->vcc10);
}

static void test_low_battery_threshold() {
  t.update(1, 100, -80, 27);
  t.update(2, 100, -80, 26);
  t.update(3, 100, -80, 0); // never reported — not "low"
  TEST_ASSERT_FALSE(NodeTable::lowBat(*t.get(1)));
  TEST_ASSERT_TRUE (NodeTable::lowBat(*t.get(2)));
  TEST_ASSERT_FALSE(NodeTable::lowBat(*t.get(3)));
}

static void test_eviction_drops_longest_silent() {
  for (uint16_t u = 1; u <= NodeTable::MAX; u++)
    t.update(u, 1000 + u, -80, 30);
  t.update(999, 5000, -70, 30); // table full — uid 1 (oldest) must go
  TEST_ASSERT_EQUAL_UINT8(NodeTable::MAX, t.size());
  TEST_ASSERT_NULL(t.get(1));
  TEST_ASSERT_NOT_NULL(t.get(999));
  TEST_ASSERT_NOT_NULL(t.get(2));
}

static void test_dirty_tracking() {
  t.update(10, 100, -80, 30);
  t.update(11, 100, -80, 30);
  int n = 0;
  NodeEntry* e;
  while ((e = t.nextDirty()) != nullptr) { e->dirty = false; n++; }
  TEST_ASSERT_EQUAL_INT(2, n);
  TEST_ASSERT_NULL(t.nextDirty());
  t.update(10, 160, -80, 30);
  TEST_ASSERT_NOT_NULL(t.nextDirty()); // re-dirtied by new packet
}

static void test_entry_json() {
  t.update(35, 1000, -80, 26);
  char one[160];
  NodeTable::entryJson(*t.get(35), 1090, "ab12", one, sizeof(one));
  TEST_ASSERT_EQUAL_STRING(
    "{\"uid\":35,\"last\":1000,\"age\":90,\"count\":1,"
    "\"rssi\":-80,\"vcc\":2.6,\"low\":1,\"node\":\"ab12\"}", one);
}

static void test_table_json_wellformed() {
  for (uint16_t u = 1; u <= 5; u++) t.update(u, 1000, -80, 30);
  size_t n = t.toJson(buf, sizeof(buf), 2000, "ab12");
  TEST_ASSERT_EQUAL_CHAR('{', buf[0]);
  TEST_ASSERT_EQUAL_CHAR('}', buf[n - 1]);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"ts\":2000"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"uid\":5"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"node\":\"ab12\""));
}

static void test_table_json_small_buffer_stays_valid() {
  for (uint16_t u = 1; u <= NodeTable::MAX; u++) t.update(u, 1000, -80, 30);
  size_t n = t.toJson(buf, 200, 2000, "ab12"); // too small for 32 nodes
  TEST_ASSERT_TRUE(n < 200);
  TEST_ASSERT_EQUAL_CHAR('{', buf[0]);
  TEST_ASSERT_EQUAL_CHAR('}', buf[n - 1]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_update_and_get);
  RUN_TEST(test_zero_vcc_keeps_previous);
  RUN_TEST(test_low_battery_threshold);
  RUN_TEST(test_eviction_drops_longest_silent);
  RUN_TEST(test_dirty_tracking);
  RUN_TEST(test_table_json_wellformed);
  RUN_TEST(test_entry_json);
  RUN_TEST(test_table_json_small_buffer_stays_valid);
  return UNITY_END();
}
