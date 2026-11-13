# Architecture & Implementation Plan: Waveshare ESP32-S3-RGB-Matrix Bring-Up

**Target:** Waveshare ESP32-S3-RGB-Matrix (HUB75 Panel Driver, ESP32-S3-WROOM-2-N32R16V)  
**Status:** Phase 1 (Audio ES7210), Phase 2 (Sensors), Phase 3 (RTC), and Phase 6 (Motion & Orientation) Complete.

---

## 1. System Overview & Bus Topology

All onboard peripherals share a single physical master I2C bus on **GPIO 47 (SDA)** and **GPIO 48 (SCL)**, arbitrated by `SystemHardwareHub`:

```
                             +------------------------+
                             |    ESP32-S3 Master     |
                             |  I2C Master (Port 0)   |
                             +-----------+------------+
                                         |
                       +-----------------+-----------------+
                       | SDA: GPIO 47       SCL: GPIO 48   |
                       |                                   |
         +-------------+-------------+-------------+-------+-----+-------------+
         |                           |             |             |             |
   +-----+-----+               +-----+-----+ +-----+-----+ +-----+-----+ +-----+-----+
   |  ES7210   |               |  ES8311   | |  QMI8658  | |  SHTC3    | | PCF85063  |
   |  (0x40)   |               |  (0x18)   | |  (0x6B)   | |  (0x70)   | |  (0x51)   |
   | Mic ADC   |               | Spk DAC   | | 6-Axis IMU| | Climate   | |   RTC     |
   +-----------+               +-----------+ +-----------+ +-----------+ +-----------+
```

---

## 2. Phase Breakdown & Current Status

### ✅ Phase 1: Audio Input Bring-Up & Calibration (DONE)
* **ES7210 Driver:** Implemented clean, standalone driver (`include/es7210.h`, `src/es7210.cpp`).
* **I2S Clocking:** ESP32-S3 acts as I2S master generating MCLK at 6.144 MHz (`256 * 24 kHz`) on GPIO 12, BCLK on GPIO 43, LRCK/WS on GPIO 38.
* **16-Bit Stereo Philips I2S:** Captures interleaved dual-mic PCM data from GPIO 39.
* **DSP & Normalization Tuning:**
  * Analog PGA gain calibrated to **30.0 dB**.
  * Software multiplier set to **1:1** to preserve 16-bit dynamic range without digital clipping.
  * `quietEnvFloorGate` tuned from **100,000.0f** down to **40,000.0f** to allow normal, comfortable room listening levels to punch cleanly through the squelch gate without false triggering on silence.
  * `energyMaxEnvCap` set to **0.0f** (uncapped) and `frameSNRGate` set to **0.0f** to eliminate artificial 4/4 chattering and speech flatlining.
* **Telemetry Gating:** Rate-limited telemetry loops enclosed in `#if DEBUG_AUDIO_PARAMS` to eliminate runtime CPU/memory overhead in production.
* **Designated Initializers:** Modernized all `AudioInputParams` instances in `include/soundanalyzer.h` to C++20 designated initializers.
* **Decoupling:** Gated behind `-DUSE_ES7210_AUDIO=1` instead of direct board macros.

---

### ✅ Phase 2: Onboard Sensors Bring-Up (`SystemHardwareHub`) (DONE)

#### 2.1 Architectural Decoupling & Stubs
* Defined `HAS_SYSTEM_HARDWARE_HUB` macro (enabled on `WAVESHARE_ESP32_S3_RGB_MATRIX` and `MATRIX_S3`).
* Equipped `include/systemhardwarehub.h` with clean inline no-op fallbacks when `HAS_SYSTEM_HARDWARE_HUB=0`, completely removing `#ifdef` guards from call sites.
* Hooked `SystemHardwareHub::Instance().Begin()` into `SensorManager::begin()` and `SystemHardwareHub::Instance().Tick()` into `main.cpp::loop()`.
* Logged live IMU vector `[ax, ay, az]` in `ShowStatusLog` alongside core and ambient metrics.

#### 2.2 SHTC3 Temperature & Relative Humidity (`0x70`)
* **Asynchronous Non-Blocking State Machine:** Replaced synchronous delays (`vTaskDelay`, `delayMicroseconds`) with a timestamped 3-state machine (`Idle` $\rightarrow$ `Waking` $\rightarrow$ `Measuring` $\rightarrow$ `Idle`) that yields to the main loop:
  * **Wakeup:** Transmits `{0x35, 0x17}`, transitions to `Waking`.
  * **Measurement Trigger:** 1 ms later, transmits `{0x78, 0x66}` (normal power, clock stretching disabled, T first), transitions to `Measuring`.
  * **Read & Process:** 15 ms later, reads 6 bytes, validates Sensirion CRC-8 (poly `0x31`, init `0xFF`), converts raw metrics to Fahrenheit and %RH, and puts SHTC3 back to sleep (`{0xB0, 0x98}`) to minimize self-heating.
* Feeds metrics directly into `g_Values.AmbientTemp` and `g_Values.AmbientHumidity`.

#### 2.3 QMI8658 6-Axis IMU (`0x6B`)
* Corrected I2C address to `0x6B` (SA0 pulled high on Waveshare board).
* Confirmed `WHO_AM_I` (`0x00`) returns `0x05`.
* Configured registers:
  * `CTRL1` (`0x02` = `0x60`): Enable address auto-increment.
  * `CTRL2` (`0x03` = `0x15`): Accelerometer $\pm 4g$ range ($8192 \text{ LSB}/g$) and 128 Hz ODR.
  * `CTRL7` (`0x08` = `0x01`): Enable accelerometer engine.
* Reads 6-byte little-endian burst from `0x35` at 50 Hz in `PollInertial()`.

---

### ✅ Phase 3: PCF85063 Real-Time Clock (`0x51`) (DONE)
* **Hardware Device Attached:** Added `rtc_handle` at I2C address `0x51` on shared master bus.
* **Cold Boot Sync:** Added `SyncSystemTimeFromRTC()` on boot in `SystemHardwareHub::Begin()`: checks the `OS` (Oscillator Stop) flag in register `0x04` to ensure power integrity, decodes BCD to `struct tm`, and invokes `settimeofday()` so clock effects have valid real-world wall-clock time offline.
* **NTP Commit:** Implemented `WriteRTC()` with atomic `STOP` bit latching in `Control_1` (`0x00`) to prevent register ripple race conditions during updates.
* **Sync Hook:** Wired `SyncRTCTimeFromSystem()` into `NTPTimeClient::ProcessPendingSyncNotification()` to automatically commit accurate UTC time to the hardware RTC upon achieving NTP synchronization.

---

### ✅ Phase 6: Motion Interactions & Orientation Control (DONE)
* **Auto-Orientation / 180° Inversion:**
  * Supported in both display drivers:
    * `ESPMPDMAGFX`: delegating `setRotation(r)` to `MatrixPanel_I2S_DMA::setRotation()`, transforming coordinates at the DMA level.
    * `ESPHUB75GFX`: mapping `setRotation(r)` to native ESPHome `driver->set_rotation(Hub75Rotation::ROTATE_*)`.
  * Monitored vertical gravity axis $a_x$ in `SystemHardwareHub`: triggers 180° inversion when $a_x < -4.0\text{ m/s}^2$ and normal when $a_x > +4.0\text{ m/s}^2$, debounced across 200 ms to eliminate lap or flip jitter. Verified live on hardware!
  * Added CLI command `rotate [0|180|auto]` to query or manually override rotation.
* **Bezel Tap / Knock Detection:**
  * Computes 3D acceleration jerk magnitude $|\Delta \vec{a}|$ across consecutive 50 Hz IMU samples.
  * Detects sharp mechanical impulse spikes ($|\Delta \vec{a}| > 18.0\text{ m/s}^2$) with 600 ms debouncing, triggering `g_ptrSystem->GetEffectManager().NextEffect()`.
  * Added CLI command `tap [on|off]` to enable/disable tap advancement.
* **Tilt-Reactive Sand (`PatternSMSand.h`):**
  * Reads horizontal tilt vector $a_y$ from `SystemHardwareHub`.
  * Automatically inverts tilt coordinate when display rotation is 180°, keeping physical gravity aligned with real-world down.
  * Biases peak scattering probability towards the lower side.
  * Enables plateau avalanching when $|a_y| > 0.20g$.
  * Drifts the top sand emitter towards the elevated edge so sand streams down the incline.

---

### ✅ Phase 7: Unified Thermal Self-Preservation & Calibration Offset (DONE)
* **Thermal Self-Preservation ("Detector vs Extinguisher"):**
  * **Detector (`SensorManager::EvaluateThermal`):** Single, unified thermal evaluator shared across all temperature sources (DHT11, internal silicon junction sensor, and SHTC3/BME280).
  * **Dual-Tier Tripwires:**
    * *Critical Shutdown (`ThermalThrottle = 0`):* Ambient $\ge 140^\circ\text{F}$ ($60^\circ\text{C}$), Die $\ge 195^\circ\text{F}$ ($90.5^\circ\text{C}$), or Die surge $\ge 25^\circ\text{F/min}$ while $\ge 140^\circ\text{F}$. Blanks LED displays completely.
    * *Warning Throttle (`ThermalThrottle = 25`, ~10% brightness):* Ambient $\ge 125^\circ\text{F}$ ($51.7^\circ\text{C}$), Die $\ge 175^\circ\text{F}$ ($79.4^\circ\text{C}$), Ambient climb $\ge 8^\circ\text{F/min}$ while $\ge 105^\circ\text{F}$, or Die climb $\ge 15^\circ\text{F/min}$ while $\ge 125^\circ\text{F}$.
    * *Hysteresis Recovery:* Ambient $< 115^\circ\text{F}$, Die $< 160^\circ\text{F}$, and climb rate $\le 1^\circ\text{F/min}$ before returning to `Normal` (`ThermalThrottle = 255`).
  * **Extinguisher:** Wired `g_Values.ThermalThrottle` into `HUB75GFX::PostProcessFrame` and `WS281xGFX::PostProcessFrame` to immediately cut thermal dissipation by 90% or 100%.
* **Temperature Calibration Offset (Option C):**
  * Persistent setting `tempOffset` stored in `DeviceConfig` (`"tempOffset"`, default `0.0f`).
  * CLI command `tempoffset [offset]` to inspect and tune calibration in real-time.
  * Safety decoupling: `tempOffset` is applied only to reported user-facing ambient temperature (`g_Values.AmbientTemp`); `EvaluateThermal` operates strictly on raw sensor readings so user offsets never bypass emergency shutdown.
* **Telemetry & Reporting Fixes:**
  * Fixed HUB75 brightness telemetry bug where status logs reported `MatrixScaledBrightness / 2.55` (always 100% regardless of user brightness).
  * Now reports true effective output `g_Values.Brite` computed from $\min(\text{brightness}, \text{fader}, \text{powerLimit}, \text{thermalThrottle})$.
  * Removed duplicate `Power: ... Brite: ...` print from status line. Added `[THROTTLED]` indicator when thermal limiter is active.
  * Added `thermal` CLI command to inspect current thermal state, throttle percentage, and climb rates.

---

### ⏳ Phase 4: Secondary Audio Output (ES8311 DAC + NS4150B PA)
* Keep PA enable (GPIO 11) `LOW` during normal operation.
* When playback support is needed: configure ES8311 via I2C (`0x18`) to receive I2S audio on GPIO 21 (`I2S_DOUT_PIN`) sharing MCLK/BCLK/LRCK, then set GPIO 11 `HIGH`.

---

### ⏳ Phase 5: FastLED 3.10.9 & `demo` Build (`ENABLE_WIFI=0`)
* **Problem:** FastLED 3.10.9 includes `<esp_wifi.h>` unconditionally in `ota_impl.cpp.hpp`, breaking builds where `ENABLE_WIFI=0` strips `esp_wifi` from the include path.
* **Solution Options:**
  1. Determine the compile-time flag FastLED uses to omit its internal OTA subsystem (e.g. `FASTLED_ESP32_DISABLE_OTA` or similar).
  2. Or keep the minimal ESP-IDF WiFi header stubs in the include search path so the preprocessor succeeds even when WiFi drivers are excluded.
* **Mac Mini Volume Access Note:**
  On macOS (Sequoia / upgraded macOS), ssh sessions cannot access `/Volumes/*` unless `sshd-keygen-wrapper` / Terminal has been granted **Full Disk Access** under *System Settings > Privacy & Security > Full Disk Access*.
