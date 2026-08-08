#ifdef USE_BRESSER
#include "bresser.h"
#include "config.h"
#include <SPI.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

// Bresser 7-in-1 protocol (model 7003600)
// 868.3 MHz · 8.21 kbps FSK · sync AA2D · 26-byte payload.
// The sync match consumes the preamble through 0xD4, so the FIFO delivers the
// 26-byte payload starting at byte 0 (no leading marker), whitened with XOR 0xAA.
// We read one extra byte (27) and ignore it.
// Integrity: LFSR-16 digest (gen=0x8810, key=0xba95, final_xor=0x6df1).

#include "bresser_decode.h" // lfsr16 + decode7in1 (pure, native-tested)

#define BRESSER_PKT_LEN  27   // fixed CC1101 packet length (1 trailing byte ignored)
#define PAYLOAD_LEN      BRESSER_PAYLOAD_LEN

// In fixed-length mode the CC1101 prepends NO length byte: the FIFO holds the
// BRESSER_PKT_LEN payload bytes verbatim. We must therefore read the FIFO
// directly (see readFixedPkt) rather than via the library's ReceiveData(),
// which is variable-length only — it consumes FIFO byte 0 as a length, eating
// payload byte 0 and returning a bogus count, so the packet is never accepted.
static bool    _ready = false;
static uint8_t _rxBuf[BRESSER_PKT_LEN];
static int     _rxRssi;

// ── ISR ───────────────────────────────────────────────────────────────────────
#ifdef CC1101_GDO0
static volatile bool _gdo0Flag = false;
#if defined(ESP32) || defined(ESP8266)
#  define ISR_ATTR IRAM_ATTR
#else
#  define ISR_ATTR
#endif
static void ISR_ATTR _bIsr() { _gdo0Flag = true; }
#endif

// ── Public API ────────────────────────────────────────────────────────────────

void Bresser::init() {
  Serial.print(F("> [Bresser] Init CC1101... "));
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
#ifdef CC1101_GDO0
  ELECHOUSE_cc1101.setGDO0(CC1101_GDO0);
  attachInterrupt(digitalPinToInterrupt(CC1101_GDO0), _bIsr, FALLING);
#endif
  // RF config for Bresser 7-in-1
  ELECHOUSE_cc1101.setCCMode(1);          // packet mode; sets IOCFG0=0x06 (sync→EOPkt)
  ELECHOUSE_cc1101.setModulation(0);      // 2-FSK
  ELECHOUSE_cc1101.setMHZ(868.30);
  ELECHOUSE_cc1101.setDeviation(57.14);   // kHz  (≈57.136 kHz target)
  ELECHOUSE_cc1101.setChannel(0);
  ELECHOUSE_cc1101.setRxBW(270.0);        // kHz
  ELECHOUSE_cc1101.setDRate(8.21);        // kbps
  ELECHOUSE_cc1101.setSyncMode(2);        // 16-bit sync word
  // Bresser 7-in-1 on-air sync is AA 2D D4 (AA = preamble). Program the trailing
  // 2D D4 so the FIFO starts at payload byte 0; matching only AA 2D would leave
  // D4 as a leading byte and shift the whole payload by one.
  ELECHOUSE_cc1101.setSyncWord(0x2D, 0xD4);
  ELECHOUSE_cc1101.setAdrChk(0);
  ELECHOUSE_cc1101.setAddr(0);
  ELECHOUSE_cc1101.setWhiteData(0);       // no hardware whitening (done in software)
  ELECHOUSE_cc1101.setPktFormat(0);
  ELECHOUSE_cc1101.setLengthConfig(0);    // fixed packet length
  ELECHOUSE_cc1101.setPacketLength(BRESSER_PKT_LEN);
  ELECHOUSE_cc1101.setCrc(0);             // no hardware CRC (LFSR digest checked in SW)
  ELECHOUSE_cc1101.setCRC_AF(0);
  ELECHOUSE_cc1101.setDcFilterOff(0);
  ELECHOUSE_cc1101.setManchester(0);
  ELECHOUSE_cc1101.setFEC(0);
  ELECHOUSE_cc1101.setPRE(0);
  ELECHOUSE_cc1101.setPQT(0);
  ELECHOUSE_cc1101.setAppendStatus(0);
  ELECHOUSE_cc1101.SetRx();
  Serial.println(F("OK"));
}

// CC1101 errata (SWRZ020): RXBYTES can return a stale value when read while
// a byte is being received — read repeatedly until two reads agree.
static uint8_t readRxBytes() {
  uint8_t rb = ELECHOUSE_cc1101.SpiReadStatus(CC1101_RXBYTES), prev;
  do {
    prev = rb;
    rb = ELECHOUSE_cc1101.SpiReadStatus(CC1101_RXBYTES);
  } while (rb != prev);
  return rb;
}

// Read exactly BRESSER_PKT_LEN bytes straight from the RX FIFO (fixed-length
// mode). RXBYTES bit7 is the overflow flag, bits6:0 the byte count. We require a
// full packet and no overflow, copy it into _rxBuf, then always flush + re-arm
// RX. Returns true once _rxBuf holds a complete packet.
static bool readFixedPkt() {
  bool got = false;
  uint8_t rb = readRxBytes();
  if (!(rb & 0x80) && (rb & 0x7F) >= BRESSER_PKT_LEN) {
    ELECHOUSE_cc1101.SpiReadBurstReg(CC1101_RXFIFO, _rxBuf, BRESSER_PKT_LEN);
    _rxRssi = ELECHOUSE_cc1101.getRssi();
    got = true;
  }
  ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
  ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
  ELECHOUSE_cc1101.SetRx();
  return got;
}

bool Bresser::pending() {
#ifdef CC1101_GDO0
  if (!_ready && _gdo0Flag) {
    _gdo0Flag = false;
    if (readFixedPkt()) _ready = true;
  }
  // Watchdog: recover from RXFIFO_OVERFLOW
  {
    static unsigned long _watchAt = 0;
    unsigned long ms = millis();
    if (!_ready && ms - _watchAt >= 10000) {
      _watchAt = ms;
      uint8_t st = ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F;
      if (st == 17) {
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
        ELECHOUSE_cc1101.SetRx();
        Serial.println(F("> [Bresser] overflow — recovered"));
      }
    }
  }
#else
  if (!_ready && (readRxBytes() & 0x7F) >= BRESSER_PKT_LEN) {
    if (readFixedPkt()) _ready = true;
  }
#endif
  return _ready;
}

BresserPacket Bresser::decode(time_t ts, const char* nodeId) {
  BresserPacket pkt = {};
  _ready = false;
  if (!decode7in1(_rxBuf, _rxRssi, ts, nodeId, MQTT_TOPIC_BRESSER, pkt)) {
#ifdef DEBUG
    Serial.printf("> [Bresser] decode failed (b0=0x%02X rssi=%d)\n  raw:", _rxBuf[0], _rxRssi);
    for (int i = 0; i < BRESSER_PKT_LEN; i++) Serial.printf(" %02X", _rxBuf[i]);
    Serial.print("\n  dwh:");
    for (int i = 0; i < PAYLOAD_LEN; i++) Serial.printf(" %02X", _rxBuf[i] ^ 0xAA);
    Serial.println();
#endif
  } else {
    Serial.printf("> [Bresser] %s\n", pkt.json);
  }
  return pkt;
}
#endif
