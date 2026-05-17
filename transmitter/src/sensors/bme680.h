#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include "../packet.h"
#include "../power.h"

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
    if (!_sensor.performReading()) {
#ifdef VERBOSE
      Serial.println(F("BME680: read failed"));
#endif
      Power::sleepDeep(255);
      return;
    }
    float t = _sensor.temperature;
    if (isnan(t)) return;
    pkt.addI16(Field::T_BME, (int16_t)round(t * 10.0f));
    pkt.addI16(Field::H_BME, (int16_t)round(_sensor.humidity * 10.0f));
    pkt.addU32(Field::P_BME, (uint32_t)(_sensor.pressure / 10.0f));
    pkt.addU16(Field::G_BME, (uint16_t)(_sensor.gas_resistance / 1000.0f));
#ifdef VERBOSE
    Serial.print(F("BME680 T="));
    Serial.print(t, 1);
    Serial.print(F(" H="));
    Serial.print(_sensor.humidity, 1);
    Serial.print(F(" P="));
    Serial.print((uint32_t)(_sensor.pressure / 10.0f));
    Serial.print(F(" G="));
    Serial.println((uint16_t)(_sensor.gas_resistance / 1000.0f));
#endif
  }
}
