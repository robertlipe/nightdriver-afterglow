#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import re
import shutil

# Default paths and configurations
PIO_PACKAGES_DIR = os.path.expanduser("~/.platformio/packages")
DEFAULT_ELF_PATH = ".pio/build/kitch/firmware.elf"

# Toolchain mapping
TOOLCHAINS = {
    "esp32c3": {
        "prefix": "riscv32-esp-elf-",
        "pkg": "toolchain-riscv32-esp"
    },
    "esp32c6": {
        "prefix": "riscv32-esp-elf-",
        "pkg": "toolchain-riscv32-esp"
    },
    "esp32s2": {
        "prefix": "xtensa-esp32s2-elf-",
        "pkg": "toolchain-xtensa-esp32s2"
    },
    "esp32": {
        "prefix": "xtensa-esp32-elf-",
        "pkg": "toolchain-xtensa-esp32"
    },
    "esp32s3": {
        "prefix": "xtensa-esp32s3-elf-",
        "pkg": "toolchain-xtensa-esp32s3"
    }
}

def find_tool(tool_name, arch_info):
    """Find the specified tool in the PlatformIO packages."""
    prefix = arch_info["prefix"]
    pkg_name = arch_info["pkg"]
    
    # Try common PIO package locations
    search_paths = [
        os.path.join(PIO_PACKAGES_DIR, pkg_name, "bin"),
        os.path.join(PIO_PACKAGES_DIR, "toolchain-xtensa-esp-elf", "bin")
    ]
    
    full_tool_name = prefix + tool_name
    exe_suffix = ".exe" if os.name == "nt" else ""
    for path in search_paths:
        tool_path = os.path.join(path, full_tool_name + exe_suffix)
        if os.path.exists(tool_path):
            return tool_path
            
    # Fallback to system path
    system_path = shutil.which(full_tool_name)
    return system_path if system_path else full_tool_name

def get_working_python(tool_path=None):
    """Find a python executable that can run the given tool_path."""
    import glob
    candidates = [sys.executable, "python", "python3"]
    if os.name == "nt":
        espressif_envs = glob.glob(os.path.expanduser("~/.espressif/python_env/*/Scripts/python.exe"))
    else:
        espressif_envs = glob.glob(os.path.expanduser("~/.espressif/python_env/*/bin/python"))
    # Sort backwards so we check newer IDF envs first if possible
    candidates.extend(sorted(espressif_envs, reverse=True))
    
    for py in candidates:
        try:
            if tool_path:
                res = subprocess.run([py, tool_path, "--help"], capture_output=True)
            else:
                res = subprocess.run([py, "-c", "import future; import construct"], capture_output=True)
            
            if res.returncode == 0:
                return py
        except Exception:
            pass
    return sys.executable

def find_rom_elf(arch):
    """Try to find the ROM ELF for the given architecture in PIO packages."""
    rom_pkg_dir = os.path.join(PIO_PACKAGES_DIR, "tool-esp-rom-elfs")
    if not os.path.exists(rom_pkg_dir):
        return None
        
    for f in os.listdir(rom_pkg_dir):
        if f.startswith(arch) and f.endswith("rom.elf"):
            return os.path.join(rom_pkg_dir, f)
    return None

def disassemble_around_address(elf_path, addr, arch_info):
    """Disassemble a few instructions around the given address."""
    objdump = find_tool("objdump", arch_info)
    if not objdump or not os.path.exists(objdump):
        return

    try:
        val = int(addr, 16)
        start = val - 32
        stop = val + 32
        
        print(f"\n--- Context Disassembly around {addr} ---")
        cmd = [objdump, "-d", f"--start-address={hex(start)}", f"--stop-address={hex(stop)}", elf_path]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.stdout:
            lines = result.stdout.splitlines()
            for line in lines:
                if addr[2:].lower() in line.lower():
                    print(f"==> {line}")
                else:
                    print(f"    {line}")
    except Exception as e:
        print(f"Could not disassemble: {e}")

def decode_backtrace(elf_path, backtrace_str, arch="esp32c6"):
    """Decode a backtrace string using addr2line."""
    arch_info = TOOLCHAINS.get(arch)
    if not arch_info:
        print(f"Error: Unsupported architecture {arch}")
        return False

    addr2line = find_tool("addr2line", arch_info)
    addresses = re.findall(r'0x[0-9a-fA-F]+', backtrace_str)
    if not addresses:
        return False

    print(f"Decoding backtrace using {elf_path}...")
    try:
        cmd = [addr2line, "-pfiaC", "-e", elf_path] + addresses
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print(result.stdout)
        return True
    except Exception as e:
        print(f"Error running addr2line: {e}")
    return False

def analyze_binary_dump(elf_path, dump_file, arch="esp32c6", interactive=False):
    """Use espcoredump.py to analyze a binary dump."""
    tool_path = shutil.which("espcoredump")
    if not tool_path:
        import glob
        search_paths = []
        espressif_tools = glob.glob(os.path.expanduser("~/.espressif/*/esp-idf/components/espcoredump/espcoredump.py"))
        search_paths.extend(sorted(espressif_tools, reverse=True))
        search_paths.extend([
            os.path.expanduser("~/esp/esp-idf/components/espcoredump/espcoredump.py"),
            "/usr/local/bin/espcoredump.py"
        ])
        for p in search_paths:
            if os.path.exists(p):
                tool_path = p
                break
            
    if not tool_path:
        print("\nBinary dump detected, but espcoredump not found.")
        print(f"Manual command: espcoredump info_corefile -t raw -c {dump_file} --elf {elf_path}")
        return

    arch_info = TOOLCHAINS.get(arch)
    gdb_path = find_tool("gdb", arch_info) if arch_info else None
    rom_elf = find_rom_elf(arch)

    if gdb_path and not os.path.isabs(gdb_path):
        pio_gdb_pkg = "tool-" + arch_info["prefix"] + "gdb"
        search_gdb = os.path.join(PIO_PACKAGES_DIR, pio_gdb_pkg, "bin", arch_info["prefix"] + "gdb")
        if os.path.exists(search_gdb):
            gdb_path = search_gdb

    mode = "dbg_corefile" if interactive else "info_corefile"
    print(f"Analyzing binary dump {dump_file} using {tool_path} ({mode})...")
    
    try:
        python_exe = [get_working_python(tool_path)] if tool_path.endswith(".py") else []
        cmd = python_exe + [tool_path, "--chip", arch, mode]
        if gdb_path and os.path.exists(gdb_path): cmd += ["--gdb", gdb_path]
        if rom_elf: cmd += ["--rom-elf", rom_elf]
        cmd += ["--core-format", "raw", "--core", dump_file, elf_path]
        
        env = os.environ.copy()
        if "IDF_PATH" not in env and "esp-idf" in tool_path:
            # e.g., tool_path = ~/esp/esp-idf/components/espcoredump/espcoredump.py
            env["IDF_PATH"] = tool_path.split("components")[0].rstrip("/")
            
        if arch_info:
            toolchain_bin = os.path.join(PIO_PACKAGES_DIR, arch_info["pkg"], "bin")
            if os.path.exists(toolchain_bin):
                env["PATH"] = toolchain_bin + os.pathsep + env.get("PATH", "")

        if interactive:
            subprocess.run(cmd, env=env)
            return

        result = subprocess.run(cmd, capture_output=True, text=True, env=env)
        if "ESP32 CORE DUMP START" in result.stdout:
            print(result.stdout)
            pc_match = re.search(r'pc\s+(0x[0-9a-fA-F]+)', result.stdout)
            if pc_match:
                disassemble_around_address(elf_path, pc_match.group(1), arch_info)
            
            print("\n--- Automatic Address Decoding ---")
            code_addresses = sorted(list(set(re.findall(r'0x[4][0-9a-fA-F]{7}', result.stdout))))
            if code_addresses:
                decode_backtrace(elf_path, " ".join(code_addresses), arch)
        else:
            print(f"Error: espcoredump failed (exit code {result.returncode})")
            print(result.stderr)
    except Exception as e:
        print(f"Error running espcoredump: {e}")

def is_binary(file_path):
    with open(file_path, 'rb') as f:
        chunk = f.read(1024)
        if not chunk: return False
        if chunk.count(b'\xff') > len(chunk) * 0.9: return True
        non_printable = sum(1 for b in chunk if b < 32 and b not in (9, 10, 13))
        return b'\0' in chunk or non_printable > len(chunk) * 0.1

def fetch_coredump(ip):
    import requests
    url = f"http://{ip}/coredump"
    print(f"Fetching coredump from {url}...")
    try:
        response = requests.get(url, timeout=10)
        if response.status_code == 200:
            with open("coredump.bin", "wb") as f:
                f.write(response.content)
            print("Coredump saved to coredump.bin")
            return "coredump.bin"
    except Exception as e:
        print(f"Error fetching: {e}")
    return None

def auto_detect_arch(elf_path):
    """Attempt to detect the architecture from the ELF file header (machine type + entry point)."""
    if not os.path.exists(elf_path):
        return "esp32c6" # Default fallback
    try:
        with open(elf_path, 'rb') as f:
            f.seek(18)
            machine = int.from_bytes(f.read(2), 'little')
            f.seek(24)
            entry = int.from_bytes(f.read(4), 'little')
        
        entry_high = entry >> 16
        if machine == 0xF3: # RISC-V
            if entry_high in (0x4038, 0x4037):
                return "esp32c3"
            elif entry_high == 0x4080:
                return "esp32c6"
            return "esp32c6" # Default to C6 for unknown RISC-V (like P4, S31)
        elif machine == 0x5E: # Xtensa
            if entry_high == 0x4008:
                return "esp32"
            elif entry_high == 0x4002:
                return "esp32s2"
            elif entry_high == 0x4037:
                return "esp32s3"
            return "esp32s3"
    except Exception as e:
        print(f"Warning: Could not auto-detect arch from ELF: {e}")
    return "esp32c6"

def main():
    parser = argparse.ArgumentParser(description="NightDriver Panic & Coredump Tool")
    parser.add_argument("--ip", help="IP address to fetch from")
    parser.add_argument("--elf", default=DEFAULT_ELF_PATH, help="Path to ELF")
    
    valid_archs = ["auto"] + list(TOOLCHAINS.keys())
    parser.add_argument("--arch", default="auto", choices=valid_archs,
                        help=f"Architecture. Valid options: {', '.join(valid_archs)}")
    parser.add_argument("--file", help="Local dump/log file")
    parser.add_argument("--backtrace", help="Backtrace string")
    parser.add_argument("--gdb", action="store_true", help="Interactive GDB")
    args = parser.parse_args()

    elf_path = args.elf
    if not os.path.exists(elf_path):
        res = subprocess.run(["find", ".pio/build", "-name", "firmware.elf"], capture_output=True, text=True)
        found_elfs = res.stdout.strip().splitlines()
        if found_elfs:
            elf_path = found_elfs[0]
            print(f"Using ELF found at: {elf_path}")

    if args.arch == "auto":
        args.arch = auto_detect_arch(elf_path)
        print(f"Auto-detected architecture: {args.arch}")

    if args.backtrace:
        decode_backtrace(elf_path, args.backtrace, args.arch)
    elif args.ip:
        dump_file = fetch_coredump(args.ip)
        if dump_file: analyze_binary_dump(elf_path, dump_file, args.arch, args.gdb)
    elif args.file:
        if is_binary(args.file):
            analyze_binary_dump(elf_path, args.file, args.arch, args.gdb)
        else:
            with open(args.file, 'r', errors='ignore') as f:
                content = f.read()
                match = re.search(r'Backtrace:(.*)', content)
                if match: decode_backtrace(elf_path, match.group(1), args.arch)
                else: 
                    if not decode_backtrace(elf_path, content, args.arch):
                        analyze_binary_dump(elf_path, args.file, args.arch, args.gdb)

if __name__ == "__main__":
    main()
