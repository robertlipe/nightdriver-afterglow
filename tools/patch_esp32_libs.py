import os
import sys
import glob
import shutil

try:
    from SCons.Script import Import
    Import("env")
except Exception:
    env = None

def patch_all(env_obj=None):
    home = os.path.expanduser("~")
    pio_dir = os.path.join(home, ".platformio")

    if not os.path.exists(pio_dir):
        return

    # 1. Toolchain shims and PATH environment setup
    tc_dirs = glob.glob(os.path.join(pio_dir, "packages", "toolchain-*", "bin"), recursive=True)
    for tc_bin in tc_dirs:
        if not os.path.exists(tc_bin):
            continue

        if env_obj is not None and "ENV" in env_obj:
            if tc_bin not in env_obj["ENV"].get("PATH", ""):
                env_obj["ENV"]["PATH"] = tc_bin + os.pathsep + env_obj["ENV"].get("PATH", "")
        if tc_bin not in os.environ.get("PATH", ""):
            os.environ["PATH"] = tc_bin + os.pathsep + os.environ.get("PATH", "")

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
                    for ext in ["", ".exe"]:
                        target_alias = base + ext
                        target_path = os.path.join(tc_bin, target_alias)
                        if not os.path.exists(target_path):
                            try:
                                shutil.copy2(src_path, target_path)
                                print(f"[Patch-Libs] Created toolchain executable shim: {target_alias}")
                            except Exception as e:
                                print(f"[Patch-Libs] Warning: Failed to copy {target_alias}: {e}")

    if env_obj is not None and env_obj.get("OBJCOPY"):
        objcopy_cmd = env_obj.get("OBJCOPY")
        resolved = shutil.which(objcopy_cmd, path=env_obj["ENV"].get("PATH", ""))
        if resolved:
            env_obj["OBJCOPY"] = resolved

    # 2. Patch _embed_files.py to replace hardcoded xtensa-{mcu}-elf-objcopy with generic xtensa-esp-elf-objcopy
    for embed_py in glob.glob(os.path.join(pio_dir, "**", "_embed_files.py"), recursive=True):
        try:
            with open(embed_py, "r") as f:
                content = f.read()
            target_str = 'f"xtensa-{mcu}-elf-objcopy"'
            if target_str in content:
                print(f"Patching _embed_files.py at {embed_py}...")
                content = content.replace(target_str, '"xtensa-esp-elf-objcopy"')
                with open(embed_py, "w") as f:
                    f.write(content)
                print("_embed_files.py patched successfully.")
        except Exception as e:
            print(f"Warning: Failed to patch {embed_py}: {e}")

    # 3. Patch missing ESP32 CPPPATH entries in pioarduino-build.py
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

    # 4. Patch platform-espressif32's component_manager.py to prevent stripping critical network includes (e.g. esp_wifi)
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
