#!/usr/bin/env python3

import os
import sys
import re
import json
import shutil
import subprocess
import time
import stat

def remove_readonly(func, path, exc_info):
    try:
        os.chmod(path, stat.S_IWRITE)
        func(path)
    except Exception:
        pass

def safe_rmtree(path):
    if not os.path.exists(path):
        return
    for attempt in range(5):
        try:
            shutil.rmtree(path, onerror=remove_readonly)
            if not os.path.exists(path):
                return
        except Exception:
            pass
        time.sleep(0.2)

# Find PlatformIO executable in various standard locations
def find_platformio_bin():
    # 1. Try relative to sys.executable (and Scripts subfolder on Windows)
    py_dir = os.path.dirname(sys.executable)
    for sub in ["", "Scripts", "bin"]:
        for name in ["platformio", "platformio.exe", "pio", "pio.exe"]:
            path = os.path.join(py_dir, sub, name) if sub else os.path.join(py_dir, name)
            if os.path.exists(path):
                return path

    # 2. Try standard user home platformio virtual environment paths
    home = os.path.expanduser("~")
    possible_dirs = [
        os.path.join(home, ".platformio", "penv", "bin"),
        os.path.join(home, ".platformio", "penv", "Scripts"),
    ]
    for p_dir in possible_dirs:
        for name in ["platformio", "platformio.exe", "pio", "pio.exe"]:
            path = os.path.join(p_dir, name)
            if os.path.exists(path):
                return path

    # 3. Try to locate via system PATH
    for name in ["platformio", "pio"]:
        resolved = shutil.which(name)
        if resolved:
            return resolved

    # 4. Fallback to raw command
    return [sys.executable, "-m", "platformio"]

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

import time

# `python tools/setup_shared_libs_cli.py --force` re-downloads everything, which
# is handy after switching platforms or when a tree is suspect.
FORCE_REINSTALL = "--force" in sys.argv[1:]


def read_resolved_version(lib_path):
    """Return the version PlatformIO actually installed, if discoverable.

    PlatformIO writes a ``.piopm`` manifest next to every installed package; it
    is the authoritative record of what is really on disk, unlike our own
    ``.nd_version`` marker which only remembers what we asked for.
    """
    try:
        with open(os.path.join(lib_path, ".piopm"), "r", encoding="utf-8") as f:
            version = json.load(f).get("version")
        if version:
            return str(version)
    except (OSError, ValueError, AttributeError):
        pass

    try:
        with open(os.path.join(lib_path, "library.json"), "r", encoding="utf-8") as f:
            version = json.load(f).get("version")
        if version:
            return str(version)
    except (OSError, ValueError, AttributeError):
        pass

    try:
        with open(os.path.join(lib_path, "library.properties"), "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                match = re.match(r"^\s*version\s*[=:]\s*(.+?)\s*$", line)
                if match:
                    return match.group(1)
    except OSError:
        pass

    return None


def read_version_marker(version_file):
    """Decode the ``.nd_version`` marker into a dict.

    Current markers are JSON (``{"spec": ..., "resolved": ...}``); older
    checkouts wrote the bare spec string. Both are understood.
    """
    try:
        with open(version_file, "r", encoding="utf-8") as f:
            raw = f.read().strip()
    except OSError:
        return {}
    if not raw:
        return {}
    try:
        data = json.loads(raw)
        if isinstance(data, dict):
            return data
    except ValueError:
        pass
    return {"spec": raw}


def write_version_marker(version_file, spec, resolved):
    """Record both the requested spec and the version we ended up with."""
    try:
        with open(version_file, "w", encoding="utf-8") as f:
            json.dump({"spec": spec, "resolved": resolved}, f, indent=2)
            f.write("\n")
    except OSError:
        pass


def exact_pin(spec):
    """Return the version if ``spec`` is an exact pin, else None.

    Accepts ``3.10.3``, ``=3.10.3`` and ``==3.10.3``. Range specs such as
    ``^3.10.1`` or ``>=1.2.3`` return None because their resolved version is
    expected to move as upstream releases.
    """
    requirement = spec.rsplit("@", 1)[-1].strip()
    match = re.fullmatch(r"=*\s*(\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.\-]+)?)", requirement)
    return match.group(1) if match else None


# Simple lock mechanism to prevent race conditions when invoked concurrently by PIO
def acquire_lock(lock_path, timeout=300):
    start = time.time()
    while True:
        try:
            fd = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_RDWR)
            return fd
        except OSError:
            if time.time() - start > timeout:
                print(f"[Shared-Libs] Warning: Lock {lock_path} timed out after {timeout}s. Still waiting...", file=sys.stderr)
                start = time.time() # Reset timer to avoid spam
            time.sleep(0.5 + (os.getpid() % 10) / 10.0) # Add jitter to prevent thundering herd

def release_lock(fd, lock_path):
    try:
        os.close(fd)
        os.remove(lock_path)
    except Exception:
        pass

# Install/upgrade all shared dependencies
lock_path = os.path.join(TARGET_DIR, ".install.lock")
lock_fd = acquire_lock(lock_path)

try:
    for lib in shared_libs:
        expected_name = lib.split('/')[-1].split('@')[0].replace('.git', '').strip()
        clean_name = expected_name.replace(' ', '_')

        lib_path = os.path.join(TARGET_DIR, clean_name)
        expected_path = os.path.join(TARGET_DIR, expected_name)
        version_file = os.path.join(lib_path, ".nd_version")

        # Decide whether the copy already on disk is the one we want. We no
        # longer trust the marker alone: a stale tree (e.g. one frozen by an old
        # "^3.10.1" marker) or a half-unpacked install used to be reused
        # forever, which is how local and CI builds silently diverged.
        resolved = read_resolved_version(lib_path) if os.path.exists(lib_path) else None
        marker = read_version_marker(version_file) if os.path.exists(version_file) else {}
        required = exact_pin(lib)

        needs_install = True
        if FORCE_REINSTALL:
            print(f"[Shared-Libs] {lib}: forced reinstall requested.", file=sys.stderr)
        elif os.path.exists(lib_path) and marker.get("spec") == lib:
            if resolved is None:
                print(f"[Shared-Libs] {lib}: no package manifest found; reinstalling.", file=sys.stderr)
            elif required and resolved != required:
                print(f"[Shared-Libs] {lib}: on-disk {resolved} != pinned {required}; reinstalling.", file=sys.stderr)
            elif marker.get("resolved") and marker["resolved"] != resolved:
                print(f"[Shared-Libs] {lib}: on-disk {resolved} != recorded {marker['resolved']}; reinstalling.", file=sys.stderr)
            else:
                needs_install = False

        if needs_install:
            if os.path.exists(lib_path):
                print(f"[Shared-Libs] Upgrading/Reinstalling shared dependency: {lib}...", file=sys.stderr)
                safe_rmtree(lib_path)
            else:
                print(f"[Shared-Libs] Installing shared dependency: {lib}...", file=sys.stderr)

            pio_bin = find_platformio_bin()

            try:
                # Use PlatformIO's package manager to install it
                cmd = pio_bin if isinstance(pio_bin, list) else [pio_bin]
                subprocess.run(cmd + [
                    "pkg", "install",
                    "--library", lib,
                    "--storage-dir", TARGET_DIR
                ], check=True, stdout=subprocess.DEVNULL) # Let stderr pass through for error visibility

                # Rename if expected_path != lib_path (spaces to underscores)
                if expected_path != lib_path and os.path.exists(expected_path):
                    safe_rmtree(lib_path)
                    os.rename(expected_path, lib_path)

                # Record both what we asked for and what we actually got, so a
                # later run can tell whether the tree on disk still matches.
                os.makedirs(lib_path, exist_ok=True)
                write_version_marker(version_file, lib, read_resolved_version(lib_path))
            except Exception as e:
                print(f"[Shared-Libs] Error: failed to install dependency {lib}: {e}", file=sys.stderr)
                safe_rmtree(lib_path)
                safe_rmtree(expected_path)
                sys.exit(1)
finally:
    release_lock(lock_fd, lock_path)

