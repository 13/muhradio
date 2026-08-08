// Bresser 7-in-1 decoder tests against synthetic frames: build a de-whitened
// payload, stamp a valid LFSR digest, whiten, decode. Run: pio test -e native_test
#include <unity.h>
#include <string.h>
#include "../../src/bresser_decode.h"

static uint8_t w[BRESSER_PAYLOAD_LEN];
static uint8_t raw[BRESSER_PAYLOAD_LEN];
static BresserPacket pkt;

void setUp() { memset(w, 0, sizeof(w)); memset(&pkt, 0, sizeof(pkt)); }
void tearDown() {}

// Fill w[2..] with a plausible weather reading (s_type=1, chan=2)
static void baseFrame() {
  w[2] = 0xAB; w[3] = 0xCD;               // id ABCD
  w[4] = 0x27; w[5] = 0x00;               // wind dir 270
  w[6] = 0x12 ^ 0xAA;                     // raw: s_type=1, chan=2
  w[7] = 0x01; w[8] = 0x20; w[9] = 0x35;  // gust 1.2, avg 3.5
  w[10] = 0x00; w[11] = 0x12; w[12] = 0x34; // rain 123.4
  w[14] = 0x21; w[15] = 0x50;             // temp 21.5, battery ok
  w[16] = 0x45;                           // humidity 45
  w[17] = 0x01; w[18] = 0x23; w[19] = 0x45; // lux 12345 -> 12.3 klx-ish
  w[20] = 0x01; w[21] = 0x30;             // uv 1.3
}

// Stamp valid digest into w[0..1], whiten into raw
static void finalize() {
  uint16_t chk = lfsr16(&w[2], 23, 0x8810, 0xba95) ^ 0x6df1;
  w[0] = chk >> 8; w[1] = chk & 0xFF;
  for (int i = 0; i < BRESSER_PAYLOAD_LEN; i++) raw[i] = w[i] ^ 0xAA;
}

static void expectField(const char* frag) {
  char msg[300];
  snprintf(msg, sizeof(msg), "missing %s in %s", frag, pkt.json);
  TEST_ASSERT_TRUE_MESSAGE(strstr(pkt.json, frag) != NULL, msg);
}

static void test_decode_valid_frame() {
  baseFrame(); finalize();
  TEST_ASSERT_TRUE(decode7in1(raw, -77, 1700000000, "ab12", "muh/bresser", pkt));
  TEST_ASSERT_TRUE(pkt.valid);
  TEST_ASSERT_EQUAL_STRING("muh/bresser/ABCD/json", pkt.topic);
  expectField("\"id\":\"ABCD\"");
  expectField("\"ch\":2");
  expectField("\"bat\":1");
  expectField("\"temp\":21.5");
  expectField("\"hum\":45");
  expectField("\"wind\":3.5");
  expectField("\"gust\":1.2");
  expectField("\"dir\":270");
  expectField("\"rain\":123.4");
  expectField("\"uv\":1.3");
  expectField("\"rssi\":-77");
  expectField("\"node\":\"ab12\"");
}

static void test_negative_temperature() {
  baseFrame();
  w[14] = 0x98; w[15] = 0x50; // 985 -> -1.5 C
  finalize();
  TEST_ASSERT_TRUE(decode7in1(raw, -77, 0, "ab12", "muh/bresser", pkt));
  expectField("\"temp\":-1.5");
}

static void test_battery_low() {
  baseFrame();
  w[15] = 0x56; // low bits 0b110 -> battery low
  finalize();
  TEST_ASSERT_TRUE(decode7in1(raw, -77, 0, "ab12", "muh/bresser", pkt));
  expectField("\"bat\":0");
}

static void test_bad_digest_rejected() {
  baseFrame(); finalize();
  raw[10] ^= 0x01; // flip one bit
  TEST_ASSERT_FALSE(decode7in1(raw, -77, 0, "ab12", "muh/bresser", pkt));
  TEST_ASSERT_FALSE(pkt.valid);
}

static void test_wrong_sensor_type_skipped() {
  baseFrame();
  w[6] = 0x22 ^ 0xAA; // s_type=2 (CO2) — digest valid, type filtered
  finalize();
  TEST_ASSERT_FALSE(decode7in1(raw, -77, 0, "ab12", "muh/bresser", pkt));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_decode_valid_frame);
  RUN_TEST(test_negative_temperature);
  RUN_TEST(test_battery_low);
  RUN_TEST(test_bad_digest_rejected);
  RUN_TEST(test_wrong_sensor_type_skipped);
  return UNITY_END();
}
