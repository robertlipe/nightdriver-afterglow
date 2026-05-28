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

static DRAM_ATTR bool l_bClockSet = false;

bool NTPTimeClient::HasClockBeenSet()
{
    return l_bClockSet;
}

static void time_sync_notification_cb(struct timeval *tv)
{
    l_bClockSet = true;

    struct tm timeinfo;
    localtime_r(&tv->tv_sec, &timeinfo);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%d %b %Y %H:%M:%S", &timeinfo);
    debugI("NTP clock: response received, updated time to: %lld.%06lld, Local: %s",
           (long long)tv->tv_sec,
           (long long)tv->tv_usec,
           time_str);
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
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
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
