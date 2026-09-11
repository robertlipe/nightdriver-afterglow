Import("env")
import re

# PlatformIO's framework-espressif32 _embed_files.py script constructs
# objcopy command names using `xtensa-${mcu}-elf-objcopy` (e.g. xtensa-esp32-elf-objcopy
# or xtensa-esp32s3-elf-objcopy). On Linux/macOS, symlinks exist for these, but on
# Windows, only `xtensa-esp-elf-objcopy.exe` is present in the toolchain package.
# This script patches TxtToBin's action to use `xtensa-esp-elf-objcopy`.

if "TxtToBin" in env.get("BUILDERS", {}):
    builder = env["BUILDERS"]["TxtToBin"]
    action = getattr(builder, "action", None)
    if hasattr(action, "action"):
        action = action.action
    if hasattr(action, "cmd_list") and isinstance(action.cmd_list, str):
        old_cmd = action.cmd_list
        new_cmd = re.sub(r"xtensa-esp32[a-z0-9]*-elf-objcopy", "xtensa-esp-elf-objcopy", old_cmd)
        if new_cmd != old_cmd:
            action.cmd_list = new_cmd
            print(f"[Fix-Objcopy] Patched TxtToBin command for Windows compatibility: {new_cmd}")
