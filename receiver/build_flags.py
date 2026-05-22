#!/usr/bin/env python3
"""
Generates PlatformIO build flags:
  - VERSION / VERSIONTAG from git
  - WIFI_PASS / MQTT_PASS / USE_CRYPTO / AES_KEY from pio_secrets.py (gitignored)

Secrets setup:
  cp pio_secrets_example.py pio_secrets.py
  # edit pio_secrets.py with your passwords and key
"""
import subprocess
import datetime
import sys
import os

def _git(*args):
    try:
        return subprocess.check_output(
            ['git'] + list(args),
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return None

build_time = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
ver = _git('describe', '--abbrev=7', '--always', '--tags', '--match', 'receiver/v*') or 'unknown'
tag = _git('describe', '--abbrev=0', '--tags', '--match', 'receiver/v*')          or 'unreleased'
if '/' in ver: ver = ver.split('/', 1)[1]
if '/' in tag: tag = tag.split('/', 1)[1]

print(f"'-DVERSION=\"{ver} ({build_time})\"'")
print(f"'-DVERSIONTAG=\"{tag}\"'")

# Secrets — read from pio_secrets.py (gitignored, never committed)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import pio_secrets
    def _s(name):  return getattr(pio_secrets, name, None) or None
    def _i(name):
        v = getattr(pio_secrets, name, None)
        return v if isinstance(v, int) else None

    if _s('WIFI_SSID'):         print(f"'-DWIFI_SSID=\"{_s('WIFI_SSID')}\"'")
    if _s('WIFI_PASS'):         print(f"'-DWIFI_PASS=\"{_s('WIFI_PASS')}\"'")
    if _s('MQTT_SERVER'):       print(f"'-DMQTT_SERVER=\"{_s('MQTT_SERVER')}\"'")
    if _i('MQTT_PORT'):         print(f"'-DMQTT_PORT={_i('MQTT_PORT')}'")
    if _s('MQTT_USER') is not None and _s('MQTT_USER') != '':
                                print(f"'-DMQTT_USER=\"{_s('MQTT_USER')}\"'")
    if _s('MQTT_PASS'):         print(f"'-DMQTT_PASS=\"{_s('MQTT_PASS')}\"'")
    if _s('MQTT_TOPIC'):        print(f"'-DMQTT_TOPIC=\"{_s('MQTT_TOPIC')}\"'")
    if _s('MQTT_TOPIC_LWT'):    print(f"'-DMQTT_TOPIC_LWT=\"{_s('MQTT_TOPIC_LWT')}\"'")
    if _s('NTP1'):              print(f"'-DNTP1=\"{_s('NTP1')}\"'")
    if _s('NTP2'):              print(f"'-DNTP2=\"{_s('NTP2')}\"'")
    if _s('NTP3'):              print(f"'-DNTP3=\"{_s('NTP3')}\"'")
    if _i('TZ_OFFSET') is not None: print(f"'-DTZ_OFFSET={_i('TZ_OFFSET')}'")
    if _i('TZ_DST_MODE') is not None: print(f"'-DTZ_DST_MODE={_i('TZ_DST_MODE')}'")
    key = _s('AES_KEY')
    if key:
        print("'-DUSE_CRYPTO'")
        print(f"'-DAES_KEY=\"{key}\"'")
except ImportError:
    pass  # no pio_secrets.py — secrets fall back to config.h defaults
