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
    wp  = getattr(pio_secrets, 'WIFI_PASS', None)
    mp  = getattr(pio_secrets, 'MQTT_PASS', None)
    key = getattr(pio_secrets, 'AES_KEY',   None)
    if wp:  print(f"'-DWIFI_PASS=\"{wp}\"'")
    if mp:  print(f"'-DMQTT_PASS=\"{mp}\"'")
    if key:
        print("'-DUSE_CRYPTO'")
        print(f"'-DAES_KEY=\"{key}\"'")
except ImportError:
    pass  # no pio_secrets.py — secrets fall back to config.h defaults
