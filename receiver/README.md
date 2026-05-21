# muh-radio receiver

ESP32 / ESP8266 gateway: receives LoRa or CC1101 packets from sensor nodes and
Bresser weather stations, publishes JSON to MQTT, and serves a live WebSocket web UI.

## Features

- **Three radio modes** — LoRa (SX1276), CC1101 custom sensors, CC1101 Bresser weather station
- Binary packet parsing — bitmap-driven field decoding, no allocations
- Optional AES-128 ECB decryption (PKCS#7, must match transmitter key)
- Bresser 7-in-1 decoder — standalone, no external library (ported from rtl_433 protocol)
- MQTT publish with LWT (`online`/`offline`), IP, and VERSION topics
- WebSocket live status: RSSI, memory, uptime, last packets with color chips
- Settings page (description, WiFi, MQTT, timezone/DST)
- mDNS (`http://esp8266-XXXX.local` / `http://esp32-XXXX.local`)
- NTP timestamping with configurable UTC offset and EU DST auto-rule
- OTA: espota (ArduinoOTA), web upload at `/update.html`
  - ESP8266: synchronous updater on port 8080 (avoids AsyncWebServer conflict)
  - Supports single firmware.bin, single littlefs.bin, or combined `ota_bundle.bin`
- CC1101: interrupt-driven receive (FALLING edge on GDO0), RXFIFO overflow watchdog

## Supported boards

| Environment | Platform | Radio | Mode |
|---|---|---|---|
| `esp32-s3-zero` | ESP32-S3 | LoRa | custom sensors (default) |
| `esp32-s3-zero-cc1101` | ESP32-S3 | CC1101 | custom sensors |
| `esp32-s3-zero-bresser` | ESP32-S3 | CC1101 | Bresser 7003600 |
| `esp32-c3-mini` | ESP32-C3 | LoRa | custom sensors |
| `esp32-c3-mini-cc1101` | ESP32-C3 | CC1101 | custom sensors |
| `esp32-c3-mini-bresser` | ESP32-C3 | CC1101 | Bresser 7003600 |
| `d1_mini` | ESP8266 | CC1101 | custom sensors |
| `d1_mini_espota` | ESP8266 | CC1101 | custom sensors, espota upload |
| `d1_mini-bresser` | ESP8266 | CC1101 | Bresser 7003600 |

## Wiring

CC1101 wiring is identical for custom sensor and Bresser environments on the same board.

### LoRa — ESP32-S3 Zero

| Signal | GPIO |
|---|---|
| SCK | 9 |
| MISO | 7 |
| MOSI | 8 |
| SS | 10 |
| RST | 11 |
| DIO0 | 2 |

### LoRa — ESP32-C3 Mini

| Signal | GPIO |
|---|---|
| SCK | 8 |
| MISO | 6 |
| MOSI | 7 |
| SS | 9 |
| RST | 10 |
| DIO0 | 2 |

### CC1101 — ESP32-S3 Zero

| Signal | GPIO |
|---|---|
| SCK | 9 |
| MISO | 7 |
| MOSI | 8 |
| SS (CSN) | 6 |
| GDO0 | 2 |

### CC1101 — ESP32-C3 Mini

| Signal | GPIO |
|---|---|
| SCK | 8 |
| MISO | 6 |
| MOSI | 7 |
| SS (CSN) | 5 |
| GDO0 | 2 |

### CC1101 — D1 Mini (ESP8266)

| Signal | GPIO | NodeMCU label |
|---|---|---|
| SCK | 14 | D5 |
| MISO | 12 | D6 |
| MOSI | 13 | D7 |
| SS (CSN) | 15 | D8 |
| GDO0 | 5 | D1 |

GDO0 is wired and used as a FALLING-edge interrupt — required for reliable
reception on ESP8266 where WiFi can block the main loop for several seconds.

## Radio settings

### CC1101 — custom sensors

| Parameter | Value |
|---|---|
| Frequency | 868.32 MHz |
| Modulation | 2-FSK |
| Data rate | 99.97 kBaud |
| Deviation | 47.6 kHz |
| RxBW | 812.50 kHz |
| Sync word | 0xD5 0x93 |
| CRC | enabled (hardware) |

### CC1101 — Bresser 7003600

| Parameter | Value |
|---|---|
| Frequency | 868.30 MHz |
| Modulation | 2-FSK |
| Data rate | 8.21 kbps |
| Deviation | 57.14 kHz |
| RxBW | 270.0 kHz |
| Sync word | 0xAA 0x2D |
| Packet length | 27 bytes (fixed) |
| CRC | disabled (LFSR-16 digest in software) |

### LoRa

| Parameter | Value |
|---|---|
| Frequency | 868 MHz |
| Spreading factor | 10 |
| Sync word | 0x13 |
| CRC | enabled |

## Installation

1. **`src/config.h`** — set `MQTT_SERVER`, `MQTT_TOPIC`, `MQTT_TOPIC_LWT`.

2. **`pio_secrets.py`** (gitignored):
   ```sh
   cp pio_secrets_example.py pio_secrets.py
   # fill in WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS
   # optional: AES_KEY = "..." (must match transmitter, custom sensors only)
   ```

3. **Build and upload**:
   ```sh
   # custom sensors
   pio run -e d1_mini -t upload        # firmware
   pio run -e d1_mini -t uploadfs      # web UI (LittleFS)

   # Bresser 7003600
   pio run -e d1_mini-bresser -t upload
   pio run -e d1_mini-bresser -t uploadfs
   ```

4. **After first boot** — open Settings (`/settings.html`) to set the
   Description (shown in the nav bar), timezone offset, and DST mode.

### OTA update (after first flash)

**espota** (ArduinoOTA, port 8266):
```sh
pio run -e d1_mini_espota -t upload
```

**Web OTA** — build a bundle and upload via `/update.html`:
```sh
pio run -e d1_mini && pio run -e d1_mini -t buildfs && pio run -e d1_mini -t otabundle
# upload .pio/build/d1_mini/ota_bundle.bin
```

ESP8266 also exposes a secondary synchronous updater at `http://<ip>:8080/update`
(plain `firmware.bin` / `littlefs.bin`, no bundle required).

## Multi-receiver dedup (CC1101 custom sensors)

CC1101 custom-sensor receivers publish on **MQTT port 1881**. A Node-RED flow
subscribes there, deduplicates by `uid+pid` with a 60 s TTL, and republishes
confirmed packets to port 1883:

```
Receiver A ──1881──► Node-RED dedup ──1883──► Home Assistant / DB
Receiver B ──1881──►
```

LoRa and Bresser receivers publish directly to port 1883.

## MQTT topics

### Custom sensors

```
{MQTT_TOPIC}/{uid}/json          ← packet JSON
{MQTT_TOPIC_LWT}/{hostname}/LWT  ← online / offline
{MQTT_TOPIC_LWT}/{hostname}/IP
{MQTT_TOPIC_LWT}/{hostname}/VERSION
```

### Bresser 7003600

```
muh/bresser/{sensor_id}/json     ← weather JSON (published on every reception)
{MQTT_TOPIC_LWT}/{hostname}/LWT
{MQTT_TOPIC_LWT}/{hostname}/IP
{MQTT_TOPIC_LWT}/{hostname}/VERSION
```

## JSON output

### Custom sensors

```json
{
  "uid": 96,
  "pid": 173,
  "T_BME": 22.4,
  "H_BME": 58.1,
  "P_BME": 1013.2,
  "G_BME": 142.0,
  "VCC": 3.2,
  "RSSI": -74,
  "SNR": 0,
  "RN": "AB12",
  "timestamp": 1716000000
}
```

`RN` is the receiver's 4-char node ID (last two MAC bytes). Fields present
depend on the transmitter's sensor configuration.

### Bresser 7003600

```json
{
  "id": "A1B2",
  "ch": 0,
  "bat": "1",
  "temp": 21.5,
  "hum": 65,
  "wind": 3.2,
  "gust": 5.1,
  "dir": 270,
  "rain": 12.4,
  "uv": 2.1,
  "lux": 15000,
  "rssi": -75,
  "node": "esp32-AB12",
  "ts": 1716000000
}
```

`bat` is `"1"` (ok) or `"0"` (low). The station transmits every ~60 s with
three retransmissions per cycle.

## License

MIT
