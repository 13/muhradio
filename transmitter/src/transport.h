#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "packet.h"

#ifdef USE_CC1101
  #include <ELECHOUSE_CC1101_SRC_DRV.h>
#else
  #include <LoRa.h>
#endif

#ifdef USE_CRYPTO
  #include <Crypto.h>
  #include <AES.h>
#endif

namespace Transport {
  static constexpr byte ADDR_SENDER   = 0x14;
  static constexpr byte ADDR_RECEIVER = 0x15;
  static bool _ready = false;

#ifdef USE_CRYPTO
  static constexpr uint8_t CIPHER_BUF = 80;
  static AES128  _aes;
  static uint8_t _cipher[CIPHER_BUF];

  static uint8_t _nibble(char c) {
    return c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0';
  }
  static void _parseHexKey(const char* hex, uint8_t* out, uint8_t len) {
    for (uint8_t i = 0; i < len; i++)
      out[i] = (_nibble(hex[i * 2]) << 4) | _nibble(hex[i * 2 + 1]);
  }
#endif

#ifdef VERBOSE
  static void _hexDump(const uint8_t* buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
      if (buf[i] < 0x10) Serial.print('0');
      Serial.print(buf[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
  }
#endif

  inline bool init() {
#ifdef USE_CC1101
    ELECHOUSE_cc1101.Init();
    if (!ELECHOUSE_cc1101.getCC1101()) {
#ifdef VERBOSE
      Serial.println(F("> CC1101: SPI ERROR"));
#endif
      return false;
    }
    ELECHOUSE_cc1101.setCCMode(1);
    ELECHOUSE_cc1101.setModulation(0);
    ELECHOUSE_cc1101.setMHZ(CC1101_MHZ);
    ELECHOUSE_cc1101.setDeviation(47.60);
    ELECHOUSE_cc1101.setChannel(0);
    ELECHOUSE_cc1101.setChsp(199.95);
    ELECHOUSE_cc1101.setRxBW(812.50);
    ELECHOUSE_cc1101.setDRate(99.97);
    ELECHOUSE_cc1101.setPA(CC1101_POWER);
    ELECHOUSE_cc1101.setSyncMode(2);
    ELECHOUSE_cc1101.setSyncWord(213, 147);
    ELECHOUSE_cc1101.setAdrChk(0);
    ELECHOUSE_cc1101.setAddr(0);
    ELECHOUSE_cc1101.setWhiteData(0);
    ELECHOUSE_cc1101.setPktFormat(0);
    ELECHOUSE_cc1101.setLengthConfig(1);
    ELECHOUSE_cc1101.setPacketLength(0);
    ELECHOUSE_cc1101.setCrc(1);
    ELECHOUSE_cc1101.setCRC_AF(0);
    ELECHOUSE_cc1101.setDcFilterOff(0);
    ELECHOUSE_cc1101.setManchester(0);
    ELECHOUSE_cc1101.setFEC(0);
    ELECHOUSE_cc1101.setPRE(0);
    ELECHOUSE_cc1101.setPQT(0);
    ELECHOUSE_cc1101.setAppendStatus(0);
    _ready = true;
#ifdef VERBOSE
    Serial.println(F("> CC1101: OK"));
#endif
#else
    _ready = LoRa.begin(LO_FREQ);
    if (_ready) {
      LoRa.setSpreadingFactor(10);
      LoRa.setSyncWord(0x13);
      LoRa.enableCrc();
    }
#ifdef VERBOSE
    Serial.println(_ready ? F("> LoRa: OK") : F("> LoRa: FAIL"));
#endif
#endif

#ifdef USE_CRYPTO
    uint8_t key[16];
    _parseHexKey(AES_KEY, key, 16);
    _aes.setKey(key, 16);
#ifdef VERBOSE
    Serial.print(F("> Crypto key: "));
    if (key[0]  < 0x10) Serial.print('0');
    Serial.print(key[0],  HEX);
    if (key[1]  < 0x10) Serial.print('0');
    Serial.print(key[1],  HEX);
    Serial.print(F(".."));
    if (key[14] < 0x10) Serial.print('0');
    Serial.print(key[14], HEX);
    if (key[15] < 0x10) Serial.print('0');
    Serial.println(key[15], HEX);
#endif
#endif
    return _ready;
  }

  inline void send(Packet& pkt) {
    if (!_ready || pkt.empty()) return;

    const uint8_t* plain    = pkt.data();
    const uint8_t  plainLen = pkt.size();

    // Build frame: [dst][src][payload or ciphertext]
    uint8_t frame[82];
    frame[0] = ADDR_RECEIVER;
    frame[1] = ADDR_SENDER;
    uint8_t frameLen;

#ifdef USE_CRYPTO
    uint8_t padLen    = 16 - (plainLen % 16);
    uint8_t cipherLen = plainLen + padLen;
    memcpy(_cipher, plain, plainLen);
    memset(_cipher + plainLen, padLen, padLen);
    for (uint8_t b = 0; b < cipherLen; b += 16)
      _aes.encryptBlock(_cipher + b, _cipher + b);
    memcpy(frame + 2, _cipher, cipherLen);
    frameLen = 2 + cipherLen;
#else
    memcpy(frame + 2, plain, plainLen);
    frameLen = 2 + plainLen;
#endif

#ifdef USE_CC1101
    ELECHOUSE_cc1101.SendData(frame, frameLen, 100);
#ifdef VERBOSE
    Serial.print(F("TX CC1101 ")); Serial.print(frameLen); Serial.print(F("B: "));
    _hexDump(frame, frameLen);
#endif
#else
    LoRa.beginPacket();
    LoRa.write(frame, frameLen);
    if (!LoRa.endPacket()) {
#ifdef VERBOSE
      Serial.println(F("LoRa: TX failed"));
#endif
    }
#ifdef VERBOSE
    Serial.print(F("TX LoRa ")); Serial.print(frameLen); Serial.print(F("B: "));
    _hexDump(frame, frameLen);
#endif
#endif
  }
}
