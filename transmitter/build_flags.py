#!/usr/bin/env python3
"""
Generates PlatformIO build flags:
  - VERSION / VERSIONTAG from git
  - USE_CRYPTO / AES_KEY from pio_secrets.py (gitignored)

AES key setup:
  cp pio_secrets_example.py pio_secrets.py
  # edit pio_secrets.py with: openssl rand -hex 16
"""
import re
import subprocess
import datetime
import sys
import os

def _check_aes_key(key):
    """AES-128 key: exactly 32 hex chars. Anything else would be parsed into a
    silently wrong key and every packet would fail to decrypt."""
    if not re.fullmatch(r'[0-9a-fA-F]{32}', key):
        print("ERROR: AES_KEY must be exactly 32 hex characters (openssl rand -hex 16)",
              file=sys.stderr)
        sys.exit(1)

def _git(*args):
    try:
        return subprocess.check_output(
            ['git'] + list(args),
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return None

build_time = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
ver = _git('describe', '--abbrev=7', '--always', '--tags', '--match', 'transmitter/v*') or 'unknown'
tag = _git('describe', '--abbrev=0', '--tags', '--match', 'transmitter/v*')          or 'unreleased'
if '/' in ver: ver = ver.split('/', 1)[1]
if '/' in tag: tag = tag.split('/', 1)[1]

print(f"'-DVERSION=\"{ver} ({build_time})\"'")
print(f"'-DVERSIONTAG=\"{tag}\"'")

# One-off UID override without editing platformio.ini:
#   NODE_UID=42 pio run -e cc1101_si7021 -t upload
# Wins over the env's CUSTOM_UID (see node.h).
uid = os.environ.get('NODE_UID')
if uid:
    if uid.isdigit() and 1 <= int(uid) <= 4095:
        print(f"'-DNODE_UID_OVERRIDE={int(uid)}'")
    else:
        print(f"ERROR: NODE_UID must be a decimal 1-4095, got '{uid}'", file=sys.stderr)
        sys.exit(1)

# AES key — read from pio_secrets.py (gitignored, never committed)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import pio_secrets
    key = getattr(pio_secrets, 'AES_KEY', None)
    if key:
        _check_aes_key(key)
        print("'-DUSE_CRYPTO'")
        print(f"'-DAES_KEY=\"{key}\"'")
    else:
        print("WARNING: pio_secrets.py found but AES_KEY not set", file=sys.stderr)
except ImportError:
    pass  # no pio_secrets.py — crypto disabled, plain packets
