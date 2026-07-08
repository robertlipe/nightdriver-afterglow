# WiFi State Management and Captive Portal Design

## Overview

This document outlines the design, implementation details, and rationale behind the WiFi state machine and Access Point (AP) captive portal functionality. Its purpose is to explain the desired user experience for WiFi configuration and to serve as a technical reference for maintaining and extending this system.

## 1. Core Scenarios & Design Goals

The primary goal of the WiFi subsystem is to reliably connect to a known network while providing a user-friendly fallback for configuration when needed. The logic is designed around several key real-world scenarios.

### 1.1. The "Harrie Case" (First-Time Setup / No Known Network)

*   **Scenario:** The device has no stored WiFi credentials, or no known SSIDs are visible.
*   **Goal:** Quickly provide the user with a way to configure the device.
*   **Behavior:** The system attempts to connect for a **short timeout** (30 seconds). Upon failure (specifically `WL_NO_SSID_AVAIL`), it launches the captive portal AP. This behavior is also triggered when the device reboots after receiving new credentials via the captive portal or Improv if it cannot connect to the specified network within the short timeout.

### 1.2. The "Dave Case" (Known Network Temporarily Unavailable)

*   **Scenario:** The device has valid credentials for a network that is temporarily unreachable (e.g., router rebooting, temporary outage).
*   **Goal:** Patiently wait for the known network to return without forcing the user to reconfigure.
*   **Behavior:** The system will continuously attempt to reconnect for a **long timeout** (15 minutes). Only after this extended period will it give up and launch the captive portal AP. This gives routers plenty of time to reboot after power outages without all smart devices immediately defaulting to AP mode.

### 1.3. The "Mistyped Password Case" (Authentication Failure)

*   **Scenario:** The device is trying to connect to a visible, known SSID, but authentication fails due to incorrect credentials. This can also occur after a fresh reboot following credential submission via the captive portal or Improv.
*   **Goal:** Fail fast and allow the user to quickly re-enter their credentials.
*   **Behavior:** The system attempts to connect for a **short timeout** (30 seconds). Upon failure (specifically `WL_CONNECT_FAILED`), it launches the captive portal AP.

### 1.4. The "Fresh Boot / Unestablished Connection" Case

*   **Scenario:** The device has just booted up, and a connection attempt is being made, but it has not yet successfully connected to any network during the current uptime.
*   **Goal:** Provide quick feedback if the first connection attempt fails.
*   **Behavior:** If a connection fails and the device has not yet established a successful connection in the current boot cycle, it will always revert to the **short timeout** (30 seconds) before launching the captive portal AP. This ensures that a user who has just entered credentials (even if correct, but the network is temporarily unavailable) isn't subjected to the full 15-minute "Dave Case" timeout before getting a chance to re-enter them or seeing the portal.

### 1.5. Robust Mode Transitions

*   **Scenario:** The system needs to switch between WiFi modes (e.g., from Station `WIFI_STA` to Access Point `WIFI_AP`). Forceful or rapid transitions, such as issuing a `startportal` command after `clearsettings`, could historically cause the WiFi stack to "wedge" or become unresponsive.
*   **Goal:** Ensure all mode transitions are stable and reliable.
*   **Behavior:** All functions that change the WiFi mode now use a hardened helper function (`SetWiFiMode`) that explicitly handles tearing down the previous mode before setting the new one, polling to confirm the change.

## 2. Implemented Solution

To achieve the behavior described above, the following key components were implemented.

### 2.1. Dedicated FreeRTOS Network Task & State-Aware Timeouts

The legacy WiFi connection logic was monolithic and blocked the main `loop()`. The solution was to decouple this completely into a dedicated FreeRTOS background task: `NetworkHandlingLoopEntry` inside `src/nd_network.cpp`. This prevents heavy WiFi stack calls from stuttering LED rendering.

Inside this task, the loop dynamically inspects the connection result from the WiFi driver (`WiFi.status()`) and the device's connection history (`l_servicesStarted` flag). This allows it to intelligently select either the short (30 seconds) or long (15 minutes) timeout before deciding to launch the captive portal. Furthermore, a global atomic flag (`g_resetWifiTimeout`) allows the rest of the system to reset this timeout clock, ensuring the AP fallback timer is always relative to the *last credential change or network reset*, rather than a hard-coded absolute boot time.

### 2.2. Robust WiFi Mode Switching

The `SetWiFiMode` function in `src/network.cpp` was enhanced to be more robust. It now ensures a clean teardown of the current WiFi mode before attempting to set a new one, polls to verify the mode change was successful, and includes delays to allow the WiFi stack to stabilize. This prevents the "wedging" issue seen in rapid state transitions. Additionally, a specific issue where `WiFi.scanNetworks()` implicitly changed the WiFi mode, causing the captive portal AP to temporarily disappear, was resolved by gently resetting the mode with `WiFi.mode(WIFI_AP)` instead of a full stack teardown after scanning.

### 2.3. Memory and Resource Management

To ensure system stability and optimize resource usage in memory-constrained environments, several specific improvements were made:
*   The `servicesStarted` flag was converted to `std::atomic<bool>` in `src/network.cpp` to guarantee thread safety across FreeRTOS tasks.
*   Memory pre-allocation for `_availableNetworks` was implemented in `src/webserver.cpp` using `_availableNetworks.reserve(n)` before populating the list of scanned networks. This prevents multiple reallocations and reduces heap fragmentation during WiFi scanning.

### 2.4. Automated On-Device Testing

A comprehensive, on-device testing framework was created (`src/wifi_test.cpp`, `include/wifi_test.h`). These tests automate the process of putting the device into each of the key scenarios (Harrie, Mistyped Password, etc.) and verify that the system behaves as expected. This is crucial for preventing future regressions in the WiFi state machine.

### 2.5. Static Network Scanning in Captive Portal

A key design choice was made to perform the WiFi network scan (`WiFi.scanNetworks()`) only *once* when the captive portal starts up. The list of available networks is then cached for the duration of the captive portal session.

**Rationale:** The `scanNetworks()` function is a blocking operation that can take several seconds to complete. More importantly, it requires the WiFi radio to change modes, which can disrupt the Access Point service. Triggering a scan from a user action (like a "Refresh" button) within a web server callback would lead to a long, unresponsive delay and could cause the AP to disconnect from the user's device, creating a frustrating and unstable user experience.

By scanning only once at startup, we ensure the web interface remains fast and responsive, and the AP connection is stable while the user is entering their credentials.

## 3. Future Work

### 3.1. AP Inactivity Timeout

**Assessment:** This is a **medium-sized** task with **medium risk**.

*   **Size (Medium):** The change will require coordinated modifications across roughly 5 files (`network.cpp`, `webserver.h`, `webserver.cpp`, `deviceconfig.h`, `deviceconfig.cpp`) to introduce the new timer, the configuration setting, and the timeout logic itself.
*   **Risk (Medium):** The primary risk comes from modifying the main network state loop in `src/network.cpp`. This is a critical part of the system, and any changes have the potential to introduce unintended behavior, such as a reboot loop if the device times out but can't find a network to connect to. The risk is manageable, but requires careful implementation.

**Proposed Implementation Plan:**

1.  **Add an Activity Tracker:** A new timestamp variable (`_lastActivityMillis`) needs to be added to the `CWebServer` class to track the last time a user interacted with the captive portal.
2.  **Record User Activity:** The web server's captive portal request handlers need to be updated to call a new `RecordActivity()` method, which will update this timestamp on every user request.
3.  **Add a Configurable Timeout:** A new setting will be added to the `DeviceConfig` system to allow the idle timeout period to be configured and saved.
4.  **Implement the Timeout Logic:** In the main `NetworkHandlingLoopEntry` function, when the device is in captive portal mode, a check will be added. If the time since the last user activity exceeds the configured timeout, it will trigger the timeout action.
5.  **Trigger a Reboot:** The safest and most robust way to exit the portal and restart the connection process is to use the web server's existing `RequestReboot()` function for a graceful restart.

### 3.2. Bluetooth Low Energy (BLE) for Provisioning

**Rationale:** While the captive portal is a universal method for WiFi provisioning, it requires the user to disconnect from their current WiFi network, connect to the device's AP, and then reconnect later. A more modern and seamless user experience can be achieved using BLE. The device can advertise itself via BLE, and a companion app (or a web app using Web Bluetooth) can securely send WiFi credentials to the device without interrupting the user's existing network connection.

**Existing Work:** The codebase already includes `include/improv.h` and `include/improvserial.h`, which is an existing standard for BLE provisioning. This provides a strong foundation to build upon.

**Proposed Work:**
1.  Add a feature flag `ENABLE_BLE_PROVISIONING`.
2.  Implement the necessary BLE service based on the Improv protocol.
3.  Integrate the received credentials into the existing `WriteWiFiConfig` system, using `WifiCredSource::ImprovCreds`.
4.  Ensure that the main network loop correctly prioritizes these credentials.

## 4. Key Lessons Learned in Debugging

The process of stabilizing this feature revealed several "hard lessons" worth documenting for future developers.

### 4.1. Set State Flags Before Fallible Operations
The memory leak was caused by repeatedly calling `StartCaptivePortal()`. The function was protected by an `IsCaptivePortalActive()` check, but the flag was only set to `true` *after* the `SetWiFiMode()` call, which was a fallible operation. The resulting loop of failures meant the flag was never set.

*   **Lesson:** When transitioning to a new state, set the corresponding "I am transitioning" state flag *before* initiating any operations that might fail. This prevents re-entrant calls into the state transition logic.

### 4.2. Verify Control Flow with Trace Logging
At one point, the device appeared to "hang" after the test suite, with no network activity. The breakthrough came from adding simple print statements to the top of the main `NetworkHandlingLoopEntry` task loop, which revealed the loop was actually stuck in a tight `if (IsCaptivePortalActive())` block due to a stale flag left over from the test suite.

*   **Lesson:** In a complex, multi-threaded embedded system, never assume code is executing the way you think it is. When faced with a "hang" or lack of activity, the first step should be to add verbose trace logs to confirm the actual, real-time control flow.

### 4.3. Be Wary of Build System Caching
We spent significant time trying to disable the WiFi test mode by changing a build flag in `platformio.ini`. The build system did not always pick up the change, causing the test suite to run when we expected it to be disabled.

*   **Lesson:** When a change to a build flag does not produce the expected change in application behavior, do not assume the code logic is wrong. The build system itself may be at fault. Use a more forceful method for diagnostics (e.g., temporarily commenting out code with `#if 0`) to definitively rule out a code path.

### 4.4. Recovery Mechanisms Must Not Depend on the Broken Subsystem
At one point, it was suggested to use the `clearsettings` debug command to fix a bad WiFi state. However, this command requires a working Telnet connection. If the WiFi is down, Telnet is unavailable, creating a catch-22.

*   **Lesson:** The primary recovery mechanism (the captive portal) must be reachable from a "zero-state" or "broken-state" condition and cannot depend on the subsystem it is designed to fix. Our final implementation, which reliably starts the AP from any failed STA state, adheres to this principle.

### 4.5. Test Automation Interference (The "Helpful" Teardown)
During late-stage manual verification, the user correctly submitted Captive Portal credentials and the browser navigated to the "Rebooting..." success page, but the device rebooted into dummy credentials anyway. It turned out the automated test suite successfully detected the manual entry, immediately marked the test as PASSED, and ran its teardown routine (restoring the backup credentials) *before* the 3-second reboot timer fired.
*   **Lesson:** Test teardown routines are dangerous if they race against the real system's asynchronous operations. Automated state management must explicitly wait for the hardware to restart or acknowledge state before wiping it, or manual interaction tests will inevitably "fail successfully".
