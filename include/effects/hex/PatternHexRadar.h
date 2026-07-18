//+--------------------------------------------------------------------------
//
// File:        PatternHexRadar.h
//
// A sweeping radar display.
// A bright scanning line sweeps in a circle leaving a fading green trail.
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

class PatternHexRadar : public EffectWithId<PatternHexRadar>
{
private:
    int speed = 40;
    int maxRadius = HEX_RINGS - 1;
    float angle = 0.0f;
    uint8_t hueOffset = 0;

public:
    PatternHexRadar() : EffectWithId<PatternHexRadar>("Hex: Radar") {}
    PatternHexRadar(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexRadar>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexRadar() {}

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

        // Fade existing pixels to create a trail
        g()->DimAll(220);
        hueOffset += speed / 30;

        // Rotate the radar beam
        angle += speed / 400.0f;
        if (angle > 2.0f * 3.14159265f) {
            angle -= 2.0f * 3.14159265f;
        }

        // Beam width in radians
        float beamWidth = 1.0f;

        // Iterate over all hexes and draw the sweep
        for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
            HexCoord hex = hexGfx->indexToHexCoord(index);

            if (hex.q == 0 && hex.r == 0) {
                // Center blip
                CRGB centerColor = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + 128, 255, LINEARBLEND);
                hexGfx->drawHexPixel(hex, centerColor);
                continue;
            }

            // Map axial coordinates to cartesian to find exact angle
            // Flat-top orientation math
            float x = std::numbers::sqrt3_v<float> * hex.q + (std::numbers::sqrt3_v<float> / 2.0f) * hex.r;
            float y = 1.5f * hex.r;

            float pixelAngle = atan2f(y, x);
            if (pixelAngle < 0.0f) {
                pixelAngle += 2.0f * std::numbers::pi_v<float>;
            }

            // Calculate angular difference
            float angleDiff = angle - pixelAngle;
            // Wrap the difference so the beam smoothly crosses the 0/2PI boundary
            if (angleDiff < 0.0f) angleDiff += 2.0f * std::numbers::pi_v<float>;

            if (angleDiff <= beamWidth) {
                // Calculate brightness (1.0 at leading edge, 0.0 at trailing edge of beamWidth)
                float intensity = 1.0f - (angleDiff / beamWidth);
                uint8_t brightness = static_cast<uint8_t>(intensity * 255.0f);

                // Add distance-based variation to hue
                float dist = sqrtf(x*x + y*y);
                uint8_t hue = hueOffset + static_cast<uint8_t>(dist * 5.0f);

                CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, brightness, LINEARBLEND);

                // Additive blending for the sweeping beam
                CRGB existing = g()->getPixel(index);
                hexGfx->drawHexPixel(hex, existing + color);
            }
        }

        // Draw occasional range rings pulsing outward
        static float ringPulse = 0.0f;
        ringPulse += speed / 500.0f;
        if (ringPulse > maxRadius) ringPulse -= maxRadius;

        // Find hexes close to the pulsing ring radius
        for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
            HexCoord hex = hexGfx->indexToHexCoord(index);
            float dist = hexGfx->hexDistance(hex, HexCoord(0,0));
            float distDiff = fabsf(dist - ringPulse);

            if (distDiff < 0.8f) {
                uint8_t ringBright = static_cast<uint8_t>((1.0f - distDiff / 0.8f) * 80.0f);
                CRGB existing = g()->getPixel(index);
                CRGB ringColor = CRGB(ringBright, ringBright, ringBright);
                hexGfx->drawHexPixel(hex, existing + ringColor);
            }
        }
    }
};
#endif
