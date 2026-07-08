//+--------------------------------------------------------------------------
//
// File:        wifi_test.h
//
// NightDriverStrip - (c) 2026 Plummer's Software LLC.  All Rights Reserved.
//
// This file is part of the NightDriver software project.
//
//    NightDriver is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    NightDriver is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Nightdriver.  It is normally found in copying.txt
//    If not, see <https://www.gnu.org/licenses/>.
//
//---------------------------------------------------------------------------

#pragma once

#include "globals.h"

#include <Arduino.h>

// Define a test mode to enable/disable the WiFi test framework
#ifdef ENABLE_WIFI_TEST_MODE

#include <WiFi.h>

// Forward declarations
void WiFiTestLoopEntry(void* pvParameters);

// --- Test Command Definitions ---
enum class WiFiTestCommand : uint8_t {
    SET_CREDENTIALS,        // Set WiFi credentials (SSID, Password)
    CLEAR_ALL_CREDENTIALS,  // Clear all stored WiFi credentials
    EXPECT_STA_CONNECTION,  // Expect STA connection (SSID)
    EXPECT_AP_MODE,         // Expect device to be in AP mode
    DISABLE_AP_MODE,        // Ensure AP mode is off (e.g., after STA connection)
    START_CAPTIVE_PORTAL,   // Explicitly start Captive Portal
    WAIT_FOR_MS,            // Wait for a specified number of milliseconds
    LOG_MESSAGE,            // Log a custom message to serial
    REBOOT_DEVICE,          // Request a device reboot
    WAIT_FOR_CAPTIVE_PORTAL_SUBMISSION // Wait for human to enter new credentials via CP
};

// --- Test Step Structure ---
struct WiFiTestStep {
    WiFiTestCommand command;
    const char* ssid;       // Used by SET_CREDENTIALS, EXPECT_STA_CONNECTION
    const char* password;   // Used by SET_CREDENTIALS
    uint32_t timeoutMs;     // Used by EXPECT_STA_CONNECTION, EXPECT_AP_MODE, WAIT_FOR_MS, WAIT_FOR_CAPTIVE_PORTAL_SUBMISSION
    wl_status_t expectedStatus; // Used by EXPECT_STA_CONNECTION (e.g., WL_CONNECTED, WL_CONNECT_FAILED)
    const char* message;    // Used by LOG_MESSAGE

    // Constructor for commands with SSID/Password/Timeout/Status
    WiFiTestStep(WiFiTestCommand cmd, const char* s = nullptr, const char* p = nullptr, uint32_t t = 0, wl_status_t es = WL_IDLE_STATUS, const char* msg = nullptr)
        : command(cmd), ssid(s), password(p), timeoutMs(t), expectedStatus(es), message(msg) {}
};

// --- Test Case Structure ---
struct WiFiTestCase {
    const char* name;
    WiFiTestStep* steps;
    size_t numSteps;

    WiFiTestCase(const char* n, WiFiTestStep* s, size_t ns)
        : name(n), steps(s), numSteps(ns) {}
};

// Global array of test cases (defined in .cpp)
extern WiFiTestCase* g_wifiTestCases[];
extern size_t g_numWiFiTestCases;

#endif // ENABLE_WIFI_TEST_MODE
