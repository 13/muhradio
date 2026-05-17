# How to receive packets

A receiver can be any platform with an SX1276/RFM95W module: Raspberry Pi, ESP32, another Arduino, a LoRaWAN gateway in raw-LoRa mode, etc.

## LoRa radio settings

The transmitter uses these settings — the receiver must match all of them exactly:

| Setting | Value |
|---|---|
| Frequency | 868 MHz EU / 915 MHz US (match `LO_FREQ`) |
| Spreading factor | SF10 |
| Sync word | `0x13` |
| CRC | enabled |
| Bandwidth | 125 kHz (library default) |
| Coding rate | 4/5 (library default) |

## Wire format

Every LoRa packet on the air looks like this:

```
[dst:1][src:1][payload:N]
```

| Bytes | Value | Description |
|---|---|---|
| 0 | `0x15` | destination address (`ADDR_RECEIVER`) |
| 1 | `0x14` | source address (`ADDR_SENDER`) |
| 2..N | see below | payload — plain or AES-encrypted |

Discard packets where `dst != 0x15`.

### Payload (plain)

```
[uid:2][pid:1][bitmap:2][field values in bit-ascending order]
```

| Offset | Size | Description |
|---|---|---|
| 0–1 | 2 | Node UID, little-endian |
| 2 | 1 | Packet ID (random 1–99, for deduplication) |
| 3–4 | 2 | Bitmap, little-endian — bit N set means field N is present |
| 5.. | varies | Field values, present fields only, in ascending bit order |

### Payload (AES-128)

If the transmitter was built with `USE_CRYPTO`, bytes 2..N are AES-128 ECB ciphertext with PKCS#7 padding. Decrypt the block(s) first, strip the padding byte, then parse as plain above.

```
padLen = last byte of decrypted data
plainLen = cipherLen - padLen
```

## Bitmap field table

Read field values sequentially by iterating bits 0 → 14. For each set bit, consume the corresponding number of bytes from the value stream.

| Bit | Field | Type | Bytes | Scale | Description |
|---|---|---|---|---|---|
| 0 | COUNTER | uint16 LE | 2 | — | Debug packet counter |
| 1 | BUTTON | uint8 | 1 | 0/1 | Button state (1=pressed) |
| 2 | SWITCH | uint8 | 1 | 0/1 | Switch state |
| 3 | PIR | uint8 | 1 | 0/1 | PIR motion |
| 4 | RADAR | uint8 | 1 | 0/1 | Radar motion |
| 5 | T_SI | int16 LE | 2 | °C×10 | Si7021 temperature |
| 6 | H_SI | int16 LE | 2 | %×10 | Si7021 humidity |
| 7 | T_DS | int16 LE | 2 | °C×10 | DS18B20 temperature |
| 8 | T_BMP | int16 LE | 2 | °C×10 | BMP280 temperature |
| 9 | P_BMP | uint32 LE | 4 | Pa/10 | BMP280 pressure |
| 10 | T_BME | int16 LE | 2 | °C×10 | BME680 temperature |
| 11 | H_BME | int16 LE | 2 | %×10 | BME680 humidity |
| 12 | P_BME | uint32 LE | 4 | Pa/10 | BME680 pressure |
| 13 | G_BME | uint16 LE | 2 | kOhm | BME680 gas resistance |
| 14 | VCC | uint8 | 1 | V×10 | Supply voltage |

All multi-byte integers are little-endian (AVR native byte order).

### Example

Si7021 packet — 10 bytes on the air:

```
Byte:  00 01 02 03 04 05 06 07 08 09
       [uid  ] pid [bitmap] [T_SI] [H_SI] vcc
       02 00  2A  60 00    01 01  D3 02  32
```

- uid = `0x0002` (node_si7021)
- pid = `0x2A` (42)
- bitmap = `0x0060` → bits 5 and 6 set → T_SI + H_SI present
- T_SI = `0x0101` = 257 → **25.7 °C**
- H_SI = `0x02D3` = 723 → **72.3 %**
- VCC = `0x32` = 50 → **5.0 V**

Wait — VCC (bit 14) is always added by the transmitter, so bitmap `0x0060` would be `0x4060`:

```
bitmap = 0x4060  →  bits 5, 6, 14 set  →  T_SI + H_SI + VCC
```

## Python receiver (Raspberry Pi + RFM95W)

Uses the [adafruit-circuitpython-rfm9x](https://github.com/adafruit/Adafruit_CircuitPython_RFM9x) library.

```python
import struct
import board
import busio
import digitalio
import adafruit_rfm9x

ADDR_RECEIVER = 0x15
FREQ_MHZ      = 868.0

# SPI wiring: MOSI=GPIO10, MISO=GPIO9, SCK=GPIO11
spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)
cs  = digitalio.DigitalInOut(board.CE1)
rst = digitalio.DigitalInOut(board.D25)

rfm = adafruit_rfm9x.RFM9x(spi, cs, rst, FREQ_MHZ)
rfm.spreading_factor   = 10
rfm.sync_word          = 0x13
rfm.enable_crc         = True

FIELD_SIZES = {
    0: (2, 'H'),   # COUNTER  uint16
    1: (1, 'B'),   # BUTTON   uint8
    2: (1, 'B'),   # SWITCH   uint8
    3: (1, 'B'),   # PIR      uint8
    4: (1, 'B'),   # RADAR    uint8
    5: (2, 'h'),   # T_SI     int16
    6: (2, 'h'),   # H_SI     int16
    7: (2, 'h'),   # T_DS     int16
    8: (2, 'h'),   # T_BMP    int16
    9: (4, 'I'),   # P_BMP    uint32
   10: (2, 'h'),   # T_BME    int16
   11: (2, 'h'),   # H_BME    int16
   12: (4, 'I'),   # P_BME    uint32
   13: (2, 'H'),   # G_BME    uint16
   14: (1, 'B'),   # VCC      uint8
}

FIELD_NAMES  = ['COUNTER','BUTTON','SWITCH','PIR','RADAR',
                'T_SI','H_SI','T_DS','T_BMP','P_BMP',
                'T_BME','H_BME','P_BME','G_BME','VCC']
FIELD_SCALES = [1, 1, 1, 1, 1, 10, 10, 10, 10, 10, 10, 10, 10, 1, 10]

def parse(raw: bytes) -> dict | None:
    if len(raw) < 7:
        return None
    dst, src = raw[0], raw[1]
    if dst != ADDR_RECEIVER:
        return None
    payload = raw[2:]
    uid    = struct.unpack_from('<H', payload, 0)[0]
    pid    = payload[2]
    bitmap = struct.unpack_from('<H', payload, 3)[0]
    pos    = 5
    fields = {}
    for bit in range(15):
        if not (bitmap >> bit & 1):
            continue
        size, fmt = FIELD_SIZES[bit]
        val = struct.unpack_from('<' + fmt, payload, pos)[0]
        pos += size
        scale = FIELD_SCALES[bit]
        fields[FIELD_NAMES[bit]] = val / scale if scale > 1 else val
    return {'uid': uid, 'pid': pid, **fields}

while True:
    pkt = rfm.receive(timeout=30.0)
    if pkt is None:
        continue
    data = parse(bytes(pkt))
    if data:
        print(data)
        # {'uid': 2, 'pid': 42, 'T_SI': 25.7, 'H_SI': 72.3, 'VCC': 5.0}
```

### With AES-128 decryption

```python
from Crypto.Cipher import AES  # pip install pycryptodome

AES_KEY = bytes.fromhex("your_32_hex_char_key_here")

def decrypt(ciphertext: bytes) -> bytes:
    cipher  = AES.new(AES_KEY, AES.MODE_ECB)
    plain   = cipher.decrypt(ciphertext)
    pad_len = plain[-1]
    return plain[:-pad_len]

def parse_encrypted(raw: bytes) -> dict | None:
    if len(raw) < 2:
        return None
    dst, src = raw[0], raw[1]
    if dst != ADDR_RECEIVER:
        return None
    payload = decrypt(raw[2:])
    # then parse payload as plain above (uid, pid, bitmap, values)
    ...
```

## Arduino receiver

```cpp
#include <SPI.h>
#include <LoRa.h>

constexpr byte ADDR_RECEIVER = 0x15;

void setup() {
  Serial.begin(9600);
  LoRa.begin(868E6);
  LoRa.setSpreadingFactor(10);
  LoRa.setSyncWord(0x13);
  LoRa.enableCrc();
}

void loop() {
  int pktSize = LoRa.parsePacket();
  if (!pktSize) return;

  uint8_t buf[64];
  uint8_t len = 0;
  while (LoRa.available() && len < sizeof(buf))
    buf[len++] = LoRa.read();

  if (len < 7 || buf[0] != ADDR_RECEIVER) return;

  uint8_t* p     = buf + 2;            // skip dst+src
  uint16_t uid   = p[0] | (p[1] << 8);
  uint8_t  pid   = p[2];
  uint16_t bmap  = p[3] | (p[4] << 8);
  uint8_t* vals  = p + 5;

  Serial.print("uid=0x"); Serial.print(uid, HEX);
  Serial.print(" pid=");   Serial.print(pid);
  Serial.print(" rssi=");  Serial.println(LoRa.packetRssi());

  // Read fields in bit order
  uint8_t sizes[] = {2,1,1,1,1, 2,2,2,2,4, 2,2,4,2,1};
  const char* names[] = {"CTR","BTN","SW","PIR","RADAR",
                          "T_SI","H_SI","T_DS","T_BMP","P_BMP",
                          "T_BME","H_BME","P_BME","G_BME","VCC"};
  uint8_t pos = 0;
  for (uint8_t bit = 0; bit < 15; bit++) {
    if (!(bmap >> bit & 1)) continue;
    int32_t v = 0;
    for (uint8_t b = 0; b < sizes[bit]; b++)
      v |= (int32_t)vals[pos + b] << (8 * b);
    pos += sizes[bit];
    Serial.print(names[bit]); Serial.print('='); Serial.print(v); Serial.print(' ');
  }
  Serial.println();
}
```

## RSSI and SNR

After `LoRa.parsePacket()` you can read signal quality:

```cpp
int rssi = LoRa.packetRssi();   // e.g. -87 dBm
float snr = LoRa.packetSnr();   // e.g. 7.5 dB
```

SF10 at 868 MHz with 17 dBm TX gives roughly 5–10 km line-of-sight range.
