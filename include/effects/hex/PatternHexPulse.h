//+--------------------------------------------------------------------------
//
// File:        PatternHexPulse.h
//
// Rhythmic concentric pulsing rings.
// Expanding waves of color radiating from the center.
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

#if HEXAGON
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cmath>
#include <vector>

class PatternHexPulse : public EffectWithId<PatternHexPulse>
{
private:
    int speed = 30;
    int maxRadius = HEX_RINGS - 1;
    uint8_t hueOffset = 0;
    float pulsePhase = 0.0f;

public:
    PatternHexPulse() : EffectWithId<PatternHexPulse>("Hex: Pulse") {}
    PatternHexPulse(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexPulse>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHexPulse() {}

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back(PTY_SPEED, "Speed", SettingSpec::SettingType::Integer, 10.0, 100.0);
        }
        return &mySettingSpecs;
    }

    bool SerializeSettingsToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeSettingsToJSON(root);

        jsonDoc[PTY_SPEED] = speed;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, PTY_SPEED, speed, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(240);
        hueOffset += speed / 20;
        pulsePhase += speed / 500.0f;

        HexCoord center(0, 0);

        // Multiple expanding rings
        for (int ring = 0; ring < 3; ring++) {
            float offsetPhase = pulsePhase + ring * 2.0f;
            float sineValue = (sinf(offsetPhase) + 1.0f) / 2.0f; // 0 to 1
            int currentRadius = static_cast<int>(sineValue * maxRadius);

            if (currentRadius > 0) {
                std::vector<HexCoord> ringHexes = hexGfx->getHexRing(center, currentRadius);
                uint8_t hue = (hueOffset + ring * 85) % 256;
                CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, 255, LINEARBLEND);

                for (const auto& hex : ringHexes) {
                    hexGfx->drawHexPixel(hex, color);
                }
            }
        }

        // Center pulse
        uint8_t centerHue = (hueOffset + 128) % 256;
        CRGB centerColor = ColorFromPalette(g()->GetCurrentPalette(), centerHue, 255, LINEARBLEND);
        hexGfx->drawHexPixel(center, centerColor);
    }
};
#endif
