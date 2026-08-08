// AES framing round-trip: replicate the transmitter's pad+encrypt
// (transmitter/src/transport.h) and the receiver's decrypt+unpad
// (receiver/src/radio.cpp) and prove they invert for every payload length.
// Cipher is the test-local AES-128 (aes128.h), pinned to the FIPS-197
// known-answer vector below. Run: pio test -e native_test
#include <unity.h>
#include <string.h>
#include "aes128.h"

static aes128::Key key;
static const uint8_t KEY[16] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
};

void setUp() { key.expand(KEY); }
void tearDown() {}

// FIPS-197 Appendix C.1 — proves the vendored AES itself is correct
static void test_fips197_vector() {
  uint8_t s[16] = {
    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
    0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
  };
  const uint8_t expect[16] = {
    0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,
    0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a
  };
  aes128::encryptBlock(key, s);
  TEST_ASSERT_EQUAL_MEMORY(expect, s, 16);
  aes128::decryptBlock(key, s);
  const uint8_t plain[16] = {
    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
    0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
  };
  TEST_ASSERT_EQUAL_MEMORY(plain, s, 16);
}

// Transmitter side (transport.h): pad to block, ECB encrypt in place
static uint8_t txEncrypt(const uint8_t* plain, uint8_t plainLen, uint8_t* cipher) {
  uint8_t padLen    = 16 - (plainLen % 16);
  uint8_t cipherLen = plainLen + padLen;
  memcpy(cipher, plain, plainLen);
  memset(cipher + plainLen, padLen, padLen);
  for (uint8_t b = 0; b < cipherLen; b += 16)
    aes128::encryptBlock(key, cipher + b);
  return cipherLen;
}

// Receiver side (radio.cpp): decrypt blocks, validate + strip padding.
// Returns payload length, or -1 when padding is rejected.
static int rxDecrypt(const uint8_t* cipher, uint8_t cipherLen, uint8_t* plain) {
  for (int i = 0; i < cipherLen / 16; i++) {
    memcpy(&plain[i * 16], &cipher[i * 16], 16);
    aes128::decryptBlock(key, &plain[i * 16]);
  }
  uint8_t padLen = plain[cipherLen - 1];
  if (padLen == 0 || padLen > 16) return -1;
  return cipherLen - padLen;
}

static void test_roundtrip_all_packet_lengths() {
  // Real packets are 5..61 bytes (header + up to 56 value bytes)
  uint8_t plain[64], cipher[80], out[80];
  for (uint8_t len = 1; len <= 61; len++) {
    for (uint8_t i = 0; i < len; i++) plain[i] = (uint8_t)(i * 7 + len);
    uint8_t cipherLen = txEncrypt(plain, len, cipher);
    TEST_ASSERT_EQUAL_UINT8(0, cipherLen % 16);
    TEST_ASSERT_TRUE(cipherLen >= len + 1 && cipherLen <= len + 16);
    int outLen = rxDecrypt(cipher, cipherLen, out);
    TEST_ASSERT_EQUAL_INT(len, outLen);
    TEST_ASSERT_EQUAL_MEMORY(plain, out, len);
  }
}

static void test_block_multiple_gets_full_pad_block() {
  uint8_t plain[16] = {0}, cipher[80];
  TEST_ASSERT_EQUAL_UINT8(32, txEncrypt(plain, 16, cipher)); // never 0 pad
}

static void test_wrong_key_never_yields_plaintext() {
  uint8_t plain[24], cipher[80], out[80];
  for (uint8_t i = 0; i < sizeof(plain); i++) plain[i] = i;
  uint8_t cipherLen = txEncrypt(plain, sizeof(plain), cipher);

  uint8_t badKey[16];
  memcpy(badKey, KEY, 16);
  badKey[0] ^= 0x01;
  key.expand(badKey);
  int outLen = rxDecrypt(cipher, cipherLen, out);
  // Padding check catches most garbage; when it doesn't, content must differ.
  if (outLen >= 0)
    TEST_ASSERT_TRUE(outLen != (int)sizeof(plain) ||
                     memcmp(plain, out, sizeof(plain)) != 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fips197_vector);
  RUN_TEST(test_roundtrip_all_packet_lengths);
  RUN_TEST(test_block_multiple_gets_full_pad_block);
  RUN_TEST(test_wrong_key_never_yields_plaintext);
  return UNITY_END();
}
