#!/usr/bin/env python3
import json
import sys
import os
import subprocess

def main():
    if not os.path.exists('compile_commands.json'):
        print("Error: compile_commands.json not found. Run a PIO build first.")
        sys.exit(1)

    with open('compile_commands.json', 'r') as f:
        db = json.load(f)

    # Flags that clang doesn't understand but GCC/Xtensa uses
    bad_flags = [
        '-mlongcalls',
        '-fno-tree-switch-conversion',
        '-fstrict-volatile-bitfields',
        '-mdisable-hardware-atomics',
        '-Wno-frame-address',
        '-Wno-format-truncation'
    ]

    for entry in db:
        command = entry.get('command', '')
        for flag in bad_flags:
            command = command.replace(flag, '')
        entry['command'] = command

    with open('compile_commands_tidy.json', 'w') as f:
        json.dump(db, f, indent=2)

    header_filter = sys.argv[1] if len(sys.argv) > 1 else 'include/.*'
    target_files = sys.argv[2:] if len(sys.argv) > 2 else []

    checks = "-*,modernize-loop-convert,modernize-use-nullptr,modernize-use-auto,bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions"

    cmd = [
        "/Users/robertlipe/.espressif/tools/esp-clang/esp-21.1.3_20260408/esp-clang/bin/clang-tidy",
        "--fix",
        "--fix-errors",
        f"-checks={checks}",
        f"-header-filter={header_filter}",
        "-p", "compile_commands_tidy.json"
    ] + target_files

    print(f"Running: {' '.join(cmd)}")
    subprocess.run(cmd)

if __name__ == '__main__':
    main()
