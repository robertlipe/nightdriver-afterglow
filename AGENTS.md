# NightDriverStrip Agent Context

This file contains critical context, constraints, and architectural information for AI agents working on the NightDriverStrip codebase. **Read this file first.**

## 🚨 CRITICAL MANDATES & CONSTRAINTS

**Safety & Operations**
*   **DELETE Operations:** 🚨 **STOP and ask for permission** before ANY delete operations.
    *   ✅ **EXCEPTION:** Single files that you *just* created in the current session are OK to delete.
    *   ❌ **NEVER** delete multiple files, directories, system files, or project files without explicit user permission.
*   **File Modifications:**
    *   ❌ **NEVER** modify or add any files under `.pio/`.
    *   **Do not** gratuitously change existing comments. **Preserve original technical documentation**, including credits, Stefan Petrick notes, Aurora/Soulmate attributions, and "dirty hack" warnings.
    *   **Do not** reformat existing files unless explicitly asked. Code styles are inconsistent; match the *local* neighborhood of the code you are editing.
    *   **Do not** end lines with trailing whitespace.
    *   Use **UNIX-style newlines** (`\n`) for all files.
*   **Consult `.md` files in the top-level directory and docs/.**

**C++ Coding Standards**
*   **Standard:** Uses **C++20**, but defers to Arduino conventions for compatibility. Prefer industry-standard C++ operations for containers, summations, time, etc. over Arduino.
*   **Exceptions:** ❌ **Do not use try-catch blocks or C++ exceptions.** Unrecoverable, fatal errors should call `throw std::runtime_error()`.
*   **Data Types:** ESP32 has hardware floating point but emulates double. **Prefer `float`** data types and function calls over `double`.
*   **Memory:**
    *   ESP32 Targets vary between 320K and 16MB (PSRAM) of RAM.
    *   Use `make_unique_psram<T>()` or `make_shared_psram<T>()` for large object allocations to prefer PSRAM when available.
    *   Avoid large stack allocations.
*   **Concurrency:** Multiple tasks run on potentially multiple cores. The design generally assumes only one task clearly owns each data. There are very few `std::lock_guard` or `std::mutex` locks needed.
*   **Best Practices:** Use C++20 best practices, like returning `std::optional`, relying upon RVO and NVRO, not using in/out parameters. Prefer constant, smart data structures and containers and `<algorithm>` over manually coded loops.
*   **Style:**
    *   Code mostly follows Microsoft C++ style guide. Match format to neighboring code in the same file or sibling files.
    *   **Indentation:** 4 spaces.
    *   **Braces:** Scoping curlies are on lines of their own.
    *   **Naming:** Classes/Methods (`PascalCase`), Local variables (`camelCase`), Class member variables (`_leadingUnderscoreCamelCase`).
    *   **Function/Method Arguments & Returns:**
        *   Error codes: `bool function() { return false; }` or custom error enums.
        *   `std::optional<T>` for functions that may not return a value.
        *   Prefer early error returns instead of deeply nested if-statements.
*   **Enums:** Prefer `class enum` to bare `enum`.
*   **Explicit Naming:** For structure members, typedefs, enum members, return types, etc., use explicitly named values.

**Logging:** Use macros `debugV`, `debugE`, `debugI`, `debugW`, `debugF`, `debugT`, `debugD`. These accept variadic C++ `std::printf` format strings and should be used sparingly.

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
*   If you spot a flaw during your internal pre-commit review, **STOP**, fix it, and explain the fix to the user before proceeding.

---

## Project Overview

NightDriverStrip is a comprehensive C++ firmware framework for the ESP32 microcontroller designed to drive various LED displays, including WS2812B strips and HUB75 matrices.

### Key Technologies
*   **Firmware:** C++20, Arduino Framework, PlatformIO.
*   **Frontend:** React, Vite, Material UI (in `site/`).
*   **Hardware Support:** ESP32, ESP32-S3, ESP32-C3.
*   **Graphics Engine:** Custom `GFXBase` extending `Adafruit_GFX`.
*   **Libraries:** FastLED, SmartMatrix, ArduinoJson, ESPAsyncWebServer.

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
Accessible via Serial (115200). core logic in `src/debug_cli.cpp`. Supports fuzzy effect matching and tab completion.

---

## Development Guides

### Code Stability & Networking
*   **"No-Net" Compatibility:** Always verify builds with `ENABLE_WIFI=0` (using `demo` env).
*   **Guards:** Guard network-specific code with `#if ENABLE_WIFI`.

### Adding New Effects
*   **Do not modify existing files in `effects/matrix/`.**
*   Implement new effects directly in their header files (e.g., `include/effects/matrix/PatternNewEffect.h`). **Implement NO corresponding `.cpp` file.**
*   Generously use color utilities from `FastLED.h` and math functions from `lib8tion.h`.

---

## Header Management & IWYU (Include What You Use) Rules

To maintain build stability across Av2 and Av3, adhere to these strict rules:

1.  **Strict IWYU**: Include the header that defines a symbol directly. Do not rely on transitive includes.
2.  **`globals.h` Precedence**: `globals.h` MUST be the first "local" (quoted) include in any `.cpp` or non-leaf header. This establishes feature macros before logic is parsed.
    *   **Exception**: Leaf headers in `include/effects/` do not include `globals.h` directly as they are only ever included into units where it is already established.
3.  **`globals.h` Minimalism**: Keep it restricted to configuration macros and constants. Do not add system logic headers.
4.  **Interface Decoupling**: Use `interfaces.h` for core types (`IJSONSerializable`, `SettingSpec`, `psram_allocator`). This prevents pulling `ledstripeffect.h` or `jsonserializer.h` into low-level components.
5.  **Include Skepticism**: Prefer forward declarations over includes in headers. Keep heavy display/driver includes in `.cpp` files.
6.  **Non-Trivial Classes**: Class method implementations MUST reside in `.cpp` files. Headers should be declaration-only.
7.  **Arduino3 Collisions**: Use `nd_network.h` for project networking; `network.h` is reserved for the Arduino framework.

---

## PlatformIO Configurations & Environment Inheritance

*   **The `extends` Trap:** When an environment inherits from another via `extends`, it automatically inherits `build_flags`. **Do not** manually inject `${parent.build_flags}` into a child's `build_src_flags` unless the parent *explicitly* defines `build_flags`. If the parent doesn't define it, PlatformIO interpolates it incorrectly, silently erasing the global `base.build_flags` (like our C++26 mandate). Rely on native `extends` inheritance whenever possible.
*   **Flag Placement (`build_flags` vs `build_src_flags`):** Frameworks (like Arduino) often inject their own default compiler flags (e.g., `-std=gnu++20`). To successfully override these (e.g., to enforce `-std=gnu++26`), your flags MUST be placed in `build_flags`, NOT `build_src_flags`.

---

## Building & Tools
*   **Audit Tools**: Run `python3 tools/audit_globals_order.py` and `python3 tools/audit_include_rules.py`.
*   **List Available Targets**: `python3 tools/show_envs.py`.
*   **Build All**: `python tools/build_all.py`.
