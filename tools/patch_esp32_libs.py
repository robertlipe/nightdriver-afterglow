import os
import sys

lib_pkg = os.path.expanduser("~/.platformio/packages/framework-arduinoespressif32-libs")
esp32_py = os.path.join(lib_pkg, "esp32", "pioarduino-build.py")
esp32s3_py = os.path.join(lib_pkg, "esp32s3", "pioarduino-build.py")

if not os.path.exists(esp32_py) or not os.path.exists(esp32s3_py):
    print("Could not find pioarduino-build.py. This patch is only needed for platform-espressif32 > 5.5.")
    sys.exit(0)

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

s3_clean = [clean_path(p) for p in s3_paths if p.strip()]
esp32_clean = [clean_path(p) for p in esp32_paths if p.strip()]

missing = []
for orig, clean in zip(s3_paths, s3_clean):
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
else:
    print("ESP32 pioarduino-build.py is already fully patched.")
