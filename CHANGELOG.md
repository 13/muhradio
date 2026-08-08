# Changelog

Components are tagged independently: `receiver/vX.Y.Z` and `transmitter/vX.Y.Z`.
History before the versions below: see `git log`.

## receiver/v1.8.0 — 2026-08-08

- Node statistics now opt-in: new Settings toggle (default **off**) gates the
  health table, `GET /nodes` and the retained MQTT health topics
- Web handlers use static buffers — the async-context stack on ESP8266 is too
  small for 2 KB locals
- Status JSON (websocket/`/json`) drops fields/packets whole on overflow —
  output is always valid JSON

## receiver/v1.7.1 — 2026-08-08

- **Fix config wipe with WEB_PASS set:** if the settings form failed to load
  (e.g. cancelled auth prompt), Save posted blanks — the handler overwrote
  wifi_ssid/mqtt_server with empty strings and cleared the passwords,
  leaving the device unreachable (USB rescue only). Server now ignores
  empty ssid/server/numeric fields; the Save button stays disabled until
  the current settings actually loaded

## receiver/v1.7.0 — 2026-08-08

- LoRa receive: FIFO drained from the main loop instead of inside the ISR
  (SPI in interrupt context is crash-prone on ESP32)
- Node health publishing waits for NTP sync — no retained records with
  1970 timestamps after boot

## transmitter/v1.7.0 — 2026-08-08

- Build-time UID override: `NODE_UID=42 pio run -e <env> -t upload` wins
  over the env's `CUSTOM_UID` (validated 1-4095)

## receiver/v1.6.1 — 2026-08-08

- Release workflow: single publish job collects all board artifacts —
  parallel per-board uploads raced on the release and dropped assets
  (v1.6.0 shipped 21 of 24)

## receiver/v1.6.0 — 2026-08-08

- **Breaking:** node health topic moved from `{MQTT_TOPIC}/{uid}/health` to
  `{MQTT_TOPIC_LWT}/{hostname}/nodes/{uid}` and gained a `node` field —
  parallel receivers no longer overwrite each other's retained records
- Settings page: Export/Import config buttons (backed by `/api/config/*`,
  require `WEB_PASS`)
- HTTP API reference table in the receiver README
- CC1101 RXBYTES errata workaround (double-read until stable) in the Bresser
  FIFO reader
- Native tests run under ASan/UBSan

## receiver/v1.5.0 — 2026-08-08

- Dashboard "Nodes" card: per-node last-seen, packet count, RSSI and battery
  from `GET /nodes`, refreshed every 30 s, low-battery rows highlighted
- All platform and library versions pinned exactly for reproducible builds
- Release notes on GitHub Releases extracted from CHANGELOG.md
- MIT LICENSE file added (READMEs claimed MIT, file was missing)

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
