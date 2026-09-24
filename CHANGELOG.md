# Changelog

Components are tagged independently: `receiver/vX.Y.Z` and `transmitter/vX.Y.Z`.
History before the versions below: see `git log`.

## transmitter/v1.11.0 — 2026-09-24

- BME680: the gas heater is off unless `-DBME680_GAS` is set. The driver arms
  it at 320 °C for 150 ms on every reading (~2 mAs), more than everything
  else on the node combined; T/H/P are unaffected. The four existing BME680
  envs carry the flag so their packets don't change until you drop it.
- BME680: the read is asynchronous; the node powers down (~5 uA) while the
  sensor measures instead of busy-waiting at ~4 mA. The driver's
  `performReading()` waited twice the measurement period.
- `Power::sleepMs()`: watchdog power-down for a millisecond wait, planned by
  the pure `sleep_plan.h` (native test `test_sleep_plan`); it advances
  `millis()` by the time slept so millis-based library waits see it. The
  DS18B20 conversion wait uses it too (same 870 ms as before).

## receiver/v1.9.0 — 2026-09-23

- CC1101: received frames longer than the 80-byte buffer are dropped; the
  library's `ReceiveData()` trusted the FIFO length byte and could overrun it.
- OTA: an upload that ends early (mid-firmware, or a bundle whose filesystem
  is short) now reports failure instead of `"success":true`.
- `/api/settings` GET builds its JSON with `JsonBuilder`, so worst-case
  escaped values can't overflow the buffer.
- No WiFi at boot (without `REQUIRES_INTERNET`): mDNS, espota, MQTT and NTP now
  start once WiFi connects instead of never being initialised.
- NTP retries every minute until the first sync (was hourly), and `boottime` is
  set only from a real sync.
- WebSocket: client messages only request a refresh, which the main loop sends
  at most every 250 ms — no more cross-task access to the status buffer, and a
  message flood can't trigger a broadcast storm.
- Settings POST ignores out-of-range `mqtt_port`, `tz_offset`, `dst_mode`
  instead of wrapping them; config load applies the same ranges.
- Config import validates the file (SSID and broker set, numbers in range)
  before it replaces the live config, and doesn't buffer the body for
  unauthenticated requests.
- **`/reboot` is POST-only** (a GET could be triggered by any `<img>` on
  another page). Authenticated endpoints and the OTA upload refuse
  cross-origin requests (`Origin` must match `Host`). New `GET /api/auth`; the
  settings and update pages warn when no `WEB_PASS` is set, and so does the
  serial log at boot.
- Status `desc` is 64 bytes like the config field, so long descriptions aren't
  cut in the UI.
- Web UI: a malformed WebSocket frame or packet string no longer breaks the
  page; the nodes card hides instead of showing a stale table when stats are
  off.
- Build fails on an `AES_KEY` that isn't 32 hex characters.
- Internal: CC1101 bring-up, re-arm and overflow watchdog shared by the
  muhradio and Bresser paths (`cc1101util.h`); one `ISR_ATTR`; one heap
  fragmentation helper; WS client count from `_ws.count()`.
- `shared/fields.h`: compile-time checks that every field has a size, scale,
  name and a valid range, and that `COUNT` matches the `Field` enum.

## transmitter/v1.10.0 — 2026-09-23

- **Button, PIR and radar nodes wake on a pin-change interrupt**, the same fix
  v1.9.0 made for switches: `attachInterrupt()` edges on INT0/INT1 can't wake
  the ATmega328P from power-down. Only the active edge transmits (press,
  motion/presence start → 1); release and motion end go straight back to
  sleep. Debounce via `-DBUTTON_DEBOUNCE_MS` (default 20). Any digital pin now
  works. PIR nodes no longer block in setup and send a VCC announce at boot,
  like button and radar nodes.
- **`cc1101_ds18b20_si7021` uses UID 14** as the registry says; it collided
  with `cc1101_si7021_bmp280` on 23. A reflashed node publishes to `…/14/json`.
- A node whose radio failed to initialise now puts it to sleep before sleeping
  forever, instead of leaving it in idle and draining the cell.
- Opt-in watchdog `-DUSE_WDT` (8 s while awake). Optiboot only.
- `pio_secrets_example.py` no longer ships a key (`AES_KEY = ""`); the build
  fails on an `AES_KEY` that isn't 32 hex characters.
- CC1101 envs extend a new `[cc1101_base]` (library + radio flags; resolved
  flags identical for all envs). `cc1101_si7021` no longer builds with
  `-DVERBOSE` (Serial on costs battery). Shared `addHumidity()` for
  Si7021/BME680. `shared/fields.h` gains compile-time table checks.
- Docs: README examples use the real config (decimal `CUSTOM_UID`,
  `DS_S`/`DS_M`, `cc1101_base`); `HOWTORECEIVE.md` pid range is 1–255; the
  README documents that the AES framing has no authentication or replay
  protection.

## transmitter/v1.9.0 — 2026-09-23

- Switch nodes (reed or rocker) now wake reliably on **both** edges: the wake
  source moved from `attachInterrupt(..., CHANGE)` to a pin-change interrupt.
  In power-down the ATmega328P stops the I/O clock, so INT0/INT1 wake on LOW
  level only and edge-triggered wakes never fired — the node slept through
  flips. PCINT is detected asynchronously, and as a side effect the switch is
  no longer restricted to D2/D3.
- Switch nodes: contact debounce (`-DSWITCH_DEBOUNCE_MS`, default 30 ms; a reed
  can use 5) and invertible encoding (`-DSWITCH_INVERT`) for normally-closed
  contacts. A wake that settles back on the position already reported now goes
  straight back to sleep instead of transmitting, so a rattling door or a
  chattering reed no longer drains the cell.
- CI builds `cc1101_switch`, so interrupt-sensor code is covered.

## transmitter/v1.8.0 — 2026-09-17

- Button nodes: optional press-feedback LED (`-DLED_PIN=N`, blink length
  `-DLED_MS`, default 50 ms); enabled on `cc1101_button_test` (D4)

## receiver/v1.8.1 — 2026-08-08

- Config save: buffer sized for worst-case escaped fields (1280 B) and save
  refuses to write a truncated config.json

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
