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

Flash budget (recorded 2026-10-19, from the Arduino 4 migration)

Kitch (ESP32-C6, `custom_kitch.ini`) is the fattest build and the canary for
core growth. Its app0 is 0x1B0000 = 1,769,472 bytes
(`config/partitions_custom-a3.csv`).

The number that matters is `firmware.bin`, not PIO's `Flash:` percentage.
PIO's figure and `checkprogsize` both omit `.eh_frame`, which is allocated and
really is written into the image. Only `tools/merge_image.py` weighs the truth.
Measured on kitch:

  PIO "program size" = .flash.text + .flash.rodata + .iram0.text + .dram0.data
  (this identity was verified: 1,784,720 == the sum of exactly those sections)

  banner              firmware.bin    PIO number    vs app0
  Arduino 3 (trunk)     1,667,472       --           fits, ~102KB spare
  Arduino 4 baseline    1,798,208       1,784,720    -28,736  (over)
  CORE_DEBUG_LEVEL=0    1,727,904       1,714,408    +41,568  (shipped)
  PARLIO=0 alone        1,766,496       1,753,088    +2,976   (razor thin)
  both (d0 + parlio)    1,695,776       1,682,368    +73,696

  So the Arduino 4 core + FastLED 3.10.8 cost kitch ~130KB vs the Arduino 3
  core. The two trim flags overlap heavily: each is worth ~52KB alone, but
  together only ~104KB.

- [ ] Kitch partition resize: do it once, on the maintainer's terms, and treat
      it as buying headroom rather than patching an overflow. It is the only fix
      that stops the every-dot-rev re-litigation. Note the table has
      reclaimable slack (nvs ends 0x00E000, otadata begins 0x00E000, but
      app0 does not start until 0x010000; the noota table has a similar 0x4000
      gap). This requires a ladder + USB, so batch it with the mDNS work below.
- [ ] Kitch: reclaim ~25.6KB of IDF mDNS code. The project has NO mDNS at all --
      no `MDNS.begin`, no `addService`, no `mdns_init`. Only `WiFi.setHostname()`
      (`nd_network.cpp:367,541`) and `ArduinoOTA.setHostname()` (`:1092`),
      i.e. DHCP hostname registration, which depends on the router's DHCP/DNS
      honoring it. The 25.6KB of `mdns_*` symbols in the image is IDF component
      code pulled in by a dependency (AsyncWebServer / esp_web / ArduinoOTA).
      Needs an `sdkconfig` knob (e.g. exclude the component) rather than a -D
      flag, and a check that nothing in the link chain hard-requires it.
      Zero behaviour change: we are not using it today.
- [ ] Kitch: `-DFASTLED_ESP32_HAS_PARLIO=0` is worth ~52KB alone and would also
      fit, but it silently moves kitch's WS2812 output from the C6's PARLIO
      peripheral to RMT5. Kitch drives one plain WS2812 strip
      (`ws281xgfx.cpp:47`). FastLED enables PARLIO by default where
      `SOC_PARLIO_SUPPORTED`. This is a permanently installed display that is
      hard to physically reach, so the driver swap wants a bench test before we
      rely on it. FREE ~52KB waiting on one test.
- [ ] Also dormant: gzip `config/timezones.json`. It is baked into the app raw
      at 17,912 bytes while every other site asset is gzipped
      (index.js.gz 16,773 + favicon.ico.gz 6,434 + index.html.gz 506).
      Worth ~10-12KB.
- [ ] Recorded as NOT worth pursuing: `-fno-rtti` is worth exactly 0 bytes
      (there is no RTTI in the image to begin with, even though nothing uses
      `dynamic_cast`/`typeid`). `-fno-omit-frame-pointer` only *looks* like
      ~13KB; that is measurement artifact, and it is deliberately kept for
      backtrace quality. The residual 12,636 bytes of `.eh_frame` cannot be
      reclaimed: all 216 locally-built objects have zero `.eh_frame` sections,
      so it comes entirely from prebuilt IDF/framework archives.

Arduino 4 migration status (recorded 2026-10-19)

Four breakages, each one commit. Three were verified by building; the HUB75 one
is build-only because the 64x64 panel is RMA'd.

  - `0b052dd2` HUB75 pins / `symlink://` PackageException. Note the install
    folder name is load-bearing: `tools/setup_shared_libs_cli.py` and
    `${shared_libs.ESP_MPDMA}` must agree on it.
  - `ed75034d` esp-hub75 `^0.3.6` + `scan_wiring` / `Hub75ScanWiring` port.
    `^0.1.0` was frozen on 0.1.7; 0.3.3 is the floor for IDF 6 GDMA (PR #87).
  - `4f86cb64` kitch `CORE_DEBUG_LEVEL=0` (+41,568 bytes).
  - `628c1d65` AMOLED `esp_adc_cal` version gate -> `analogReadMilliVolts`.
    Verified SUCCESS on both Arduino 4 (61.04.00-RC1, IDF 6.1) and Arduino 3
    (55.03.311, IDF 5.5). IDF 6 deletes `esp_adc_cal.h` but KEEPS
    `esp_adc/adc_cali.h`, so a missing header is a reorganized API, not a
    removal.

- [ ] Consider pinning the platform to the 55.03.311 Arduino 3 line (or a
      released Arduino 4) rather than tracking 61.04.00-RC1, until RC churn
      settles.
- [ ] On the Open question of "does Arduino 4 add junk": yes, ~130KB on kitch.
      This is a core-size story, not a missing-trunk-merge story. `origin/main`
      == `be0c99e6` is an ancestor of `arduino4`, so there is no outstanding
      trunk work to bring in.

Watchdog / uptime observation (recorded 2026-10-19)

- [ ] Investigate the Task Watchdog bark. Two long-running units reported
      `Last boot reason: (6): Task Watchdog barked` with uptimes of 31d and 26d.
      At roughly one reboot per month this is not urgent, but it is a real
      defect.

      Diagnosis of why `uptime` showed no panic message: the display half is
      already built and works. `DoUptime()` (debug_cli.cpp:537-541) already
      prints `Persistent Panic Message` out of the RTC-persistent
      `l_rtcPanicRecord`, and `RTC_PanicRecord` lives in `RTC_NOINIT_ATTR` RAM
      (debug_cli.h:44) so it survives a software reboot. The gap is that the
      IDF TWDT reset path resets the chip directly and never calls
      `PanicAt()`, so nothing ever wrote that record. `ESP_RST_TASK_WDT` is a
      hardware path; `Panic()` is the software path, and they are not connected.
      Reproducer already exists: `crashme wdt` (debug_cli.cpp:619) spins in
      `while(true)` purely to trip the TWDT.

      BUT -- the two barks almost certainly DID leave evidence, just not in the
      place we were looking. The C6 sdkconfig has BOTH
      `CONFIG_ESP_TASK_WDT_PANIC 1` (TWDT timeout escalates to the IDF panic
      handler, not just a plain reset) and `CONFIG_ESP_COREDUMP_ENABLE 1` /
      `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH 1`. So a bark dumps the crashing
      task's stack, task list and backtrace into the 64KB `coredump` partition
      at 0x370000 in `config/partitions_custom-a3.csv` -- which is flash, so it
      SURVIVES the reboot. FIRST ACTION, before any code change: if those two
      devices are still running the firmware that barked,
      `tools/panic_tool.py --ip <addr>` fetches `http://<addr>/coredump` and
      decodes it with espcoredump. The one hard requirement is that the `.elf`
      matches the flashed binary -- see the ELF-stamp item below. If no dump is
      found, `DoClearCoredump` in the CLI can rule out a stale one.

      Also note the triggering mechanism: `TaskManager::begin()`
      (taskmgr.cpp) deliberately REMOVES the system idle tasks from the TWDT
      and adds its own Idle0/Idle1 measurement tasks in their place, because
      the measurement tasks burn idle time on purpose. So a TWDT bark here is
      most likely one of those replacement idle tasks failing to be fed --
      i.e. something starved the low-priority measurement task, which points at
      a hot effect loop or a spinning task rather than a random crash. Bound
      the search there first.

- [ ] Wire the TWDT path into Panic() so a bark leaves a breadcrumb (forgotten
      special case). Give the TWDT a callback (`esp_task_wdt_init` / the TWDT
      panic handler) that calls `PanicAt()` with the offending task name before
      the reset, so a bark records file/line/function into `l_rtcPanicRecord`
      and shows up in `uptime` like any other panic. This is the ergonomic fix
      (a readable breadcrumb without a coredump round-trip); the forensic path
      above already captures the stack. Small and self-contained.
      Caveat to respect: `PanicAt()` (main.cpp:301) is `[[noreturn]]` and calls
      `esp_restart()` after a `Serial.flush()` + 100ms delay, which is fine
      from a watchdog callback only if it is safe to format and print from that
      context -- verify against the TWDT callback's ISR/task constraints before
      committing to the same code path, and fall back to writing the RTC record
      only (skipping the console print) if it is not. Note also that a custom
      TWDT callback and `CONFIG_ESP_TASK_WDT_PANIC` interact, so decide which
      one owns the escalation rather than having both.
      Sequencing: see the IdleTask item below -- retire the idle-burner first.

- [ ] Make a device able to report WHICH BINARY it is running. This is the one
      genuinely missing piece and the real lesson from the two 31d/26d barks:
      the coredump may be sitting in flash right now, but an ELF that matches
      the flashed image is required to decode it, and none was saved. Action:
      stamp the git SHA + build date into a readable symbol and surface it from
      the CLI (`uptime`, `status`, or a new `buildinfo`), so a device can be
      asked which build it is. Consider keeping the last N `.elf` files per env
      as a side effect of the build. Cheap, and it protects every future
      post-mortem, not just this one.

- [ ] Retire the IdleTask busy-loop CPU measurement (do this BEFORE the TWDT
      callback above -- see the sequencing note below). `IdleTask`
      (`include/taskmgr.h:72-105`, ~30 lines) spins at `tskIDLE_PRIORITY + 1`
      calling `delayMicroseconds()` purely to measure how much time it *could*
      have burned, and derives `GetCPUUsage()` as "100 less the idle time we
      managed to squander". To make room for those burners,
      `TaskManager::begin()` (taskmgr.cpp) removes the SYSTEM idle tasks from
      the TWDT and re-adds its own Idle0/Idle1. That is what makes the watchdog
      need hand-feeding, so this machinery creates the problem the TWDT
      callback would then patch around.

      The modern replacement is `vTaskGetRunTimeStats()`
      (`CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`), which reports per-task CPU
      directly instead of inferring it from squandered idle time. Bonus: the
      CPU then gets to actually idle, which matters for power management and
      for not perturbing the scheduler.

      Scope note: `TaskManager` is really two unrelated things in one class.
      The task launcher half -- `StartNetworkThread`, `StartDebugThread`,
      `StartSocketThread`, `StartEffectThread`, `NotifyNetworkThread`,
      `GetStackUsageSummary`, and the `*_STACK_SIZE` / `*_PRIORITY` / `*_CORE`
      constants -- is load-bearing threading policy and should stay (under a
      name like ThreadManager). Only the idle-burner half is up for deletion.
      Do NOT read this as "delete NightDriverTaskManager".

      Unicore sweep, VERIFIED (the earlier guess that `taskmgr.cpp:44-45`'s
      `#define CONFIG_FREERTOS_NUMBER_OF_CORES 2` fallback fires on the C6 was
      WRONG -- a previous note here said so and has been corrected). The macro
      is declared long before line 44: `taskmgr.h:48` includes `<Arduino.h>`,
      which includes `freertos/FreeRTOS.h`, which includes `FreeRTOSConfig.h`,
      which includes `sdkconfig.h`, and the C6 sdkconfig.h defines
      `CONFIG_FREERTOS_NUMBER_OF_CORES 1` (and `CONFIG_FREERTOS_UNICORE 1`).
      So the `#ifndef` never triggers and the build correctly takes the
      single-core paths. Two real consequences to keep in mind:
        * `GetCPUUsagePercent(1)` on a unicore target still hits
          `Panic("Invalid core passed to GetCPUUsagePercentCPU")`
          (taskmgr.h:133), so passing core 1 on a C6 is a crash path today.
          Consumers: webserver.cpp:840-842 and screen.cpp:188. Worth guarding.

      Metrics caveat is NOT a blocker: the maintainer's calibration is that
      `>90` catastrophic and `<10` unrealistic, and 64-vs-58 is noise, so the
      numbers only need to stay in a plausible band -- they do not need to be
      numerically comparable to the old implementation. `CPU_USED` /
      `CPU_USED_CORE0` / `CPU_USED_CORE1` are surfaced in the web UI
      (webserver.cpp:840-842) and on screen (screen.cpp:188).

      Sequencing: retire the idle-burner FIRST, then wire the TWDT callback
      into Panic(). Doing the callback first means writing it for a watchdog
      configuration that is about to disappear, and then rewriting it. Kitch is
      the right proving ground -- it is the constrained single-core target where
      this machinery is least justified.

Networking / naming (recorded 2026-10-19)

- [ ] Decide whether to keep `WiFi.setHostname()`. Names do not resolve on the
      maintainer's Ubiquiti network today (the C6 reports
      `esp32c6-170b54.localdomain`, which is the DHCP hostname path, not mDNS).
      If `nightdriver.local`-style discovery is actually wanted, that requires
      calling `MDNS.begin(hostname)` -- a feature gap, not a size problem. Note
      `cszHostname` is a single compile-time constant in `secrets.h`, so two
      devices built from the same secrets collide on one name. If mDNS is added
      it belongs behind a feature flag (`.ini` philosophy).

Build system / platformio.ini additions (recorded 2026-10-19)

- [ ] `custom_*.ini` tracking is inconsistent: `custom_kitch.ini` is tracked in
      git while several siblings are not. Decide the rule and encode it in
      `.gitignore`; the "everything in a .ini" scheme is blocked on it.
- [ ] `_`-suffixed files litter the tree and are a live trap for greps and
      editors: `src/effects.cpp_`, `src/main.cpp_`, `src/nd_network.cpp_`,
      `platformio.ini_`. `effects.cpp_` contains near-duplicates of the
      `EFFECTS_DEMO` / `EFFECTS_FULLMATRIX` guards. Delete or relocate.
- [ ] The hand-vendored AMOLED island (`src/amoled/`, `include/amoled/`) is the
      last vendored-by-copy library; every other vendor lib is pinned through
      `shared_lib_deps`. It is consumed only by `screen_impl.cpp:245-269` and
      only for `demo_amoled` (`dev_lilygo_amoled`, `-DAMOLED_S3=1`). Our copy is
      an older upstream revision (static `PowersSY6970` API, no 191_SPI board,
      no RTC), so a sync is a port, not a copy. Long term: pin it like lvgl
      (`lvgl.git#release/v8.3`) and delete the copy. Deleting the island also
      drops the lvgl / XPowersLib / SensorLib dependency surface. Caveat:
      `-DUSE_SCREEN=1` and the `screen_impl` abstraction serve other displays, so
      removing the amoled branch is cheap but not entirely free.
