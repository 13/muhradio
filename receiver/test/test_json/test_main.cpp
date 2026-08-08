// JsonBuilder unit tests — overflow behavior must never produce invalid JSON.
// Run: pio test -e native_test
#include <unity.h>
#include <string.h>
#include "../../src/jsonbuilder.h"

static char buf[512];

void setUp() { memset(buf, 0xAA, sizeof(buf)); }
void tearDown() {}

// Every finished JSON must be null-terminated, start with '{' and end with '}'
static void assertWellFormed(const char* s) {
  size_t len = strlen(s);
  TEST_ASSERT_TRUE(len >= 2);
  TEST_ASSERT_EQUAL_CHAR('{', s[0]);
  TEST_ASSERT_EQUAL_CHAR('}', s[len - 1]);
}

static void test_basic_object() {
  JsonBuilder jb(buf, sizeof(buf));
  jb.kv("uid", 4660L);
  jb.kv("T_SI", -25.5f);
  jb.kvs("RN", "abcd");
  jb.finish();
  TEST_ASSERT_EQUAL_STRING("{\"uid\":4660,\"T_SI\":-25.5,\"RN\":\"abcd\"}", buf);
}

static void test_empty_object() {
  JsonBuilder jb(buf, sizeof(buf));
  jb.finish();
  TEST_ASSERT_EQUAL_STRING("{}", buf);
}

static void test_overflow_drops_whole_pair() {
  JsonBuilder jb(buf, 16); // room for {"a":1} + a bit
  jb.kv("a", 1L);
  jb.kv("toolongkey", 123456789L); // can't fit — must vanish entirely
  jb.finish();
  TEST_ASSERT_EQUAL_STRING("{\"a\":1}", buf);
}

static void test_overflow_first_pair() {
  JsonBuilder jb(buf, 8);
  jb.kv("waytoolongkey", 1L);
  jb.finish();
  TEST_ASSERT_EQUAL_STRING("{}", buf);
}

static void test_sep_restored_after_drop() {
  JsonBuilder jb(buf, 24);
  jb.kv("a", 1L);
  jb.kvs("dropped_key", "long value that cannot fit");
  jb.kv("b", 2L); // must still get its comma right
  jb.finish();
  TEST_ASSERT_EQUAL_STRING("{\"a\":1,\"b\":2}", buf);
}

static void test_string_overflow_dropped() {
  JsonBuilder jb(buf, 20);
  jb.kvs("k", "0123456789012345678901234567890");
  jb.finish();
  TEST_ASSERT_EQUAL_STRING("{}", buf);
}

static void test_kvs_escapes_specials() {
  JsonBuilder jb(buf, sizeof(buf));
  jb.kvs("k", "a\"b\\c\nd\te");
  jb.finish();
  TEST_ASSERT_EQUAL_STRING("{\"k\":\"a\\\"b\\\\c\\nd\\te\"}", buf);
}

static void test_kvs_escapes_control_and_bad_utf8() {
  JsonBuilder jb(buf, sizeof(buf));
  char v[] = { 'x', 0x07, (char)0xC3, (char)0xA9, (char)0xFF, 'y', 0 }; // BEL, é, invalid
  jb.kvs("k", v);
  jb.finish();
  TEST_ASSERT_EQUAL_STRING("{\"k\":\"x\\u0007\xC3\xA9?y\"}", buf);
}

static void test_kvs_truncated_escape_drops_pair() {
  JsonBuilder jb(buf, 18);
  jb.kv("a", 1L);
  jb.kvs("k", "\"\"\"\"\"\"\"\"\"\""); // escapes double the size — can't fit
  jb.finish();
  TEST_ASSERT_EQUAL_STRING("{\"a\":1}", buf);
}

static void test_escape_append_reports_truncation() {
  bool trunc = false;
  size_t n = jsonAppendEscaped(buf, 0, 8, "abcdefghij", &trunc);
  TEST_ASSERT_TRUE(trunc);
  TEST_ASSERT_TRUE(n < 8);
  n = jsonAppendEscaped(buf, 0, 64, "abc", &trunc);
  TEST_ASSERT_FALSE(trunc);
  TEST_ASSERT_EQUAL_STRING("abc", buf);
  TEST_ASSERT_EQUAL_UINT32(3, (uint32_t)n);
}

static void test_exact_boundary_never_invalid() {
  // Sweep caps: whatever fits, output must stay well-formed
  for (size_t cap = 4; cap <= 40; cap++) {
    JsonBuilder jb(buf, cap);
    jb.kv("temp", -12.3f);
    jb.kv("rssi", -98L);
    jb.kvs("id", "ff01");
    jb.finish();
    assertWellFormed(buf);
    TEST_ASSERT_TRUE(strlen(buf) <= cap - 1);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_basic_object);
  RUN_TEST(test_empty_object);
  RUN_TEST(test_overflow_drops_whole_pair);
  RUN_TEST(test_overflow_first_pair);
  RUN_TEST(test_sep_restored_after_drop);
  RUN_TEST(test_string_overflow_dropped);
  RUN_TEST(test_kvs_escapes_specials);
  RUN_TEST(test_kvs_escapes_control_and_bad_utf8);
  RUN_TEST(test_kvs_truncated_escape_drops_pair);
  RUN_TEST(test_escape_append_reports_truncation);
  RUN_TEST(test_exact_boundary_never_invalid);
  return UNITY_END();
}
