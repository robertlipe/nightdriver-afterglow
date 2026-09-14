import os
import shutil
from SCons.Script import Import

Import("env")

def fix_objcopy():
    # 1. Create executable shims in toolchain bin directory
    platformio_packages = os.path.expanduser('~/.platformio/packages')
    toolchain_bin = os.path.join(platformio_packages, 'toolchain-xtensa-esp-elf', 'bin')

    if os.path.exists(toolchain_bin):
        generic_objcopy = os.path.join(toolchain_bin, 'xtensa-esp-elf-objcopy.exe')
        if not os.path.exists(generic_objcopy):
            # Look for any objcopy executable in toolchain bin
            for f in os.listdir(toolchain_bin):
                if 'objcopy' in f and f.endswith('.exe'):
                    generic_objcopy = os.path.join(toolchain_bin, f)
                    break

        if os.path.exists(generic_objcopy):
            # Create target-specific binary copies so Windows can invoke them directly
            for target_name in [
                'xtensa-esp32-elf-objcopy.exe',
                'xtensa-esp32s3-elf-objcopy.exe',
                'xtensa-esp32s2-elf-objcopy.exe',
                'xtensa-esp32c3-elf-objcopy.exe',
                'xtensa-esp32-elf-objcopy',
                'xtensa-esp32s3-elf-objcopy',
                'xtensa-esp32s2-elf-objcopy',
                'xtensa-esp32c3-elf-objcopy'
            ]:
                target_path = os.path.join(toolchain_bin, target_name)
                if not os.path.exists(target_path):
                    try:
                        shutil.copy(generic_objcopy, target_path)
                    except Exception:
                        pass

    # 2. Also patch SCons env OBJCOPY variable and TxtToBin builder if present
    env.Replace(OBJCOPY="xtensa-esp-elf-objcopy")

    builders = env.get('BUILDERS', {})
    if 'TxtToBin' in builders:
        txt_to_bin = builders['TxtToBin']
        action_str = str(txt_to_bin.action)
        if 'xtensa-esp32' in action_str:
            new_action_str = action_str.replace('xtensa-esp32s3-elf-objcopy', 'xtensa-esp-elf-objcopy')
            new_action_str = new_action_str.replace('xtensa-esp32-elf-objcopy', 'xtensa-esp-elf-objcopy')
            try:
                import SCons.Action
                txt_to_bin.action = SCons.Action.Action(new_action_str, str(txt_to_bin.action))
            except Exception:
                pass

fix_objcopy()
