//+--------------------------------------------------------------------------
//
// File:        PatternHexVoronoi.h
//
// Voronoi cellular diagram.
// Moving seeds color the grid based on mathematical proximity, forming sharp cells.
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

#include <vector>
#include <cmath>

class PatternHexVoronoi : public EffectWithId<PatternHexVoronoi>
{
private:
    int speed = 50;

    struct Seed {
        float x;
        float y;
        float vx;
        float vy;
        uint8_t hue;
    };

    std::vector<Seed> seeds;
    int numSeeds = 6;
    float maxCartesianRadius;

public:
    PatternHexVoronoi() : EffectWithId<PatternHexVoronoi>("Hex: Voronoi") {}
    PatternHexVoronoi(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexVoronoi>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHexVoronoi() {}

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

    void Reset() {
        seeds.clear();
        maxCartesianRadius = (HEX_RINGS - 1) * std::numbers::sqrt3_v<float>;

        for (int i = 0; i < numSeeds; i++) {
            Seed s;
            s.x = random_float(-maxCartesianRadius, maxCartesianRadius);
            s.y = random_float(-maxCartesianRadius, maxCartesianRadius);
            s.vx = random_float(-0.5f, 0.5f);
            s.vy = random_float(-0.5f, 0.5f);
            s.hue = i * (256 / numSeeds);
            seeds.push_back(s);
        }
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        if (seeds.empty()) {
            Reset();
        }

        // Update seed positions
        float timeStep = speed / 50.0f;
        for (auto& s : seeds) {
            s.x += s.vx * timeStep;
            s.y += s.vy * timeStep;

            // Bounce off circular boundary
            float dist = sqrtf(s.x * s.x + s.y * s.y);
            if (dist > maxCartesianRadius) {
                // Reflect velocity vector
                float nx = s.x / dist;
                float ny = s.y / dist;
                float dot = s.vx * nx + s.vy * ny;
                s.vx -= 2 * dot * nx;
                s.vy -= 2 * dot * ny;

                // Add some randomness
                s.vx += random_float(-0.1f, 0.1f);
                s.vy += random_float(-0.1f, 0.1f);

                // Push back inside
                s.x = nx * maxCartesianRadius;
                s.y = ny * maxCartesianRadius;

                s.hue += random(0, 10);
            }
        }

        // Draw voronoi
        for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
            HexCoord hex = hexGfx->indexToHexCoord(index);

            // Convert hex to cartesian
            float cx = std::numbers::sqrt3_v<float> * hex.q + (std::numbers::sqrt3_v<float> / 2.0f) * hex.r;
            float cy = 1.5f * hex.r;

            // Find closest seed
            float minD = 999999.0f;
            float minD2 = 999999.0f;
            int closest = 0;

            for (size_t i = 0; i < seeds.size(); i++) {
                float dx = cx - seeds[i].x;
                float dy = cy - seeds[i].y;
                float d = dx*dx + dy*dy;
                if (d < minD) {
                    minD2 = minD;
                    minD = d;
                    closest = i;
                } else if (d < minD2) {
                    minD2 = d;
                }
            }

            // Draw
            // Add a border highlight if it's near the boundary of two cells
            uint8_t brightness = 255;
            if (sqrtf(minD2) - sqrtf(minD) < 0.8f) {
                brightness = 100; // Dark border
            } else if (sqrtf(minD) < 0.5f) {
                brightness = 255; // Bright center
            } else {
                brightness = 180;
            }

            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), seeds[closest].hue + millis() / 100, brightness, LINEARBLEND);
            hexGfx->drawHexPixel(hex, color);
        }
    }

    float random_float(float min, float max) {
        return min + (max - min) * (random(0, 10000) / 10000.0f);
    }
};
#endif
