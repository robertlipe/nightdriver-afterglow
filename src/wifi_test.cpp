//+--------------------------------------------------------------------------
//
// File:        wifi_test.cpp
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

#include "globals.h"
#include "nd_network.h"
#include "soundanalyzer.h"
#include "systemcontainer.h" // For g_ptrSystem
#include "taskmgr.h" // For NightDriverTaskManager
#include "webserver.h"
#include "wifi_test.h"
#include "wifi_test_config.h" // For test configuration parameters

#if ENABLE_WIFI && ENABLE_WIFI_TEST_MODE

// --- Globals for credential backup/restore ---
static String g_backup_ssid;
static String g_backup_password;
static WifiCredSource g_backup_source = WifiCredSource::CaptivePortal; // Default
static bool g_credentials_backed_up = false;

// --- Dynamically Constructed Credentials ---
static String g_mistypedPassword;

// --- Helper Functions for Test Execution ---

// Backup existing WiFi credentials before running tests
void backupWiFiCredentials() {
    g_credentials_backed_up = false;
    const WifiCredSource sources[] = {
        WifiCredSource::CaptivePortal,
        WifiCredSource::ImprovCreds,
        WifiCredSource::CompileTimeCreds
    };

    for (const auto& source : sources) {
        if (ReadWiFiConfig(source, g_backup_ssid, g_backup_password)) {
            g_backup_source = source;
            g_credentials_backed_up = true;
            debugI("TEST: Backed up credentials from source %d for SSID '%s'", (int)source, g_backup_ssid.c_str());
            return;
        }
    }
    debugI("TEST: No existing credentials found to back up.");
}

// Restore WiFi credentials after tests are complete
void restoreWiFiCredentials() {
    g_ptrSystem->GetWebServer().SetCaptivePortalActive(false); // Ensure portal is considered inactive
    if (g_credentials_backed_up) {
        debugI("TEST: Restoring credentials for SSID '%s' to source %d", g_backup_ssid.c_str(), (int)g_backup_source);
        // Clear any test credentials first
        ClearWiFiConfig(WifiCredSource::CaptivePortal);
        ClearWiFiConfig(WifiCredSource::ImprovCreds);
        ClearWiFiConfig(WifiCredSource::CompileTimeCreds);
        if (!WriteWiFiConfig(g_backup_source, g_backup_ssid, g_backup_password)) {
            debugE("TEST: FAILED to restore WiFi credentials.");
        }
    } else {
        debugI("TEST: No credentials to restore. Leaving system to start captive portal via main loop.");
        // Main loop will handle starting the portal now.
    }
}

// Set WiFi Credentials
bool setWiFiCredentials(const char* ssid, const char* password) {
    debugI("TEST: Setting WiFi credentials: SSID='%s'", ssid);
    // Clear existing credentials for a clean slate
    ClearWiFiConfig(WifiCredSource::CaptivePortal);
    ClearWiFiConfig(WifiCredSource::ImprovCreds);
    ClearWiFiConfig(WifiCredSource::CompileTimeCreds);
    return WriteWiFiConfig(WifiCredSource::CaptivePortal, ssid, password);
}

// Clear All WiFi Credentials
void clearAllWiFiCredentials() {
    debugI("TEST: Clearing all WiFi credentials.");
    ClearWiFiConfig(WifiCredSource::CaptivePortal);
    ClearWiFiConfig(WifiCredSource::ImprovCreds);
    ClearWiFiConfig(WifiCredSource::CompileTimeCreds);
    if (g_ptrSystem->HasWebServer()) {
        g_ptrSystem->GetWebServer().SetCaptivePortalActive(false); // Ensure captive portal flag is reset
        g_ptrSystem->GetWebServer().Stop(); // Stop the server entirely for a clean test slate
    }
}

// Expect STA Connection
bool expectStaConnection(const char* ssid, const char* password, uint32_t timeoutMs, wl_status_t expectedStatus) {
    debugI("TEST: Expecting STA connection to '%s' with status '%d' within %u ms.", ssid, expectedStatus, timeoutMs);
    unsigned long startTime = millis();
    bool expectedStatusReached = false; // Flag to indicate if the expected status was seen

    // Pause the ADC before touching the WiFi radio to avoid the
    // "cache disabled but cached memory region accessed" panic that
    // occurs when WiFi channel association disables the flash cache
    // while the continuous-ADC DMA ISR is still executing.
    //
    // NOTE: This pause is now held for the entire test run (see WiFiTestLoopEntry).
    // This per-operation pause/resume is kept only as documentation of the danger;
    // the effective guard is the global pause at task startup.

    // Ensure previous connections are cleared for a clean test without tearing down the radio
    WiFi.disconnect(false, true);
    WiFi.softAPdisconnect(false);

    // Switch directly to STA mode to avoid Arduino core memory leak when turning radio completely off
    bool success = nd_network::SetWiFiMode(nd_network::WiFiMode::STA);
    if (!success) {
        debugW("TEST: Failed to set STA mode in expectStaConnection.");
    }
    delay(100); // Give time for disconnect and driver teardown to process

    debugI("TEST: Attempting WiFi.begin('%s', '%s')", ssid, password);
    WiFi.begin(ssid, password);

    unsigned long reportInterval = timeoutMs / 10; // Report progress 10 times
    if (reportInterval == 0) reportInterval = 100;

    for (unsigned long elapsed = 0; elapsed < timeoutMs; elapsed += reportInterval) {
        wl_status_t currentStatus = WiFi.status();
        debugI("TEST: Waiting for connection... %lu/%u ms. Current status: %d (%s)", elapsed, timeoutMs, currentStatus, nd_network::WLtoString(currentStatus));

        if (currentStatus == expectedStatus) {
            expectedStatusReached = true;
            break; // Expected status reached, break early
        }
        delay(reportInterval);
    }

    // After the loop, check if the expected status was reached during polling
    if (expectedStatusReached) {
        debugI("TEST: STA connection to '%s' successful with expected status '%d'. Final status: %d (%s)", ssid, expectedStatus, WiFi.status(), nd_network::WLtoString(WiFi.status()));
    } else {
        debugE("TEST: STA connection to '%s' FAILED within %u ms. Expected status: %d, Final status: %d (%s)", ssid, timeoutMs, expectedStatus, WiFi.status(), nd_network::WLtoString(WiFi.status()));
    }
    return expectedStatusReached;
}

// Expect AP Mode
bool expectAPMode(uint32_t timeoutMs) {
    debugI("TEST: Expecting AP mode within %u ms.", timeoutMs);
    unsigned long startTime = millis();
    bool apModeActive = false;

    unsigned long reportInterval = timeoutMs / 10; // Report progress 10 times
    if (reportInterval == 0) reportInterval = 100;

    delay(100); // Give the system a moment to settle before checking mode

    for (unsigned long elapsed = 0; elapsed < timeoutMs; elapsed += reportInterval) {
        wifi_mode_t currentMode = WiFi.getMode();
        debugI("TEST: Waiting for AP mode... %lu/%u ms. Current mode: %d", elapsed, timeoutMs, currentMode);

        if (currentMode == WIFI_AP_STA) {
            debugW("TEST: Detected WIFI_AP_STA, attempting to force to WIFI_AP.");
            nd_network::SetWiFiMode(nd_network::WiFiMode::AP);
            currentMode = WiFi.getMode(); // Re-check mode after attempting to force
            debugI("TEST: Mode after force attempt: %d", currentMode);
        }

        if (currentMode == WIFI_AP && WiFi.softAPgetStationNum() >= 0) { // Check for active AP
            apModeActive = true;
            break;
        }
        delay(reportInterval);
    }

    if (apModeActive) {
        debugI("TEST: AP mode active. SSID: %s", WiFi.softAPSSID().c_str());
    } else {
        debugE("TEST: AP mode FAILED to activate within %u ms. Current mode: %d", timeoutMs, WiFi.getMode());
    }
    return apModeActive;
}

// Disable AP Mode (e.g., after a test that uses AP mode)
bool disableAPMode() {
    debugI("TEST: Disabling AP mode.");
    if (g_ptrSystem->HasWebServer()) {
        g_ptrSystem->GetWebServer().Stop();
    }
    WiFi.disconnect(false, true);
    WiFi.softAPdisconnect(false);

    // Instead of completely leaving it off, ensure it's in STA mode
    if (nd_network::SetWiFiMode(nd_network::WiFiMode::STA)) { // Attempt to switch to STA to disable AP
        debugI("TEST: AP mode successfully disabled (switched to STA).");
        return true;
    }
    debugE("TEST: Failed to disable AP mode.");
    return false;
}

// Wait for Human Captive Portal Submission
bool waitForCaptivePortalSubmission(uint32_t timeoutMs) {
    debugI("TEST: Waiting for human to submit credentials via Captive Portal... timeout: %u ms", timeoutMs);

    // Check if new credentials appear in NVS
    String newSsid, newPass;
    unsigned long reportInterval = timeoutMs / 10;
    if (reportInterval == 0) reportInterval = 1000;

    for (unsigned long elapsed = 0; elapsed < timeoutMs; elapsed += reportInterval) {
        if (ReadWiFiConfig(WifiCredSource::CaptivePortal, newSsid, newPass)) {
            // Found some credentials! Let's verify they are different from dummy if they were dummy,
            // or just the fact that they exist if we cleared them prior.
            if (newSsid.length() > 0 && newSsid != TEST_DUMMY_SSID) {
                debugI("TEST: Human successfully submitted new credentials via portal. SSID: '%s'", newSsid.c_str());
                return true;
            }
        }

        debugI("TEST: Waiting for human captive portal interaction... %lu/%u ms. Please connect to 'NightDriver_%s' and enter credentials.", elapsed, timeoutMs, nd_network::GetMacAddress("").substring(6).c_str());
        delay(reportInterval);
    }

    debugE("TEST: Timed out waiting for human interaction on Captive Portal.");
    return false;
}

// --- Test Cases Definition ---

// Test for Harrie Case (No Credentials / SSID Not Found)
WiFiTestStep harrieCaseSteps[] = {
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "Starting Harrie Case Test"),
    WiFiTestStep(WiFiTestCommand::CLEAR_ALL_CREDENTIALS),
    WiFiTestStep(WiFiTestCommand::EXPECT_STA_CONNECTION, TEST_NON_EXISTENT_SSID, "dummy_pass", 5000, WL_NO_SSID_AVAIL), // Expect failure to connect
    WiFiTestStep(WiFiTestCommand::START_CAPTIVE_PORTAL), // Explicitly start portal after STA failure
    WiFiTestStep(WiFiTestCommand::EXPECT_AP_MODE, nullptr, nullptr, TEST_SHORT_TIMEOUT_MS), // Should enter AP mode within ~30s timeout + some buffer
    WiFiTestStep(WiFiTestCommand::DISABLE_AP_MODE),
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "Harrie Case Test Complete")
};
WiFiTestCase harrieCase("Harrie Case", harrieCaseSteps, sizeof(harrieCaseSteps) / sizeof(WiFiTestStep));

// Test for Mistyped Password Case
WiFiTestStep mistypedPasswordCaseSteps[] = {
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "Starting Mistyped Password Case Test"),
    WiFiTestStep(WiFiTestCommand::CLEAR_ALL_CREDENTIALS),
    WiFiTestStep(WiFiTestCommand::SET_CREDENTIALS, cszSSID, nullptr), // Password gets set dynamically in setup
    WiFiTestStep(WiFiTestCommand::EXPECT_STA_CONNECTION, cszSSID, nullptr, TEST_SHORT_TIMEOUT_MS, WL_DISCONNECTED), // Expect connection failure
    WiFiTestStep(WiFiTestCommand::EXPECT_AP_MODE, nullptr, nullptr, 40000), // Should enter AP mode after 30s timeout + buffer
    WiFiTestStep(WiFiTestCommand::DISABLE_AP_MODE),
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "Mistyped Password Case Test Complete")
};
WiFiTestCase mistypedPasswordCase("Mistyped Password Case", mistypedPasswordCaseSteps, sizeof(mistypedPasswordCaseSteps) / sizeof(WiFiTestStep));

// Test for Correct Password Case
WiFiTestStep correctPasswordCaseSteps[] = {
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "Starting Correct Password Case Test"),
    WiFiTestStep(WiFiTestCommand::CLEAR_ALL_CREDENTIALS),
    WiFiTestStep(WiFiTestCommand::SET_CREDENTIALS, cszSSID, cszPassword),
    WiFiTestStep(WiFiTestCommand::EXPECT_STA_CONNECTION, cszSSID, cszPassword, TEST_SHORT_TIMEOUT_MS, WL_CONNECTED), // Expect successful connection
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "Correct Password Case Test Complete")
};
WiFiTestCase correctPasswordCase("Correct Password Case", correctPasswordCaseSteps, sizeof(correctPasswordCaseSteps) / sizeof(WiFiTestStep));

// Test for clearsettings + startportal robustness
WiFiTestStep clearSettingsStartPortalCaseSteps[] = {
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "Starting clearsettings + startportal Robustness Test"),
    WiFiTestStep(WiFiTestCommand::CLEAR_ALL_CREDENTIALS), // Simulate clearsettings
    WiFiTestStep(WiFiTestCommand::SET_CREDENTIALS, TEST_DUMMY_SSID, TEST_DUMMY_PASSWORD), // Set dummy credentials to trigger STA attempt/failure
    WiFiTestStep(WiFiTestCommand::EXPECT_STA_CONNECTION, TEST_DUMMY_SSID, TEST_DUMMY_PASSWORD, TEST_SHORT_TIMEOUT_MS, WL_DISCONNECTED), // Expect STA connection to fail
    WiFiTestStep(WiFiTestCommand::START_CAPTIVE_PORTAL), // Explicitly start portal after STA failure
    WiFiTestStep(WiFiTestCommand::EXPECT_AP_MODE, nullptr, nullptr, TEST_AP_STABILIZE_MS), // Expect AP
    WiFiTestStep(WiFiTestCommand::DISABLE_AP_MODE),
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "clearsettings + startportal Robustness Test Complete")
};
WiFiTestCase clearSettingsStartPortalCase("clearsettings + startportal Robustness Case", clearSettingsStartPortalCaseSteps, sizeof(clearSettingsStartPortalCaseSteps) / sizeof(WiFiTestStep));


// Human-Guided Captive Portal Case
WiFiTestStep humanGuidedCPCaseSteps[] = {
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "\n\n*** STARTING HUMAN-GUIDED CAPTIVE PORTAL TEST ***\n\n"),
    WiFiTestStep(WiFiTestCommand::CLEAR_ALL_CREDENTIALS),
    WiFiTestStep(WiFiTestCommand::SET_CREDENTIALS, TEST_DUMMY_SSID, TEST_DUMMY_PASSWORD), // Set dummy credentials to force portal
    WiFiTestStep(WiFiTestCommand::EXPECT_STA_CONNECTION, TEST_DUMMY_SSID, TEST_DUMMY_PASSWORD, TEST_SHORT_TIMEOUT_MS, WL_DISCONNECTED),
    WiFiTestStep(WiFiTestCommand::START_CAPTIVE_PORTAL),
    WiFiTestStep(WiFiTestCommand::EXPECT_AP_MODE, nullptr, nullptr, TEST_AP_STABILIZE_MS),
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "TEST: Action Required! Please connect your phone/laptop to the 'NightDriver_*' AP and submit valid credentials via the portal."),
    WiFiTestStep(WiFiTestCommand::WAIT_FOR_CAPTIVE_PORTAL_SUBMISSION, nullptr, nullptr, 300000), // Wait up to 5 minutes for human
    WiFiTestStep(WiFiTestCommand::DISABLE_AP_MODE),
    WiFiTestStep(WiFiTestCommand::LOG_MESSAGE, nullptr, nullptr, 0, WL_IDLE_STATUS, "\n\n*** HUMAN-GUIDED CAPTIVE PORTAL TEST COMPLETE ***\n\n")
};
WiFiTestCase humanGuidedCPCase("Human-Guided Captive Portal Case", humanGuidedCPCaseSteps, sizeof(humanGuidedCPCaseSteps) / sizeof(WiFiTestStep));

// Global array of pointers to all test cases
WiFiTestCase* g_wifiTestCases[] = {
    &harrieCase,
    &mistypedPasswordCase,
    &correctPasswordCase,
    &clearSettingsStartPortalCase,
    &humanGuidedCPCase
};
size_t g_numWiFiTestCases = sizeof(g_wifiTestCases) / sizeof(WiFiTestCase*);



// --- Test Environment Setup / Teardown ---

static void SetupDynamicCredentials() {
    if (cszPassword && strlen(cszPassword) > 0) {
        g_mistypedPassword = String(cszPassword) + "_broken";
    } else {
        g_mistypedPassword = "broken_password";
    }

    // Inject dynamic broken password into the mistyped case steps
    mistypedPasswordCaseSteps[2].password = g_mistypedPassword.c_str(); // SET_CREDENTIALS
    mistypedPasswordCaseSteps[3].password = g_mistypedPassword.c_str(); // EXPECT_STA_CONNECTION
}

static void PauseAdcForTesting() {
    // WiFi.mode(WIFI_STA) in setup() starts the RF radio, which continuously
    // scans for networks in the background. Each scan briefly disables the
    // flash cache. adc_hal_get_reading_result() is in flash (missing IRAM_ATTR
    // in this Arduino3 prebuilt libhal.a), so the ADC continuous DMA ISR will
    // fault if it fires during any cache-disable window.
    //
    // The test thread does not need audio. Hold the ADC paused for the entire
    // test run. The audio task initializes the ADC handle, so wait until it has
    // done so before calling Pause().
    constexpr uint32_t kAdcInitWaitMs = 2000;
    uint32_t waitStart = millis();
    while (!g_Analyzer.IsADCHandleValid() && (millis() - waitStart < kAdcInitWaitMs)) {
        delay(50);
    }
    if (g_Analyzer.IsADCHandleValid()) {
        debugI("TEST: Pausing ADC for test duration (adc_hal IRAM_ATTR bug workaround).");
        g_Analyzer.Pause();
    } else {
        debugW("TEST: ADC handle not valid after %u ms; skipping pause.", kAdcInitWaitMs);
    }
}

static void ResumeAdcFromTesting() {
    g_Analyzer.Resume();
    debugI("TEST: ADC resumed.");
}

// --- Step and Case Execution ---

static bool ExecuteTestStep(const WiFiTestStep& step) {
    bool stepPassed = false;
    switch (step.command) {
        case WiFiTestCommand::LOG_MESSAGE:
            debugI("TEST MESSAGE: %s", step.message);
            stepPassed = true; // Logging always passes
            break;

        case WiFiTestCommand::SET_CREDENTIALS:
            stepPassed = setWiFiCredentials(step.ssid, step.password);
            break;

        case WiFiTestCommand::CLEAR_ALL_CREDENTIALS:
            clearAllWiFiCredentials();
            stepPassed = true; // Clear credentials always passes unless NVS fails
            break;

        case WiFiTestCommand::EXPECT_STA_CONNECTION:
            stepPassed = expectStaConnection(step.ssid, step.password, step.timeoutMs, step.expectedStatus);
            break;

        case WiFiTestCommand::EXPECT_AP_MODE:
            stepPassed = expectAPMode(step.timeoutMs);
            // After expecting AP mode, make sure to kick the webserver to start it
            if (stepPassed && !g_ptrSystem->GetWebServer().IsCaptivePortalActive()) {
                debugI("TEST: WebServer not active, starting Captive Portal WebServer explicitly.");
                nd_network::StartCaptivePortal(); // This call will initiate the AP, if not already
            }
            break;

        case WiFiTestCommand::DISABLE_AP_MODE:
            stepPassed = disableAPMode();
            break;

        case WiFiTestCommand::START_CAPTIVE_PORTAL:
            debugI("TEST: Explicitly starting Captive Portal.");
            nd_network::StartCaptivePortal();
            delay(1000); // Give the captive portal time to start and stabilize
            stepPassed = true; // Assume success for now, expectAPMode will verify
            break;

        case WiFiTestCommand::WAIT_FOR_MS:
            debugI("TEST: Waiting for %u ms.", (unsigned int)step.timeoutMs);
            delay(step.timeoutMs);
            stepPassed = true;
            break;

        case WiFiTestCommand::REBOOT_DEVICE:
            debugI("TEST: Requesting device reboot for next test phase.");
            // This will restart the system, so the next loop iteration won't happen.
            // The next test phase would start after reboot.
            esp_restart();
            break;

        case WiFiTestCommand::WAIT_FOR_CAPTIVE_PORTAL_SUBMISSION:
            stepPassed = waitForCaptivePortalSubmission(step.timeoutMs);
            break;

        default:
            debugE("TEST ERROR: Unknown command in test step.");
            stepPassed = false;
            break;
    }
    return stepPassed;
}

static bool RunTestCase(WiFiTestCase* currentCase) {
    bool testCasePassed = true;
    for (size_t j = 0; j < currentCase->numSteps; ++j) {
        const WiFiTestStep& step = currentCase->steps[j];
        debugI("--- Step %u of %u: Command %d ---", (unsigned int)(j + 1), (unsigned int)currentCase->numSteps, (int)step.command);

        bool stepPassed = ExecuteTestStep(step);

        if (!stepPassed) {
            testCasePassed = false;
            debugE("===== Test Step FAILED in %s: Command %d =====", currentCase->name, (int)step.command);
            break; // Exit current test case on first failure
        } else {
             debugI("TEST Step PASSED in %s: Command %d", currentCase->name, (int)step.command);
        }
    }
    return testCasePassed;
}

// --- WiFi Test Loop Task Entry Point ---
void WiFiTestLoopEntry(void* pvParameters) {
    debugI("Starting WiFi Test Loop Task.");

    SetupDynamicCredentials();
    PauseAdcForTesting();

    // Give some time for system to fully initialize
    delay(TEST_BOOT_STABILIZE_MS);

    backupWiFiCredentials();

    size_t passedCases = 0;
    size_t failedCases = 0;

    for (size_t i = 0; i < g_numWiFiTestCases; ++i) {
        WiFiTestCase* currentCase = g_wifiTestCases[i];
        debugI("===== Running Test Case %u of %u: %s =====", (unsigned int)(i + 1), (unsigned int)g_numWiFiTestCases, currentCase->name);

        bool testCasePassed = RunTestCase(currentCase);

        if (testCasePassed) {
            debugI("===== Test Case %s: PASSED =====", currentCase->name);
            passedCases++;
        } else {
            debugE("===== Test Case %s: FAILED =====", currentCase->name);
            failedCases++;
        }
        delay(5000); // Small delay between test cases
    }

    debugI("===== All WiFi Tests Completed: %u PASSED, %u FAILED of %u =====", (unsigned int)passedCases, (unsigned int)failedCases, (unsigned int)g_numWiFiTestCases);

    restoreWiFiCredentials();

    // Resume ADC before handing back to the normal network thread.
    ResumeAdcFromTesting();

    debugI("TEST: Starting main network loop to connect with restored credentials or run captive portal.");
    g_ptrSystem->GetTaskManager().StartNetworkThread();

    debugI("TEST: Test task complete. Exiting.");
    vTaskDelete(NULL); // Delete the current task
}

#endif // ENABLE_WIFI && ENABLE_WIFI_TEST_MODE
