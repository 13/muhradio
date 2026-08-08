#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LowPower.h>
#include "../packet.h"

namespace DS18B20 {
  static OneWire           _ow(SENSOR_PIN_DS18B20);
  static DallasTemperature _sensor(&_ow);

  inline void setup() {
    _sensor.begin();
    _sensor.setWaitForConversion(false);
  }

  inline void read(Packet& pkt) {
    _sensor.requestTemperatures();
    // 12-bit conversion takes 750 ms; power down instead of busy-waiting.
    // WDT periods vary ~±10%, so sleep a nominal 870 ms (worst case >750 ms).
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
    float t = _sensor.getTempCByIndex(0);
    // -127 = disconnected; exactly 85.0 is the power-on-reset value the chip
    // returns when the conversion never ran (marginal wiring/power).
    if (t == DEVICE_DISCONNECTED_C || t == 85.0f) return;
    pkt.addI16(Field::T_DS, (int16_t)round(t * 10.0f));
#ifdef VERBOSE
    Serial.print(F("DS18B20 T="));
    Serial.println(t, 1);
#endif
  }
}
