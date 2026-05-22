# Copy this file to pio_secrets.py (gitignored) and fill in your values.
#
#   cp pio_secrets_example.py pio_secrets.py
#   openssl rand -hex 16       # generate AES key (must match transmitter)
#
# build_flags.py reads pio_secrets.py and emits -D flags that override
# the defaults in config.h.  Omit or set to None/empty to keep the
# config.h default.  If pio_secrets.py does not exist entirely,
# crypto is disabled and everything falls back to config.h.

# ── WiFi ──────────────────────────────────────────────────────────────────────
WIFI_SSID = "muhxnetwork"
WIFI_PASS = ""

# ── MQTT ──────────────────────────────────────────────────────────────────────
MQTT_SERVER    = "192.168.22.5"
MQTT_PORT      = 1883            # int
MQTT_USER      = ""
MQTT_PASS      = ""
MQTT_TOPIC     = "muh/sensors"
MQTT_TOPIC_LWT = "muh/esp"

# ── NTP ───────────────────────────────────────────────────────────────────────
NTP1 = "192.168.22.5"
NTP2 = "pool.ntp.org"
NTP3 = "time.cloudflare.com"


# ── Timezone ──────────────────────────────────────────────────────────────────
TZ_OFFSET   = 60    # UTC offset in minutes (e.g. 60 = UTC+1)
TZ_DST_MODE = 2    # 0=off, 1=always on, 2=auto EU rules

# ── Crypto ────────────────────────────────────────────────────────────────────
# Generate with:  openssl rand -hex 16   (must match transmitter)
AES_KEY   = ""