//+--------------------------------------------------------------------------
//
// File:        ntptimeclient.cpp
//
// NightDriverStrip - (c) 2018 Plummer's Software LLC.  All Rights Reserved.
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
// Description:
//
//    Sets the system clock from the specified NTP Server
//
// History:     Jul-12-2018         Davepl      Created for BigBlueLCD
//              Oct-09-2018         Davepl      Copied to LEDWifi project
//---------------------------------------------------------------------------

#include "globals.h"
#include <atomic>
#include <ctime>
#include <mutex>
#include <sys/time.h>
#include <WiFiUdp.h>
#include "nd_network.h"

#include "deviceconfig.h"
#include "ledbuffer.h"
#include "ntptimeclient.h"
#include "systemcontainer.h"
#include "values.h"

#if ENABLE_NTP

#include <esp_sntp.h>

static DRAM_ATTR std::atomic<bool> l_bClockSet{false};
static DRAM_ATTR std::atomic<bool> l_bPendingNotification{false};
static DRAM_ATTR timeval l_pendingTv;
static DRAM_ATTR int64_t l_pendingAdjustmentUs = 0;
static DRAM_ATTR bool l_pendingLog = false;

bool NTPTimeClient::HasClockBeenSet()
{
    return l_bClockSet.load(std::memory_order_acquire);
}

static void time_sync_notification_cb(struct timeval *tv)
{
    if (tv)
    {
        struct timeval now;
        gettimeofday(&now, nullptr);

        int64_t diffUs = 0;
        bool shouldLog = false;

        if (!l_bClockSet.load(std::memory_order_relaxed))
        {
            shouldLog = true;
        }
        else
        {
            diffUs = ((int64_t)tv->tv_sec - now.tv_sec) * 1000000LL + ((int64_t)tv->tv_usec - now.tv_usec);

            if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
            {
                shouldLog = true;
            }
            else if (std::abs(diffUs) >= 2000000LL) // 2 seconds or more
            {
                shouldLog = true;
            }
        }

        l_bClockSet.store(true, std::memory_order_release);
        l_pendingTv = *tv;
        l_pendingAdjustmentUs = diffUs;
        l_pendingLog = shouldLog;
        l_bPendingNotification.store(true, std::memory_order_release);
    }
}

void NTPTimeClient::ProcessPendingSyncNotification()
{
    if (l_bPendingNotification.load(std::memory_order_acquire))
    {
        l_bPendingNotification.store(false, std::memory_order_relaxed);

        if (l_pendingLog)
        {
            timeval tv = l_pendingTv;
            struct tm timeinfo;
            localtime_r(&tv.tv_sec, &timeinfo);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);

            if (l_pendingAdjustmentUs != 0)
            {
                float adjSec = static_cast<float>(l_pendingAdjustmentUs) / 1000000.0f;
                debugI("NTP clock: response received, updated time to: %lld.%06lld (adjustment: %.3fs), Local: %s",
                       (long long)tv.tv_sec,
                       (long long)tv.tv_usec,
                       adjSec,
                       time_str);
            }
            else
            {
                debugI("NTP clock: response received, updated time to: %lld.%06lld, Local: %s",
                       (long long)tv.tv_sec,
                       (long long)tv.tv_usec,
                       time_str);
            }
        }
    }
}

// UpdateClockFromWeb
//
// Initializes or restarts the native ESP-IDF SNTP service
bool NTPTimeClient::UpdateClockFromWeb(WiFiUDP * pUDP)
{
    if (g_Values.UpdateStarted)
    {
        debugW("Update already in progress, skipping native SNTP synchronization.");
        return false;
    }

    if (!esp_sntp_enabled())
    {
        debugI("Initializing native ESP-IDF SNTP with smooth sync...");
        esp_sntp_setoperatingmode(static_cast<esp_sntp_operatingmode_t>(SNTP_OPMODE_POLL));
        esp_sntp_setservername(0, g_ptrSystem->GetDeviceConfig().GetNTPServer().c_str());
        esp_sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
        esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);

        // Sync every 15 minutes (900000 ms) instead of the default 1 hour, to compensate for drift
        esp_sntp_set_sync_interval(900000);

        esp_sntp_init();
    }
    else
    {
        // If already enabled, force an immediate sync request (e.g. from the CLI "clock" command)
        debugI("Forcing immediate native SNTP sync...");
        esp_sntp_restart();
    }

    return true;
}

#endif
