# Changelog

Components are tagged independently: `receiver/vX.Y.Z` and `transmitter/vX.Y.Z`.
History before the versions below: see `git log`.

## receiver/v1.4.1 — 2026-08-08

- ESP8266 (d1_mini) OTA bundles: merge_bins.py re-enabled for the d1_mini
  envs, so `pio run -t otabundle` and release artifacts work there too
  (the v1.4.0 release lacked d1_mini binaries)

## receiver/v1.4.0 — 2026-08-08

- Watchdog: task WDT (ESP32) / software WDT (ESP8266), 60 s, recovers a hung
  network stack or driver without a power cycle
- Heap-exhaustion guard: reboot after 3 consecutive minute marks below
  `HEAP_MIN_FREE` (default 6 KB)
- Config backup: `GET /api/config/export` and `POST /api/config/import`
  (both require `WEB_PASS`)
- `/config.json` now carries a `cfg_ver` schema version
- CI: `receiver/v*` tags build all boards and attach firmware, filesystem and
  OTA-bundle binaries to a GitHub Release

## receiver/v1.3.0 — 2026-08-08

- HTTP basic auth (`WEB_USER`/`WEB_PASS`) on `/update`, `/reboot`, `/api/*`;
  espota and the ESP8266 :8080 updater get the same password
- Settings API masks WiFi/MQTT passwords (`***`); plaintext never leaves the device
- Web OTA aborts on flash errors and validates bundle header sizes
- Per-node health: `GET /nodes` + retained `{MQTT_TOPIC}/{uid}/health`
  (last-seen, packet count, RSSI, VCC, low-battery flag)
- MQTT publish backoff when the broker is down (no per-packet blocking connect);
  retained LWT/IP/VERSION no longer republished every minute; NTP re-sync hourly
- Decode hardening: shared field tables, payload truncation and plausibility
  range checks, single JSON escaper everywhere, JsonBuilder overflow safety
- Bresser: decoder extracted and unit-tested, configurable topic prefix
  (`MQTT_TOPIC_BRESSER`), CC1101 init retry + reboot
- Config escape decoding fixed (`\n`, `\u00XX` round-trip)
- Host-side unit tests (`pio test -e native_test`) and CI

## transmitter/v1.6.0 — 2026-08-08

- Sensor reads hardened: a failed BME680 read no longer aborts the packet;
  marginal RH clamped, implausible values skipped; DS18B20 85.0 °C
  power-on value rejected
- Deep sleep covers the remainder (`DS_S=10` now sleeps ~10 s, was 8 s)
- Radio init checked: 3 attempts, then sleep forever instead of draining the
  battery with a dead radio
- Wire-format tables moved to `shared/fields.h` (shared with the receiver),
  guarded by a native round-trip test
