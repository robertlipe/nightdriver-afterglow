import os
import sys
import re
import shutil
import subprocess

# Simple platformio.ini parser
class SimplePIOConfig:
    def __init__(self, filepath):
        self.sections = {}
        self.filepath = filepath
        self.parse()

    def parse(self):
        current_section = None
        with open(self.filepath, "r") as f:
            for line in f:
                line_stripped = line.strip()
                if not line_stripped or line_stripped.startswith(";") or line_stripped.startswith("#"):
                    continue
                # Section header
                m = re.match(r"^\[([^\]]+)\]", line_stripped)
                if m:
                    current_section = m.group(1)
                    self.sections[current_section] = {}
                    continue
                if current_section:
                    if "=" in line:
                        name, val = line.split("=", 1)
                        name = name.strip()
                        self.sections[current_section][name] = val.strip()
                    else:
                        if self.sections[current_section]:
                            last_option = list(self.sections[current_section].keys())[-1]
                            self.sections[current_section][last_option] += "\n" + line_stripped

    def get(self, section, option):
        curr = section
        visited = set()
        while curr and curr not in visited:
            visited.add(curr)
            if curr in self.sections and option in self.sections[curr]:
                return self.resolve_interpolation(self.sections[curr][option])
            if curr in self.sections and "extends" in self.sections[curr]:
                curr = self.sections[curr]["extends"].strip()
            else:
                curr = None
        return None

    def resolve_interpolation(self, val):
        def repl(match):
            parts = match.group(1).split(".")
            if len(parts) == 2:
                sec, opt = parts
            else:
                sec, opt = "base", parts[0]
            res = self.get(sec, opt)
            return res if res is not None else ""

        while "${" in val:
            new_val = re.sub(r"\${([^}]+)}", repl, val)
            if new_val == val:
                break
            val = new_val
        return val

config = SimplePIOConfig("platformio.ini")

def parse_multiline_list(val):
    if not val:
        return []
    if isinstance(val, list):
        lines = val
    else:
        lines = val.split('\n')

    result = []
    for line in lines:
        line = line.strip()
        if not line:
            continue
        # Clean up spaces around '@'
        if '@' in line:
            parts = [p.strip() for p in line.split('@')]
            line = '@'.join(parts)
        result.append(line)
    return result

# Read shared_lib_deps from [base] to know which libraries we can install
try:
    shared_libs_raw = config.get("base", "shared_lib_deps")
    shared_libs = parse_multiline_list(shared_libs_raw)
except Exception:
    shared_libs = []

TARGET_DIR = os.path.join(".pio", "shared_libdeps")

if not os.path.exists(TARGET_DIR):
    os.makedirs(TARGET_DIR)

# Install/upgrade all shared dependencies
for lib in shared_libs:
    expected_name = lib.split('/')[-1].split('@')[0].replace('.git', '').strip()
    clean_name = expected_name.replace(' ', '_')

    lib_path = os.path.join(TARGET_DIR, clean_name)
    expected_path = os.path.join(TARGET_DIR, expected_name)
    version_file = os.path.join(lib_path, ".nd_version")

    needs_install = True
    if os.path.exists(lib_path) and os.path.exists(version_file):
        try:
            with open(version_file, "r") as f:
                installed_version = f.read().strip()
            if installed_version == lib:
                needs_install = False
        except Exception:
            pass

    if needs_install:
        if os.path.exists(lib_path):
            print(f"[Shared-Libs] Upgrading/Reinstalling shared dependency: {lib}...", file=sys.stderr)
            try:
                shutil.rmtree(lib_path)
            except Exception as e:
                print(f"[Shared-Libs] Warning: failed to clean up directory {lib_path}: {e}", file=sys.stderr)
        else:
            print(f"[Shared-Libs] Installing shared dependency: {lib}...", file=sys.stderr)

        try:
            # Use PlatformIO's package manager to install it
            subprocess.run([
                "platformio", "pkg", "install",
                "--library", lib,
                "--storage-dir", TARGET_DIR
            ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

            # Rename if expected_path != lib_path (spaces to underscores)
            if expected_path != lib_path and os.path.exists(expected_path):
                if os.path.exists(lib_path):
                    shutil.rmtree(lib_path)
                os.rename(expected_path, lib_path)

            # Write version file
            os.makedirs(lib_path, exist_ok=True)
            with open(version_file, "w") as f:
                f.write(lib)
        except Exception as e:
            print(f"[Shared-Libs] Error: failed to install dependency {lib}: {e}", file=sys.stderr)
            if os.path.exists(lib_path):
                shutil.rmtree(lib_path, ignore_errors=True)
            if expected_path != lib_path and os.path.exists(expected_path):
                shutil.rmtree(expected_path, ignore_errors=True)
            sys.exit(1)

# Read base_build_flags from [base]
try:
    base_flags_raw = config.get("base", "base_build_flags")
    if isinstance(base_flags_raw, list):
        base_flags_str = " ".join([f.strip() for f in base_flags_raw if f.strip()])
    else:
        base_flags_str = " ".join([f.strip() for f in base_flags_raw.split('\n') if f.strip()])
except Exception:
    base_flags_str = "-std=gnu++2a -g3 -Ofast -ffunction-sections -fdata-sections -include string.h"

# Print a dummy flag and all base build flags for build_flags
print(f"-DSHARED_LIBS_OK {base_flags_str}")
