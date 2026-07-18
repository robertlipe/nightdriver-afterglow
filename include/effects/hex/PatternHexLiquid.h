//+--------------------------------------------------------------------------
//
// File:        PatternHexLiquid.h
//
// Smooth liquid blending using simplex noise.
// Creates an organic, lava-lamp style morphing fluid.
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
#include <numbers>

class PatternHexLiquid : public EffectWithId<PatternHexLiquid>
{
private:
    int speed = 30;
    int scale = 40;
    uint8_t hueOffset = 0;
    uint16_t z_offset = 0;

public:
    PatternHexLiquid() : EffectWithId<PatternHexLiquid>("Hex: Liquid") {}
    PatternHexLiquid(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexLiquid>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
        if (jsonObject[PTY_SCALE].is<int>()) scale = jsonObject[PTY_SCALE].as<int>();
    }
    virtual ~PatternHexLiquid() {}

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back(PTY_SPEED, "Speed", SettingSpec::SettingType::Integer, 1.0, 100.0);
            mySettingSpecs.emplace_back(PTY_SCALE, "Scale", SettingSpec::SettingType::Integer, 10.0, 100.0);
        }
        return &mySettingSpecs;
    }

    bool SerializeSettingsToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeSettingsToJSON(root);

        jsonDoc[PTY_SPEED] = speed;
        jsonDoc[PTY_SCALE] = scale;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, PTY_SPEED, speed, value);
        RETURN_IF_SET(name, PTY_SCALE, scale, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        z_offset += speed;
        hueOffset += speed / 20;

        constexpr float sqrt3 = std::numbers::sqrt3_v<float>;

        for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
            HexCoord hex = hexGfx->indexToHexCoord(index);

            // Convert to pseudo-cartesian for noise sampling
            float x = sqrt3 * hex.q + (sqrt3 / 2.0f) * hex.r;
            float y = 1.5f * hex.r;

            // Scale coordinates
            uint32_t nx = static_cast<uint32_t>((x + 20.0f) * scale * 256);
            uint32_t ny = static_cast<uint32_t>((y + 20.0f) * scale * 256);

            // Get 3D noise
            uint8_t noiseVal = inoise8(nx, ny, z_offset);

            // Create a sharp threshold for the lava-lamp "blobs"
            // Smoothstep-like transition between blobs and background
            uint8_t brightness = 0;
            uint8_t hue = hueOffset;

            if (noiseVal > 140) {
                brightness = 255;
                hue += 40; // Blobs are a different color
            } else if (noiseVal > 120) {
                // Anti-aliased edge
                brightness = (noiseVal - 120) * 12;
                hue += (noiseVal - 120) * 2;
            } else {
                // Background
                brightness = 40;
                hue += 128; // Complementary background color
            }

            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, brightness, LINEARBLEND);
            hexGfx->drawHexPixel(hex, color);
        }
    }
};
#endif
