# Copy this file to pio_secrets.py (gitignored) and fill in your values.
#
#   cp pio_secrets_example.py pio_secrets.py
#   openssl rand -hex 16       # generate AES key (must match transmitter)
#
# build_flags.py reads pio_secrets.py and emits -D flags that override
# the empty defaults in config.h.  If pio_secrets.py does not exist,
# crypto is disabled and WiFi/MQTT passwords fall back to config.h.

WIFI_PASS = "your_wifi_password"
MQTT_PASS = "your_mqtt_password"
AES_KEY   = "your_32_hex_char_key_here"
