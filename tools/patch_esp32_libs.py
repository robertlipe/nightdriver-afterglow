import os
import sys
import shutil

def ensure_objcopy_shims():
    home = os.path.expanduser("~")
    possible_dirs = [
        os.path.join(home, ".platformio", "packages", "toolchain-xtensa-esp-elf", "bin"),
        os.path.join(home, ".platformio", "packages", "toolchain-xtensa-esp32s3-elf", "bin"),
        os.path.join(home, ".platformio", "packages", "toolchain-xtensa-esp32s2-elf", "bin"),
    ]

    for tc_bin in possible_dirs:
        if not os.path.exists(tc_bin):
            continue

        for name in os.listdir(tc_bin):
            if "objcopy" in name:
                src_path = os.path.join(tc_bin, name)
                ext = ".exe" if name.endswith(".exe") else ""
                aliases = [
                    f"xtensa-esp32-elf-objcopy{ext}",
                    f"xtensa-esp32s2-elf-objcopy{ext}",
                    f"xtensa-esp32s3-elf-objcopy{ext}",
                    f"xtensa-esp-elf-objcopy{ext}",
                ]
                for alias in aliases:
                    target_path = os.path.join(tc_bin, alias)
                    if not os.path.exists(target_path):
                        try:
                            shutil.copy2(src_path, target_path)
                            print(f"[Patch-Libs] Created missing toolchain executable shim: {alias}")
                        except Exception as e:
                            print(f"[Patch-Libs] Warning: Failed to copy {alias}: {e}")

ensure_objcopy_shims()

# 1. Patch missing ESP32 CPPPATH entries in framework-arduinoespressif32-libs
lib_pkg = os.path.expanduser("~/.platformio/packages/framework-arduinoespressif32-libs")
esp32_py = os.path.join(lib_pkg, "esp32", "pioarduino-build.py")
esp32s3_py = os.path.join(lib_pkg, "esp32s3", "pioarduino-build.py")

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
        print(f"Patching {len(missing)} missing ESP-IDF include paths into esp32/pioarduino-build.py...")
        with open(esp32_py, 'r') as f:
            content = f.read()
        start = content.find("CPPPATH=[")
        bracket_idx = content.find("[", start)
        new_content = content[:bracket_idx+1] + "\n" + "\n".join(missing) + content[bracket_idx+1:]
        with open(esp32_py, 'w') as f:
            f.write(new_content)
        print("Patch applied successfully.")

# 2. Patch platform-espressif32's component_manager.py to prevent stripping critical network includes (e.g. esp_wifi)
platform_cm_py = os.path.expanduser("~/.platformio/platforms/espressif32/builder/frameworks/component_manager.py")

if os.path.exists(platform_cm_py):
    with open(platform_cm_py, 'r') as f:
        cm_content = f.read()

    # Modify _critical_components set
    target_str = "'lwip',           # Network stack"
    if target_str in cm_content and "'esp_wifi'" not in cm_content:
        print("Patching component_manager.py to protect esp_wifi from lib_ignore stripping...")
        replacement = "'esp_wifi',        # WiFi stack\n            'esp_netif',       # Netif stack\n            'lwip',           # Network stack"
        cm_content = cm_content.replace(target_str, replacement)

    # Modify extended_mapping dict to prevent mapping 'wifi' -> 'esp_wifi'
    map_str = "'wifi': 'esp_wifi',"
    if map_str in cm_content:
        print("Patching component_manager.py extended_mapping for wifi...")
        cm_content = cm_content.replace(map_str, "# 'wifi': 'esp_wifi',")

    with open(platform_cm_py, 'w') as f:
        f.write(cm_content)
    print("component_manager.py patched successfully.")
