import os
import shutil
import SCons.Action
from SCons.Script import Import

Import("env")

def fix_objcopy():
    # Patch TxtToBin action in SCons Builders so embedded text files use generic xtensa-esp-elf-objcopy
    builders = env.get("BUILDERS", {})
    if "TxtToBin" in builders:
        txt_to_bin = builders["TxtToBin"]
        action_str = str(txt_to_bin.action)
        new_action_str = action_str.replace("xtensa-esp32s3-elf-objcopy", "xtensa-esp-elf-objcopy")
        new_action_str = new_action_str.replace("xtensa-esp32-elf-objcopy", "xtensa-esp-elf-objcopy")
        new_action_str = new_action_str.replace("xtensa-esp32s2-elf-objcopy", "xtensa-esp-elf-objcopy")
        new_action_str = new_action_str.replace("xtensa-esp32c3-elf-objcopy", "xtensa-esp-elf-objcopy")
        txt_to_bin.action = SCons.Action.Action(new_action_str, str(txt_to_bin.action))

    # Create executable shims in toolchain bin directory on Windows
    packages_dir = os.path.expanduser("~/.platformio/packages")
    toolchain_dir = os.path.join(packages_dir, "toolchain-xtensa-esp-elf", "bin")

    if os.path.exists(toolchain_dir):
        source_exe = os.path.join(toolchain_dir, "xtensa-esp-elf-objcopy.exe")
        if not os.path.exists(source_exe):
            for item in os.listdir(toolchain_dir):
                if item.endswith("objcopy.exe"):
                    source_exe = os.path.join(toolchain_dir, item)
                    break

        if os.path.exists(source_exe):
            for target_exe_name in [
                "xtensa-esp32-elf-objcopy.exe",
                "xtensa-esp32s3-elf-objcopy.exe",
                "xtensa-esp32s2-elf-objcopy.exe",
                "xtensa-esp32c3-elf-objcopy.exe",
                "xtensa-esp32-elf-objcopy",
                "xtensa-esp32s3-elf-objcopy",
                "xtensa-esp32s2-elf-objcopy",
                "xtensa-esp32c3-elf-objcopy",
            ]:
                target_path = os.path.join(toolchain_dir, target_exe_name)
                if not os.path.exists(target_path):
                    try:
                        shutil.copy(source_exe, target_path)
                    except Exception:
                        pass

fix_objcopy()
