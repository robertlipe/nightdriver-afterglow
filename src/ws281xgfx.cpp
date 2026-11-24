//+--------------------------------------------------------------------------
//
// File:        ws281xgfx.cpp
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
//    Code for handling LED strips with the FastLED library
//
// History:     Jul-22-2023         Rbergen      Created
//
//---------------------------------------------------------------------------

#include "globals.h"

#include <iterator>
#include <vector>

#include "deviceconfig.h"
#include "effectmanager.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "nd_panic.h"
#include "systemcontainer.h"
#include "values.h"
#include "ws281xgfx.h"

void WS281xGFX::AddLEDs(std::vector<std::shared_ptr<GFXBase>>& devices)
{
    debugI("Adding LEDs to FastLED via Channels API...");

    const int defaultPins[] = {
#if NUM_CHANNELS >= 1 && LED_PIN0 >= 0
        LED_PIN0,
#endif
#if NUM_CHANNELS >= 2 && LED_PIN1 >= 0
        LED_PIN1,
#endif
#if NUM_CHANNELS >= 3 && LED_PIN2 >= 0
        LED_PIN2,
#endif
#if NUM_CHANNELS >= 4 && LED_PIN3 >= 0
        LED_PIN3,
#endif
#if NUM_CHANNELS >= 5 && LED_PIN4 >= 0
        LED_PIN4,
#endif
#if NUM_CHANNELS >= 6 && LED_PIN5 >= 0
        LED_PIN5,
#endif
#if NUM_CHANNELS >= 7 && LED_PIN6 >= 0
        LED_PIN6,
#endif
#if NUM_CHANNELS >= 8 && LED_PIN7 >= 0
        LED_PIN7,
#endif
    };
    constexpr size_t kDefaultPinCount = sizeof(defaultPins) / sizeof(defaultPins[0]);

    if constexpr (kDefaultPinCount == 0)
        return;

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();

    for (size_t ch = 0; ch < devices.size() && ch < kDefaultPinCount; ++ch)
    {
        int pin = defaultPins[ch];
        if (pin < 0)
            continue;

        debugI("Adding %zu LEDs to pin %d from channel %zu on FastLED.",
               devices[ch]->GetLEDCount(), pin, ch);

        fl::ClocklessChipset chipset(pin, timing);
        fl::ChannelConfig config(
            chipset,
            fl::span<CRGB>(devices[ch]->leds, devices[ch]->GetLEDCount()),
            COLOR_ORDER
        );

        auto channel = FastLED.add(config);
#if FASTLED_EXPERIMENTAL_ESP32_RGBW_ENABLED
        if (channel)
        {
            channel->setRgbw(Rgbw(kRGBWDefaultColorTemp, FASTLED_EXPERIMENTAL_ESP32_RGBW_MODE));
        }
#endif
        pinMode(pin, OUTPUT);
    }

#ifdef POWER_LIMIT_MW
    set_max_power_in_milliwatts(POWER_LIMIT_MW);
#endif
}

WS281xGFX::WS281xGFX(size_t w, size_t h) : GFXBase(w, h)
{
    debugV("Creating Device of size %zu x %zu", w, h);
    leds = static_cast<CRGB *>(calloc(w * h, sizeof(CRGB)));
    if(!leds)
        Panic("Unable to allocate LEDs in WS281xGFX");
}

WS281xGFX::~WS281xGFX()
{
    free(leds);
    leds = nullptr;
}

void WS281xGFX::InitializeHardware(std::vector<std::shared_ptr<GFXBase>>& devices)
{
    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        debugW("Allocating WS281xGFX for channel %d", i);
        devices.push_back(std::make_shared<WS281xGFX>(MATRIX_WIDTH, MATRIX_HEIGHT));
    }

    AddLEDs(devices);
}

// PostProcessFrame
//
// PostProcessFrame sends the data to the LED strip.  If it's fewer than the size of the strip, we only send that many.

void WS281xGFX::PostProcessFrame(uint16_t localPixelsDrawn, uint16_t wifiPixelsDrawn)
{
    auto pixelsDrawn = wifiPixelsDrawn > 0 ? wifiPixelsDrawn : localPixelsDrawn;

    // If we drew no pixels, there's nothing to post process
    if (pixelsDrawn == 0)
    {
        debugV("Frame draw ended without any pixels drawn.");
        return;
    }

    // If there are no LEDs to show, we can just return now

    if (FastLED.count() == 0)
    {
        static auto lastDrawTime = millis();
        g_Values.FPS = 1000.0 / max(1UL, millis() - lastDrawTime);
        lastDrawTime = millis();
        return;
    }

    auto& effectManager = g_ptrSystem->GetEffectManager();

    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        FastLED[i].setLeds(effectManager.g(i)->leds, pixelsDrawn);
        fadeLightBy(FastLED[i].leds(), FastLED[i].size(), 255 - g_ptrSystem->GetDeviceConfig().GetBrightness());
    }
    FastLED.show(min(g_Values.Fader, g_Values.ThermalThrottle)); //Shows the pixels

    g_Values.FPS = FastLED.getFPS();
    #ifdef POWER_LIMIT_MW
        g_Values.Brite = 100.0 * calculate_max_brightness_for_power_mW(g_ptrSystem->GetDeviceConfig().GetBrightness(), POWER_LIMIT_MW) / 255;
    #else
        g_Values.Brite = 100.0 * g_ptrSystem->GetDeviceConfig().GetBrightness() / 255;
    #endif
    g_Values.Brite = (g_Values.Brite * min(g_Values.Fader, g_Values.ThermalThrottle)) / 255.0f;
    g_Values.Watts = calculate_unscaled_power_mW(effectManager.g()->leds, pixelsDrawn) / 1000; // 1000 for mw->W
}

