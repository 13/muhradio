// Config JSON round-trip: whatever jsonAppendEscaped writes into config.json,
// jsonGetStr must read back unchanged. Run: pio test -e native_test
#include <unity.h>
#include <string.h>
#include <stdio.h>
#include "../../src/jsonbuilder.h"
#include "../../src/confjson.h"

static char json[512];
static char out[128];

void setUp() {}
void tearDown() {}

// Build {"k":"<escaped v>"} then parse k back
static void roundtrip(const char* v) {
  size_t n = snprintf(json, sizeof(json), "{\"k\":\"");
  n = jsonAppendEscaped(json, n, sizeof(json) - 3, v);
  snprintf(json + n, sizeof(json) - n, "\"}");
  TEST_ASSERT_TRUE(jsonGetStr(json, "k", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING(v, out);
}

static void test_roundtrip_plain()    { roundtrip("muhxnetwork"); }
static void test_roundtrip_quotes()   { roundtrip("pa\"ss\\word"); }
static void test_roundtrip_control()  { roundtrip("a\nb\rc\td\x07e"); }
static void test_roundtrip_utf8()    { roundtrip("caf\xC3\xA9"); }

static void test_get_long() {
  long v = 0;
  TEST_ASSERT_TRUE(jsonGetLong("{\"mqtt_port\":1883,\"tz_offset\":-60}", "mqtt_port", v));
  TEST_ASSERT_EQUAL_INT32(1883, v);
  TEST_ASSERT_TRUE(jsonGetLong("{\"mqtt_port\":1883,\"tz_offset\":-60}", "tz_offset", v));
  TEST_ASSERT_EQUAL_INT32(-60, v);
  TEST_ASSERT_FALSE(jsonGetLong("{\"a\":1}", "missing", v));
}

static void test_missing_key() {
  TEST_ASSERT_FALSE(jsonGetStr("{\"a\":\"x\"}", "missing", out, sizeof(out)));
}

static void test_multiple_keys() {
  const char* j = "{\"wifi_ssid\":\"net\",\"wifi_pass\":\"p\\\"w\",\"desc\":\"home\"}";
  TEST_ASSERT_TRUE(jsonGetStr(j, "wifi_pass", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("p\"w", out);
  TEST_ASSERT_TRUE(jsonGetStr(j, "desc", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("home", out);
}

static void test_output_bounded() {
  char tiny[4];
  TEST_ASSERT_TRUE(jsonGetStr("{\"k\":\"abcdefgh\"}", "k", tiny, sizeof(tiny)));
  TEST_ASSERT_EQUAL_STRING("abc", tiny);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_roundtrip_plain);
  RUN_TEST(test_roundtrip_quotes);
  RUN_TEST(test_roundtrip_control);
  RUN_TEST(test_roundtrip_utf8);
  RUN_TEST(test_get_long);
  RUN_TEST(test_missing_key);
  RUN_TEST(test_multiple_keys);
  RUN_TEST(test_output_bounded);
  return UNITY_END();
}
