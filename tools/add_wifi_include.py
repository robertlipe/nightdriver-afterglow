Import("env")

import os

packages_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs")
if packages_dir:
    mcu = env.BoardConfig().get("build.mcu", "esp32")
    # Some mcus are named differently in the libs folder, e.g. esp32s3 is esp32s3
    wifi_inc = os.path.join(packages_dir, mcu, "include", "esp_wifi", "include")
    if os.path.exists(wifi_inc):
        env.Append(CPPPATH=[wifi_inc])
