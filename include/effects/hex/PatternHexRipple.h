//+--------------------------------------------------------------------------
//
// File:        PatternHexRipple.h
//
// Water droplet ripples.
// Simulates intersecting circular wave physics from random raindrop impacts.
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
#include <numbers>

struct HexRipple {
    HexCoord center;
    float radius;
    float maxRadius;
    uint8_t hue;
    float speed;
};

class PatternHexRipple : public EffectWithId<PatternHexRipple>
{
private:
    int speed = 40;
    std::vector<HexRipple> ripples;
    uint8_t baseHue = 0;

public:
    PatternHexRipple() : EffectWithId<PatternHexRipple>("Hex: Ripple") {}
    PatternHexRipple(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexRipple>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexRipple() {}

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

        g()->DimAll(230);
        baseHue += 1;

        constexpr float sqrt3 = std::numbers::sqrt3_v<float>;

        // Randomly spawn new ripples
        if (random(0, 100) < (speed / 5)) {
            HexRipple r;
            r.center = hexGfx->indexToHexCoord(random(0, TOTAL_LEDS_IN_HEX));
            r.radius = 0.0f;
            r.maxRadius = 6.0f + random(0, 6);
            r.hue = baseHue + random(0, 64);
            r.speed = 0.2f + (random(0, 20) / 100.0f);
            ripples.push_back(r);
        }

        // We could render by drawing circles, but it's smoother to iterate pixels and calc distance
        // Since we don't have too many pixels (271), iterating all is fast enough.
        for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
            HexCoord hex = hexGfx->indexToHexCoord(index);

            float x = sqrt3 * hex.q + (sqrt3 / 2.0f) * hex.r;
            float y = 1.5f * hex.r;

            int pixelBrightness = 0;
            int pixelHue = 0;

            for (const auto& r : ripples) {
                float rx = sqrt3 * r.center.q + (sqrt3 / 2.0f) * r.center.r;
                float ry = 1.5f * r.center.r;

                float dist = sqrtf((x - rx) * (x - rx) + (y - ry) * (y - ry));

                // If pixel is near the expanding ripple ring
                float diff = std::abs(dist - r.radius);
                if (diff < 1.5f) {
                    float intensity = 1.0f - (diff / 1.5f);
                    // Fade out as it gets larger
                    float fade = 1.0f - (r.radius / r.maxRadius);
                    if (fade < 0.0f) fade = 0.0f;

                    int b = static_cast<int>(intensity * fade * 255.0f);
                    if (b > pixelBrightness) {
                        pixelBrightness = b;
                        pixelHue = r.hue;
                    }
                }
            }

            if (pixelBrightness > 0) {
                CRGB color = ColorFromPalette(g()->GetCurrentPalette(), pixelHue, pixelBrightness, LINEARBLEND);
                // Additively blend with what's there
                hexGfx->drawHexPixel(hex, color); // DimAll provides the trailing effect
            }
        }

        // Update ripples and remove dead ones
        for (auto it = ripples.begin(); it != ripples.end(); ) {
            it->radius += it->speed;
            if (it->radius > it->maxRadius) {
                it = ripples.erase(it);
            } else {
                ++it;
            }
        }
    }
};
#endif
