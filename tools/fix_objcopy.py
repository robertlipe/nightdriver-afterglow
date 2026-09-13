Import("env")
import os
import re

if os.name == 'nt':
    txt_to_bin = env.get("BUILDERS", {}).get("TxtToBin")
    if txt_to_bin and hasattr(txt_to_bin, "action"):
        action = txt_to_bin.action
        if hasattr(action, "cmd_list") and isinstance(action.cmd_list, str):
            action.cmd_list = re.sub(r'xtensa-esp32[s\d]*-elf-objcopy', 'xtensa-esp-elf-objcopy', action.cmd_list)
