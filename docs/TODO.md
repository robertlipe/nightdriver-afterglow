# NightDriver Afterglow TODO & Roadmap

## 1. Hexagon & Q*bert (`~/src/hex`)
* [x] **Audit & salvage `~/src/hex`**: Synthesized complete architecture guide ([docs/hex/hex_architecture.md](hex/hex_architecture.md)) and rescued browser rapid-prototyping sandbox ([docs/hex/hexproto.html](hex/hexproto.html)). Ready to retire `~/src/hex`.
* [ ] Implement isometric Q*bert arcade simulation on 271-pixel hex grid (per [docs/qbert.md](qbert.md)).

## 2. Hardware Bring-up & Multi-Channel
* [ ] **LightCrafter-16 Bring-up**: When 80A hardware is on the bench, apply [docs/ws281xgfx_16channels.patch](ws281xgfx_16channels.patch), enable `custom_ld16.in_`, and verify GPIO 3 / `WIZ_RESET_PIN` bootstrap behavior across all 16 channels.
* [x] **Waveshare ESP32-S3-RGB-Matrix** (`ws_s3_hub75-64x64`): Complete esp-hub75 bring-up once the RMA board arrives.
* [x] **Memory type check**: Verified `dev_ws_s3_hub75` memory type (`opi_opi`) against schematic (ESP32-S3-WROOM-2-N32R16V has 32MB Octal Flash & 16MB Octal PSRAM). Updated comments in `platformio.ini`.

## 3. Effects Modernization & Engine Optimization
* [x] **PatternCyclicCA (Scrubbed)**: Retired from active roadmap. Boundary absorption on low-res matrices collapses spiral rotors into diagonal wipes; code preserved in git history.
* [ ] **SoulMate (`*SM*`) & Legacy Matrix Effects**:
  * [x] Audit ATMega-era code: matrix render paths are 100% single-precision `float`/fixed-point FPU math.
  * [x] Optimize `PatternLife`: eliminated 65,536 modulo divisions per frame via branchless coordinate wrapping; converted generational transitions to $O(1)$ ping-pong buffer toggles; removed dead 4,096-iteration copy loop and redundant CRC allocation.
  * [x] Optimize `PatternSMStarDeep`: converted 8-bit `sin8()`/`cos8()` float pipeline to integer fixed-point math; halved inner vertex trig calculations; added $O(1)$ bounding-box clipping in `DrawStarLine()`.
  * [ ] Refactor remaining heavy patterns (`PatternSMFire2021/2012`, `PatternSMNoise*`).
  * [ ] Audit bounds checking & loop variable underflows (`uint8_t` iterators vs native `int`).
* [ ] **Audio/FFT Optimization**: Evaluate replacing `kosme/arduinoFFT` with Espressif’s `esp-dsp` (leveraging ESP32-S3 LX7 PIE vector instructions).
* [x] **FastLED Tech Debt**: Clean up `reinterpret_cast` warnings/errors in the LED controller bottom-half.

## 4. Core Firmware Modernization (C++26 Standards)
* [x] **Constants**: Replaced legacy math macros (`PI`, `M_PI`, `TWO_PI`) with `<numbers>` (`std::numbers::pi_v<float>`).
* [ ] **Data Structures**: Migrate raw C arrays to `std::array` across effects and math classes for bounds safety and range support.
* [ ] **Memory Operations**: Audit manual `memset`/`memcpy` in network/display buffers; migrate to `std::fill`, `std::copy`, or value initialization.
* [ ] **Ownership / Allocations**: Review raw `new` and `auto p = ...` in network/socket code for migration to `std::make_unique`.

## 5. PlatformIO & Build Hygiene
* [x] Modularize `platformio.ini` using `custom_*.ini` includes to compose hardware + capability flags.
* [x] Prune redundant environments and reduce cold-build times.
* [x] **Triage abandoned boards**: `m5stackdemo` and `ledstrip_feather_wrover` are already safely parked in `custom_legacy_props.in_` outside the active build matrix.
* [x] **Triage stale remote branches**: Pruned 5 obsolete/merged branches from GitHub (`waveshare-hub75`, `feat-tinyled-qmi8658-hub`, `robertlipe-patch-1`, `fix/ci-windows-and-fastled-pin`, `jules-fix-windows-ci-parallel-*`). Retained `origin/node` and `origin/arduino4`.
