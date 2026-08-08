#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include "../packet.h"

namespace BME680 {
  static Adafruit_BME680 _sensor;

  inline void setup() {
    if (!_sensor.begin()) {
#ifdef VERBOSE
      Serial.println(F("BME680: not detected"));
#endif
    }
  }

  inline void read(Packet& pkt) {
    // On failure just skip our fields — the packet still goes out on schedule
    // with whatever other sensors (and VCC) provided.
    if (!_sensor.performReading()) {
#ifdef VERBOSE
      Serial.println(F("BME680: read failed"));
#endif
      return;
    }
    float t = _sensor.temperature;
    if (isnan(t)) return;
    pkt.addI16(Field::T_BME, (int16_t)round(t * 10.0f));
    // Compensation can yield slightly <0% or >100% at the extremes — clamp
    // those; values further out are read errors, skip them.
    float h = _sensor.humidity;
    if (!isnan(h) && h > -5.0f && h < 105.0f) {
      h = constrain(h, 0.0f, 100.0f);
      pkt.addI16(Field::H_BME, (int16_t)round(h * 10.0f));
    }
    // Plausible surface pressure: 300–1100 hPa (0 means the read failed)
    uint32_t p = _sensor.pressure; // Pa
    if (p >= 30000UL && p <= 110000UL)
      pkt.addU32(Field::P_BME, p / 10);
    uint32_t g = _sensor.gas_resistance; // Ohm
    if (g > 0 && g / 1000 <= 65535UL)
      pkt.addU16(Field::G_BME, (uint16_t)(g / 1000));
#ifdef VERBOSE
    Serial.print(F("BME680 T="));
    Serial.print(t, 1);
    Serial.print(F(" H="));
    Serial.print(h, 1);
    Serial.print(F(" P="));
    Serial.print(p / 10);
    Serial.print(F(" G="));
    Serial.println(g / 1000);
#endif
  }
}
