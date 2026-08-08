#pragma once
#include <stdint.h>

// Single source of truth for the radio wire format, shared by the
// transmitter (transmitter/src/packet.h) and receiver (receiver/src/radio.cpp).
//
// Wire format: [uid:2][pid:1][bitmap:2][values in bit-ascending order, present only]
//
// Field bit positions — MUST be added in ascending order.
// Each bit maps to a (sensor, measurement) pair with a fixed known size,
// so the receiver can read values sequentially by checking each bit in order.
enum class Field : uint8_t {
  COUNTER = 0,   // uint16 — packet counter (debug)
  BUTTON  = 1,   // uint8  — 0=open, 1=pressed
  SWITCH  = 2,   // uint8  — 0/1
  PIR     = 3,   // uint8  — 0/1
  RADAR   = 4,   // uint8  — 0/1
  T_SI    = 5,   // int16  — Si7021  °C*10
  H_SI    = 6,   // int16  — Si7021  %*10
  T_DS    = 7,   // int16  — DS18B20 °C*10
  T_BMP   = 8,   // int16  — BMP280  °C*10
  P_BMP   = 9,   // uint32 — BMP280  Pa/10
  T_BME   = 10,  // int16  — BME680  °C*10
  H_BME   = 11,  // int16  — BME680  %*10
  P_BME   = 12,  // uint32 — BME680  Pa/10
  G_BME   = 13,  // uint16 — BME680  kOhm
  VCC     = 14,  // uint8  — V*10
};

namespace Fields {
  static constexpr uint8_t COUNT = 15;

  // Encoded size in bytes
  static constexpr uint8_t SIZES[COUNT]  = {2,1,1,1,1, 2,2,2,2,4, 2,2,4,2,1};
  // Divide the raw value by this to get engineering units
  static constexpr uint8_t SCALES[COUNT] = {1,1,1,1,1, 10,10,10,10,10, 10,10,10,1,10};
  static constexpr bool    IS_SIGNED[COUNT] = {
    false,false,false,false,false,
    true,true,true,true,false,
    true,true,false,false,false
  };
  static const char* const NAMES[COUNT] = {
    "COUNTER","BUTTON","SWITCH","PIR","RADAR",
    "T_SI","H_SI","T_DS","T_BMP","P_BMP",
    "T_BME","H_BME","P_BME","G_BME","VCC"
  };
  // Plausible raw-value ranges (pre-scaling units, e.g. °C*10, %*10, Pa/10).
  // The receiver drops values outside from the JSON — defends against old
  // firmware and decode edge cases.
  static constexpr int32_t MIN_RAW[COUNT] = {
    0,0,0,0,0,
    -400,0,-550,-400,3000,
    -400,0,3000,0,0
  };
  static constexpr int32_t MAX_RAW[COUNT] = {
    65535,1,1,1,1,
    1250,1000,1250,850,11000,
    850,1000,11000,65535,255
  };
}
