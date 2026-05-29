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

### Phase 2: Excise SmartMatrix and Adopt `MatrixPanel-DMA`
*   **Target Driver**: Migrate `HUB75GFX` to **`ESP32-HUB75-MatrixPanel-DMA`** (mrcodetastic library).
*   **Immediate S3 Support**: This library features highly optimized LCD_CAM/GDMA driver backends for the ESP32-S3, immediately unlocking modern hardware targets like the **Adafruit MatrixPortal S3**.
*   **Active Community**: We leverage community-driven maintenance for chip errata and future silicon revisions (like the ESP32-S31).

### Phase 3: Platform Decoupling for Future Silicon
Neither SmartMatrix nor the mrcodetastic library is portable to non-ESP32 architectures:
*   **Unified Abstraction**: Keep the `GFXBase` class as the clean boundary separating effect logic from hardware drivers.
*   **Platform-Specific Backends**:
    *   **ESP32 Family (S3, classic)**: Use `ESP32-HUB75-MatrixPanel-DMA`.
    *   **ESP32-P4**: Target the official ESP-IDF `esp_lcd` driver stack (using its parallel RGB engine).
    *   **RP2040 / RP2350**: Implement a backend using PIO (Programmable I/O) state machine DMA clocks (e.g., Raspberry Pi's Interstate 75 / 75W).
