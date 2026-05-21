#pragma once
// Non-sensitive device configuration — safe to commit.
// Secrets (WIFI_PASS, MQTT_PASS, AES_KEY) come from pio_secrets.py → build_flags.py.
//   cp pio_secrets_example.py pio_secrets.py   # then fill in your values

// ── Debug ─────────────────────────────────────────────────────────────────────
#define VERBOSE            // one-minute mark + connect/publish messages
// #define DEBUG           // packet-level hex + field logging

// ── Network behaviour ─────────────────────────────────────────────────────────
#define REQUIRES_INTERNET  // reboot when WiFi is lost; remove for offline use

// ── Device ────────────────────────────────────────────────────────────────────
#define DEVICE_DESCRIPTION "Receiver"

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_SSID "muhxnetwork"
#ifndef WIFI_PASS
#  define WIFI_PASS ""
#endif

// ── Time ──────────────────────────────────────────────────────────────────────
// Europe/Rome: CET = UTC+1.  DST mode 2 = auto EU (last Sun Mar / last Sun Oct).
// Override at runtime via the Settings page; these are first-boot defaults only.
#define TZ_OFFSET   60  // UTC offset in minutes: 60 = UTC+1 (CET)
#define TZ_DST_MODE  2  // 0=off, 1=always on, 2=auto EU rules

// ── NTP ───────────────────────────────────────────────────────────────────────
#define NTP1 "192.168.22.5"
#define NTP2 "2.europe.pool.ntp.org"
#define NTP3 "time.cloudflare.com"

// ── MQTT ──────────────────────────────────────────────────────────────────────
// Packet topic:  MQTT_TOPIC/<node_uid>/json
// LWT topic:     MQTT_TOPIC_LWT/<hostname>/LWT
#define MQTT_SERVER    "192.168.22.5"
#define MQTT_PORT      1883
#define MQTT_USER      ""
#ifndef MQTT_PASS
#  define MQTT_PASS    ""
#endif
#define MQTT_TOPIC     "muh/sensors"
#define MQTT_TOPIC_LWT "muh/esp"
