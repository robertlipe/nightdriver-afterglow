Import("env")
import os
import shutil
import re

def ensure_objcopy_shims():
    home = os.path.expanduser("~")
    possible_dirs = [
        os.path.join(home, ".platformio", "packages", "toolchain-xtensa-esp-elf", "bin"),
        os.path.join(home, ".platformio", "packages", "toolchain-xtensa-esp32s3-elf", "bin"),
        os.path.join(home, ".platformio", "packages", "toolchain-xtensa-esp32s2-elf", "bin"),
    ]

    for tc_bin in possible_dirs:
        if not os.path.exists(tc_bin):
            continue

        for name in os.listdir(tc_bin):
            if "objcopy" in name:
                src_path = os.path.join(tc_bin, name)
                ext = ".exe" if name.endswith(".exe") else ""
                aliases = [
                    f"xtensa-esp32-elf-objcopy{ext}",
                    f"xtensa-esp32s2-elf-objcopy{ext}",
                    f"xtensa-esp32s3-elf-objcopy{ext}",
                    f"xtensa-esp-elf-objcopy{ext}",
                ]
                for alias in aliases:
                    target_path = os.path.join(tc_bin, alias)
                    if not os.path.exists(target_path):
                        try:
                            shutil.copy2(src_path, target_path)
                            print(f"[Fix-Objcopy] Created missing toolchain executable shim: {alias}")
                        except Exception as e:
                            print(f"[Fix-Objcopy] Warning: Failed to copy {alias}: {e}")

ensure_objcopy_shims()

# Also patch TxtToBin builder action in SCons env
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
            print(f"[Fix-Objcopy] Patched TxtToBin command: {new_cmd}")
