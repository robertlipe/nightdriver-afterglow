Import("env")
import os
import re

if os.name == 'nt':
    if "OBJCOPY" in env:
        objcopy_val = env.get("OBJCOPY")
        if isinstance(objcopy_val, str):
            env.Replace(OBJCOPY=re.sub(r'xtensa-esp32[s\d]*-elf-objcopy', 'xtensa-esp-elf-objcopy', objcopy_val))

    txt_to_bin = env.get("BUILDERS", {}).get("TxtToBin")
    if txt_to_bin:
        action = getattr(txt_to_bin, "action", None)
        if action:
            cmd_list = getattr(action, "cmd_list", None)
            if isinstance(cmd_list, str) and "xtensa-esp32" in cmd_list:
                new_cmd = re.sub(r'xtensa-esp32[s\d]*-elf-objcopy', 'xtensa-esp-elf-objcopy', cmd_list)
                # DO NOT assign to txt_to_bin.action since SCons forbids it.
                # Just replace the action's command list directly.
                txt_to_bin.action.cmd_list = new_cmd
