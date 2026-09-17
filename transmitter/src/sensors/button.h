#pragma once
#include <Arduino.h>
#include "../packet.h"

#if defined(LED_PIN) && !defined(LED_MS)
#define LED_MS 50
#endif

namespace Button {
  static volatile bool _detected = false;
  static volatile bool _state    = false;

  static void _isr() {
    _state    = digitalRead(SENSOR_PIN_BUTTON);
    _detected = true;
  }

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
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN_BUTTON), _isr, FALLING);
  }

  inline void read(Packet& pkt) {
    if (!_detected) return;
    bool s = _state;
    _detected = false;
#ifdef LED_PIN
    // Press feedback; D13 (LED_BUILTIN) is SPI SCK, so this is an external LED
    digitalWrite(LED_PIN, HIGH);
    delay(LED_MS);
    digitalWrite(LED_PIN, LOW);
#endif
    // INPUT_PULLUP: LOW = pressed → 1
    uint8_t state = (s == HIGH) ? 0 : 1;
    pkt.addU8(Field::BUTTON, state);
#ifdef VERBOSE
    Serial.print(F("Button: ")); Serial.println(state);
#endif
  }
}
