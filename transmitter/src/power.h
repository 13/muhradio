#pragma once
#include <Arduino.h>
#include <LowPower.h>
#include "transport.h"
#include "sleep_plan.h"
#ifdef USE_WDT
#include <avr/wdt.h>
#endif

#ifndef DS_D
#define DS_D 0
#endif

// Arduino's millis() counter (wiring.c). Timer0 stops in power-down, so
// Power::sleepMs() moves it on by the time slept; otherwise a library that
// waits on millis() (the BME680 driver) would wait the whole period again.
extern "C" volatile unsigned long timer0_millis;

namespace Power {
  // Opt-in watchdog (-DUSE_WDT). Needs Optiboot: the stock Pro Mini
  // ATmegaBOOT boot-loops after a watchdog reset. Armed while awake so a hung
  // I2C/OneWire/SPI transaction resets the node instead of draining it; timed
  // LowPower sleeps reuse the WDT as their wake timer, so sleepDeep() disarms
  // it for the duration.
  inline void wdtArm() {
#ifdef USE_WDT
    wdt_enable(WDTO_8S);
#endif
  }
  inline void wdtDisarm() {
#ifdef USE_WDT
    wdt_disable();
#endif
  }

  // t=0: sleep forever (interrupt wake)
  // t>0: sleep t seconds
  inline void sleepDeep(uint16_t t = 0) {
    Transport::sleep();
    wdtDisarm();
#if DS_D > 0
    delay(DS_D);
#endif

    if (t == 0) {
#ifdef VERBOSE
      Serial.println(F("Sleep: forever"));
#endif
#if defined(VERBOSE) || defined(DEBUG)
      Serial.flush();
#endif
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
      wdtArm();
      return;
    }

#ifdef VERBOSE
    Serial.print(F("Sleep: "));
    Serial.print(t);
    Serial.println(F("s"));
#endif
#if defined(VERBOSE) || defined(DEBUG)
    Serial.flush();
#endif
    for (uint16_t i = 0; i < t / 8; i++) {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    }
    // Remainder in 4/2/1 s steps so e.g. t=10 sleeps ~10 s, not 8 s
    uint8_t r = t % 8;
    if (r & 4) LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
    if (r & 2) LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
    if (r & 1) LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
    wdtArm();
  }

  // Power down for at least `ms` (WDT periods, rounded up to 15 ms; the WDT
  // oscillator runs up to ~10 % slow, so add margin for a hard deadline) and
  // advance millis() by the nominal time slept. Used to wait out sensor
  // conversions at ~5 uA instead of busy-waiting at ~4 mA.
  inline void sleepMs(uint16_t ms) {
    uint16_t plan[SLEEP_PLAN_MAX];
    uint8_t n = sleepPlan(ms, plan, SLEEP_PLAN_MAX);
    uint32_t slept = 0;
    for (uint8_t i = 0; i < n; i++) {
      period_t p;
      switch (plan[i]) {
        case 8000: p = SLEEP_8S;    break;
        case 4000: p = SLEEP_4S;    break;
        case 2000: p = SLEEP_2S;    break;
        case 1000: p = SLEEP_1S;    break;
        case 500:  p = SLEEP_500MS; break;
        case 250:  p = SLEEP_250MS; break;
        case 120:  p = SLEEP_120MS; break;
        case 60:   p = SLEEP_60MS;  break;
        case 30:   p = SLEEP_30MS;  break;
        default:   p = SLEEP_15MS;  break;
      }
      LowPower.powerDown(p, ADC_OFF, BOD_OFF);
      slept += plan[i];
    }
    uint8_t sreg = SREG;
    cli();
    timer0_millis += slept;
    SREG = sreg;
    wdtArm(); // LowPower's wake ISR disables the WDT; re-arm the awake watchdog (USE_WDT)
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
    static_assert(DS_S >= 1, "DS_S must be >= 1 (0 means sleep forever)");
    sleepDeep(DS_S);
#elif defined(DS_M)
    sleepDeep((uint16_t)DS_M * 60);
#else
#error "Define either DS_S (seconds) or DS_M (minutes)"
#endif
  }
}
