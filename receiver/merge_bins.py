"""
PlatformIO extra script — adds two custom targets:

  pio run -t otabundle   →  .pio/build/<env>/ota_bundle.bin
      Single file for web OTA (/update.html). Contains firmware + LittleFS
      with a 12-byte header. The receiver detects the magic and flashes both
      partitions in one upload, then you reboot once.

      Format:
        [magic: 4 B  'M','R','B','F']
        [fw_size: 4 B  uint32 LE]
        [fs_size: 4 B  uint32 LE]
        [firmware.bin data]
        [littlefs.bin data]

  pio run -t mergebin    →  .pio/build/<env>/merged.bin
      Full flash image for factory flashing via USB:
        esptool.py --chip auto write_flash 0x0 merged.bin
      Includes bootloader, partition table, boot_app0, firmware, and LittleFS.
      NOT suitable for web OTA.

Build both firmware and filesystem first:
    pio run
    pio run -t buildfs
    pio run -t otabundle    # web OTA
    pio run -t mergebin     # factory USB flash

LittleFS offset (mergebin only) assumes default.csv on 4 MB flash.
Adjust FS_OFFSET below if using a custom partition table.
"""
Import("env")
import os
import struct
import subprocess
import sys

FS_OFFSET = "0x290000"   # default.csv, 4 MB — adjust for custom partitions


# ── ota_bundle ─────────────────────────────────────────────────────────────────

def _otabundle(source, target, env):
    build = env.subst("$BUILD_DIR")
    fw    = os.path.join(build, "firmware.bin")
    fs    = os.path.join(build, "littlefs.bin")
    out   = os.path.join(build, "ota_bundle.bin")

    for path, label in [(fw, "firmware.bin"), (fs, "littlefs.bin")]:
        if not os.path.exists(path):
            print(f"[otabundle] {label} missing — build it first")
            return

    with open(fw, "rb") as f: fw_data = f.read()
    with open(fs, "rb") as f: fs_data = f.read()

    MAGIC = b"MRBF"
    header = MAGIC + struct.pack("<II", len(fw_data), len(fs_data))

    with open(out, "wb") as f:
        f.write(header)
        f.write(fw_data)
        f.write(fs_data)

    total = len(header) + len(fw_data) + len(fs_data)
    print(f"\n[otabundle] OK  {out}  ({total / 1024:.0f} KB)")
    print(f"[otabundle]     fw={len(fw_data) / 1024:.0f} KB  "
          f"fs={len(fs_data) / 1024:.0f} KB\n")


env.AddCustomTarget(
    name="otabundle",
    dependencies=["$BUILD_DIR/firmware.bin"],
    actions=_otabundle,
    title="Build OTA bundle (firmware + LittleFS, single web-upload file)",
    description="Produces ota_bundle.bin for /update.html. Build firmware and FS first.",
)


# ── merged.bin (factory / USB flash) ──────────────────────────────────────────

def _mergebin(source, target, env):
    build = env.subst("$BUILD_DIR")
    fw    = os.path.join(build, "firmware.bin")
    fs    = os.path.join(build, "littlefs.bin")
    out   = os.path.join(build, "merged.bin")

    for path, label in [(fw, "firmware.bin"), (fs, "littlefs.bin")]:
        if not os.path.exists(path):
            print(f"[mergebin] {label} missing — build it first")
            return

    images = [(str(off), str(path))
              for off, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET"), fw))
    images.append((FS_OFFSET, fs))
    images.sort(key=lambda x: int(x[0], 16))

    try:
        esptool = os.path.join(
            env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py"
        )
    except Exception:
        esptool = "esptool.py"

    cmd = [
        sys.executable, esptool,
        "--chip", env.get("BOARD_MCU", "esp32"),
        "merge_bin", "--flash_size", "4MB", "-o", out,
    ]
    for off, img in images:
        cmd += [off, img]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode == 0:
        kb = os.path.getsize(out) / 1024
        print(f"\n[mergebin] OK  {out}  ({kb:.0f} KB)")
        print(f"[mergebin]     esptool.py --chip auto write_flash 0x0 merged.bin\n")
    else:
        print(result.stderr or result.stdout)


env.AddCustomTarget(
    name="mergebin",
    dependencies=["$BUILD_DIR/firmware.bin"],
    actions=_mergebin,
    title="Merge firmware + LittleFS (factory USB flash image)",
    description="Produces merged.bin for esptool write_flash. NOT for web OTA.",
)
