#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include "../packet.h"
#include "../power.h"
#include "humidity.h"

namespace BME680 {
  static Adafruit_BME680 _sensor;

  inline void setup() {
    if (!_sensor.begin()) {
#ifdef VERBOSE
      Serial.println(F("BME680: not detected"));
#endif
    }
#ifndef BME680_GAS
    // begin() arms the gas heater (320 C for 150 ms on every reading, ~2 mAs:
    // by far the biggest consumer on a BME680 node). Off unless the G_BME
    // field is wanted (-DBME680_GAS); the T/H/P fields are unaffected.
    _sensor.setGasHeater(0, 0);
#endif
  }

  inline void read(Packet& pkt) {
    // On failure just skip our fields — the packet still goes out on schedule
    // with whatever other sensors (and VCC) provided.
    // Start the measurement, power down while the sensor works (the driver's
    // performReading() would busy-wait twice the period instead), then collect.
    if (_sensor.beginReading() == 0) {
#ifdef VERBOSE
      Serial.println(F("BME680: read failed"));
#endif
      return;
    }
    int wait = _sensor.remainingReadingMillis();
    if (wait > 0) Power::sleepMs((uint16_t)(wait + wait / 4)); // +25 %: WDT tolerance, deadline is hard
    if (!_sensor.endReading()) {
#ifdef VERBOSE
      Serial.println(F("BME680: read failed"));
#endif
      return;
    }
    float t = _sensor.temperature;
    if (isnan(t)) return;
    pkt.addI16(Field::T_BME, (int16_t)round(t * 10.0f));
    float h = _sensor.humidity;
    addHumidity(pkt, Field::H_BME, h);
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
