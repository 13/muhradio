#pragma once
#include <Arduino.h>
#include "status.h"

namespace Net {
  extern char hostname[24]; // "esp32-XXXX"
  extern char nodeId[5];    // last 4 hex chars of MAC

  // Call once from setup(). Connects WiFi, MQTT, NTP, mDNS; populates s.
  void begin(Status& s);

  // Call every loop(). Handles MQTT keepalive and the 1-minute mark.
  // Returns true when clients should receive a fresh status push.
  bool loop(Status& s);

  // Publish a payload to topic. Attempts reconnect if disconnected.
  bool publish(const char* topic, const char* payload);

  // Current NTP epoch time.
  time_t now();
}
