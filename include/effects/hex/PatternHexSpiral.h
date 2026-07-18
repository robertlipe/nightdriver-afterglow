//+--------------------------------------------------------------------------
//
// File:        PatternHexSpiral.h
//
// Archimedean spirals.
// Sweeping multi-colored spiral arms twist inward and outward.
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

class PatternHexSpiral : public EffectWithId<PatternHexSpiral>
{
private:
    int speed = 50;
    int maxRadius = HEX_RINGS - 1;
    uint8_t hueOffset = 0;

public:
    PatternHexSpiral() : EffectWithId<PatternHexSpiral>("Hex: Spiral") {}
    PatternHexSpiral(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexSpiral>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHexSpiral() {}

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

        g()->Clear();
        hueOffset += speed / 10;

        // Use precomputed spiral data - no allocation
        auto spiral = hexGfx->getHexSpiral();
        for (size_t i = 0; i < spiral.size; i++) {
            uint8_t hue = (hueOffset + i * 5) % 256;
            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, 255, LINEARBLEND);
            hexGfx->drawHexPixel(spiral.data[i], color);
        }
    }
};
#endif
