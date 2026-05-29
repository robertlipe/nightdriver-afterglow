# HUB75 Display Modernization Roadmap

This document outlines the architectural plan to modernize the HUB75 matrix display driver backend, transitioning from the legacy `SmartMatrix` library to a modern, supported driver stack while preparing the graphics pipeline for future microcontrollers (ESP32-S3, ESP32-P4, RP2350, etc.).

## Current Status & Limitations

The current `HUB75GFX` implementation relies on the **SmartMatrix** library:
*   **Legacy Hardware Only**: SmartMatrix uses classic ESP32 I2S parallel DMA, meaning it does not support post-2020 chips like the ESP32-S3 (which replaced I2S parallel mode with the new `LCD_CAM` peripheral and `GDMA`).
*   **Custom Library Fork**: We depend on a private fork of SmartMatrix to support a custom hardware ChromaKey text caption layer.
*   **Driver-Level Text Compositing**: Caption and label rendering are currently performed in the low-level display driver layer using SmartMatrix layers, creating tight coupling and preventing other matrix displays (like WS2812-based panels) from using these telemetry layouts.

---

## Modernization Strategy

### Phase 1: Upstream Text "De-Cleverization"
Instead of relying on hardware-assisted library layers and ChromaKey overlays in the driver:
*   **Software Compositing**: Move caption and label rendering upstream into the common rendering pipeline. Text is printed directly onto the active frame buffer (`leds[]` array) in software before the frame is dispatched to the display hardware.
*   **Simplification**: Reduces the driver's role to a simple, generic pixel streamer ("here is an array of RGB values, display it").
*   **Feature Portability**: Caption and telemetry layouts immediately become supported on all display devices, including WS2812 strip-matrices.

### Phase 2: Excise SmartMatrix and Evaluate Modern Alternatives
We have two main candidate libraries to replace SmartMatrix. While `ESP32-HUB75-MatrixPanel-DMA` has the most immediate hobbyist mindshare, `esphome-libs/esp-hub75` is a highly compelling, pure C++ option that matches our architecture better:

*   **Option A: [ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA) (mrcodetastic)**
    *   *Pros*: Huge hobbyist community; widely used with Arduino wrappers.
    *   *Cons*: Heavy Arduino coupling (implements `Adafruit_GFX` internally), no native support for ESP32-P4/C6 because it lacks `PARLIO` support.
*   **Option B (Preferred): [esphome-libs/esp-hub75](https://github.com/esphome-libs/esp-hub75) (ESPHome / Nabu Casa)**
    *   *Pros*:
        *   **Multi-Platform out of the box**: Supports GDMA (S3), I2S (ESP32/S2), and PARLIO (P4/C6).
        *   **No Arduino Baggage**: It is a pure C++/ESP-IDF component. It acts solely as a high-performance buffer pump, exposing a clean `draw_pixels(x, y, w, h, buffer, format)` API for raw arrays.
        *   **We already have the Graphics API**: Because NightDriverStrip already pulls in the [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) externally and renders everything into a software framebuffer, we don't need the driver to bundle graphics primitives; we just need a fast, robust RGB[]-to-DMA writer.
        *   **Massive Indirect Testing**: Backed by Nabu Casa for ESPHome. The massive Home Assistant user base acts as a testing laboratory for a wide variety of cheap panels, and timing/chip initialization fixes (FM6126A, ICN2038S, etc.) are pushed directly to this library.

---

### Phase 3: Platform Decoupling for Future Silicon
Neither SmartMatrix nor the mrcodetastic library is portable to non-ESP32 architectures, but Option B (`esp-hub75`) already solves the P4/C6 problem within the Espressif family:
*   **Unified Abstraction**: Keep the `GFXBase` class as the clean boundary separating effect logic from hardware drivers.
*   **Platform-Specific Backends**:
    *   **Espressif Family (ESP32, S3, C6, P4)**: Use `esp-hub75` as the unified backend (leveraging its native I2S, GDMA, and PARLIO drivers).
    *   **RP2040 / RP2350**: Implement a backend using PIO (Programmable I/O) state machine DMA clocks (e.g., Raspberry Pi's Interstate 75 / 75W).
