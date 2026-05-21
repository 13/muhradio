#include "radio.h"
#include "config.h"
#include <version.h>
#include <SPI.h>

#ifdef USE_LORA
#include <LoRa.h>
#endif

#ifdef USE_CC1101
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#endif

#ifdef USE_CRYPTO
#include <Crypto.h>
#include <AES.h>
#endif

// ── SPI / LoRa pin assignments ─────────────────────────────────────────────────
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  static constexpr int PIN_MISO = 7;
  static constexpr int PIN_MOSI = 8;
  static constexpr int PIN_SCK  = 9;
  static constexpr int PIN_SS   = 10;
  static constexpr int PIN_RST  = 11;
  static constexpr int PIN_DIO0 = 2;
#elif defined(ESP8266)
  static constexpr int PIN_MISO = 12; // D6
  static constexpr int PIN_MOSI = 13; // D7
  static constexpr int PIN_SCK  = 14; // D5
  static constexpr int PIN_SS   = 2;  // D4
  static constexpr int PIN_RST  = 0;  // D3
  static constexpr int PIN_DIO0 = 5;  // D1
#else
  static constexpr int PIN_MISO = 6;
  static constexpr int PIN_MOSI = 7;
  static constexpr int PIN_SCK  = 8;
  static constexpr int PIN_SS   = 9;
  static constexpr int PIN_RST  = 10;
  static constexpr int PIN_DIO0 = 2;
#endif

static constexpr uint8_t ADDR_RECEIVER = 0x15;

// ── Field table (matches transmitter packet.h) ─────────────────────────────────
static constexpr uint8_t FIELD_SIZES[]  = {2,1,1,1,1, 2,2,2,2,4, 2,2,4,2,1};
static constexpr uint8_t FIELD_SCALES[] = {1,1,1,1,1, 10,10,10,10,10, 10,10,10,1,10};
static constexpr bool    FIELD_SIGNED[] = {
  false,false,false,false,false,
  true,true,true,true,false,
  true,true,false,false,false
};
static const char* const FIELD_NAMES[] = {
  "COUNTER","BUTTON","SWITCH","PIR","RADAR",
  "T_SI","H_SI","T_DS","T_BMP","P_BMP",
  "T_BME","H_BME","P_BME","G_BME","VCC"
};

// ── AES globals ────────────────────────────────────────────────────────────────
#ifdef USE_CRYPTO
static byte   _aeskey[16];
static AES128 _aes;

static void _hexToBytes(const char* hex, byte* out, uint8_t len) {
  auto nibble = [](char c) -> uint8_t {
    return c >= 'a' ? c-'a'+10 : c >= 'A' ? c-'A'+10 : c-'0';
  };
  for (uint8_t i = 0; i < len; i++)
    out[i] = (nibble(hex[i*2]) << 4) | nibble(hex[i*2+1]);
}
#endif

// ── ISR attribute ─────────────────────────────────────────────────────────────
#if defined(ESP32) || defined(ESP8266)
  #define ISR_ATTR IRAM_ATTR
#else
  #define ISR_ATTR
#endif

// ── ISR-safe LoRa receive buffer ───────────────────────────────────────────────
#ifdef USE_LORA

static volatile bool _pktReady = false;
static RxPacket      _rxPkt;

static void ISR_ATTR _onReceive(int size) {
  if (_pktReady || size == 0 || size > (int)sizeof(_rxPkt.buf)) return;
  uint8_t l = 0;
  while (LoRa.available() && l < sizeof(_rxPkt.buf))
    _rxPkt.buf[l++] = LoRa.read();
  _rxPkt.len  = l;
  _rxPkt.rssi = LoRa.packetRssi();
  _rxPkt.snr  = LoRa.packetSnr();
  _pktReady   = true;
}
#endif // USE_LORA

// ── CC1101 receive buffer ──────────────────────────────────────────────────────
#ifdef USE_CC1101
static bool     _cc1101Ready = false;
static RxPacket _cc1101Pkt;
#ifdef CC1101_GDO0
static volatile bool _gdo0Flag = false;
static void ISR_ATTR _cc1101ISR() { _gdo0Flag = true; }
#endif
#endif

// ── Minimal JSON builder (no heap, no ArduinoJson) ────────────────────────────
struct JsonBuilder {
  char*  buf;
  size_t cap;
  size_t n;
  bool   sep;

  JsonBuilder(char* b, size_t c) : buf(b), cap(c), n(0), sep(false) {
    buf[n++] = '{';
  }
  void finish() {
    if (n < cap - 1) { buf[n++] = '}'; buf[n] = '\0'; }
  }
  bool ok() const { return n < cap - 16; }

  void kv(const char* k, long v) {
    _key(k); n += snprintf(buf+n, cap-n, "%ld", v);
  }
  void kv(const char* k, float v) {
    _key(k); n += snprintf(buf+n, cap-n, "%.1f", v);
  }
  void kvs(const char* k, const char* v) {
    _key(k);
    if (n < cap) buf[n++] = '"';
    for (const char* s = v; *s && n < cap-2; s++) buf[n++] = *s;
    if (n < cap) buf[n++] = '"';
  }

private:
  void _key(const char* k) {
    if (sep && n < cap) buf[n++] = ',';
    n += snprintf(buf+n, cap-n, "\"%s\":", k);
    sep = true;
  }
};

// ── Public API ─────────────────────────────────────────────────────────────────

void Radio::init() {
#ifdef USE_LORA
  Serial.print(F("> [LoRa] Init... "));
#ifdef ESP8266
  SPI.begin();
#else
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SS);
#endif
  LoRa.setSPI(SPI);
  LoRa.setPins(PIN_SS, PIN_RST, PIN_DIO0);
  if (!LoRa.begin(LO_FREQ)) {
    Serial.println(F("FAIL — rebooting"));
    ESP.restart();
  }
  LoRa.setSpreadingFactor(10);
  LoRa.setSyncWord(0x13);
  LoRa.enableCrc();
  LoRa.onReceive(_onReceive);
  LoRa.receive();
  Serial.println(F("OK"));
#endif // USE_LORA

#ifdef USE_CRYPTO
  _hexToBytes(AES_KEY, _aeskey, 16);
  _aes.setKey(_aeskey, 16);
  Serial.printf("> [CRYPTO] key: %02x%02x..%02x%02x\n",
    _aeskey[0], _aeskey[1], _aeskey[14], _aeskey[15]);
#endif

#ifdef USE_CC1101
  Serial.print(F("> [CC1101] Init... "));
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
  attachInterrupt(digitalPinToInterrupt(CC1101_GDO0), _cc1101ISR, FALLING);
#endif
  ELECHOUSE_cc1101.setCCMode(1);
  ELECHOUSE_cc1101.setModulation(0);
  ELECHOUSE_cc1101.setMHZ(CC1101_MHZ);
  ELECHOUSE_cc1101.setDeviation(47.60);
  ELECHOUSE_cc1101.setChannel(0);
  ELECHOUSE_cc1101.setChsp(199.95);
  ELECHOUSE_cc1101.setRxBW(812.50);
  ELECHOUSE_cc1101.setDRate(99.97);
  ELECHOUSE_cc1101.setSyncMode(2);
  ELECHOUSE_cc1101.setSyncWord(213, 147);
  ELECHOUSE_cc1101.setAdrChk(0);
  ELECHOUSE_cc1101.setAddr(0);
  ELECHOUSE_cc1101.setWhiteData(0);
  ELECHOUSE_cc1101.setPktFormat(0);
  ELECHOUSE_cc1101.setLengthConfig(1);
  ELECHOUSE_cc1101.setPacketLength(0);
  ELECHOUSE_cc1101.setCrc(1);
  ELECHOUSE_cc1101.setCRC_AF(0);
  ELECHOUSE_cc1101.setDcFilterOff(0);
  ELECHOUSE_cc1101.setManchester(0);
  ELECHOUSE_cc1101.setFEC(0);
  ELECHOUSE_cc1101.setPRE(0);
  ELECHOUSE_cc1101.setPQT(0);
  ELECHOUSE_cc1101.setAppendStatus(0);
  ELECHOUSE_cc1101.SetRx();
  Serial.println(F("OK"));
#endif // USE_CC1101
}

bool Radio::pending() {
  bool ready = false;
#ifdef USE_LORA
  ready = _pktReady;
#endif
#ifdef USE_CC1101
#ifdef CC1101_GDO0
  // Interrupt-driven: ISR sets _gdo0Flag on FALLING edge (GDO0 de-asserts at
  // end of packet OR on RXFIFO overflow), so this fires even if the main loop
  // was blocked by WiFi/MQTT when the event occurred.
  if (!_cc1101Ready && _gdo0Flag) {
    _gdo0Flag = false;
    if (ELECHOUSE_cc1101.CheckCRC()) {
      uint8_t len     = ELECHOUSE_cc1101.ReceiveData(_cc1101Pkt.buf);
      _cc1101Pkt.len  = len;
      _cc1101Pkt.rssi = ELECHOUSE_cc1101.getRssi();
      _cc1101Pkt.snr  = 0;
      _cc1101Ready    = true;
    } else {
      ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
      ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
      ELECHOUSE_cc1101.SetRx();
      Serial.println(F("> [CC1101] CRC fail"));
    }
  }
  // Watchdog: belt-and-suspenders in case an ISR was somehow lost
  {
    static unsigned long _watchAt = 0;
    unsigned long ms = millis();
    if (!_cc1101Ready && ms - _watchAt >= 10000) {
      _watchAt = ms;
      uint8_t st = ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F;
      if (st == 17) { // RXFIFO_OVERFLOW
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
        ELECHOUSE_cc1101.SetRx();
        Serial.println(F("> [CC1101] overflow watchdog — recovered"));
      }
    }
  }
#else
  if (!_cc1101Ready && ELECHOUSE_cc1101.CheckRxFifo(0)) {
    if (ELECHOUSE_cc1101.CheckCRC()) {
      uint8_t len     = ELECHOUSE_cc1101.ReceiveData(_cc1101Pkt.buf);
      _cc1101Pkt.len  = len;
      _cc1101Pkt.rssi = ELECHOUSE_cc1101.getRssi();
      _cc1101Pkt.snr  = 0;
      _cc1101Ready    = true;
    } else {
      ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
      ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
      ELECHOUSE_cc1101.SetRx();
      Serial.println(F("> [CC1101] CRC fail"));
    }
  }
#endif
  ready = ready || _cc1101Ready;
#endif
  return ready;
}

RxPacket Radio::take() {
#ifdef USE_LORA
  if (_pktReady) {
    RxPacket p = _rxPkt;
    _pktReady  = false;
    return p;
  }
#endif
#ifdef USE_CC1101
  RxPacket p   = _cc1101Pkt;
  _cc1101Ready = false;
  return p;
#else
  return RxPacket{};
#endif
}

DecodedPacket Radio::decode(const RxPacket& pkt, time_t ts, const char* nodeId) {
  DecodedPacket result = {};

  if (pkt.len < 7 || pkt.buf[0] != ADDR_RECEIVER) {
    Serial.printf("> [Radio] discard len=%u b0=0x%02X\n", pkt.len, pkt.buf[0]);
    return result;
  }

  const uint8_t* payload    = pkt.buf + 2;
  uint8_t        payloadLen = pkt.len - 2;

#ifdef USE_CRYPTO
  uint8_t plain[80] = {0};
  for (int i = 0; i < payloadLen / 16; i++)
    _aes.decryptBlock(&plain[i * 16], &payload[i * 16]);
  uint8_t padLen = plain[payloadLen - 1];
  if (padLen == 0 || padLen > 16) {
    Serial.printf("> [CRYPTO] bad padding (padLen=%u, payloadLen=%u)\n", padLen, payloadLen);
    Serial.print(F(">  cipher: "));
    for (uint8_t i = 0; i < payloadLen; i++) { if (payload[i]<0x10) Serial.print('0'); Serial.print(payload[i],HEX); Serial.print(' '); }
    Serial.println();
    Serial.print(F(">  plain:  "));
    for (uint8_t i = 0; i < payloadLen; i++) { if (plain[i]<0x10) Serial.print('0'); Serial.print(plain[i],HEX); Serial.print(' '); }
    Serial.println();
    return result;
  }
  payloadLen -= padLen;
  payload     = plain;
#ifdef DEBUG
  Serial.print(F("> [CRYPTO] ")); Serial.print(payloadLen); Serial.println(F("B"));
#endif
#endif

  if (payloadLen < 5) {
    Serial.println(F("> [Radio] too short"));
    return result;
  }

  uint16_t       uid  = payload[0] | ((uint16_t)payload[1] << 8);
  uint8_t        pid  = payload[2];
  uint16_t       bmap = payload[3] | ((uint16_t)payload[4] << 8);
  result.retained     = (bmap >> 15) & 1;
  bmap               &= 0x7FFF;
  const uint8_t* vals = payload + 5;

  Serial.print(F("> [Radio] uid=0x")); Serial.print(uid, HEX);
  Serial.print(F(" pid="));            Serial.print(pid);
  Serial.print(F(" rssi="));           Serial.println(pkt.rssi);

  JsonBuilder jb(result.json, sizeof(result.json));
  jb.kv("uid", (long)uid);
  jb.kv("pid", (long)pid);

  uint8_t pos = 0;
  for (uint8_t bit = 0; bit < 15; bit++) {
    if (!(bmap >> bit & 1)) continue;
    int32_t v = 0;
    for (uint8_t b = 0; b < FIELD_SIZES[bit]; b++)
      v |= (int32_t)vals[pos + b] << (8 * b);
    if (FIELD_SIGNED[bit] && (v & 0x8000)) v |= 0xFFFF0000; // sign-extend int16
    pos += FIELD_SIZES[bit];
    if (FIELD_SCALES[bit] > 1)
      jb.kv(FIELD_NAMES[bit], (float)v / FIELD_SCALES[bit]);
    else
      jb.kv(FIELD_NAMES[bit], (long)v);
  }

  jb.kv("RSSI", (long)pkt.rssi);
  jb.kv("SNR",  (long)(int)pkt.snr);
  jb.kvs("RN",  nodeId);
  jb.kv("timestamp", (long)ts);
  jb.finish();

  snprintf(result.topic, sizeof(result.topic), "%s/%u/json", MQTT_TOPIC, uid);
  result.valid = true;

  Serial.print(F("> [JSON] "));
  Serial.println(result.json);

  return result;
}
