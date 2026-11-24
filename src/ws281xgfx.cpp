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

    const auto& strips = g_ptrSystem->GetDeviceConfig().GetStrips();

    for (size_t ch = 0; ch < devices.size() && ch < strips.size(); ++ch)
    {
        const auto& strip = strips[ch];
        if (!strip.enabled || strip.dataPin < 0)
            continue;

        debugI("Adding %zu LEDs to pin %d from channel %zu on FastLED (chipset %s, order %s).",
               devices[ch]->GetLEDCount(), strip.dataPin, ch,
               ChipsetToString(strip.chipset), ColorOrderToString(strip.colorOrder));

        fl::ChipsetTimingConfig timing;
        switch (strip.chipset)
        {
        case ChipsetType::WS2815:
            timing = fl::makeTimingConfig<fl::TIMING_WS2815>();
            break;
        case ChipsetType::WS2811:
        case ChipsetType::WS2812B:
        default:
            timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
            break;
        }

        fl::ClocklessChipset chipset(strip.dataPin, timing);
        fl::ChannelConfig config(
            chipset,
            fl::span<CRGB>(devices[ch]->leds, devices[ch]->GetLEDCount()),
            strip.colorOrder
        );

        auto channel = FastLED.add(config);
#if FASTLED_EXPERIMENTAL_ESP32_RGBW_ENABLED
        if (channel)
        {
            channel->setRgbw(Rgbw(kRGBWDefaultColorTemp, FASTLED_EXPERIMENTAL_ESP32_RGBW_MODE));
        }
#endif
        pinMode(strip.dataPin, OUTPUT);
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
    const auto& strips = g_ptrSystem->GetDeviceConfig().GetStrips();
    if (strips.empty())
    {
        for (int i = 0; i < NUM_CHANNELS; i++)
        {
            debugW("Allocating WS281xGFX for channel %d (legacy fallback)", i);
            devices.push_back(std::make_shared<WS281xGFX>(MATRIX_WIDTH, MATRIX_HEIGHT));
        }
    }
    else
    {
        for (size_t i = 0; i < strips.size(); i++)
        {
            debugW("Allocating WS281xGFX for strip %zu (len=%u, pin=%d, enabled=%d)",
                   i, strips[i].length, strips[i].dataPin, strips[i].enabled);
            size_t w = strips[i].length;
            size_t h = 1;
#if MATRIX_HEIGHT > 1
            if (strips.size() == 1)
            {
                w = MATRIX_WIDTH;
                h = MATRIX_HEIGHT;
            }
#endif
            devices.push_back(std::make_shared<WS281xGFX>(w, h));
        }
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

