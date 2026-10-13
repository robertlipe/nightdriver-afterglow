#!/usr/bin/env python3
"""Restore ESP-IDF include paths that pioarduino forgets for the base esp32 target.

Why this exists
---------------
Since platform-espressif32 5.5 (we pin 55.03.311), the platform generates one
``pioarduino-build.py`` per MCU under
``~/.platformio/packages/framework-arduinoespressif32-libs/<mcu>/``.  The
generator fills in the complete ESP-IDF ``CPPPATH`` for ``esp32s3`` but ships an
``esp32`` variant that is missing dozens of ESP-IDF component include
directories (``esp_wifi``, ``esp_netif``, ``nvs_flash``, ...).  Those headers are
present on disk, so any code that transitively includes them - FastLED reaching
``esp_wifi.h``, or our own users of ``esp_wifi_types.h`` - fails with "No such
file or directory" for no good reason.

What we patch
-------------
Every ``CPPPATH`` entry present for ``esp32s3`` but absent for ``esp32`` is
copied into the ``esp32`` build file, rewriting the target segment
(``"esp32s3"`` -> ``"esp32"``).  The entries are relative to the package, so
this is portable across host OSes.  It is a no-op once pioarduino ships a
correct ``esp32`` include list, at which point this script (and its entry in
``platformio.ini``'s ``extra_scripts``) can be deleted.

Caveat
------
The platform's ComponentManager rewrites ``pioarduino-build.py`` *after* the
``pre:`` extra-script phase, so paths it deliberately strips (for example the
``esp_wifi`` component in a no-WiFi build such as ``demo``) are removed again
and cannot be rescued from here.  This script only restores paths that
ComponentManager would otherwise leave in place.

See also
--------
The unrelated Windows-only failure where pioarduino's toolchain install aborts
under ESP-IDF's ``idf_tools.py`` is handled in ``.github/workflows/CI.yml``
(the ``MSYSTEM`` presence check), not here.
"""

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
else:
    print("ESP32 pioarduino-build.py is already fully patched.")
