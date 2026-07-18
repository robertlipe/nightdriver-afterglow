//+--------------------------------------------------------------------------
//
// File:        PatternHexAudio.h
//
// Radial audio visualizer for hexagon grids.
// Displays a pulsating hot core and outward expanding frequency rings.
//
// NightDriverStrip - (c) 2026 Robert Lipe.  All Rights Reserved.
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
//+--------------------------------------------------------------------------

#pragma once

#include "globals.h"

#if HEXAGON && ENABLE_AUDIO
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cmath>
#include <vector>

class PatternHexAudio : public EffectWithId<PatternHexAudio>
{
private:
    int speed = 30;
    uint8_t hueOffset = 0;
    int maxRadius = HEX_RINGS - 1;

public:
    PatternHexAudio() : EffectWithId<PatternHexAudio>("Hex: Audio") {}
    PatternHexAudio(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexAudio>(jsonObject) {
        if (jsonObject.containsKey("speed")) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexAudio() {}

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back("speed", "Speed", SettingSpec::SettingType::Integer, 10.0, 100.0);
        }
        return &mySettingSpecs;
    }

    bool SerializeSettingsToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeSettingsToJSON(root);

        jsonDoc["speed"] = speed;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, "speed", speed, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(200);
        hueOffset += speed / 20;

        HexCoord center(0, 0);
        float audioLevel = g_Analyzer.VURatio();

        // Draw radial audio visualization using rings
        for (int r = 1; r <= maxRadius; r++) {
            float normalizedRadius = static_cast<float>(r) / maxRadius;

            // Each ring responds to different frequency range
            float ringLevel = audioLevel;

            // Add some variation based on radius
            ringLevel *= (1.0f + 0.3f * sinf(r * 0.5f + millis() / 1000.0f));

            ringLevel = std::max(0.0f, std::min(1.0f, ringLevel));

            if (ringLevel > 0.1f) {
                std::vector<HexCoord> ring = hexGfx->getHexRing(center, r);
                uint8_t hue = (hueOffset + r * 20) % 256;
                uint8_t brightness = static_cast<uint8_t>(ringLevel * 255);

                CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, brightness, LINEARBLEND);

                for (const auto& hex : ring) {
                    hexGfx->drawHexPixel(hex, color);
                }
            }
        }

        // Center pulse with beat
        float beatEnhance = g_Analyzer.BeatEnhance(1.5f);
        uint8_t centerHue = (hueOffset + 128) % 256;
        uint8_t centerBrightness = static_cast<uint8_t>(audioLevel * beatEnhance * 255);
        CRGB centerColor = ColorFromPalette(g()->GetCurrentPalette(), centerHue, centerBrightness, LINEARBLEND);
        hexGfx->drawHexPixel(center, centerColor);
    }
};
#endif
