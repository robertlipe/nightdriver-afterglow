#!/usr/bin/env python3

import os
import re
import sys

def audit_directory(dir_path):
    violations = []

    # regexes to match
    re_globals = re.compile(r'^\s*#include\s+["<]?globals\.h[">]?\s*')
    re_if = re.compile(r'^\s*#\s*(if|ifdef|ifndef).*\b(USE_|ENABLE_|SHOW_|M5|TFT|WROVER|TTGO|ELECROW|AMOLED_S3|ARDUINO_HELTEC)\b.*')

    for root, dirs, files in os.walk(dir_path):
        # Skip include/effects as they are leaf nodes that inherit globals.h from parents
        if 'include/effects' in root:
            continue

        for file in files:
            if not file.endswith(('.h', '.cpp')):
                continue
            if file == 'globals.h':
                continue

            file_path = os.path.join(root, file)
            globals_line = -1
            first_include_line = -1
            first_include_text = ""
            first_if_line = -1
            first_if_text = ""
            pragma_once_count = 0

            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                for line_no, line in enumerate(f, 1):
                    if '#pragma once' in line:
                        pragma_once_count += 1

                    if line.lstrip().startswith('#include'):
                        if first_include_line == -1:
                            first_include_line = line_no
                            first_include_text = line.strip()

                    if re_globals.match(line):
                        if globals_line == -1:
                            globals_line = line_no

                    match = re_if.match(line)
                    if match:
                        if first_if_line == -1:
                            first_if_line = line_no
                            first_if_text = line.strip()

            if pragma_once_count > 1:
                violations.append((file_path, f"Found {pragma_once_count} #pragma once lines"))

            # Exceptions for missing globals.h
            is_3rdparty_or_test = 'uzlib' in file_path or 'amoled' in file_path or 'test_' in file_path or 'interfaces.h' in file_path

            if first_include_line != -1 and not is_3rdparty_or_test:
                if globals_line == -1:
                    violations.append((file_path, "No globals.h included, but has: " + first_include_text))
                elif first_include_line < globals_line and first_include_text.startswith('#include "'):
                    violations.append((file_path, f"Local Include '{first_include_text}' occurs before globals.h at Line {globals_line}"))

            if first_if_line != -1 and not is_3rdparty_or_test:
                if globals_line == -1:
                    violations.append((file_path, "No globals.h, but has: " + first_if_text))
                elif first_if_line < globals_line:
                    violations.append((file_path, f"Line {first_if_line} ({first_if_text}) occurs before globals.h at Line {globals_line}"))

    return violations

def main():
    proj_dir = '.'
    v1 = audit_directory(os.path.join(proj_dir, 'include'))
    v2 = audit_directory(os.path.join(proj_dir, 'src'))

    all_v = v1 + v2
    if not all_v:
        print("No violations found!")
        sys.exit(0)
    else:
        print("Violations found:")
        for path, reason in all_v:
            # make path relative
            rel_path = os.path.relpath(path, proj_dir)
            print(f"{rel_path}: {reason}")
        sys.exit(1)

if __name__ == '__main__':
    main()
