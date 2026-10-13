# Universal Firmware Architecture: Escaping the Build Matrix

**Date:** August 2026
**Status:** Aspirational design memo. Nothing here is scheduled work and nothing has been prototyped.
**Revised:** October 2026 — reality-checked against measured flash budgets and FastLED's compile-time constraints. See *Known Obstacles*.

**Aspiration:** collapse the 50+ compile-time PlatformIO environments into a small
number of runtime-configured binaries, so a full-matrix build drops from ~45
minutes to a few.

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
1. **Cache Destruction:** If `-DM5STICKC=1` is on the global command line, `ccache` treats *every single file* (including the heavy Bluetooth and WiFi stacks) as a completely unique object file. You end up recompiling the exact same ESP-IDF libraries once per environment — 49 of them today. *(This is the load-bearing complaint. Universal firmware is one answer to it; building fewer environments is another, cheaper one.)*
2. **Maintenance Hell:** Adding a new board requires modifying the build system (`platformio.ini`).
3. **OTA Fragmentation:** Releasing a software update requires publishing a distinct `.bin` per environment.

As of October 2026, `platformio.ini` plus `custom_*.ini` declare **49** environments.

---

## The Aspiration (Runtime Hardware)

Shift the hardware definition from **Compile-Time** to **Runtime**.

The aspiration is to compile one universal binary per architecture family:

1. `nightdriver_esp32_universal.bin` (LX6)
2. `nightdriver_esp32s3_universal.bin` (LX7)
3. `nightdriver_esp32c3_universal.bin` (and other RISC-V parts as needed: C6, P4, ...)

**Honest caveat:** "one binary per architecture family" is too aggressive, but it's convenient framing for this consideration. A single-core RISC-V C6 like `kitch` driving one WS2812 strip has almost nothing in common with an ESP32-S3 pushing a HUB75 panel through a large pile of matrix effects. The realistic end state is likely a *handful* of binaries grouped by capability tier — "strip + audio", "matrix/HUB75", "screen + buttons" — with the ISA as one axis among several rather than the definition of the family. So it's not "3", but it's closer to three than to 49.

These binaries contain the code for *all* features (Screens, HUB75, WS2812, Audio). When the ESP32 boots, it reads a tiny configuration file (e.g., `board_config.json`) from a specific LittleFS or NVS partition to decide *who it is* and *which features to activate*.

### The 3 Pillars of Universal Firmware

#### 1. The Configuration Schema
Instead of macros, we define a universal C++ struct that dictates the hardware topology:

```cpp
struct HardwareConfig {
    bool has_screen;
    std::optional<ScreenType> screen_type;   // e.g., OLED_SSD1306, TFT_ST7789
    int led_pin;
    int led_count;             // These might be repeating, like an array for multi-strip
    bool has_audio_in;
    std::optional<int> i2s_audio_pin;
    std::optional<int> i2s_bclk_pin;
    std::optional<int> i2s_ws_pin;
    int btn0_pin;
    int btn1_pin;
    // RGB configurations for HUB75
    // temperature sensor types, configs // Consult src/sensors.cpp and src/espmpdmagfx.cpp, which are currently growing gardens of configuration weeds, working hard to NOT pollute platformio.ini and/or globals.h as awareness of the issues discussed here.
    // orientation sensors, pinouts
    // ...
};

extern HardwareConfig g_hwConfig;
```

Aside: those familiar with the Google Way may find these spellings influenced by protobufs throughout. They certainly are. Whether they're a proto, a JSON, or a C struct is intentionally kept loose at this point.

> `led_pin` is the one field that does not work as written. See *Known Obstacles #2*.

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
*Note: This doesn't even have to be a partition! It can just be a standard file in the existing data directory. Note also that this sketch mixes conventions — the struct expresses "absent" with `std::optional` while the JSON uses `-1`. Pick one before implementing.*

#### 3. Dynamic Initialization (if vs #ifdef)
Instead of `#ifdef USE_SCREEN`, the code evaluates the configuration at runtime.
Because C++ classes like `ScreenImpl` can be — and often are — allocated dynamically, we only instantiate what the hardware actually possesses:

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

## Known Obstacles (Reality Check)

Two constraints measured since this memo was first written. Neither kills the idea; both change its shape.

### 1. Flash is not negligible

The first draft assumed that pulling every optional library into one image would cost "slightly more Flash space ... entirely negligible" on 4MB/8MB boards. Measurement says otherwise.

Retiring C++ exceptions alone freed **153 KB** on `kitch` — a 4 MB, OTA-updating C6 — which is roughly 9% of its app slot. That board had *zero* real headroom at the time: PlatformIO's size report read "95.4%" while the actual app image was **102.9%** of the partition. The report omits `.eh_frame`, which is allocated and genuinely is flashed. The image had been silently spilling into the empty second OTA slot, which means no OTA update could ever have fitted it. It's our current known worst case.

A universal binary compiles *every* feature set into *every* image, and any target that keeps OTA pays for two app slots. The flash budget has to lead this design, not follow it. Note that `tools/merge_image.py` now fails the build when an app image does not fit its partition — that guard exists because of exactly this class of surprise, and any experiment in this direction has to live within it.

This reality is where our "more than three" comes into play. A C6 strip design already bursting at the seams has no room to bring seventy matrix effects along for the ride. But devices with 8MB or 16MB (or more!) of flash are common in our world, so an 8MB board feeding multiple 16x16 WS2812 grids certainly will need those effects. Realistically that means an 8MB device, or one that gives up OTA.

### 2. FastLED pins are compile-time

`FastLED.addLeds<CHIPSET, PIN, ORDER>(...)` takes the chipset, the data pin, *and* the color order as **template** arguments, so a runtime `g_hwConfig.led_pin` cannot drive it. This is the blocker that stalled the first attempt. It's also one of our motivations for replacing FastLED's *driver*, probably with something runtime-pinnable like the ESP-IDF [`led_strip`](https://github.com/espressif/idf-extra-components/tree/master/led_strip) component.

Scope matters here, and it is better news than it sounds: the driver is invoked from exactly two places (`src/ws281xgfx.cpp`, plus one onboard-pixel case in `src/drawing.cpp`), while FastLED's *types and helpers* — `CRGB`, `CHSV`, `blur1d`, `lib8tion` — reach well over a hundred files. Replacing the driver is tractable; replacing the library would be neither cheap nor useful.

In practice, the residual compile-time axis is the *LED output signature*: chipset, pin, and color order. Everything else — led count, brightness, audio configuration, screen type, buttons, effect sets — can be runtime. The options, none of them free:

1. Compile in a fixed set of plausible pins and `switch` on the config to select one.
2. Replace FastLED's driver layer with something that accepts a runtime pin.
3. Accept the LED output signature as the one thing that stays compile-time, and universalize everything else. Probably the best value per unit of risk.

---

## Action Plan for Refactoring (Future Work)

Migrating to a Universal Architecture is a multi-step process. Do not attempt it all at once.

### Phase 0: The Cheap 80% (do this first, regardless)

Most of the build-time pain does not require universal firmware at all. The matrix is padded with environments for hardware that is no longer in service, and each one costs a full compile in CI and on every bare `pio run`. Deleting dead environments is a small, reversible, unglamorous change that recovers much of the ~45 minutes and shrinks the surface any later work has to cover. Do it before, and independently of, everything below. Even just moving them out of the default build would be a build-time win, and it can be had today without touching `platformio.ini`: ship the definitions in a file the `custom_*.ini` glob does *not* match — e.g. `custom_foo.ini_` — and "turn those environments on" by renaming it to `custom_foo.ini`. (Parking them in a file the glob *does* match buys nothing: `extra_configs` globs `custom_*.ini` in, and `default_envs` is empty, which means "build everything". An explicit `default_envs` list is the other lever; excluded environments stay reachable on demand with `-e` once their file is in place.)

### Phase 1: The Config Struct
1. Create `include/hw_config.h`.
2. Define the `HardwareConfig` struct mapping every `-D` variable currently in `platformio.ini`.
3. Hardcode a global instance: `HardwareConfig g_hwConfig = { ... };` that mimics your main dev board (e.g., pdpwopr).

### Phase 2: Purging `#ifdef`
1. Go through `globals.h`, `main.cpp`, and the `effects/` folders.
2. Replace `#ifdef USE_SCREEN` with `if (g_hwConfig.has_screen)`.
3. *Note: heavy library dependencies (U8g2, HUB75, audio) now compile into every image. An earlier draft of this memo called that "entirely negligible"; see Known Obstacles #1. Audit the flash budget per target before committing to this phase.*

Note: since this was drafted, large inroads have been made into moving board configuration OUT of `globals.h` and into per-environment build flags in `platformio.ini`.

### Phase 3: The JSON Parser
1. Integrate ArduinoJson to parse `/board_config.json` from LittleFS during `setup()`.
2. Map the JSON values into `g_hwConfig`.

### Phase 4: Adding Infrastructure
1. We probably need rich data types, like a `std::variant`.
2. These probably have observers that are called on change. Most of the things that allow the above steps also would allow for runtime configuration changes via REST calls from a Web interface. Changing a pin count from a web interface is a reasonable expectation in modern times.
3. These have somewhat awkward constraints because persisting these might schedule a file write and touching flash from protected context in ESP32 is a hazardous process.

### Phase 5: The Build System Massacre
1. Delete the environments that are provably dead (see Phase 0), then reduce what remains to a handful of runtime-configured targets.
2. Create the universal environments — one per capability tier actually chosen, not blindly one per ISA.
3. Create a python deployment script (e.g., `tools/deploy_board.py helteclorav3demo`).
   - The script compiles the universal `.elf` (if it changed).
   - It writes a temporary `board_config.json` for `helteclorav3demo`.
   - It builds a LittleFS image using `mklittlefs`.
   - It calls `esptool.py` to flash the universal `.bin` and the config LittleFS `.bin`.

---

## The Holy Grail

If this ever lands:
1. **Compile times** drop to a few minutes.
2. **Adding a new board** is literally just creating a new JSON file in a folder. Zero C++ code changes, zero `platformio.ini` changes.
3. **Over-The-Air (OTA) Updates** become trivial. You push one binary per family/tier to all your devices, and they adapt to their hardware automatically based on their preserved LittleFS config partition.
