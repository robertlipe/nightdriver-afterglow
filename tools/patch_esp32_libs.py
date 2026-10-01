import os
import sys
import glob
import shutil

try:
    from SCons.Script import Import, Builder
    Import("env")
except Exception:
    env = None

def update_toolchain_path_and_shims(env_obj=None):
    home = os.path.expanduser("~")
    pio_dir = os.path.join(home, ".platformio")
    tc_dirs = glob.glob(os.path.join(pio_dir, "packages", "toolchain-*", "bin"), recursive=True)

    github_path = os.environ.get("GITHUB_PATH")

    for tc_bin in tc_dirs:
        if not os.path.exists(tc_bin):
            continue

        native_bin = os.path.normpath(tc_bin)

        # 1. Prepend to SCons environment PATH
        if env_obj is not None and "ENV" in env_obj:
            scons_path = env_obj["ENV"].get("PATH", "")
            if native_bin not in scons_path and tc_bin not in scons_path:
                env_obj["ENV"]["PATH"] = native_bin + os.pathsep + scons_path

        # 2. Prepend to OS environment PATH
        os_path = os.environ.get("PATH", "")
        if native_bin not in os_path and tc_bin not in os_path:
            os.environ["PATH"] = native_bin + os.pathsep + os_path

        # 3. Append to GITHUB_PATH for subsequent workflow steps
        if github_path and os.path.exists(github_path):
            try:
                with open(github_path, "a") as f:
                    f.write(native_bin + "\n")
            except Exception:
                pass

        # 4. Create executable shims
        for name in os.listdir(tc_bin):
            if "objcopy" in name.lower():
                src_path = os.path.join(tc_bin, name)
                if not os.path.isfile(src_path):
                    continue

                base_names = [
                    "xtensa-esp32-elf-objcopy",
                    "xtensa-esp32s2-elf-objcopy",
                    "xtensa-esp32s3-elf-objcopy",
                    "xtensa-esp-elf-objcopy",
                    "riscv32-esp-elf-objcopy",
                    "riscv32-esp32c3-elf-objcopy",
                    "riscv32-esp32c6-elf-objcopy",
                    "riscv32-esp32h2-elf-objcopy",
                ]

                for base in base_names:
                    for ext in (["", ".exe"] if os.name == "nt" or sys.platform == "win32" or name.endswith(".exe") else [""]):
                        target_alias = base + ext
                        target_path = os.path.join(tc_bin, target_alias)
                        if not os.path.exists(target_path):
                            try:
                                shutil.copy2(src_path, target_path)
                                print(f"[Patch-Libs] Created toolchain executable shim: {target_alias}")
                            except Exception as e:
                                print(f"[Patch-Libs] Warning: Failed to copy {target_alias}: {e}")

def fix_scons_txttobin(env_obj):
    if env_obj is None or "BUILDERS" not in env_obj:
        return

    board = env_obj.BoardConfig()
    mcu = board.get("build.mcu", "esp32")
    is_xtensa = mcu in ("esp32", "esp32s2", "esp32s3")

    objcopy_cmd = "riscv32-esp-elf-objcopy" if not is_xtensa else "xtensa-esp-elf-objcopy"
    target_arch = "elf32-littleriscv" if not is_xtensa else "elf32-xtensa-le"
    binary_arch = "riscv" if not is_xtensa else "xtensa"

    scons_path = env_obj["ENV"].get("PATH", os.environ.get("PATH", ""))
    resolved = shutil.which(objcopy_cmd, path=scons_path) or shutil.which(objcopy_cmd + ".exe", path=scons_path)

    if not resolved:
        candidates = [
            "xtensa-esp32-elf-objcopy",
            "xtensa-esp32s3-elf-objcopy",
            "xtensa-esp-elf-objcopy"
        ] if is_xtensa else [
            "riscv32-esp-elf-objcopy",
            "riscv32-esp32c3-elf-objcopy",
            "riscv32-esp32c6-elf-objcopy",
            "riscv32-esp32h2-elf-objcopy"
        ]
        for candidate in candidates:
            resolved = shutil.which(candidate, path=scons_path) or shutil.which(candidate + ".exe", path=scons_path)
            if resolved:
                break

    if resolved:
        resolved = os.path.normpath(resolved)

    final_objcopy = f'"{resolved}"' if resolved else objcopy_cmd

    cmd_str = " ".join([
        final_objcopy,
        "--input-target", "binary",
        "--output-target", target_arch,
        "--binary-architecture", binary_arch,
        "--rename-section", ".data=.rodata.embedded",
        "$SOURCE", "$TARGET"
    ])

    env_obj["BUILDERS"]["TxtToBin"] = Builder(
        action=env_obj.VerboseAction(cmd_str, "Converting $TARGET"),
        suffix=".txt.o"
    )
    print(f"[Patch-Libs] Successfully configured TxtToBin builder with objcopy: {final_objcopy}")

def patch_all(env_obj=None):
    home = os.path.expanduser("~")
    pio_dir = os.path.join(home, ".platformio")

    update_toolchain_path_and_shims(env_obj)

    if env_obj is not None:
        fix_scons_txttobin(env_obj)

    if not os.path.exists(pio_dir):
        return

    # 1. Patch _embed_files.py on disk
    for embed_py in glob.glob(os.path.join(pio_dir, "**", "_embed_files.py"), recursive=True):
        try:
            with open(embed_py, "r") as f:
                content = f.read()
            target_str = 'f"xtensa-{mcu}-elf-objcopy"'
            if target_str in content:
                print(f"Patching _embed_files.py at {embed_py}...")
                content = content.replace(target_str, '"xtensa-esp-elf-objcopy.exe" if os.name == "nt" else "xtensa-esp-elf-objcopy"')
                with open(embed_py, "w") as f:
                    f.write(content)
                print("_embed_files.py patched successfully.")
        except Exception as e:
            print(f"Warning: Failed to patch {embed_py}: {e}")

    # 2. Patch missing ESP32 CPPPATH entries in pioarduino-build.py
    for lib_dir in glob.glob(os.path.join(pio_dir, "packages", "framework-arduinoespressif32-libs*"), recursive=True):
        esp32_py = os.path.join(lib_dir, "esp32", "pioarduino-build.py")
        esp32s3_py = os.path.join(lib_dir, "esp32s3", "pioarduino-build.py")

        if os.path.exists(esp32_py) and os.path.exists(esp32s3_py):
            def get_cpppath(filepath):
                with open(filepath, 'r') as f:
                    content = f.read()

                start = content.find("CPPPATH=[")
                if start == -1: return []
                end = content.find("]", start)
                return content[start:end].split('\n')

            s3_paths = get_cpppath(esp32s3_py)
            esp32_paths = get_cpppath(esp32_py)

            def clean_path(p):
                return p.strip().replace('"esp32s3"', '"{board}"').replace('"esp32"', '"{board}"').strip(',')

            esp32_clean = set(clean_path(p) for p in esp32_paths if p.strip())

            missing = []
            for orig in s3_paths:
                if not orig.strip():
                    continue
                clean = clean_path(orig)
                if clean not in esp32_clean and clean != "CPPPATH=[":
                    missing.append(orig.replace('"esp32s3"', '"esp32"'))

            if missing:
                print(f"Patching {len(missing)} missing ESP-IDF include paths into {esp32_py}...")
                with open(esp32_py, 'r') as f:
                    content = f.read()
                start = content.find("CPPPATH=[")
                bracket_idx = content.find("[", start)
                new_content = content[:bracket_idx+1] + "\n" + "\n".join(missing) + content[bracket_idx+1:]
                with open(esp32_py, 'w') as f:
                    f.write(new_content)
                print("pioarduino-build.py patched successfully.")

    # 3. Patch platform-espressif32's component_manager.py to prevent stripping critical network includes (e.g. esp_wifi)
    for cm_py in glob.glob(os.path.join(pio_dir, "**", "component_manager.py"), recursive=True):
        try:
            with open(cm_py, "r") as f:
                cm_content = f.read()

            target_str = "'lwip',           # Network stack"
            if target_str in cm_content and "'esp_wifi'" not in cm_content:
                print(f"Patching component_manager.py at {cm_py} to protect esp_wifi...")
                replacement = "'esp_wifi',        # WiFi stack\n            'esp_netif',       # Netif stack\n            'lwip',           # Network stack"
                cm_content = cm_content.replace(target_str, replacement)

            map_str = "'wifi': 'esp_wifi',"
            if map_str in cm_content:
                cm_content = cm_content.replace(map_str, "# 'wifi': 'esp_wifi',")

            with open(cm_py, "w") as f:
                f.write(cm_content)
            print("component_manager.py patched successfully.")
        except Exception as e:
            print(f"Warning: Failed to patch {cm_py}: {e}")

patch_all(env)
