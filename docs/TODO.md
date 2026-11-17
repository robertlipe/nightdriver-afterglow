No no particular order.

DONE Implement std::byteswap
DONE Use Saturation arithmetic

Salvage doc, ideas code from ~/src/hex and revive hexagon effort.
  * DONE Split gfx for hex QRS addressing into gfxhex.h
  * Distill HTML demos to C++.
  * DONE Weirdly, I think my games matrix effects are here. :-/
  * Create a Q-Bert isometric cube effect for Hex grids.
    (see docs/qbert.md)

DONE Mine the AP work from /Users/robertlipe/src/tmp/src/wifi-ap
  - WiFi AP robustness & captive portal fully incorporated into afterglow
    (see docs/Wifi_Configuration_AP_design.md).
  - Runtime partition dumper and directory listing absorbed into debug_cli.cpp (`partitions`, `ls`).
  - Remaining files are build logs and Dec 2025 AI handover notes. Ready to delete.

DONE Mine /Users/robertlipe/src/tmp/src/rpi
  Audited all 40 branches against afterglow/main. All code upstreamed or superseded.
  - Salvaged `include/effects/matrix/PatternCyclicCA.h` (Greenberg-Hastings & Cyclic CA).
    TODO: Modernize, optimize with modern tools, and evaluate for matrix displays.
  - LightCrafter-16: 16-channel, 80A board.
    SALVAGED from .worktrees/ld16:
    * `docs/16ch_led_strip_driver.pdf` (hardware schematic!)
    * `docs/lightcrafter16.md` (complete hardware notes, IO0/46 mux gotcha on GPIO 3, U6 transceiver debug LED, APA102 analysis)
    * `custom_ld16.in_` (pin mappings for LED0-15, W5500 SPI Ethernet, IR, Voltage/Current ADC)
    TODO: Integrate 16-channel `ws281xgfx.cpp` expansion (`LEDS_AS_MATRIX` & channels 8-15) when hardware is tested.

DONE Mine /Users/robertlipe/tmp/src/nightdriverstrip
  - Audited against afterglow/main. Head commit 6c7a7f8d is an ancestor of afterglow.
  - mpdma.md thoughts fully implemented via src/espmpdmagfx.cpp and docs/HUB75-modernization.md.
  - array_utils removal and std::array cleanups are already in afterglow.
  - Ready to delete.

DONE Mine each of /Users/robertlipe/src/nightdriverstrip/.worktrees
  - `ld16`: HARVESTED! Salvaged schematic, custom_ld16.in_, lightcrafter16.md, and 16-channel patch.
  - `arduino3-exp`: Arduino 3.0 / objcopy / FreeRTOS changes superseded and in afterglow.
  - `channels`: Stale commit 6763b775 (merged) with deleted 3D assets.
  - `m5tab-staging`: Magic static getPolarMap, SIOF fixes, and salvage scripts all in afterglow.
  - `refactor-headers`: PR #823/#825 IRAM/DRAM attributes and clean headers already merged.
  - `remotedebug-pr`: Stale merge conflict from replace-remotedebug, superseded.
  All worktrees in .worktrees/ are safely cleared for pruning.

Mine /Users/robertlipe/src/tinyled -  harvest 'tinyled' work.
  ➜  tinyled git:(tinyled) ✗ git branch --list | wc -l
      41

DONE Mine /Users/robertlipe/tmp/ld-virgin/NightDriverStrip
  - Audited against afterglow/main. Stale 2023 head (commit 71af2306).
  - Untracked files were mock 2022 RapidAPI sports JSON and an empty PatternRABaseball stub.
  - Zero value, ready to delete.

DONE Mine /Users/robertlipe/tmp/src/ndstrip-test
  - Audited against afterglow/main. Stale early 2024 clone (v1.1.1 tag).
  - Diff was minor IR remote logging tweaks that were superseded long ago.
  - Zero value, ready to delete.

Finally: Run  locate gfxbase.cpp - find yet OTHER trees to salvage

DONE /Users/robertlipe/src/tmp/src/build
  - Dec/Jan Ninja/CMake build system refactor experiment.
  - Succeeded in generating demo build.ninja via log extraction, but proved to be
    an unmaintainable dead end.
  - Superseded by cleaning up platformio.ini targets (12 active envs, 2-min CI). Ready to delete.
- [x] Trig Performance Optimization (HexRipple/General): Investigated replacing `std::cos` and `std::sin` with FastLED `cos8`/`sin8` table lookups for performance. Found that modern ESP32 targets (like ESP32-S3) have hardware FPUs that execute trig functions extremely fast (~121 clocks). The minimal speedup from 8-bit table lookups is not worth the loss in precision. Closed as a false positive. Do not implement custom sin/cos.
- [ ] FastLED Tech Debt: Refactor the 'bottom half' of FastLED to address reinterpret_cast warnings/errors.
- [x] sensors.cpp Surgery: Refactor the merge-collision issue in sensors.cpp.
- [ ] FFT Optimization: Evaluate replacing kosme/arduinoFFT with Espressif’s esp-dsp (LX7 PIE optimized).
- [ ] Matrix Effects Modernization (ATMega Legacy / "ESP32-edness"):
    - Review and optimize `*SM*` (SoulMate) patterns. Many were originally written for ATMega and lack ESP32 hardware advantages (e.g., single-point hardware floats).
    - Refactor heavy effects (`PatternLife`, `PatternSMStarDeep`, `PatternSMFire2021/2012`, `PatternSMNoise*`) where appropriate.
    - Evaluate `PatternSMStarDeep` for leveraging precomputed radial math from `gfxbase` (or `gfxmatrix`) to remove heavy per-frame trig calculations.
    - Standardize array flipping: use `std::swap` (or similar standard algorithms) instead of manual buffer copying in heavy effects like `PatternLife`.
    - Evaluate bounds checking, wrapping logic, and `uint8_t` iterators vs `int`. Safe pixel-setting methods are ideal, but if `g()->drawPixel` harms 60fps performance on hot paths, explore optimized manual bounds-checking and wrapping alternatives to mitigate the risks of unchecked array access (`g()->leds[XY(...)]`).
- [ ] Core Codebase Modernization (src/ & include/):
    - **C++20/C++26 Features**: With the solid move to Arduino3 (IDF5) and looking toward IDF6, deeply integrate modern C++ features. Explore `std::ranges` or `<algorithm>` to replace manual `for`/`while` loops where appropriate, to improve readability and safety.
    - **Constants Standardization**: Replace legacy math macros (like `PI`, `M_PI`, `TWO_PI`) spread across `PatternCube.h`, `PatternSMTixyLand.h`, `Vector.h`, etc., with modern `<numbers>` equivalents like `std::numbers::pi` or `std::numbers::pi_v<float>`.
    - **Data Structures**: Convert raw C-style arrays (e.g., `int rules[4]`, `int digits[5]`, `Point screen[8]`, `float ReelPos[NUM_FANS]`) to `std::array` to benefit from standard iterators, bounds-checking in debug mode, and better interoperability with C++ algorithms.
    - **Memory Management**: Audit occurrences of manual `memset` and `memcpy` (found in `hub75gfx.cpp`, `telnetserver.cpp`, `socketserver.cpp`, `ledbuffer.cpp`, and various effects). Where possible, migrate to `std::fill`, `std::copy`, or rely on zero-initialization semantics of modern C++ structures and smart pointers. Evaluate if LVGL config (`lv_conf.h`) should continue preferring standard C functions or be customized.
    - **Dead Code / Format Cleanup**: Review the extensive use of `auto p = ...` and raw `new` calls in network/socket code for potential leaks or migration to `std::make_unique`/`std::make_shared`. Evaluate `<fmt>` alternatives or optimizations to mitigate flash space bloat.
- [x] Remove legacy IDF4 / Arduino2 code:
    - `IS_IDF5` is now effectively a constant `true` since we no longer support IDF4 (having moved to Arduino3).
    - Remove `IS_IDF5` macros and all the code in the `#else` (IDF4) paths. Affected files include `remotecontrol.cpp`, `soundanalyzer.cpp`, and `soundanalyzer.h`.
    - Remove includes of `esp_idf_version.h` and checks for `ESP_IDF_VERSION` (except in vendored code like `amoled/`).

Build system / platformio.ini  (recorded 2026-10-13, after the esp-hub75 move)

- [ ] Split the monolithic `platformio.ini` into `custom_*.ini` includes
      ("custom_$EVERYTHING.ini") so each env composes hardware + capability
      sections instead of duplicating flags. Design north star:
      `docs/universal_firmware_architecture.md` (aspirational, not a plan of record).
- [ ] Cut environment count and cold-build time. Same memo.
- [ ] esp-hub75 hardware bring-up on `ws_s3_hub75-64x64` (Waveshare
      ESP32-S3-RGB-Matrix). Software side is green on esp-hub75 (PR #97); this is
      blocked on the RMA'd board.
- [ ] `dev_ws_s3_hub75` declares `board_build.arduino.memory_type = opi_opi`, but
      the comment above the env says the offboard PSRAM is QSPI. Confirm against
      the board; likely `opi_qspi`.
- [ ] Triage `m5stackdemo` and `ledstrip_feather_wrover`: presumed broken (never
      run by the maintainer), no verdict yet. Fix or delete.
- [ ] Triage stale remote branches: `fix/ci-windows-and-fastled-pin`,
      `jules-fix-windows-ci-parallel-*`, `node`, `robertlipe-patch-1`.
