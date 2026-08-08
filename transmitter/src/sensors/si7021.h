#pragma once
#include <Arduino.h>
#include <Adafruit_Si7021.h>
#include "../packet.h"

namespace Si7021 {
  static Adafruit_Si7021 _sensor;

  inline void setup() {
    // begin()'s ID check expects user register == 0x3A, which Si7021-compatible
    // clones (e.g. GY-21P) fail even though reads work. Call it for the I2C
    // init side effect and ignore the result; read() validates via isnan().
    _sensor.begin();
  }

  inline void read(Packet& pkt) {
    float t = _sensor.readTemperature();
    float h = _sensor.readHumidity();
    if (isnan(t)) {
#ifdef VERBOSE
      Serial.println(F("Si7021: read failed"));
#endif
      return;
    }
    pkt.addI16(Field::T_SI, (int16_t)round(t * 10.0f));
    // Datasheet: the RH conversion can yield slightly <0% or >100% at the
    // extremes — clamp those; values further out are read errors, skip them.
    if (!isnan(h) && h > -5.0f && h < 105.0f) {
      h = constrain(h, 0.0f, 100.0f);
      pkt.addI16(Field::H_SI, (int16_t)round(h * 10.0f));
    }
#ifdef VERBOSE
    else {
      Serial.println(F("Si7021: humidity out of range"));
    }
#endif
#ifdef VERBOSE
    Serial.print(F("Si7021 T="));
    Serial.print(t, 1);
    Serial.print(F(" H="));
    Serial.println(h, 1);
#endif
  }
}
