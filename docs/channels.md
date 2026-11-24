# FastLED Channels API Implementation Plan: Dynamic Strip Management

**Date:** November 2026
**Branch:** `channels`
**Toolchain:** PlatformIO / Arduino 4.0 / ESP-IDF 6.1 (GCC 15, C++26)
**Status:** Architecture Specification & Implementation Plan

---

## 1. Executive Summary & Goals

The goal of the `channels` branch is to decouple NightDriver's LED hardware layer from compile-time templates, transitioning `WS281xGFX::AddLEDs()` to FastLED 3.10's non-template **Channels API** (`FastLED.add(fl::ChannelConfig)`).

This unlocks:
1. **Runtime Pin & Length Configuration:** Change data pins, pixel counts, and color orders via CLI, Web UI, and JSON configs without recompiling.
2. **Arbitrary Strip Topologies:** Break the historical 8-channel ceiling; support $N$ variable-length strips driven concurrently.
3. **Build Matrix Deconstruction:** Eliminate `-DLED_PIN0`, `-DCOLOR_ORDER`, and `-DNUM_CHANNELS` from `globals.h`, allowing `ccache` and PlatformIO to share object files across dozens of strip targets.

---

## 2. Architecture Overview

```mermaid
flowchart TD
    subgraph Config["Configuration & Storage"]
        NVS["NVS / LittleFS (/current.cfg)"] -->|JSON| DC["DeviceConfig (std::vector<StripConfig>)"]
        CLI["Debug CLI ('strip' command)"] -->|Modify| DC
        Web["Web / REST API (/api/strips)"] -->|Modify| DC
    end

    subgraph Core["NightDriver Core"]
        DC -->|Initializes| GFX["WS281xGFX Devices (vector<shared_ptr<GFXBase>>)"]
        GFX -->|Provides CRGB spans| Add["WS281xGFX::AddLEDs()"]
    end

    subgraph FastLED_Engine["FastLED 3.10 Channels Engine"]
        Add -->|ChannelConfig| FAdd["FastLED.add(config)"]
        FAdd --> CM["fl::ChannelManager"]
        CM -->|Dispatch| Drivers["Hardware Drivers (RMT / PARLIO / LCD_CAM / SPI)"]
    end
```

---

## 3. Data Structures & Schema

### 3.1 `StripConfig` Struct & The Unified Pin Abstraction
In `include/deviceconfig.h`:

```cpp
enum class ChipsetType : uint8_t {
    WS2812B = 0,   // 800kHz clockless (WS2812, WS2812B, WS2813, SK6812)
    WS2811,        // 400kHz / 800kHz
    WS2815,        // 12V clockless
    APA102,        // 2-wire SPI (Data + Clock)
    SK9822,        // 2-wire SPI (Data + Clock)
};

struct StripConfig {
    int dataPin = -1;
    int clockPin = -1;                // For SPI chipsets (APA102/SK9822), -1 for clockless
    uint16_t length = 60;
    EOrder colorOrder = EOrder::GRB;
    ChipsetType chipset = ChipsetType::WS2812B;
    bool enabled = true;
};
```

> **Design Note: Pin Abstraction & System-Wide Peripheral Scope**
> When modeling GPIO pin types (with observers for change detection), keep other dynamically configurable peripherals in view:
> - **IR Remote Receiver Pin** (`IR_REMOTE_PIN`)
> - **I2S Audio Input Pins** (`I2S_BCLK_PIN`, `I2S_WS_PIN`, `I2S_DATA_IN_PIN`)
> - **Buttons & Encoders** (`TOGGLE_BUTTON_0`, etc.)
>
> A unified pin assignment model prevents hardware collisions (e.g., ensuring a strip cannot claim a pin already assigned to I2S or IR).
>
> **Scope Boundary:** HUB75 matrices require 14–16 tightly coupled DMA pins and remain compile-time/board-specific for now. Dynamic reconfiguration applies to LED strips, IR remote, and I2S audio.

### 3.2 JSON Representation in `current.cfg`
```json
{
  "strips": [
    {
      "pin": 14,
      "clock": -1,
      "length": 64,
      "order": "GRB",
      "chipset": "WS2812B",
      "enabled": true
    }
  ]
}
```

### 3.3 Default Fallback & The `-DLED_PINS` Array Notation
To support boards with $\ge 8$ channels (e.g., LightCrafter-16, 16-channel WS2812 boards) cleanly without defining 16 separate macros, we support modern array notation in `platformio.ini`:

```ini
; Single-strip board
build_flags = -DLED_PINS=14

; Multi-strip board (e.g., LightCrafter-16)
build_flags = -DLED_PINS=16,17,18,19,21,22,23,25,26,27,32,33,12,13,14,15
```

In C++:
```cpp
#if defined(LED_PINS)
    static constexpr int kDefaultPins[] = { LED_PINS };
#else
    // Legacy fallback to -DLED_PIN0 .. -DLED_PIN15
    static constexpr int kDefaultPins[] = {
        #if defined(LED_PIN0) && LED_PIN0 >= 0
        LED_PIN0,
        #endif
        #if defined(LED_PIN1) && LED_PIN1 >= 0
        LED_PIN1,
        #endif
        // ...
    };
#endif
```

If no `"strips"` array exists in `/current.cfg` (first boot), `DeviceConfig` initializes from `kDefaultPins`:
- Data pins populated from `kDefaultPins`
- `length = NUM_LEDS` (or per-strip length array)
- `colorOrder = COLOR_ORDER`
- `chipset = WS2812B`

This completely eliminates:
1. The historical `#error The maximum value of NUM_CHANNELS is 8` ceiling.
2. The manual 8-way and 16-way macro towers in `ws281xgfx.cpp` and `globals.h`.
3. In `include/effects/strip/faneffects.h`, replacing `for (int i = 0; i < NUM_CHANNELS; i++)` with `for (size_t i = 0; i < g_pSystem->GetGFXCount(); i++)` decouples strip effects from compile-time channel counts.

---

## 4. Hardware Reconfiguration & DMA Safety

### The Dilemma: Live Mutation vs. Staged Reboot
ESP32 hardware peripherals (RMT channels, LCD_CAM, and PARLIO DMA descriptors) are actively read by DMA loops during frame rendering. If a strip buffer is reallocated or a GPIO pin is severed mid-DMA burst:
- RMT throws channel state errors.
- DMA may read freed memory, causing bus faults.
- Active visual effects expecting $N$ pixels crash when drawing past the end of a shrunk buffer.

### Two-Tier Reconfiguration Strategy:
1. **Tier 1: Fast/Safe Live Adjustments (No Buffer Reallocation)**
   - Adjusting color order (`EOrder`) on the fly $\rightarrow$ Supported natively by `fl::Channel::applyConfig()` without restarting hardware.
   - Changing strip brightness or gamma $\rightarrow$ Safe at runtime.
2. **Tier 2: Structural Changes (Pins, Strip Resizing, Adding/Removing Strips)**
   - **Recommended Approach: Clean Transaction + Apply**
     1. Command stages modifications to `DeviceConfig`.
     2. `strip apply` or `strip commit`:
        - Either:
          - **Option A (In-Process Restart):** Pause `DrawingTask` via mutex/flag $\rightarrow$ flush active DMA $\rightarrow$ release channels with `FastLED.remove()` $\rightarrow$ reallocate `GFXBase` buffers $\rightarrow$ re-register with `FastLED.add()` $\rightarrow$ reset active effect $\rightarrow$ resume `DrawingTask`.
          - **Option B (Clean Reboot):** Save to LittleFS / NVS and call `esp_restart()`. Guarantees clean DMA hardware states and eliminates all race conditions.

---

## 5. Interactive Debug CLI (`src/debug_cli.cpp`)

We will expose a new `strip` command using our zero-allocation `ParseInt` infrastructure:

### Syntax:
- `strip` or `strip list`:
  Displays configured channels:
  ```
  Ch  Pin  Clock  Count  Order  Chipset  Status
   0   14     --     64    GRB  WS2812B  ACTIVE
   1    5     --    300    RGB  WS2812B  DISABLED
  ```
- `strip set <ch> pin <data_pin> [clock_pin]`: Change GPIO pin.
- `strip set <ch> count <num_leds>`: Change length.
- `strip set <ch> order <RGB|GRB|BRG|...>`: Change color order.
- `strip set <ch> chipset <WS2812B|APA102|...>`: Change chipset.
- `strip add <pin> <count> [order] [chipset] [clock_pin]`: Add a new channel.
- `strip remove <ch>`: Remove a channel.
- `strip apply`: Commit configuration and apply (in-process or reboot).

---

## 6. Phased Implementation Roadmap

### Phase 1: Dynamic `WS281xGFX::AddLEDs()` (The Core Engine)
- Include `<fl/channels/channel.h>`.
- Replace the 8-way `ADD_CHANNEL(i)` macro tower in `src/ws281xgfx.cpp` with a loop calling `FastLED.add(fl::ChannelConfig(...))`.
- Drive it from legacy `#define` values first to verify FastLED 3.10 Channels API links and drives silicon cleanly on `demo` and `tinyled-demo`.

### Phase 2: `DeviceConfig` Strip Topology & JSON Serialization
- Add `StripConfig` struct and `std::vector<StripConfig>` to `include/deviceconfig.h`.
- Implement `DeviceConfig::SerializeStrips()` and `DeviceConfig::DeserializeStrips()`.
- Populate initial defaults from compile-time flags on cold boot.

### Phase 3: Interactive CLI `strip` Command
- Implement `DoStrip` in `src/debug_cli.cpp`.
- Validate pin inputs using `TryParseGpioPin` and `ParseInt`.
- Support listing, editing, and staging strip configurations.

### Phase 4: Lifecycle Management (Live Reconfig vs. Reboot)
- Implement `ApplyStripConfig()`:
  - Phase 4A: Safe reboot workflow (`strip apply` saves and reboots).
  - Phase 4B: In-process teardown and reallocation (pausing `DrawingTask`, `FastLED.remove()`, buffer resize).

### Phase 5: Web UI & REST Endpoints
- Expose `/api/strips` REST endpoints (GET / POST) in `src/nd_network.cpp` / web server.
- Provide simple React configuration pane in `site/` for strip layout.
