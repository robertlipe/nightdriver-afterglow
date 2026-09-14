import os
import shutil
import SCons.Action
from SCons.Script import Import

Import("env")

def fix_builder_action(env):
    # Fix TxtToBin action in SCons builder if present
    builders = env.get('BUILDERS', {})
    if 'TxtToBin' in builders:
        txt_to_bin = builders['TxtToBin']
        action_str = str(txt_to_bin.action)
        if '$OBJCOPY' in action_str or 'xtensa-esp32' in action_str:
            # Replace target-specific objcopy (like xtensa-esp32s3-elf-objcopy) with generic xtensa-esp-elf-objcopy
            new_action_str = action_str.replace('xtensa-esp32s3-elf-objcopy', 'xtensa-esp-elf-objcopy')
            new_action_str = new_action_str.replace('xtensa-esp32-elf-objcopy', 'xtensa-esp-elf-objcopy')
            txt_to_bin.action = SCons.Action.Action(new_action_str, str(txt_to_bin.action))

    # On Windows, ensure xtensa-esp-elf-objcopy executable shim exists in toolchain bin directory
    platformio_packages = os.path.expanduser('~/.platformio/packages')
    toolchain_bin = os.path.join(platformio_packages, 'toolchain-xtensa-esp-elf', 'bin')

    if os.path.exists(toolchain_bin):
        generic_objcopy = os.path.join(toolchain_bin, 'xtensa-esp-elf-objcopy.exe')
        if not os.path.exists(generic_objcopy):
            # Find any xtensa-esp*-objcopy.exe in the bin folder to copy as generic
            for f in os.listdir(toolchain_bin):
                if 'objcopy.exe' in f:
                    shutil.copy(os.path.join(toolchain_bin, f), generic_objcopy)
                    break

        # Also create target-specific aliases if requested on Windows
        for target in ['xtensa-esp32-elf-objcopy.exe', 'xtensa-esp32s3-elf-objcopy.exe']:
            target_path = os.path.join(toolchain_bin, target)
            if os.path.exists(generic_objcopy) and not os.path.exists(target_path):
                shutil.copy(generic_objcopy, target_path)

fix_builder_action(env)
