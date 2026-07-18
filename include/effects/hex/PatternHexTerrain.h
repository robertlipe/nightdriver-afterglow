//+--------------------------------------------------------------------------
//
// File:        PatternHexTerrain.h
//
// Topographical map effect.
// Uses 2D noise to render colored heightmaps with shifting sea levels.
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

class PatternHexTerrain : public EffectWithId<PatternHexTerrain>
{
private:
    int speed = 20;
    float scale = 0.2f;
    float time = 0.0f;
    uint8_t hueOffset = 0;
    int maxRadius = HEX_RINGS - 1;

    // Simple pseudo-noise function (since we may not have Perlin noise available)
    float noise(float x, float y, float z)
    {
        // Simple 3D noise approximation using sin/cos
        float n = sinf(x * 0.5f + z) * cosf(y * 0.5f + z) * 0.5f +
                  sinf(x * 1.3f + z * 0.7f) * cosf(y * 1.7f + z * 0.9f) * 0.25f +
                  sinf(x * 2.1f + z * 0.5f) * cosf(y * 1.9f + z * 1.1f) * 0.125f;
        return (n + 1.0f) * 0.5f; // Normalize to 0-1
    }

    // Get terrain color based on height (0-1)
    CRGB getTerrainColor(float height, uint8_t hueShift)
    {
        // Terrain levels: water, sand, grass, forest, rock, snow
        CRGB color;

        if (height < 0.25f) {
            // Deep water to shallow water
            uint8_t waterHue = (160 + hueShift) % 256;
            color = CHSV(waterHue, 200, 100 + height * 400);
        } else if (height < 0.35f) {
            // Sand/beach
            color = CHSV(30 + hueShift / 4, 100, 180 + (height - 0.25f) * 500);
        } else if (height < 0.55f) {
            // Grass
            color = CHSV(96 + hueShift / 4, 180, 150 + (height - 0.35f) * 250);
        } else if (height < 0.70f) {
            // Forest
            color = CHSV(80 + hueShift / 4, 200, 120 + (height - 0.55f) * 200);
        } else if (height < 0.85f) {
            // Rock/mountain
            color = CHSV(0, 0, 100 + (height - 0.70f) * 300);
        } else {
            // Snow
            color = CHSV(0, 0, 200 + (height - 0.85f) * 350);
        }

        return color;
    }

public:
    PatternHexTerrain() : EffectWithId<PatternHexTerrain>("Hex: Terrain") {}
    PatternHexTerrain(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexTerrain>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
        if (jsonObject["scale"].is<float>()) scale = jsonObject["scale"].as<float>();
    }
    virtual ~PatternHexTerrain() {}

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back("speed", "Speed", SettingSpec::SettingType::Integer, 10.0, 100.0);
            mySettingSpecs.emplace_back("scale", "Scale", SettingSpec::SettingType::Float, 0.1, 1.0);
        }
        return &mySettingSpecs;
    }

    bool SerializeSettingsToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeSettingsToJSON(root);

        jsonDoc["speed"] = speed;
        jsonDoc["scale"] = scale;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, "speed", speed, value);
        RETURN_IF_SET(name, "scale", scale, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        time += speed * 0.001f;
        hueOffset = (hueOffset + 1) % 256;

        // Clear with a dim fade for smooth transitions
        g()->DimAll(230);

        // Generate and draw terrain
        for (int r = -(HEX_RINGS - 1); r <= (HEX_RINGS - 1); ++r) {
            int q1 = std::max(-(HEX_RINGS - 1), -r - (HEX_RINGS - 1));
            int q2 = std::min(HEX_RINGS - 1, -r + (HEX_RINGS - 1));
            for (int q = q1; q <= q2; ++q) {
                HexCoord hex(q, r);

                // Get pixel coordinates for noise sampling
                PixelCoord pixel = hexGfx->hexToPixelFlatTop(hex, 1.0f, {0.0f, 0.0f});

                // Sample noise at this position with time for animation
                float height = noise(pixel.x * scale, pixel.y * scale, time);

                // Get terrain color based on height
                CRGB color = getTerrainColor(height, hueOffset);

                hexGfx->drawHexPixel(hex, color);
            }
        }
    }
};
#endif
