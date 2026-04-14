#pragma once

//+--------------------------------------------------------------------------
//
// File:        Globals.h
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
// History:     Jul-12-2018         Davepl      Created
//              Apr-29-2019         Davepl      Adapted from BigBlueLCD project
//
//---------------------------------------------------------------------------

// NightDriver features can be selectively enabled here. Most depend on other
// includes or libraries, so check the README for more information.

#define PROJECT_NAME "NightDriverStrip"
#define FLASH_VERSION 300
#define FLASH_VERSION_NAME "3.0.0"

#include <Arduino.h>
#include <FastLED.h>

#include "types.h"
#include "logger.h"

#ifdef __cplusplus
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#endif

// PIN DEFINITIONS

#ifndef LED_PIN
    #define LED_PIN 15
#endif

#ifndef IR_REMOTE_PIN
    #define IR_REMOTE_PIN -1
#endif

#ifndef INPUT_PIN
    #define INPUT_PIN -1
#endif

// CONFIGURATION

#ifndef NUM_LEDS
    #define NUM_LEDS 1
#endif

#ifndef NUM_BANDS
    #define NUM_BANDS 1
#endif

#ifndef MAX_RINGS
    #define MAX_RINGS 5
#endif

#ifndef RING_SIZE_0
    #define RING_SIZE_0 0
#endif
#ifndef RING_SIZE_1
    #define RING_SIZE_1 0
#endif
#ifndef RING_SIZE_2
    #define RING_SIZE_2 0
#endif
#ifndef RING_SIZE_3
    #define RING_SIZE_3 0
#endif
#ifndef RING_SIZE_4
    #define RING_SIZE_4 0
#endif

// TIME DEFINITIONS

#define MILLIS_PER_SECOND 1000
#define MICROS_PER_SECOND 1000000

#ifndef NTP_DELAY_SECONDS
    #define NTP_DELAY_SECONDS (60 * 60)
#endif

#ifndef NTP_DELAY_ERROR_SECONDS
    #define NTP_DELAY_ERROR_SECONDS (60 * 5)
#endif

// CORE ASSIGNMENTS (ESP32 HAS TWO CORES, 0 AND 1)

#define DRAWING_CORE 1
#define DRAWING_PRIORITY 10

#define SCREEN_CORE 0
#define SCREEN_PRIORITY 2

#define NET_CORE 0
#define NET_PRIORITY 5

#define AUDIO_CORE 0
#define AUDIO_PRIORITY 15

#define SOCKET_CORE 0
#define SOCKET_PRIORITY 5

#define DEBUG_CORE 0
#define DEBUG_PRIORITY 1

#define REMOTE_CORE 0
#define REMOTE_PRIORITY 1

#define JSONWRITER_CORE 0
#define JSONWRITER_PRIORITY 1

#define COLORDATA_CORE 0
#define COLORDATA_PRIORITY 1

#define AUDIOSERIAL_CORE 0
#define AUDIOSERIAL_PRIORITY 1

// COLOR DEFINITIONS (FOR SCREEN)

#define BLACK16   0x0000
#define BLUE16    0x001F
#define RED16     0xF800
#define GREEN16   0x07E0
#define CYAN16    0x07FF
#define MAGENTA16 0xF81F
#define YELLOW16  0xFFE0
#define WHITE16   0xFFFF

// TASK MONITORING

#define WATCHDOG_TIMEOUT_SECONDS 5

// SHARED UTILITIES

template<typename T>
constexpr auto to_value(T e) noexcept
{
    return static_cast<std::underlying_type_t<T>>(e);
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

// GLOBAL INSTANCES

class SystemContainer;
extern std::unique_ptr<SystemContainer> g_ptrSystem;

#include "values.h"
extern CValues g_Values;

// STUB REMOTEDEBUG IF ENABLED (TURNING DOWN)
#if ENABLE_REMOTEDEBUG
    #include "console.h"
    #define Debug ConsoleManager::Instance()
#endif

// Helper to keep code compiling when RemoteDebug is disabled
#if !ENABLE_REMOTEDEBUG
    class RemoteDebug {
        public:
            void handle() {}
            void setSerialEnabled(bool) {}
    };
    extern RemoteDebug Debug;
#endif
