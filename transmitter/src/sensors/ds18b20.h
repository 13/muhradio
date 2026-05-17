#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "../packet.h"

namespace DS18B20 {
  static OneWire           _ow(SENSOR_PIN_DS18B20);
  static DallasTemperature _sensor(&_ow);

  inline void setup() {
    _sensor.begin();
  }

  inline void read(Packet& pkt) {
    _sensor.requestTemperatures();
    float t = _sensor.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C) return;
    pkt.addI16(Field::T_DS, (int16_t)round(t * 10.0f));
#ifdef VERBOSE
    Serial.print(F("DS18B20 T="));
    Serial.println(t, 1);
#endif
  }
}
