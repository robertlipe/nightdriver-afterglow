//+--------------------------------------------------------------------------
//
// File:        PatternHexFlowField.h
//
// Perlin noise vector flow field.
// Particles drift smoothly along a constantly morphing noise landscape.
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

class PatternHexFlowField : public EffectWithId<PatternHexFlowField>
{
private:
    int speed = 30;
    float scale = 0.3f;
    float time = 0.0f;
    uint8_t hueOffset = 0;

public:
    PatternHexFlowField() : EffectWithId<PatternHexFlowField>("Hex: Flow Field") {}
    PatternHexFlowField(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexFlowField>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
        if (jsonObject[PTY_SCALE].is<float>()) scale = jsonObject[PTY_SCALE].as<float>();
    }
    virtual ~PatternHexFlowField() {}

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back(PTY_SPEED, "Speed", SettingSpec::SettingType::Integer, 10.0, 100.0);
            mySettingSpecs.emplace_back(PTY_SCALE, "Scale", SettingSpec::SettingType::Float, 0.1, 1.0);
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

    float noise(float x, float y, float t)
    {
        // Simple pseudo-noise function
        return sinf(x * scale + t) * cosf(y * scale + t) + sinf((x + y) * scale * 0.5f + t * 0.5f);
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(210);
        hueOffset += speed / 25;
        time += speed / 500.0f;

        for (int r = -(HEX_RINGS - 1); r <= (HEX_RINGS - 1); ++r) {
            int q1 = std::max(-(HEX_RINGS - 1), -r - (HEX_RINGS - 1));
            int q2 = std::min(HEX_RINGS - 1, -r + (HEX_RINGS - 1));
            for (int q = q1; q <= q2; ++q) {
                HexCoord hex(q, r);

                // Get pixel coordinates for noise sampling
                PixelCoord pixel = hexGfx->hexToPixelFlatTop(hex, 1.0f, {0.0f, 0.0f});

                // Sample noise
                float n = noise(pixel.x, pixel.y, time);

                // Map noise to color
                uint8_t hue = (hueOffset + static_cast<uint8_t>((n + 1.0f) * 127.5f)) % 256;
                auto brightness = static_cast<uint8_t>((n + 1.0f) * 127.5f);

                if (brightness > 30) {
                    CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, brightness, LINEARBLEND);
                    hexGfx->drawHexPixel(hex, color);
                }
            }
        }
    }
};
#endif
