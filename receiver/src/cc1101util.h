#pragma once
// CC1101 bring-up and recovery shared by radio.cpp (muhradio packets) and
// bresser.cpp (Bresser 7-in-1). Header-only; include after config.h.
#include <Arduino.h>
#include <SPI.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

// SPI + chip detect with retries; reboots if the chip never answers (wiring).
static inline void cc1101Begin() {
#ifdef ESP8266
  SPI.begin();
#else
  SPI.begin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, CC1101_SS);
#endif
  ELECHOUSE_cc1101.setSpiPin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, CC1101_SS);
  ELECHOUSE_cc1101.Init();
  for (uint8_t attempt = 1; !ELECHOUSE_cc1101.getCC1101(); attempt++) {
    if (attempt >= 3) {
      Serial.println(F("SPI ERROR — check wiring, rebooting"));
      delay(2000);
      ESP.restart();
    }
    delay(200);
    ELECHOUSE_cc1101.Init();
  }
}

// Flush the RX FIFO and re-enter RX. SFRX is only honoured in IDLE or
// RXFIFO_OVERFLOW, hence the SIDLE first.
static inline void cc1101Rearm() {
  ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
  ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
  ELECHOUSE_cc1101.SetRx();
}

// Belt-and-braces for a lost GDO0 edge: every 10 s, recover from
// RXFIFO_OVERFLOW (MARCSTATE 17). Returns true when it had to recover.
static inline bool cc1101OverflowWatch(unsigned long& watchAt) {
  unsigned long ms = millis();
  if (ms - watchAt < 10000) return false;
  watchAt = ms;
  if ((ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F) != 17) return false;
  cc1101Rearm();
  return true;
}
