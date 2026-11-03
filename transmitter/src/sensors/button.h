#pragma once
#include <Arduino.h>
#include "../packet.h"
#include "wake.h"
#include "hold_gate.h"

#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 20
#endif
// Hold-to-confirm (garage door): the press is only sent once the button has
// been held this long. 0 = off, sent on press as before.
#ifndef BUTTON_HOLD_MS
#define BUTTON_HOLD_MS 0
#endif
// Releases up to this long during a hold are contact chatter, not a release.
#ifndef BUTTON_GAP_MS
#define BUTTON_GAP_MS 30
#endif
#if defined(LED_PIN) && !defined(LED_MS)
#if BUTTON_HOLD_MS > 0
#define LED_MS 300 // "sent" flash, distinct from the blinking while held
#else
#define LED_MS 50
#endif
#endif

namespace Button {
  static WakeEdge _edge;
  static bool _fire = false; // this packet carries BUTTON=1

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

#if BUTTON_HOLD_MS > 0
  // Blocks for up to BUTTON_HOLD_MS while awake (Timer0 runs, WDT is 8 s).
  // LED blinks at 5 Hz while held.
  inline bool _hold() {
    const HoldGate gate{BUTTON_HOLD_MS, BUTTON_GAP_MS};
    bool ok = gate.wait(
      [] { return digitalRead(SENSOR_PIN_BUTTON) == LOW; },
      [] { return (uint32_t)millis(); },
      [](uint32_t el) {
#ifdef LED_PIN
        digitalWrite(LED_PIN, ((el / 100) & 1) ? LOW : HIGH);
#else
        (void)el;
#endif
        delay(10);
      });
#ifdef LED_PIN
    digitalWrite(LED_PIN, LOW);
#endif
#ifdef VERBOSE
    if (!ok) Serial.println(F("Button: released early"));
#endif
    return ok;
  }
#endif

  // INPUT_PULLUP: LOW = pressed. Only a press transmits; release is ignored.
  inline bool pending() {
    if (!Wake::take()) return false;
    const bool boot = _edge.boot;
    const bool tx = _edge.update(
      Wake::settle(SENSOR_PIN_BUTTON, BUTTON_DEBOUNCE_MS) == LOW);
#if BUTTON_HOLD_MS > 0
    // Boot announce is VCC only, even with the button held: a battery swap
    // must never open the door.
    if (!tx || boot) { _fire = false; return tx; }
    _fire = _hold();
    // Released early: back to sleep, and the next press is a new rise. Edges
    // seen during the hold leave the wake flag set; that wake settles on the
    // level already recorded and sends nothing.
    if (!_fire) _edge.active = false;
    return _fire;
#else
    (void)boot;
    _fire = _edge.active; // boot announce while released: VCC only
    return tx;
#endif
  }

  inline void read(Packet& pkt) {
    if (!_fire) return;
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
