#--------------------------------------------------------------------------
#
# File:        merge_image.py
#
# NightDriverStrip - (c) 2023 Plummer's Software LLC.  All Rights Reserved.
#
# This file is part of the NightDriver software project.
#
#    NightDriver is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    NightDriver is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Nightdriver.  It is normally found in copying.txt
#    If not, see <https://www.gnu.org/licenses/>.
#
# Description:
#
#    PlatformIO post-build script to create a merged firmware file, and to check
#    that the application image actually fits in the partition it goes into.
#
# History:     Aug-08-2023         Rbergen      Added header
#              Oct-13-2026         Robert Lipe  Fail the build when the app image
#                                                overflows its app partition
#
#---------------------------------------------------------------------------

import os
import struct
import sys

Import("env")

# A partition table binary is an array of these fixed 32-byte records; see
# esp_partition_info_t in the IDF's components/partition_table.
PARTITION_MAGIC    = 0x50AA
PARTITION_ENTRY    = 32
PARTITION_TYPE_APP = 0x00
SUBTYPE_FACTORY    = 0x00
SUBTYPE_OTA_0      = 0x10

def app_partition_from_table(partitions_bin):
    """
    Return the (label, size) of the app partition that a normal upload writes to,
    read out of the generated partitions.bin, or None if there isn't one.
    """
    with open(partitions_bin, "rb") as f:
        table = f.read()

    apps = []
    for offset in range(0, len(table) - PARTITION_ENTRY + 1, PARTITION_ENTRY):
        entry = table[offset:offset + PARTITION_ENTRY]
        if struct.unpack_from("<H", entry, 0)[0] != PARTITION_MAGIC:
            break
        if entry[2] != PARTITION_TYPE_APP:
            continue
        label = entry[12:28].split(b"\0")[0].decode("ascii", "replace")
        apps.append((entry[3], label, struct.unpack_from("<I", entry, 8)[0]))

    if not apps:
        return None

    # Prefer ota_0, which is where both an OTA update and a normal upload land, then
    # factory, then whatever app partition the table happens to have.
    for preferred in (SUBTYPE_OTA_0, SUBTYPE_FACTORY):
        for subtype, label, size in apps:
            if subtype == preferred:
                return label, size
    return apps[0][1], apps[0][2]

def check_app_image_fits(source, target, env):
    # PlatformIO's own "Flash:" percentage excludes the .eh_frame section even though
    # .eh_frame is allocated and really is written into firmware.bin, so a board can
    # overflow its app partition while the summary still looks comfortable.  kitch did
    # exactly that: PlatformIO reported 95.4% while the image was really 102.9% of
    # app0, too big for an OTA update to ever fit.  Compare what the linker produced
    # against the partition it is about to be flashed into.
    image = target[0].get_abspath()
    try:
        image_size = os.path.getsize(image)
        app = app_partition_from_table(os.path.join(env.subst("$BUILD_DIR"), "partitions.bin"))
    except OSError as e:
        print(f"    [app size] cannot verify the app image size ({e}); skipping")
        return

    if app is None:
        print("    [app size] no app partition in partitions.bin; skipping")
        return

    label, capacity = app
    percent = 100.0 * image_size / capacity
    print(f"    [app size] {os.path.basename(image)} is {image_size:,} of {capacity:,} bytes "
          f"({percent:.1f}%) in '{label}'")

    if image_size > capacity:
        print(f"    [app size] ERROR: the app image is {image_size - capacity:,} bytes too big for "
              f"'{label}' ({image_size:,} > {capacity:,}, {percent:.1f}%).", file=sys.stderr)
        print("    [app size] An OTA update could never be written, and flashing this image would "
              "run into the next partition.", file=sys.stderr)
        print("    [app size] Enlarge the app partition or shrink the image.", file=sys.stderr)
        sys.exit(1)

def merge_bin(source, target, env):
    # The list contains all extra images (bootloader, partitions, eboot) and the final application binary
    flash_images = env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])) + ["${ESP32_APP_OFFSET}", target[0].get_abspath()]
    board_config = env.BoardConfig()
    merged_image = os.path.join("${BUILD_DIR}", "merged_image.bin")

    # Figure out flash frequency and mode
    flash_freq = board_config.get("build.f_flash", "40000000L")
    flash_freq = str(flash_freq).replace("L", "")
    flash_freq = str(int(int(flash_freq) / 1000000)) + "m"
    flash_mode = board_config.get("build.flash_mode", "dio")
    memory_type = board_config.get("build.arduino.memory_type", "qio_qspi")

    if flash_mode == "qio" or flash_mode == "qout":
        flash_mode = "dio"
    if memory_type == "opi_opi" or memory_type == "opi_qspi":
        flash_mode = "dout"

    # Run esptool to merge images into a single binary
    esptool_path = "esptool.py"
    try:
        platform = env.PioPlatform()
        package_dir = platform.get_package_dir("tool-esptoolpy")
        if package_dir:
            candidate = os.path.join(package_dir, "esptool.py")
            if os.path.exists(candidate):
                esptool_path = candidate
            else:
                candidate = os.path.join(package_dir, "esptool")
                if os.path.exists(candidate):
                    esptool_path = candidate
    except Exception:
        pass

    env.Execute(
        " ".join(
            [
                f'"{env.subst("${PYTHONEXE}")}"',
                f'"{esptool_path}"',
                "--chip",
                board_config.get("build.mcu", "esp32"),
                "merge_bin",
                "--flash_size",
                board_config.get("upload.flash_size", "4MB"),
                "--flash_mode",
                flash_mode,
                "--flash_freq",
                flash_freq,
                "-o",
                f'"{merged_image}"',
            ]
            + flash_images
        )
    )

# Refuse to produce an image that no OTA update could fit before we bother merging it
env.AddPostAction("${BUILD_DIR}/${PROGNAME}.bin", check_app_image_fits)

# Add a post action that runs esptoolpy to merge available flash images
env.AddPostAction("${BUILD_DIR}/${PROGNAME}.bin", merge_bin)
