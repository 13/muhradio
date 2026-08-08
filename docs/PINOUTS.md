# Pinouts

Canonical wiring reference for all muhradio boards. The tables here are the
single source — READMEs link here instead of duplicating them.

## Transmitter — Arduino Pro Mini (3.3 V / 8 MHz)

### Pro Mini → LoRa (RFM95W / SX1276)

| Pro Mini | RFM95W |
|---|---|
| VCC (3.3 V) | 3V |
| GND | GND |
| D13 | SCK |
| D12 | MISO |
| D11 | MOSI |
| D10 | NSS |
| D9 | RST |
| D2 | DIO0 |

### Pro Mini → CC1101

Default hardware SPI pins. GDO0 is required for TX-done detection.

| Pro Mini | CC1101 |
|---|---|
| VCC (3.3 V) | VCC |
| GND | GND |
| D13 | SCK |
| D12 | MISO |
| D11 | MOSI |
| D10 | SS (CSN) |
| D2 | GDO0 (override: `-DCC1101_GDO0=<pin>`) |

### Sensors

**I2C — Si7021 / BMP280 / BME680** (shared bus):

| Pro Mini | Sensor |
|---|---|
| A4 | SDA |
| A5 | SCL |
| VCC (3.3 V) | VCC |
| GND | GND |

**DS18B20 (1-Wire)** — 4.7 kΩ pull-up between DATA and VCC:

| Pro Mini | DS18B20 |
|---|---|
| D3 (default, `-DSENSOR_PIN_DS18B20=N`) | DATA |
| VCC (3.3 V) | VCC |
| GND | GND |

**Button / Switch / PIR / Radar (digital interrupt)**:

| Pro Mini | Sensor |
|---|---|
| D3 (default, `-DSENSOR_PIN_xxx=N`) | OUT / signal |
| VCC (3.3 V) | VCC |
| GND | GND |

D2 is reserved for the radio (DIO0 / GDO0); D3 is the only other interrupt
pin on the Pro Mini, so interrupt sensors default there. Override with
`-DSENSOR_PIN_BUTTON=N`, `-DSENSOR_PIN_SWITCH=N`, `-DSENSOR_PIN_PIR=N` or
`-DSENSOR_PIN_RADAR=N` (any digital pin works for non-interrupt wake if you
poll instead).

## Receiver

CC1101 wiring is identical for custom-sensor and Bresser environments on the
same board.

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

GDO0 is wired and used as a FALLING-edge interrupt on all receiver boards —
required for reliable reception on ESP8266, where WiFi can block the main
loop for several seconds.

Receiver pin constants live in `receiver/src/radio.cpp` (LoRa) and the
`-DCC1101_*` build flags in `receiver/platformio.ini` (CC1101).
