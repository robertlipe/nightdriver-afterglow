# NightDriverStrip Agent Context

This file contains critical context, constraints, and architectural information for AI agents working on the NightDriverStrip codebase. **Read this file first.**

## 🚨 CRITICAL MANDATES & CONSTRAINTS

**Safety & Operations**
*   **DELETE Operations:** 🚨 **STOP and ask for permission** before ANY delete operations.
    *   ✅ **EXCEPTIONS:**
        *   Single files that you *just* created in the current session are OK to delete.
        *   **Build cache & artifact hygiene:** Purging `.pio/build_cache` or `.pio/build/<env>` to recover from stale objects, duplicate symbols, or corrupted unity builds is permitted without prompting.
    *   ❌ **NEVER** delete project source files, directories, or system files without explicit user permission.
*   **File Modifications:**
    *   ❌ **NEVER** edit, patch, or commit library/build files under `.pio/` (modify `platformio.ini` or project sources instead).
    *   **Do not** gratuitously change existing comments. **Preserve original technical documentation**, including credits, Stefan Petrick notes, Aurora/Soulmate attributions, and "dirty hack" warnings.
    *   **Do not** reformat existing files unless explicitly asked. Code styles are inconsistent; match the *local* neighborhood of the code you are editing.
    *   **Do not** end lines with trailing whitespace.
    *   Use **UNIX-style newlines** (`\n`) for all files.
*   **Consult `.md` files in the top-level directory and docs/.**

**C++ Coding Standards**
*   **Standard:** Uses **C++26** (`-std=gnu++26`), but defers to Arduino conventions for compatibility. Prefer industry-standard C++ operations for containers, summations, time, etc. over Arduino.
*   **Exceptions & Error Handling:** ❌ **C++ exceptions are disabled** (`-fno-exceptions`; see `[base] build_flags` in `platformio.ini`). Do not write `try`, `catch`, or `throw` anywhere. Unrecoverable, fatal errors (e.g., OTA failures, critical allocation failures) MUST call `Panic("printf-style message", ...)`, declared in `include/nd_panic.h`.
    *   **Architecture Note:** `Panic()` logs the message together with the file/line/function of the caller, records it in the RTC flight recorder for post-mortem, and reboots. The older `throw std::runtime_error(...)` path was retired: the exception machinery (unwind tables, `.eh_frame`, `.gcc_except_table`, typeinfo, `__cxa_*`) cost ~130KB of flash, and nothing in the image ever unwinds a stack.
    *   **Terminal Safety Net:** `std::set_terminate(TerminateHandler)` is still installed for the paths that do not go through `Panic()` (a violated `noexcept`, a bad `std::function` target, ...).
    *   **AI Agent Rule:** 🛑 Do NOT flag `Panic()` paths as "missing error path tests" or "missing test coverage." Do not attempt to write unit tests for these terminal states. It is a false positive; this is the intended terminal recovery path.
*   **Data Types & Math:** ESP32 and ESP32-S3 feature hardware single-precision floating point (FPU). **Prefer `float`** data types and math calls (`std::sinf`, `std::cosf`) over `double`. **Do not down-convert floating-point trigonometry to 8-bit lookup tables (`sin8`, `cos8`);** hardware `std::sinf`/`std::cosf` executes in ~121 clocks, so lossy integer lookups degrade visual fidelity for negligible gain on modern ESP32 targets.
*   **Memory & Allocations:**
    *   ESP32 Targets vary between 320K and 16MB (PSRAM) of RAM.
    *   `src/main.cpp` sets `heap_caps_malloc_extmem_enable(16);`. This means any dynamic allocation (`new`, `malloc`, `std::make_unique`) over 16 bytes will automatically route to PSRAM first, seamlessly falling back to Internal SRAM if PSRAM is full.
    *   Prefer embedding large arrays as member variables inside `Effect` classes. Since `Effect` objects are dynamically allocated and large, they will automatically land in PSRAM, saving you the overhead of managing a second heap pointer.
    *   Avoid large stack allocations.
    *   **`JsonDocument` capacity:** When reusing a persistent `JsonDocument` member across repeated network fetches or parses, call `doc.clear()` before `deserializeJson()` to reset its internal structure while retaining allocated capacity, preventing heap fragmentation.
    *   **Display text APIs:** In `Screen` and text measurement functions, provide `const char*` overloads alongside `const String&` so string literals and C-strings don't construct temporary heap-allocated Arduino `String` objects on every frame.
*   **Concurrency & FreeRTOS Tasks:**
    *   Multiple tasks run on potentially multiple cores. The design generally assumes only one task clearly owns each data. There are very few `std::lock_guard` or `std::mutex` locks needed.
    *   **Task destruction scoping:** Inside FreeRTOS task entry lambdas, wrap local variables (e.g. smart pointers, buffers) in an explicit block scope `{}` so their destructors run cleanly *before* clearing task progress flags (e.g., `_isFetching = false`) or calling `vTaskDelete(NULL)`.
    *   **Worker cancellation:** Background worker tasks launched by effects must not capture `this` unsafely. The effect destructor must signal cancellation and wait for in-flight tasks to exit before completing object teardown.
*   **Best Practices:** Use C++26 best practices, like returning `std::optional`, relying upon RVO and NVRO, not using in/out parameters. Prefer constant, smart data structures and containers and `<algorithm>` over manually coded loops.
*   **Style:**
    *   Code mostly follows Microsoft C++ style guide. Match format to neighboring code in the same file or sibling files.
    *   **Indentation:** 4 spaces.
    *   **Braces:** Scoping curlies are on lines of their own.
    *   **Naming:** Classes/Methods (`PascalCase`), Local variables (`camelCase`), Class member variables (`_leadingUnderscoreCamelCase`).
    *   **Function/Method Arguments & Returns:**
        *   Error codes: `bool function() { return false; }` or custom error enums.
        *   `std::optional<T>` for functions that may not return a value.
        *   Prefer early error returns instead of deeply nested if-statements.
*   **Enums:** Prefer `enum class` to bare `enum`.
*   **Explicit Naming:** For structure members, typedefs, enum members, return types, etc., use explicitly named values.

**Logging:**
*   Use macros `debugV`, `debugE`, `debugI`, `debugW`, `debugF`, `debugT`, `debugD`. These accept variadic C++ `std::printf` format strings and should be used sparingly.
*   **Critical section safety:** Never call logging macros inside critical sections where interrupts are disabled (`noInterrupts()` or inside an ISR/RAII interrupt guard). Buffer the status or error context inside the critical section and defer logging until interrupts are re-enabled.

**Render Path & Performance Notes**
*   **Where perf lives:** Most runtime cost is inside effect `Draw()` loops, not `EffectManager::Update()` or `drawing.cpp`.
*   **Known heavy effects:** `PatternLife`, `PatternSMPicasso3in1` (blur), `PatternSMStarDeep` (trig + lines), `PatternSMFire2021/2012` (full-frame noise + blend), `PatternSMNoise*`.
*   **Debug logging:** `debugV` inside audio/draw loops is expensive if enabled; keep it sparse in hot paths.
*   **Reuse helpers:** Prefer `GFXBase` helpers (`drawPixelXYF_Wu`, `GetNoise()`, `DimAll`) over local duplicates when refactoring.
*   **Performance notes file:** Keep research findings in `performance_todo.md` to avoid rediscovery.

---

## 🕵️ AI Code Review Protocols
*   **Pre-Commit Rigor:** Before committing or pushing ANY code, act as a strict, senior C++ security and safety reviewer. Do not just commit because the code compiles.
*   **Memory & Lifetimes:** Rigorously audit the lifetimes of `std::string_view`, `std::span`, pointers, and references. **Never** construct views over local/stack variables that could outlive their scope or dangle.
*   **Concurrency & Reentrancy:** Check for race conditions, unprotected shared states, and interrupt/ISR safety.
*   **Modernization Anti-Patterns:** Ensure modern C++20/26 features are used safely, not just blindly applied.
*   **Data Types:** Watch for silent narrowing conversions, sign-extension bugs, and legacy `uint8_t` vs `char` mismatching in text/string paths.
*   **Bounds Checking & Array Access:** Never use raw unchecked array access (e.g., `g()->leds[XY(...)]`). Always prefer safe wrappers like `g()->drawPixel(...)`. Ensure clamping happens *before* nested loops if it defines origin points.
*   **Initialization & Constructors:** Strictly check for uninitialized members in constructors (e.g., using a variable to initialize another before it's initialized itself). Mark single-argument constructors as `explicit` to prevent unintended implicit type conversions.
*   **Loop Variables & Underflows:** Watch out for `uint8_t` or `uint16_t` loop iterators that can underflow/wrap around, especially when doing math like `center - 2`. Prefer native `int` for loop iterators unless specifically optimizing structure sizes.
*   **Math Boundary Conditions:** Check clamp and oscillation boundaries carefully. Ensure oscillation logic (like `< 5` and `> 80`) won't get stuck in an infinite flip-flop loop.
*   **Namespace Pollution:** Never use `using namespace` in header files at the global scope.
*   If you spot a flaw during your internal pre-commit review, **STOP**, fix it, and explain the fix to the user before proceeding.

---

## Project Overview

NightDriverStrip is a comprehensive C++ firmware framework for the ESP32 microcontroller designed to drive various LED displays, including WS2812B strips and HUB75 matrices.

### Key Technologies
*   **Firmware:** C++26, Arduino Framework, PlatformIO.
*   **Frontend:** React, Vite, Material UI (in `site/`).
*   **Hardware Support:** ESP32, ESP32-S3, ESP32-C3.
*   **Graphics Engine:** Custom `GFXBase` extending `Adafruit_GFX`.
*   **Libraries:** FastLED, ArduinoJson, ESPAsyncWebServer, esp-hub75, ESP32_HUB75_LED_MATRIX_PANEL_DMA_Display

## Directory Structure
*   `src/`: C++ source files.
*   `include/`: Header files.
*   `site/`: React-based web frontend.
*   `tools/`: Python and Shell scripts for automation.
*   `data/`: SPIFFS data.

---

## Architecture & Core Components

**Threading Model** (Main, Drawing, Audio, Network, Remote, Socket, Web)

**Key Classes**
*   **`GFXBase` (`include/gfxbase.h`):** Foundational graphics class.
*   **`EffectManager` (`include/effectmanager.h`):** Manages active effects and transitions.
*   **`LEDStripEffect` (`include/ledstripeffect.h`):** Base class for visual effects.
*   **`SystemContainer` (`include/systemcontainer.h`):** Central hub for system objects.

---

## Interactive Debugging & CLI
Accessible via Serial (115200). Core logic in `src/debug_cli.cpp`. Supports fuzzy effect matching and tab completion.
*   **Three-tier output standard (`cli_write` / `cli_print` / `cli_printf`):**
    *   **`cli_write(std::string_view)`:** Use for literal strings, usage text, banners, tables, and static notices. Sends the string view straight to `ConsoleSession::WriteText()`, avoiding `vsnprintf` stack buffering, heap allocations, and format parsing overhead.
    *   **`cli_print(Args&&...)`:** Use for token concatenation, status messages, and error notices involving `std::string_view`, characters, booleans, and integers. Uses a C++ variadic fold expression with non-template primitive outputters (`std::to_chars` for numbers). Completely eliminates `%.*s` and `(int)sv.size()` string-view overrun risks with near-zero runtime/flash cost.
    *   **`cli_printf(fmt, ...)`:** Reserve strictly for aligned columnar tables, hex dumps, and formatting where field widths (`%2zu`, `%-20s`) or floating-point precision (`%.1f`) are genuinely required.

---

## Development Guides

### Code Stability & Networking
*   **"No-Net" Compatibility:** Always verify builds with `ENABLE_WIFI=0` (using `demo` env).
*   **Guards:** Guard network-specific code with `#if ENABLE_WIFI`.

### Adding New Effects
*   **Foundation graphics stability:** New effects should take full advantage of our extensive pixel-bashing primitives in `GFXBase` and `GFXHex`. Avoid modifying foundational graphics classes to suit a single effect without careful consideration of how that change might impact scores of other effects.
*   **Existing effects stability:** Fixing genuine bugs, eliminating unsafe globals, preventing bounds overwrites, or optimizing hot loops is encouraged. However, preserve recognizable community algorithms (Stefan Petrick, Aurora, Soulmate) and their original math/artistic character; avoid cosmetic churn or restyling working effects solely for stylistic conformity.
*   Implement new effects directly in their header files (e.g., `include/effects/matrix/PatternNewEffect.h`). **Implement NO corresponding `.cpp` file.**
*   Generously use color utilities from `FastLED.h` and math functions from `lib8tion.h`.

---

## Header Management & IWYU (Include What You Use) Rules

To maintain build stability across Av2 and Av3, adhere to these strict rules:

1.  **Strict IWYU**: Include the header that defines a symbol directly. Do not rely on transitive includes.
2.  **`globals.h` Precedence**: `globals.h` MUST be the first "local" (quoted) include in any `.cpp` or non-leaf header. This establishes feature macros before logic is parsed.
    *   **Exception**: Leaf headers in `include/effects/` do not include `globals.h` directly as they are only ever included into units where it is already established.
3.  **`globals.h` Minimalism**: Keep it restricted to configuration macros and constants. Do not add system logic headers.
4.  **Interface Decoupling**: Use `interfaces.h` for core types (`IJSONSerializable`, `SettingSpec`). This prevents pulling `ledstripeffect.h` or `jsonserializer.h` into low-level components.
5.  **Include Skepticism**: Prefer forward declarations over includes in headers. Keep heavy display/driver includes in `.cpp` files.
6.  **Non-Trivial Classes**: Class method implementations MUST reside in `.cpp` files. Headers should be declaration-only.
    *   **Exception**: Effect classes under `include/effects/` are header-only by design (see "Adding New Effects"). This rule exists to stop headers from becoming transitive-include magnets, and an effect header is a leaf.
7.  **Arduino3 Collisions**: Use `nd_network.h` for project networking; `network.h` is reserved for the Arduino framework.

---

## PlatformIO Configurations & Environment Inheritance

*   **The `extends` Trap:** When an environment inherits from another via `extends`, it automatically inherits `build_flags`. **Do not** manually inject `${parent.build_flags}` into a child's `build_src_flags` unless the parent *explicitly* defines `build_flags`. If the parent doesn't define it, PlatformIO interpolates it incorrectly, silently erasing the global `base.build_flags` (like our C++26 mandate). Rely on native `extends` inheritance whenever possible.
*   **Flag Placement (`build_flags` vs `build_src_flags`):** Frameworks (like Arduino) often inject their own default compiler flags (e.g., `-std=gnu++20`). To successfully override these (e.g., to enforce `-std=gnu++26`), your flags MUST be placed in `build_flags`, NOT `build_src_flags`.
*   **The `lib_ignore` Framework Poisoning Trap:** ❌ **NEVER** put core Arduino/ESP-IDF framework components (`WiFi`, `Network`, `HTTPClient`, `Update`, etc.) into `lib_ignore`. In `pioarduino`, specifying framework components triggers `ComponentManager` to dynamically rewrite `framework-arduinoespressif32-libs/.../pioarduino-build.py` on disk before compilation. If the build aborts before the teardown step, the backup is not restored, permanently corrupting include paths (`esp_wifi_types.h: No such file or directory`) for subsequent classic ESP32 builds. When disabling networking (`ENABLE_WIFI=0`), let the compiler and linker (`-ffunction-sections`, `-fdata-sections`, `--gc-sections`) strip unused code automatically.
*   **`lib_ignore` Inheritance:** Environments automatically inherit `lib_ignore` from parent sections via `extends`. Do not redundantly redeclare `lib_ignore = ${base.lib_ignore}` in child environments.

---

## Building & Utility Tools

The `tools/` directory contains diagnostic, auditing, and maintenance utilities. These are **optional, on-demand tools** to assist your work when needed—not a mandatory per-turn checklist:

*   **Include & Style Audits (`tools/audit_globals_order.py`, `tools/audit_include_rules.py`):** Checks `#include` alphabetization and `globals.h` precedence. PlatformIO's pre-build scripts automatically run these before compilation; you only need to run them standalone when actively modifying header blocks or investigating an audit failure.
*   **Target Selection & Verification Map:** Rather than building all targets locally, verify against the specific subsystem touched:
    *   `demo`: Fastest 1D strip build (WS2812B rainbow on Pin 5). Best for sanity-checking core framework changes, no-net (`ENABLE_WIFI=0`) compatibility, and basic build health. Does not compile matrix or hex effects.
    *   `mesmerizer` (or `ws_s3_hub75-64x64` for S3): Primary 2D HUB75 matrix targets. Exercises `effects/matrix/` and `hub75gfx.cpp`.
    *   `tinyled-hex` or `yulc-hex`: Exercises `effects/hex/` and `gfxhex.cpp`.
    *   Full matrix (all 20+ targets across LX6, LX7, and RISC-V): Let GitHub Actions CI do the heavy lifting; avoid running local full-matrix batch builds unless specifically verifying release branches.
*   **Target Discovery (`tools/show_envs.py`):** Lists all defined board environments for reference if you are unsure which environment to target.
*   **Batch Verification (`tools/build_all.py`):** Iterates through all targets. Very slow; prefer targeting the specific environment you are working on (e.g., `pio run -e demo`, `-e mesmerizer`, `-e tinyled-demo`).
*   **Build Cache Hygiene (`.pio/build_cache`):** When major library versions change, unity build bundles change, or headers were previously missing, `.pio/build_cache` can serve stale object files containing stub/weak symbols that clash with new implementations (e.g. `multiple definition of fl::net::wifi::...`). If duplicate symbol linker errors occur across FastLED or framework unity objects, purge the cache with `rm -rf .pio/build_cache`.
*   **🚨 Reading the size report:** PlatformIO's `Flash:` percentage **excludes the `.eh_frame` section**, even though `.eh_frame` is allocated and sits inside a `LOAD` segment, so it really *is* written into `firmware.bin`. On kitch that hid ~130KB: the report said "95.4%" while the actual app image was 102.9% of `app0`. Decide whether a board fits by comparing `firmware.bin`'s size against the app partition in `config/partitions_custom-*.csv`, never by the percentage. `tools/merge_image.py` now enforces this on every build and fails it if the app image does not fit.
