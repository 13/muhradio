#pragma once
#include <Arduino.h>
#include <LowPower.h>
#include <LoRa.h>

#ifndef DS_D
#define DS_D 100
#endif

namespace Power {
  // t=0: sleep forever (interrupt wake)
  // t>0: sleep t seconds
  inline void sleepDeep(uint16_t t = 0) {
    LoRa.sleep();
    delay(DS_D);

    if (t == 0) {
#ifdef VERBOSE
      Serial.println(F("Sleep: forever"));
#endif
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
      return;
    }

#ifdef VERBOSE
    Serial.print(F("Sleep: "));
    Serial.print(t);
    Serial.println(F("s"));
#endif
    for (uint16_t i = 0; i < t / 8; i++) {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    }
  }

  inline void init() {
#ifdef VERBOSE
#if defined(DS_M)
    Serial.print(F("> DS: ")); Serial.print(DS_M); Serial.println(F("m"));
#elif defined(DS_S)
    Serial.print(F("> DS: ")); Serial.print(DS_S); Serial.println(F("s"));
#endif
#endif
  }

  inline void sleepSensor() {
#if defined(SENSOR_TYPE_pir)    || defined(SENSOR_TYPE_radar) || \
    defined(SENSOR_TYPE_switch) || defined(SENSOR_TYPE_button)
    sleepDeep();
#elif defined(DS_S) && defined(DS_M)
#error "Define only one of DS_S (seconds) or DS_M (minutes), not both"
#elif defined(DS_S)
    static_assert(DS_S >= 8, "DS_S must be >= 8 (minimum sleep period is SLEEP_8S)");
    sleepDeep(DS_S);
#elif defined(DS_M)
    sleepDeep((uint16_t)DS_M * 60);
#else
#error "Define either DS_S (seconds) or DS_M (minutes)"
#endif
  }
}
