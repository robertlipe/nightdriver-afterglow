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
                    # Indented lines are continuations of the previous option,
                    # even when they contain '=' (e.g. "Name = <url>" library
                    # specs or "-DFOO=1" build flags).
                    if "=" in line and not line.startswith((" ", "\t")):
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
VERBOSE = "-v" in sys.argv[1:] or "--verbose" in sys.argv[1:]


def format_pio_error(stdout, stderr):
    """Extract a concise, readable error message from PlatformIO's output."""
    combined = (stderr.strip() or stdout.strip())
    lines = [line.strip() for line in combined.splitlines() if line.strip()]
    if not lines:
        return "Unknown error (no output from PlatformIO)"

    # Look for the last exception line or 'Error:' line
    error_line = None
    for line in reversed(lines):
        # Ignore PlatformIO troubleshooting banner lines
        if line.startswith("=") or "http" in line or line.startswith("*") or "Further steps:" in line:
            continue
        if any(line.startswith(prefix) for prefix in [
            "Error:", "PackageException:", "AttributeError:", "ValueError:",
            "TypeError:", "KeyError:", "FileNotFoundError:", "Exception:",
            "ResolverError:"
        ]) or (": " in line and line.split(":")[0].endswith("Error")):
            error_line = line
            break

    if not error_line:
        error_line = lines[-1]

    msg = [error_line]
    if "PlatformPlatform" in combined:
        msg.append("Hint: A platform in ~/.platformio/platforms may be corrupted or have an invalid directory name.")
    return "\n    ".join(msg)


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


def spec_library_name(spec):
    """Derive the folder name PlatformIO installs a dependency into.

    Registry specs look like "owner/Name @ version". For external (git/URL)
    deps the folder name normally comes from the package manifest, which we
    cannot know in advance, so those must use PIO's custom-name syntax
    "Name = <url>" to pin it; that also keeps the spaces-to-underscores rename
    below deterministic so ${shared_libs.*} symlink paths stay valid.
    """
    tokens = spec.split("=", 1)
    if len(tokens) == 2 and "/" not in tokens[0]:
        return tokens[0].strip()
    return spec.split('/')[-1].split('@')[0].replace('.git', '').strip()


def prune_fastled(lib_path):
    """Prune FastLED's multimedia/codec/asio bloat down to core LED drivers.

    FastLED 3.10+ unconditionally compiles 37 unity translation units (fl/build/*.cpp),
    dragging in MPEG-1, H.264, MP3, Vorbis, libnsgif, TJpg_Decoder, a custom Boost.Asio
    TCP/IP stack, and Web UI.
    This rewrites library.json srcFilter to only compile the 11 load-bearing units,
    replaces fl/stl/asio/_build.cpp.hpp with an empty stub, and adds fl.audio_stub.cpp
    to satisfy CFastLED::add() without pulling in the audio/codec avalanche.
    """
    marker_path = os.path.join(lib_path, ".nd_pruned")
    prune_version = "v1"
    if os.path.exists(marker_path):
        try:
            with open(marker_path, "r", encoding="utf-8") as f:
                if f.read().strip() == prune_version:
                    return
        except OSError:
            pass

    print("[Shared-Libs] Pruning FastLED multimedia/codec/asio bloat...", file=sys.stderr)

    # 1. Stub fl/stl/asio/_build.cpp.hpp to disable rogue TCP/HTTP parser in STL
    asio_build = os.path.join(lib_path, "src", "fl", "stl", "asio", "_build.cpp.hpp")
    if os.path.exists(os.path.dirname(asio_build)):
        with open(asio_build, "w", encoding="utf-8") as f:
            f.write("// Pruned by NightDriver: Asio/HTTP stack not needed by LED drivers\n")

    # 2. Add fl.audio_stub.cpp to satisfy CFastLED::add() without audio/codec deps
    stub_path = os.path.join(lib_path, "src", "fl", "build", "fl.audio_stub.cpp")
    os.makedirs(os.path.dirname(stub_path), exist_ok=True)
    with open(stub_path, "w", encoding="utf-8") as f:
        f.write(
            '/// @file fl.audio_stub.cpp\n'
            '/// Minimal stub for FastLED AudioManager to satisfy CFastLED::add()\n'
            '#include "fl/audio/audio_manager.h"\n\n'
            'namespace fl {\n'
            'namespace audio {\n\n'
            'AudioManager &AudioManager::instance() FL_NO_EXCEPT {\n'
            '    static AudioManager sInstance;\n'
            '    return sInstance;\n'
            '}\n\n'
            'shared_ptr<Processor> AudioManager::add(const Config &) FL_NO_EXCEPT {\n'
            '    return nullptr;\n'
            '}\n\n'
            'shared_ptr<Processor> AudioManager::add(shared_ptr<IInput>) FL_NO_EXCEPT {\n'
            '    return nullptr;\n'
            '}\n\n'
            'shared_ptr<Processor> AudioManager::add(UIAudio &) FL_NO_EXCEPT {\n'
            '    return nullptr;\n'
            '}\n\n'
            'void AudioManager::remove(shared_ptr<Processor>) FL_NO_EXCEPT {}\n\n'
            'shared_ptr<Processor> &AudioManager::processor() FL_NO_EXCEPT {\n'
            '    static shared_ptr<Processor> sProc;\n'
            '    return sProc;\n'
            '}\n\n'
            '} // namespace audio\n'
            '} // namespace fl\n'
        )

    # 3. Whitelist core compilation units in library.json
    lib_json_path = os.path.join(lib_path, "library.json")
    if os.path.exists(lib_json_path):
        try:
            with open(lib_json_path, "r", encoding="utf-8") as f:
                data = json.load(f)

            data["build"]["srcFilter"] = [
                "-<**/*.cpp>",
                "+<fl/build/src.cpp>",
                "+<fl/build/platforms+.cpp>",
                "+<fl/build/fl.channels+.cpp>",
                "+<fl/build/fl.chipsets+.cpp>",
                "+<fl/build/fl.gfx+.cpp>",
                "+<fl/build/fl.log+.cpp>",
                "+<fl/build/fl.math+.cpp>",
                "+<fl/build/fl.stl+.cpp>",
                "+<fl/build/fl.system+.cpp>",
                "+<fl/build/fl.task+.cpp>",
                "+<fl/build/fl.audio_stub.cpp>",
                "+<*.h>",
                "+<platforms/esp/32/drivers/gpio_isr_rx/*.S>",
            ]

            with open(lib_json_path, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=4)
        except Exception as e:
            print(f"[Shared-Libs] Error: failed to patch FastLED library.json: {e}", file=sys.stderr)
            return

    try:
        with open(marker_path, "w", encoding="utf-8") as f:
            f.write(prune_version + "\n")
    except OSError as e:
        print(f"[Shared-Libs] Warning: failed to write prune marker: {e}", file=sys.stderr)


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
        expected_name = spec_library_name(lib)
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
                proc = subprocess.run(
                    cmd + [
                        "pkg", "install",
                        "--library", lib,
                        "--storage-dir", TARGET_DIR
                    ],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True
                )

                if proc.returncode != 0:
                    formatted = format_pio_error(proc.stdout, proc.stderr)
                    print(f"[Shared-Libs] Error: failed to install dependency {lib}:\n    {formatted}", file=sys.stderr)
                    if VERBOSE:
                        if proc.stdout.strip():
                            print(f"\n[PlatformIO stdout]:\n{proc.stdout}", file=sys.stderr)
                        if proc.stderr.strip():
                            print(f"\n[PlatformIO stderr]:\n{proc.stderr}", file=sys.stderr)
                    else:
                        print("    (Pass --verbose to see full PlatformIO output)", file=sys.stderr)
                    safe_rmtree(lib_path)
                    safe_rmtree(expected_path)
                    sys.exit(1)

                # Rename if expected_path != lib_path (spaces to underscores)
                if expected_path != lib_path and os.path.exists(expected_path):
                    safe_rmtree(lib_path)
                    os.rename(expected_path, lib_path)

                # Record both what we asked for and what we actually got, so a
                # later run can tell whether the tree on disk still matches.
                os.makedirs(lib_path, exist_ok=True)
                write_version_marker(version_file, lib, read_resolved_version(lib_path))

                if clean_name == "FastLED":
                    prune_fastled(lib_path)
            except Exception as e:
                print(f"[Shared-Libs] Error: failed to install dependency {lib}: {e}", file=sys.stderr)
                safe_rmtree(lib_path)
                safe_rmtree(expected_path)
                sys.exit(1)

    # Ensure FastLED on disk is pruned even if it was installed in a previous run
    fastled_dir = os.path.join(TARGET_DIR, "FastLED")
    if os.path.exists(fastled_dir):
        prune_fastled(fastled_dir)
finally:
    release_lock(lock_fd, lock_path)

