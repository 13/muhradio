#pragma once
#include <Arduino.h>
#include <string.h>

// Wire format: [uid:2][pid:1][bitmap:2][values in bit-ascending order, present only]
//
// Field bit positions — MUST be added in ascending order.
// Each bit maps to a (sensor, measurement) pair with a fixed known size,
// so the receiver can read values sequentially by checking each bit in order.
enum class Field : uint8_t {
  COUNTER = 0,   // uint16 — packet counter (debug)
  BUTTON  = 1,   // uint8  — 0=open, 1=pressed
  SWITCH  = 2,   // uint8  — 0/1
  PIR     = 3,   // uint8  — 0/1
  RADAR   = 4,   // uint8  — 0/1
  T_SI    = 5,   // int16  — Si7021  °C*10
  H_SI    = 6,   // int16  — Si7021  %*10
  T_DS    = 7,   // int16  — DS18B20 °C*10
  T_BMP   = 8,   // int16  — BMP280  °C*10
  P_BMP   = 9,   // uint32 — BMP280  Pa/10
  T_BME   = 10,  // int16  — BME680  °C*10
  H_BME   = 11,  // int16  — BME680  %*10
  P_BME   = 12,  // uint32 — BME680  Pa/10
  G_BME   = 13,  // uint16 — BME680  kOhm
  VCC     = 14,  // uint8  — V*10
};

class Packet {
public:
  // Wire layout: uid(2) + pid(1) + bitmap(2) + values(up to 56)
  static constexpr uint8_t HDR_SIZE  = 5;
  static constexpr uint8_t MAX_SIZE  = 61;
  static constexpr uint8_t MAX_VALS  = MAX_SIZE - HDR_SIZE; // 56

private:
  uint8_t  _buf[MAX_SIZE];
  uint16_t _bitmap = 0;
  uint8_t  _vLen   = 0;
#ifdef DEBUG
  uint8_t  _lastBit = 0xFF; // 0xFF = no field added yet
#endif

  bool _add(Field f, const void* data, uint8_t n) {
    uint8_t bit  = (uint8_t)f;
    uint16_t mask = (uint16_t)1u << bit;
#ifdef DEBUG
    if (_lastBit != 0xFF && bit <= _lastBit) {
      Serial.print(F("Packet: out-of-order bit "));
      Serial.println(bit);
      return false;
    }
    _lastBit = bit;
#endif
    if ((_bitmap & mask) || _vLen + n > MAX_VALS) {
#ifdef VERBOSE
      Serial.print(F("Packet: overflow bit ")); Serial.println(bit);
#endif
      return false;
    }
    memcpy(_buf + HDR_SIZE + _vLen, data, n);
    _vLen    += n;
    _bitmap  |= mask;
    _buf[3]   = (uint8_t)(_bitmap);
    _buf[4]   = (uint8_t)(_bitmap >> 8);
    return true;
  }

public:
  // uid and pid are always present in the fixed header — not part of the bitmap.
  void reset(uint16_t uid, uint8_t pid) {
    _bitmap = 0;
    _vLen   = 0;
    _buf[0] = (uint8_t)(uid);
    _buf[1] = (uint8_t)(uid >> 8);
    _buf[2] = pid;
    _buf[3] = 0;
    _buf[4] = 0;
#ifdef DEBUG
    _lastBit = 0xFF;
#endif
  }

  bool addU8 (Field f, uint8_t v)  { return _add(f, &v, 1); }
  bool addU16(Field f, uint16_t v) { return _add(f, &v, 2); }
  bool addI16(Field f, int16_t v)  { return _add(f, &v, 2); }
  bool addU32(Field f, uint32_t v) { return _add(f, &v, 4); }

  const uint8_t* data() const { return _buf; }
  uint8_t        size() const { return HDR_SIZE + _vLen; }
  bool           empty() const { return _vLen == 0; }

#ifdef DEBUG
  void print() const {
    Serial.print(F("Pkt ")); Serial.print(size()); Serial.println(F("B bitmap:"));
    for (uint8_t i = 0; i < 15; i++) {
      if (_bitmap & ((uint16_t)1u << i)) {
        Serial.print(F("  bit ")); Serial.println(i);
      }
    }
  }
#endif
};
