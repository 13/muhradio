# muh-radio transmitter

Arduino Pro Mini sensor node. Wakes from deep sleep, reads one or more sensors,
builds a compact binary packet, transmits over **LoRa** or **CC1101**, and
sleeps again.

## Supported sensors

| Sensor | Fields | Wake mode |
|---|---|---|
| Si7021 | T_SI, H_SI | timed |
| DS18B20 | T_DS | timed |
| BMP280 | T_BMP, P_BMP | timed |
| BME680 | T_BME, H_BME, P_BME, G_BME | timed |
| PIR | PIR | interrupt |
| Radar | RADAR | interrupt |
| Button | BUTTON | interrupt |
| Switch | SWITCH | interrupt |

VCC (battery voltage) is always appended to every packet.

## Packet format

```
[dst:1] [src:1] [uid:2] [pid:1] [bitmap:2] [fields in bit order...]
```

- `uid` — 12-bit node identity (hex `CUSTOM_UID` or random on first boot)
- `pid` — 8-bit packet ID, random 1–255 per transmission (receiver dedup key)
- `bitmap` — 16-bit field presence mask

| Bit | Field | Type | Unit | Sensor |
|---|---|---|---|---|
| 0 | COUNTER | uint16 | — | debug packet counter |
| 1 | BUTTON | uint8 | 0/1 | button |
| 2 | SWITCH | uint8 | 0/1 | switch |
| 3 | PIR | uint8 | 0/1 | PIR motion |
| 4 | RADAR | uint8 | 0/1 | radar motion |
| 5 | T_SI | int16 | °C×10 | Si7021 temperature |
| 6 | H_SI | int16 | %×10 | Si7021 humidity |
| 7 | T_DS | int16 | °C×10 | DS18B20 temperature |
| 8 | T_BMP | int16 | °C×10 | BMP280 temperature |
| 9 | P_BMP | uint32 | Pa/10 | BMP280 pressure |
| 10 | T_BME | int16 | °C×10 | BME680 temperature |
| 11 | H_BME | int16 | %×10 | BME680 humidity |
| 12 | P_BME | uint32 | Pa/10 | BME680 pressure |
| 13 | G_BME | uint16 | kΩ | BME680 gas resistance |
| 14 | VCC | uint8 | V×10 | supply voltage |

## Hardware

### Arduino Pro Mini → LoRa (RFM95W / SX1276)

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

### Arduino Pro Mini → CC1101

Default hardware SPI pins:

| Pro Mini | CC1101 |
|---|---|
| VCC (3.3 V) | VCC |
| GND | GND |
| D13 | SCK |
| D12 | MISO |
| D11 | MOSI |
| D10 | SS (CSN) |
| D2 | GDO0 (optional) |

Interrupt-driven sensors (button, switch, PIR, radar) use **D3** by default
(the only other interrupt pin). Configure with `-DSENSOR_PIN_xxx=N`.

### Sensor pin connections

#### I2C sensors — Si7021 / BMP280 / BME680

| Pro Mini | Sensor |
|---|---|
| A4 | SDA |
| A5 | SCL |
| VCC (3.3 V) | VCC |
| GND | GND |

All three share the same I2C bus. See [I2C address notes](#i2c-address-notes) if
combining BMP280 and BME680.

#### DS18B20 (1-Wire)

| Pro Mini | DS18B20 |
|---|---|
| D3 (default) | DATA |
| VCC (3.3 V) | VCC |
| GND | GND |

Place a 4.7 kΩ pull-up resistor between DATA and VCC.
Override pin with `-DSENSOR_PIN_DS18B20=N`.

#### Button / Switch / PIR / Radar (digital interrupt)

| Pro Mini | Sensor |
|---|---|
| D3 (default) | OUT / signal |
| VCC (3.3 V) | VCC |
| GND | GND |

D2 is reserved for the radio (DIO0 / GDO0), so D3 is the only remaining
external-interrupt pin. Override with `-DSENSOR_PIN_BUTTON=N`,
`-DSENSOR_PIN_SWITCH=N`, `-DSENSOR_PIN_PIR=N`, or `-DSENSOR_PIN_RADAR=N`
(any digital pin works for non-interrupt wake if you poll instead).

## Getting started

### 1. Add a node environment in `platformio.ini`

Each physical node is one `[env:node_xxx]` block. Copy an existing one:

**LoRa node** (Si7021, wakes every 36 s):
```ini
[env:node_bedroom]
build_flags =
    !python build_flags.py
    -DSENSOR_TYPE_si7021="SI7021"
    -DDS_L=36 -DDS_S=8 -DDS_D=100
    -DLO_FREQ=868E6 -DLO_POWER=17
    -DCUSTOM_UID="A1"
    -DVERBOSE
```

**CC1101 node** (DS18B20 + BME680, wakes every 60 s):
```ini
[env:cc1101_workshop]
lib_deps =
    ${env.lib_deps}
    LSatan/SmartRC-CC1101-Driver-Lib
build_flags =
    !python build_flags.py
    -DSENSOR_TYPE_ds18b20="DS18B20"
    -DSENSOR_PIN_DS18B20=3
    -DSENSOR_TYPE_bme680="BME680"
    -DDS_L=60 -DDS_S=8 -DDS_D=100
    -DUSE_CC1101
    -DCC1101_MHZ=868.32
    -DCC1101_POWER=12
    -DCUSTOM_UID="A2"
    -DVERBOSE
```

`CUSTOM_UID` is a **hex string** (e.g. `"A2"` → uid 162). Omit it to get a
random uid generated once at boot and stored in EEPROM.

### 2. All `build_flags`

| Flag | Description |
|---|---|
| `-DSENSOR_TYPE_si7021="SI7021"` | Enable Si7021 |
| `-DSENSOR_TYPE_ds18b20="DS18B20"` | Enable DS18B20 |
| `-DSENSOR_TYPE_bmp280="BMP280"` | Enable BMP280 |
| `-DSENSOR_TYPE_bme680="BME680"` | Enable BME680 |
| `-DSENSOR_TYPE_pir="PIR"` | Enable PIR (interrupt wake) |
| `-DSENSOR_TYPE_radar="RADAR"` | Enable radar (interrupt wake) |
| `-DSENSOR_TYPE_button="BUTTON"` | Enable button (interrupt wake) |
| `-DSENSOR_TYPE_switch="SWITCH"` | Enable switch (interrupt wake) |
| `-DSENSOR_PIN_xxx=N` | Override pin for sensor xxx |
| `-DCUSTOM_UID="hex"` | Fixed node UID (hex string) |
| `-DDS_L=36` | Timed sleep in seconds (≥8) |
| `-DDS_S=8` | Short sleep in seconds |
| `-DDS_D=100` | Pre-sleep delay in ms |
| `-DLO_FREQ=868E6` | LoRa frequency (868E6 EU, 915E6 US) |
| `-DLO_POWER=17` | LoRa TX power in dBm |
| `-DUSE_CC1101` | Use CC1101 instead of LoRa |
| `-DCC1101_MHZ=868.32` | CC1101 frequency in MHz |
| `-DCC1101_POWER=12` | CC1101 TX power (dBm, library units) |
| `-DVERBOSE` | Serial debug output |
| `-DDEBUG` | Verbose packet hex dump |
| `-DVERBOSE_PC` | Include packet counter field |

### 3. Node registry

Keep a comment block at the top of `platformio.ini` as a registry of all
physical nodes with their UIDs to avoid collisions:

```ini
;   UID   DEC  env                    radio   name         sensors
;   0x01    1  cc1101_button          CC1101  Hallway      button
;   0x16   22  cc1101_ds18b20_bme680  CC1101  Shower       DS18B20+BME680
;   0xA1  161  node_bedroom           LoRa    Bedroom      Si7021
```

### 4. AES-128 encryption (optional)

```sh
cp pio_secrets_example.py pio_secrets.py
openssl rand -hex 16   # generate key
# paste into pio_secrets.py as AES_KEY = "..."
```

`build_flags.py` automatically adds `-DUSE_CRYPTO -DAES_KEY=...`. The key must
match exactly on both transmitter and receiver. `pio_secrets.py` is gitignored.

### 5. Build and upload

```sh
pio run -e cc1101_button -t upload
```

## I2C address notes

| Sensor | Default I2C address |
|---|---|
| Si7021 | 0x40 |
| BMP280 | 0x76 |
| BME680 | 0x76 (SDO low) / 0x77 (SDO high) |

BMP280 and BME680 share 0x76 — pull BME680 SDO high and pass `0x77` to
`_sensor.begin(0x77)` in `src/sensors/bme680.h` if both are on the same bus.

## Project structure

```
src/
  main.cpp          — setup() / loop()
  packet.h          — Packet class: bitmap protocol, field encoding
  node.h            — UID (EEPROM-backed, hex CUSTOM_UID or random)
  transport.h       — radio send (LoRa or CC1101) + AES-128 encryption
  power.h           — deep sleep management
  sensors/
    si7021.h / ds18b20.h / bmp280.h / bme680.h
    button.h / switch.h / pir.h / radar.h
build_flags.py      — emits VERSION, VERSIONTAG, crypto flags
pio_secrets_example.py
platformio.ini      — one [env] block per physical node
```

## License

MIT
