No no particular order.

DONE Implement std::byteswap
DONE Use Saturation arithmetic 


Salvage doc, ideas code from ~/src/hex and revive hexagon effort.
  * Split gfx for hex QRS addressing into gfxhex.h
  * Distill HTML demos to C++.
  * Weirdly, I think my games matrix effects are here. :-/
  * Create a Q-Bert isometric cube effect for Hex grids.

Mine the AP work from /Users/robertlipe/src/tmp/src/wifi-apA
   On branch wifi-ap
 git remote -v:
  origin	https://github.com/robertlipe/NightDriverStrip.git (fetch)
  origin	https://github.com/robertlipe/NightDriverStrip.git (push)
  upstream	https://github.com/PlummersSoftwareLLC/NightDriverStrip.git (fetch)
  upstream	https://github.com/PlummersSoftwareLLC/NightDriverStrip.git (push)

Mind /Users/robertlipe/src/tmp/src/rpi
  40 branches. :-(

Mine /Users/robertlipe/tmp/src/nightdriverstrip - see if there's anything
  there remaining to be salvaged. Potentially delete.

Mine each of /Users/robertlipe/src/nightdriverstrip/.worktrees
  (Probably each one a project...) for ideas, code worth salvaging.

Mine /Users/robertlipe/src/tinyled -  harvest 'tinyled' work.
  ➜  tinyled git:(tinyled) ✗ git branch --list | wc -l
      41

Mine  /Users/robertlipe/tmp/ld-virgin/NightDriverStrip
  11 branches (looks like low value)

Mine /Users/robertlipe/tmp/src/ndstrip-test
  2 branches (looks like low value)

Finally: Run  locate gfxbase.cpp - find yet OTHER trees to salvage

Low priority
  /Users/robertlipe/src/tmp/src/build - see if anything is worth salvaging.
- [x] Trig Performance Optimization (HexRipple/General): Investigated replacing `std::cos` and `std::sin` with FastLED `cos8`/`sin8` table lookups for performance. Found that modern ESP32 targets (like ESP32-S3) have hardware FPUs that execute trig functions extremely fast (~121 clocks). The minimal speedup from 8-bit table lookups is not worth the loss in precision. Closed as a false positive. Do not implement custom sin/cos.
- [ ] FastLED Tech Debt: Refactor the 'bottom half' of FastLED to address reinterpret_cast warnings/errors.
- [x] sensors.cpp Surgery: Refactor the merge-collision issue in sensors.cpp.
- [ ] FFT Optimization: Evaluate replacing kosme/arduinoFFT with Espressif’s esp-dsp (LX7 PIE optimized).
- [ ] Matrix Effects Modernization (ATMega Legacy / "ESP32-edness"):
    - Review and optimize `*SM*` (SmartMatrix) patterns. Many were originally written for ATMega and lack ESP32 hardware advantages (e.g., single-point hardware floats).
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
