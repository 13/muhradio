#pragma once
#include <Arduino.h>
#include <LowPower.h>
#include <LoRa.h>

namespace Power {
  // t=0: sleep forever (interrupt wake)
  // t=1..7: sleep t minutes  (note: uint16_t arithmetic prevents overflow)
  // t=8+: sleep t seconds
  inline void sleepDeep(uint8_t t = 0) {
    LoRa.sleep();
    delay(DS_D);

    if (t == 0) {
#ifdef VERBOSE
      Serial.println(F("Sleep: forever"));
#endif
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
      return;
    }

    uint16_t seconds = (t < 8) ? (uint16_t)t * 60 : t;
#ifdef VERBOSE
    Serial.print(F("Sleep: "));
    if (t < 8) { Serial.print(t); Serial.println(F("min")); }
    else        { Serial.print(t); Serial.println(F("s"));   }
#endif
    for (uint16_t i = 0; i < seconds / 8; i++) {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    }
  }

  inline void sleepSensor() {
#if defined(SENSOR_TYPE_pir)    || defined(SENSOR_TYPE_radar) || \
    defined(SENSOR_TYPE_switch) || defined(SENSOR_TYPE_button)
    sleepDeep();
#else
    sleepDeep(DS_L);
#endif
  }
}
