# NightDriverStrip - Custom Changes (Robert Lipe Fork)

This file documents the custom reliability, diagnostic, and performance improvements made to the NightDriverStrip firmware for running on ESP32-C6 and similar hardware.

---

## 🛠️ Reliability & Crash Fixes

### 1. Single-Core Race Condition Fix (`src/ledbuffer.cpp`)
- **Issue**: Preemption in FreeRTOS on the single-core ESP32-C6 was causing rare but consistent null-pointer dereferences (crashes 6-8 times per day) when OLED or telemetry tasks called `AgeOfOldestBuffer()` concurrently with frame rendering.
- **Fix**: Wrapped buffer accesses under `g_buffer_mutex` using `std::shared_ptr` to ensure thread-safe peek and draw logic, eliminating the crashes entirely.

### 2. Float Conversion for Hardware FPU (`src/ledbuffer.cpp`)
- **Optimization**: Cast time calculations (`MICROS_PER_SECOND`) to `(float)` instead of `(double)` to leverage the ESP32-C6's hardware single-precision floating point unit and avoid emulated double-precision overhead.

### 3. Fader Recovery & Fading Math Fixes
- **Fader Lock Fix (`src/drawing.cpp`)**: When switching between local effects and network-driven color streams (WLED, UDP, etc.), the fader could get stuck at 0, leaving the LED strips pitch black while previews remained colorful. Added `g_Values.Fader = 255` in `WiFiDraw` to force fader recovery when network streams resume.
- **Fade Division Fix (`src/effectmanager.cpp`)**: Fixed integer division rounding errors inside `ApplyFadeLogic` that caused intermediate fade levels to snap to complete darkness.

### 4. Exception Safety & Web Server Memory Leaks (`src/webserver.cpp`)
- **Fix**: Replaced raw `new AsyncJsonResponse()` allocations with `std::unique_ptr<AsyncJsonResponse>`. When JSON buffers overflowed during large effect or setting spec serialization, the early-return paths were previously leaking the raw response allocations. Now they are automatically cleaned up.

### 5. Software Watchdog / Deadlock Recovery (`src/taskmgr.cpp`)
- **Issue**: The standard Task Watchdog Timer (TWDT) only registers CPU Idle tasks. When application tasks deadlock or block on a mutex, they yield the CPU, allowing the Idle tasks to run and feed the TWDT. Consequently, the hardware/software watchdogs fail to detect task freezes.
- **Fix**: Added atomic heartbeat timestamps (`LastLoopHeartbeat`, `LastDrawHeartbeat`) to `g_Values`. Instrumented the main `loop()` and `Draw` thread to continuously update their respective heartbeats. Inside the CPU Idle task (`IdleTask::ProcessIdleTime()`), a supervisor checks the ages of these heartbeats every second. If either thread is frozen or deadlocked for more than 30 seconds (and no OTA update is in progress), it prints a diagnostic error directly to `Serial` and performs a hard reset via `ESP.restart()`.

---

## 📊 Diagnostics & Diagnostic Console Commands

### 1. In-Memory Circular Logging (`dmesg`)
- **Feature**: Added an in-memory `256` line circular log buffer (`dmesg`) inside `ConsoleManager`. This allows capturing boot logs and system events directly in RAM.
- **Optimization**: Modified logging callbacks to format using stack buffers (`char stack_buf[256]`) and insert entries using `std::string::assign`. Once full, the buffer performs **zero heap allocations** at steady state, preventing heap fragmentation.

### 2. Status Log Silencing
- **Feature**: Added the `g_Values.ShowStatusLog` flag to allow turning off the noisy 5-second periodic status log (`WiFi: ..., Mem: ..., LED FPS: ...`).
- **Buffer Protection**: Periodic status logs are automatically filtered out of the `dmesg` buffer so it only contains actual warnings, errors, and system events.

### 3. WebSocket Event Logging & Connection Tracking
- **Feature**: Registered event callbacks for both color data (`/ws/frames`) and effect status (`/ws/effects`) WebSockets to log connect, disconnect, and socket error events with client ID and remote IP. This helps identify "ghost" socket build-up when browser clients sleep.
- **HTTP Server Logging**: Replaced raw `Serial.printf` calls with `debugI` inside `ServeEmbeddedFile` so web requests are captured in `dmesg` and telnet logs.

### 4. CLI Debug Commands (`src/debug_cli.cpp` & `src/network.cpp`)
- **`dmesg`**: Prints the circular log buffer to the active terminal session.
- **`statuslog [on|off]`**: Disables/enables the periodic 5-second console status print.
- **`stats` update**: Now outputs the number of active WebSocket clients (`WS  : frames:X effects:Y`) to diagnose socket exhaustion.
- **`heap` update**: Replaced the direct ESP-IDF `heap_caps_print_heap_info` call (which writes directly to raw serial stdout) with a custom query and format using `cli_printf`. This enables the `heap` command to print correctly over telnet as well as serial.

---

## ⏰ NTP Clock Optimization & Native SNTP

### 1. ESP-IDF Native SNTP Integration
- **Migration**: Replaced the custom UDP-based NTP packet client (`src/ntptimeclient.cpp`) with the native ESP-IDF LwIP SNTP service, leveraging standard OS-level networking.
- **Smooth Slewing**: Configured the clock sync mode to `SNTP_SYNC_MODE_SMOOTH`. This instructs ESP-IDF to use `adjtime()` to gradually slew the system clock for offsets up to 35 minutes, eliminating sudden time steps and rendering jitter. Large offsets (e.g. at boot) are still corrected instantly.
- **Sync Interval**: Customized the sync interval to 15 minutes (900,000 ms) instead of the default 1 hour, maintaining clock accuracy against hardware tick drift without spamming requests.
- **Simplified Main Loop**: Modified `UpdateNTPTime()` in `src/network.cpp` to only initialize the native SNTP client once at boot. ESP-IDF manages all periodic updates and timing corrections in the background.
- **Reduced Log Spam**: Removed periodic UDP sync debug outputs, logging only a single info message via the time sync callback (`time_sync_notification_cb`) when a synchronization completes.
- **LwIP Deadlock Fix (Safe Callback Logging)**: Fixed a deadlock where the background SNTP/LwIP network thread would call `debugI` inside the callback, blocking on the `ConsoleManager` mutex while another thread holding that mutex was blocked waiting for LwIP socket operations. The callback now stores the sync event in an atomic flag and `timeval` structure, which are processed and logged safely on the network and main threads via `NTPTimeClient::ProcessPendingSyncNotification()`.

