Import("env")
import os
import re

if os.name == 'nt':
    txt_to_bin = env.get("BUILDERS", {}).get("TxtToBin")
    if txt_to_bin:
        action = getattr(txt_to_bin, "action", None)
        if action:
            cmd_list = getattr(action, "cmd_list", None)
            if isinstance(cmd_list, str) and "xtensa-esp32" in cmd_list:
                new_cmd = re.sub(r'xtensa-esp32[s\d]*-elf-objcopy', 'xtensa-esp-elf-objcopy', cmd_list)
                txt_to_bin.action = env.Action(new_cmd, txt_to_bin.action.cmdstr)
