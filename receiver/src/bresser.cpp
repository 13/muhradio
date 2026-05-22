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

#define BRESSER_PKT_LEN  27   // fixed CC1101 packet length (1 trailing byte ignored)
#define PAYLOAD_LEN      26   // whitened payload bytes, from FIFO byte 0

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

// ── LFSR-16 digest (Bresser 7-in-1) ──────────────────────────────────────────
static uint16_t lfsr16(const uint8_t* msg, unsigned n, uint16_t gen, uint16_t key) {
  uint16_t sum = 0;
  for (unsigned k = 0; k < n; ++k) {
    uint8_t d = msg[k];
    for (int i = 7; i >= 0; --i) {
      if ((d >> i) & 1) sum ^= key;
      key = (key & 1) ? ((key >> 1) ^ gen) : (key >> 1);
    }
  }
  return sum;
}

// ── Decoder ───────────────────────────────────────────────────────────────────
// raw  — CC1101 FIFO; raw[0] is the first whitened payload byte
// rssi — signal strength in dBm
// Returns true and fills `out` on success.
static bool decode7in1(const uint8_t* raw, int rssi,
                       time_t ts, const char* nodeId, BresserPacket& out) {
  // De-whiten 26-byte payload
  const uint8_t* msg = raw;
  uint8_t w[PAYLOAD_LEN];
  for (int i = 0; i < PAYLOAD_LEN; i++) w[i] = msg[i] ^ 0xAA;

  // LFSR-16 integrity check
  uint16_t chk    = ((uint16_t)w[0] << 8) | w[1];
  uint16_t digest = lfsr16(&w[2], 23, 0x8810, 0xba95);
  if ((chk ^ digest) != 0x6df1) {
#ifdef DEBUG
    Serial.printf("> [Bresser] digest %04X^%04X=%04X (want 6DF1)\n",
                  chk, digest, chk ^ digest);
#endif
    return false;
  }

  // s_type is extracted from the RAW (non-de-whitened) byte
  uint8_t s_type = msg[6] >> 4;
  if (s_type != 1) {
    // Only handle SENSOR_TYPE_WEATHER1; skip CO2, VOC, PM subtypes
#ifdef DEBUG
    Serial.printf("> [Bresser] s_type=%u (skip)\n", s_type);
#endif
    return false;
  }

  uint16_t id      = ((uint16_t)w[2] << 8) | w[3];
  uint8_t  chan    = msg[6] & 0x07;
  bool     bat_ok  = ((w[15] & 0x06) != 0x06);

  int wdir     = (w[4] >> 4)*100 + (w[4] & 0x0f)*10 + (w[5] >> 4);
  int wgst     = (w[7] >> 4)*100 + (w[7] & 0x0f)*10 + (w[8] >> 4);
  int wavg     = (w[8] & 0x0f)*100 + (w[9] >> 4)*10 + (w[9] & 0x0f);
  int rain_raw = (w[10] >> 4)*100000 + (w[10] & 0x0f)*10000
               + (w[11] >> 4)*1000   + (w[11] & 0x0f)*100
               + (w[12] >> 4)*10     + (w[12] & 0x0f);
  int temp_raw = (w[14] >> 4)*100 + (w[14] & 0x0f)*10 + (w[15] >> 4);
  int humidity = (w[16] >> 4)*10  + (w[16] & 0x0f);
  int lux_raw  = (w[17] >> 4)*100000 + (w[17] & 0x0f)*10000
               + (w[18] >> 4)*1000   + (w[18] & 0x0f)*100
               + (w[19] >> 4)*10     + (w[19] & 0x0f);
  int uv_raw   = (w[20] >> 4)*100 + (w[20] & 0x0f)*10 + (w[21] >> 4);

  float temp_c = (temp_raw > 600) ? (temp_raw - 1000) * 0.1f : temp_raw * 0.1f;

  snprintf(out.topic, sizeof(out.topic), "muh/bresser/%04X/json", id);
  snprintf(out.json, sizeof(out.json),
    "{\"id\":\"%04X\",\"ch\":%u,\"bat\":%s,"
    "\"temp\":%.1f,\"hum\":%d,"
    "\"wind\":%.1f,\"gust\":%.1f,\"dir\":%d,"
    "\"rain\":%.1f,\"uv\":%.1f,\"lux\":%.1f,"
    "\"rssi\":%d,\"node\":\"%s\",\"ts\":%ld}",
    id, chan, bat_ok ? "1" : "0",
    temp_c, humidity,
    wavg * 0.1f, wgst * 0.1f, wdir,
    rain_raw * 0.1f, uv_raw * 0.1f, lux_raw * 0.001f,
    rssi, nodeId, (long)ts);

  out.valid = true;
  return true;
}

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
  if (!ELECHOUSE_cc1101.getCC1101()) {
    Serial.println(F("SPI ERROR — check wiring"));
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
  ELECHOUSE_cc1101.setSyncWord(0xAA, 0x2D);
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

bool Bresser::pending() {
#ifdef CC1101_GDO0
  if (!_ready && _gdo0Flag) {
    _gdo0Flag = false;
    uint8_t len  = ELECHOUSE_cc1101.ReceiveData(_rxBuf);
    _rxRssi      = ELECHOUSE_cc1101.getRssi();
    if (len == BRESSER_PKT_LEN) {
      _ready = true;
    } else {
      ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
      ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
      ELECHOUSE_cc1101.SetRx();
    }
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
  if (!_ready && ELECHOUSE_cc1101.CheckRxFifo(0)) {
    uint8_t len  = ELECHOUSE_cc1101.ReceiveData(_rxBuf);
    _rxRssi      = ELECHOUSE_cc1101.getRssi();
    if (len == BRESSER_PKT_LEN) {
      _ready = true;
    } else {
      ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
      ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
      ELECHOUSE_cc1101.SetRx();
    }
  }
#endif
  return _ready;
}

BresserPacket Bresser::decode(time_t ts, const char* nodeId) {
  BresserPacket pkt = {};
  _ready = false;
  if (!decode7in1(_rxBuf, _rxRssi, ts, nodeId, pkt)) {
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
