#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include "../packet.h"

namespace BMP280 {
  static Adafruit_BMP280 _sensor;

  inline void setup() {
    // GY-21P: try both I2C addresses and common clone chip IDs (0x60, 0x58, 0x56)
    bool ok = _sensor.begin(0x76, 0x60) || _sensor.begin(0x77, 0x60)
           || _sensor.begin(0x76, 0x58) || _sensor.begin(0x77, 0x58)
           || _sensor.begin(0x76, 0x56) || _sensor.begin(0x77, 0x56);
#ifdef VERBOSE
    Serial.println(ok ? F("BMP280 OK") : F("BMP280 FAIL"));
#endif
  }

  inline void read(Packet& pkt) {
    float p = _sensor.readPressure();
    if (isnan(p) || p <= 0) return;
    float t = _sensor.readTemperature();
    pkt.addI16(Field::T_BMP, (int16_t)round(t * 10.0f));
    pkt.addU32(Field::P_BMP, (uint32_t)round(p / 10.0f));
#ifdef VERBOSE
    Serial.print(F("BMP280 T="));
    Serial.print(t, 1);
    Serial.print(F(" P="));
    Serial.println((uint32_t)(p / 10.0f));
#endif
  }
}
