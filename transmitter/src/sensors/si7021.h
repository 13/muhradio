#pragma once
#include <Arduino.h>
#include <Adafruit_Si7021.h>
#include "../packet.h"

namespace Si7021 {
  static Adafruit_Si7021 _sensor;

  inline void setup() {
    if (!_sensor.begin()) {
#ifdef VERBOSE
      Serial.println(F("Si7021: not detected"));
#endif
    }
  }

  inline void read(Packet& pkt) {
    float t = _sensor.readTemperature();
    float h = _sensor.readHumidity();
    if (isnan(t)) return;
    pkt.addI16(Field::T_SI, (int16_t)round(t * 10.0f));
    pkt.addI16(Field::H_SI, (int16_t)round(h * 10.0f));
#ifdef VERBOSE
    Serial.print(F("Si7021 T="));
    Serial.print(t, 1);
    Serial.print(F(" H="));
    Serial.println(h, 1);
#endif
  }
}
