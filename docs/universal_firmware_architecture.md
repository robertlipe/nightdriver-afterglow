# Universal Firmware Architecture: Escaping the Build Matrix

**Date:** August 2026
**Goal:** Reduce 50+ compile-time PlatformIO environments into 2 Universal Binaries (ESP32 and ESP32-S3), dropping total project compile time from ~45 minutes to < 3 minutes.

---

## The Current Problem (Compile-Time Hardware)

Currently, the NightDriver project manages hardware diversity (M5Stick, Heltec, custom PCBs) by passing hardware definitions as compiler flags in `platformio.ini`:

```ini
build_src_flags = -DUSE_SCREEN=1
                  -DTOGGLE_BUTTON_0=37
                  -DM5STICKC=1
```

These flags are intercepted by `include/globals.h` and used in `#ifdef` blocks to selectively compile code. 

**Why this is killing us:**
1. **Cache Destruction:** If `-DM5STICKC=1` is on the global command line, `ccache` treats *every single file* (including the heavy Bluetooth and WiFi stacks) as a completely unique object file. You end up recompiling the exact same ESP-IDF libraries 53 times.
2. **Maintenance Hell:** Adding a new board requires modifying the build system (`platformio.ini`).
3. **OTA Fragmentation:** Releasing a software update requires publishing 53 distinct `.bin` files.

---

## The Solution (Runtime Hardware)

We shift the hardware definition from **Compile-Time** to **Runtime**.

We will compile exactly **two** universal binaries:
1. `nightdriver_esp32_universal.bin`
2. `nightdriver_esp32s3_universal.bin`

These binaries contain the code for *all* features (Screens, HUB75, WS2812, Audio). When the ESP32 boots, it reads a tiny configuration file (e.g., `board_config.json`) from a specific LittleFS or NVS partition to decide *who it is* and *which features to activate*.

### The 3 Pillars of Universal Firmware

#### 1. The Configuration Schema
Instead of macros, we define a universal C++ struct that dictates the hardware topology:

```cpp
struct HardwareConfig {
    bool has_screen;
    int screen_type;      // e.g., OLED_SSD1306, TFT_ST7789
    int led_pin;
    int led_count;
    bool has_audio_in;
    int btn0_pin;
    int btn1_pin;
    // ...
};

extern HardwareConfig g_hwConfig;
```

#### 2. The Bootloader Phase (LittleFS/NVS)
Before initializing FastLED or the Screen, the firmware mounts a small LittleFS partition and reads `board_config.json`:

```json
{
  "board": "helteclorav3demo",
  "has_screen": true,
  "screen_type": "OLED_SSD1306",
  "led_pin": 4,
  "btn0_pin": 0,
  "btn1_pin": -1
}
```
*Note: This doesn't even have to be a partition! It can just be a standard file in the existing data directory.*

#### 3. Dynamic Initialization (if vs #ifdef)
Instead of `#ifdef USE_SCREEN`, the code evaluates the configuration at runtime. 
Because C++ classes like `ScreenImpl` can be allocated dynamically, we only instantiate what the hardware actually possesses:

```cpp
// Old Way:
#ifdef USE_SCREEN
   initScreen();
#endif

// New Way:
if (g_hwConfig.has_screen) {
   g_screen = ScreenFactory::Create(g_hwConfig.screen_type);
   g_screen->Init();
}
```

---

## Action Plan for Refactoring (Future You)

Migrating to a Universal Architecture is a multi-step process. Do not attempt it all at once.

### Phase 1: The Config Struct
1. Create `include/hw_config.h`.
2. Define the `HardwareConfig` struct mapping every `-D` variable currently in `platformio.ini`.
3. Hardcode a global instance: `HardwareConfig g_hwConfig = { ... };` that mimics your main dev board (e.g., pdpwopr).

### Phase 2: Purging `#ifdef`
1. Go through `globals.h`, `main.cpp`, and the `effects/` folders.
2. Replace `#ifdef USE_SCREEN` with `if (g_hwConfig.has_screen)`. 
3. *Note: For heavy library dependencies (like pulling in U8g2), the library will now be compiled into the universal binary, taking up slightly more Flash space. With 4MB/8MB ESP32s, this is entirely negligible.*

### Phase 3: The JSON Parser
1. Integrate ArduinoJson to parse `/board_config.json` from LittleFS during `setup()`.
2. Map the JSON values into `g_hwConfig`.

### Phase 4: The Build System Massacre
1. Delete the 50+ environments in `platformio.ini`.
2. Create just `[env:universal_esp32]` and `[env:universal_esp32s3]`.
3. Create a python deployment script (e.g., `tools/deploy_board.py helteclorav3demo`).
   - The script compiles the universal `.elf` (if it changed).
   - It writes a temporary `board_config.json` for `helteclorav3demo`.
   - It builds a LittleFS image using `mklittlefs`.
   - It calls `esptool.py` to flash the universal `.bin` and the config LittleFS `.bin`.

---

## The Holy Grail
Once completed:
1. **Compile times** drop to a few minutes. 
2. **Adding a new board** is literally just creating a new JSON file in a folder. Zero C++ code changes, zero `platformio.ini` changes.
3. **Over-The-Air (OTA) Updates** become trivial. You push ONE `nightdriver_esp32.bin` to all your devices, and they adapt to their hardware automatically based on their preserved LittleFS config partition.
