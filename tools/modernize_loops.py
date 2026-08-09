#!/usr/bin/env python3
import os
import re

def process_file(filepath):
    with open(filepath, 'r', errors='ignore') as f:
        content = f.read()

    # Find loop declarations like: for (uint8_t i = 0; ...
    # and replace with: for (int i = 0; ...
    pattern = r'for\s*\(\s*(?:uint8_t|uint16_t)\s+([a-zA-Z0-9_]+)\s*='
    
    def replacer(match):
        return f"for (int {match.group(1)} ="

    new_content, count = re.subn(pattern, replacer, content)

    if count > 0:
        print(f"Upgraded {count} loop(s) in {filepath}")
        with open(filepath, 'w', errors='ignore') as f:
            f.write(new_content)
    return count

def main():
    total_upgrades = 0
    for target_dir in ['src', 'include']:
        for root, dirs, files in os.walk(target_dir):
            for file in files:
                if file.endswith(('.cpp', '.h', '.c', '.hpp')):
                    total_upgrades += process_file(os.path.join(root, file))
    
    print(f"\nDone! Upgraded a total of {total_upgrades} loop iterators to native 'int'.")

if __name__ == '__main__':
    main()
