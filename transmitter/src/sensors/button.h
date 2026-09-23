#pragma once
#include <Arduino.h>
#include "../packet.h"
#include "wake.h"

#if defined(LED_PIN) && !defined(LED_MS)
#define LED_MS 50
#endif
#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 20
#endif

namespace Button {
  static WakeEdge _edge;

  inline void setup() {
    pinMode(SENSOR_PIN_BUTTON, INPUT_PULLUP);
#ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
#endif
#ifdef VERBOSE
    Serial.print(F("Button: "));
    Serial.println(digitalRead(SENSOR_PIN_BUTTON) == HIGH ? F("open") : F("pressed"));
#endif
    Wake::attach(SENSOR_PIN_BUTTON);
  }

  // INPUT_PULLUP: LOW = pressed. Only a press transmits; release is ignored.
  inline bool pending() {
    return Wake::pending(_edge, SENSOR_PIN_BUTTON, LOW, BUTTON_DEBOUNCE_MS);
  }

  inline void read(Packet& pkt) {
    if (!_edge.active) return; // boot announce while released: VCC only
#ifdef LED_PIN
    // Press feedback; D13 (LED_BUILTIN) is SPI SCK, so this is an external LED
    digitalWrite(LED_PIN, HIGH);
    delay(LED_MS);
    digitalWrite(LED_PIN, LOW);
#endif
    pkt.addU8(Field::BUTTON, 1);
#ifdef VERBOSE
    Serial.println(F("Button: 1"));
#endif
  }
}
