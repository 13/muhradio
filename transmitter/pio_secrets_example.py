# Copy this file to pio_secrets.py (gitignored) and fill in your key.
#
#   cp pio_secrets_example.py pio_secrets.py
#   openssl rand -hex 16       # generate a key
#
# build_flags.py reads pio_secrets.py and emits -DUSE_CRYPTO -DAES_KEY=...
# If pio_secrets.py does not exist, crypto is simply disabled.

AES_KEY = "7f2cb15cb58eb5d1dc2752615eaea08e"
