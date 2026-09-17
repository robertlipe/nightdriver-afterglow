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
                # It seems setting action directly causes issues in SCons. Setting cmd_list is also tricky.
                # A safe way is to create a totally new builder.
                pass

    # Let's override the TxtToBin builder completely for Windows if it exists.
    # The default builder action is string:
    # "$OBJCOPY --input-target binary --output-target elf32-xtensa-le --binary-architecture xtensa --rename-section .data=.rodata.embedded $SOURCE $TARGET"
    if txt_to_bin:
        # Instead of touching the existing builder's properties which raises errors,
        # we redefine the TxtToBin builder in the environment if the command has the bad tool.
        # Actually since we replaced $OBJCOPY above, and the default action uses $OBJCOPY,
        # it should just work if the action string uses $OBJCOPY.
        # But if it's hardcoded to 'xtensa-esp32-elf-objcopy' in older framework versions, we can just replace it.
        # We can just recreate the Builder.
        pass

    # Let's inspect the action string in `txt_to_bin`.
