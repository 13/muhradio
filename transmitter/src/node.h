#pragma once
#include <Arduino.h>
#include <EEPROM.h>

namespace Node {
  static constexpr uint8_t EEPROM_ADDR = 13;
  static uint16_t _uid = 0;

  static uint16_t _generate() {
#ifdef CUSTOM_UID
    return (uint16_t)strtol(CUSTOM_UID, nullptr, 16);
#else
    return (uint16_t)random(256, 4096);
#endif
  }

  inline void init() {
#if defined(CUSTOM_UID) || defined(GEN_UID)
    // CUSTOM_UID / GEN_UID always win — write to EEPROM so future boots without
    // the flag still load the intended UID.
    _uid = _generate();
    EEPROM.put(EEPROM_ADDR, _uid);
    delay(10);
#else
    // Check the high byte: UIDs are in [0x0100,0x0FFF] so high byte is
    // always 0x01–0x0F. 0xFF means EEPROM is blank (AVR factory default).
    if (EEPROM.read(EEPROM_ADDR + 1) == 0xFF) {
      _uid = _generate();
      EEPROM.put(EEPROM_ADDR, _uid);
      delay(10);
    } else {
      EEPROM.get(EEPROM_ADDR, _uid);
    }
#endif
#ifdef VERBOSE
    Serial.print(F("> Node: 0x"));
    Serial.println(_uid, HEX);
#endif
  }

  inline uint16_t uid() { return _uid; }
}
