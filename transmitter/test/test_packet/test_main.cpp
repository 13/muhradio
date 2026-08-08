// Round-trip test: encode with the transmitter's Packet class, decode with the
// shared field tables the receiver uses. Catches drift between packet.h and
// the tables in shared/fields.h. Run: pio test -e native_test
#include <unity.h>
#include "../../src/packet.h"

struct Decoded {
  uint16_t uid;
  uint8_t  pid;
  bool     retained;
  bool     truncated;
  bool     has[Fields::COUNT];
  int32_t  val[Fields::COUNT];
};

// Mirrors the field walk in receiver/src/radio.cpp Radio::decode()
static bool decode(const uint8_t* p, uint8_t len, Decoded& d) {
  d = Decoded{};
  if (len < Packet::HDR_SIZE) return false;
  d.uid = p[0] | (uint16_t)p[1] << 8;
  d.pid = p[2];
  uint16_t bmap = p[3] | (uint16_t)p[4] << 8;
  d.retained = bmap >> 15 & 1;
  bmap &= 0x7FFF;
  const uint8_t* vals = p + Packet::HDR_SIZE;
  uint8_t valsLen = len - Packet::HDR_SIZE;
  uint8_t pos = 0;
  for (uint8_t bit = 0; bit < Fields::COUNT; bit++) {
    if (!(bmap >> bit & 1)) continue;
    if (pos + Fields::SIZES[bit] > valsLen) { d.truncated = true; return true; }
    int32_t v = 0;
    for (uint8_t b = 0; b < Fields::SIZES[bit]; b++)
      v |= (int32_t)vals[pos + b] << (8 * b);
    if (Fields::IS_SIGNED[bit] && (v & 0x8000)) v |= 0xFFFF0000;
    pos += Fields::SIZES[bit];
    d.has[bit] = true;
    d.val[bit] = v;
  }
  return true;
}

static Packet pkt;
static Decoded d;

void setUp() {}
void tearDown() {}

static void test_roundtrip_all_field_sizes() {
  pkt.reset(0x1234, 42);
  TEST_ASSERT_TRUE(pkt.addU16(Field::COUNTER, 65500));      // u16
  TEST_ASSERT_TRUE(pkt.addU8 (Field::BUTTON, 1));           // u8
  TEST_ASSERT_TRUE(pkt.addI16(Field::T_SI, -255));          // i16 negative
  TEST_ASSERT_TRUE(pkt.addI16(Field::H_SI, 998));           // i16 positive
  TEST_ASSERT_TRUE(pkt.addU32(Field::P_BMP, 10132));        // u32
  TEST_ASSERT_TRUE(pkt.addU8 (Field::VCC, 33));             // u8, highest bit

  TEST_ASSERT_TRUE(decode(pkt.data(), pkt.size(), d));
  TEST_ASSERT_FALSE(d.truncated);
  TEST_ASSERT_EQUAL_UINT16(0x1234, d.uid);
  TEST_ASSERT_EQUAL_UINT8(42, d.pid);
  TEST_ASSERT_FALSE(d.retained);
  TEST_ASSERT_EQUAL_INT32(65500,  d.val[(uint8_t)Field::COUNTER]);
  TEST_ASSERT_EQUAL_INT32(1,      d.val[(uint8_t)Field::BUTTON]);
  TEST_ASSERT_EQUAL_INT32(-255,   d.val[(uint8_t)Field::T_SI]);
  TEST_ASSERT_EQUAL_INT32(998,    d.val[(uint8_t)Field::H_SI]);
  TEST_ASSERT_EQUAL_INT32(10132,  d.val[(uint8_t)Field::P_BMP]);
  TEST_ASSERT_EQUAL_INT32(33,     d.val[(uint8_t)Field::VCC]);
  TEST_ASSERT_FALSE(d.has[(uint8_t)Field::T_DS]); // absent field stays absent
}

static void test_packet_size_matches_tables() {
  pkt.reset(1, 1);
  pkt.addI16(Field::T_SI, 210);
  pkt.addU32(Field::P_BME, 10000);
  uint8_t expected = Packet::HDR_SIZE
                   + Fields::SIZES[(uint8_t)Field::T_SI]
                   + Fields::SIZES[(uint8_t)Field::P_BME];
  TEST_ASSERT_EQUAL_UINT8(expected, pkt.size());
}

static void test_retained_bit() {
  pkt.reset(7, 9);
  pkt.addU8(Field::VCC, 30);
  pkt.setRetained();
  TEST_ASSERT_TRUE(decode(pkt.data(), pkt.size(), d));
  TEST_ASSERT_TRUE(d.retained);
  TEST_ASSERT_EQUAL_INT32(30, d.val[(uint8_t)Field::VCC]); // bit 15 not a field
}

static void test_duplicate_field_rejected() {
  pkt.reset(1, 1);
  TEST_ASSERT_TRUE(pkt.addI16(Field::T_SI, 100));
  TEST_ASSERT_FALSE(pkt.addI16(Field::T_SI, 200));
  TEST_ASSERT_TRUE(decode(pkt.data(), pkt.size(), d));
  TEST_ASSERT_EQUAL_INT32(100, d.val[(uint8_t)Field::T_SI]);
}

static void test_truncated_payload_detected() {
  pkt.reset(1, 1);
  pkt.addI16(Field::T_SI, 100);
  pkt.addU32(Field::P_BMP, 10132);
  TEST_ASSERT_TRUE(decode(pkt.data(), pkt.size() - 2, d)); // chop the u32
  TEST_ASSERT_TRUE(d.truncated);
  TEST_ASSERT_EQUAL_INT32(100, d.val[(uint8_t)Field::T_SI]); // prefix still good
  TEST_ASSERT_FALSE(d.has[(uint8_t)Field::P_BMP]);
}

static void test_tables_consistent() {
  uint16_t total = 0;
  for (uint8_t i = 0; i < Fields::COUNT; i++) {
    TEST_ASSERT_TRUE(Fields::MIN_RAW[i] <= Fields::MAX_RAW[i]);
    TEST_ASSERT_TRUE(Fields::SIZES[i] >= 1 && Fields::SIZES[i] <= 4);
    total += Fields::SIZES[i];
  }
  TEST_ASSERT_TRUE(total <= Packet::MAX_VALS); // all fields at once must fit
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_roundtrip_all_field_sizes);
  RUN_TEST(test_packet_size_matches_tables);
  RUN_TEST(test_retained_bit);
  RUN_TEST(test_duplicate_field_rejected);
  RUN_TEST(test_truncated_payload_detected);
  RUN_TEST(test_tables_consistent);
  return UNITY_END();
}
