//+--------------------------------------------------------------------------
//
// File:        wifi_test_config.h
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

// --- WiFi Test Configuration ---
//
// This file centralizes all user-configurable parameters for the automated WiFi test framework.
// Developers should modify the values below to match their test environment.

// --- General Test Parameters ---

// SSID for a non-existent network (Harrie Case)
#define TEST_NON_EXISTENT_SSID "NON_EXISTENT_SSID_XYZ" // Ensure this SSID does not exist in your environment

// --- Dummy Credentials (for clearsettings + startportal robustness test) ---
// These are used for scenarios where some credentials need to be present initially
// but are not critical for actual connection (e.g., to trigger STA mode attempts).
#define TEST_DUMMY_SSID         "TEST_DUMMY_SSID_AP"
#define TEST_DUMMY_PASSWORD     "TEST_DUMMY_PASSWORD"

// --- Test Timeouts ---
// Adjust if your environment or ESP32 model requires longer stabilization times.

#define TEST_SHORT_TIMEOUT_MS   35000   // General timeout for short waits, e.g., Harrie/Mistyped Password
#define TEST_LONG_TIMEOUT_MS    950000  // Timeout for Dave Case (not yet implemented)
#define TEST_BOOT_STABILIZE_MS  5000    // Time to wait after boot for system stabilization
#define TEST_AP_STABILIZE_MS    10000   // Time to wait for AP mode to become active

// --- Test Control Files ---
#define TEST_STATE_FILE         "/test_state.json" // File to store test state across reboots
