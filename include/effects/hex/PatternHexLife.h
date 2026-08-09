//+--------------------------------------------------------------------------
//
// File:        PatternHexLife.h
//
// Hexagonal Conway's Game of Life.
// A zero-player game of cellular reproduction and starvation.
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

class HexCell
{
public:
    uint8_t alive : 1;
    uint8_t prev : 1;
    uint8_t hue;
    uint8_t brightness;
    uint8_t age; // Track how many generations cell has been alive
};

class PatternHexLife : public EffectWithId<PatternHexLife>
{
private:
    std::unique_ptr<HexCell[]> world;
    int speed = 50;
    int generation = 0;
    unsigned long lastUpdate = 0;
    unsigned int density = 30;
    unsigned long seed;
    int maxRadius = HEX_RINGS - 1;
    int totalHexes;

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);

    int getHexIndex(HexCoord hex)
    {
        auto hexGfx = hg();
        if (!hexGfx) return -1;
        auto idx = hexGfx->hexToIndex(hex);
        return idx.has_value() ? idx.value() : -1;
    }

    int countNeighbors(HexCoord hex)
    {
        auto hexGfx = hg();
        if (!hexGfx) return 0;

        int count = 0;
        // Manually iterate the 6 hex directions to avoid vector allocation
        for (int i = 0; i < 6; i++) {
            HexCoord neighbor = hexGfx->getHexNeighbor(hex, i);
            int idx = getHexIndex(neighbor);
            if (idx >= 0 && world[idx].prev) {
                count++;
            }
        }
        return count;
    }

    void randomFillWorld()
    {
        seed = random();
        srand(seed);

        for (int i = 0; i < totalHexes; i++) {
            if ((rand() % 100) < density) {
                world[i].alive = 1;
                world[i].brightness = 128;
                world[i].age = 0;
            } else {
                world[i].alive = 0;
                world[i].brightness = 0;
                world[i].age = 0;
            }
            world[i].prev = world[i].alive;
            world[i].hue = 0;
        }
    }

public:
    PatternHexLife() : EffectWithId<PatternHexLife>("Hex: Life")
    {
        totalHexes = TOTAL_LEDS_IN_HEX;
    }
    PatternHexLife(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexLife>(jsonObject)
    {
        totalHexes = TOTAL_LEDS_IN_HEX;
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHexLife() {}

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

    bool Init(std::vector<std::shared_ptr<GFXBase>>& gfx) override
    {
        LEDStripEffect::Init(gfx);

        totalHexes = TOTAL_LEDS_IN_HEX;
        world = std::make_unique<HexCell[]>(totalHexes);

        return true;
    }

    bool RequiresDoubleBuffering() const override
    {
        return true;
    }

    void Reset()
    {
        randomFillWorld();
        generation = 0;
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        if (generation == 0)
            Reset();

        // Display current generation with age-based coloring
        for (int r = -(HEX_RINGS - 1); r <= (HEX_RINGS - 1); ++r) {
            int q1 = std::max(-(HEX_RINGS - 1), -r - (HEX_RINGS - 1));
            int q2 = std::min(HEX_RINGS - 1, -r + (HEX_RINGS - 1));
            for (int q = q1; q <= q2; ++q) {
                HexCoord hex(q, r);
                int idx = getHexIndex(hex);
                if (idx >= 0 && world[idx].brightness > 0) {
                    // Age-based hue shift: older cells shift through the palette
                    uint8_t ageHue = (world[idx].hue + world[idx].age * 8) % 256;
                    // Brightness based on age (fade in, stay bright, fade out)
                    uint8_t ageBrightness = world[idx].brightness;
                    if (world[idx].age < 5) {
                        ageBrightness = world[idx].brightness * world[idx].age / 5;
                    } else if (world[idx].age > 50) {
                        ageBrightness = world[idx].brightness * (60 - world[idx].age) / 10;
                    }
                    CRGB color = ColorFromPalette(g()->GetCurrentPalette(), ageHue, ageBrightness, LINEARBLEND);
                    hexGfx->drawHexPixel(hex, color);
                } else if (idx >= 0) {
                    hexGfx->drawHexPixel(hex, CRGB::Black);
                }
            }
        }

        // Birth and death cycle, throttled by speed
        unsigned long now = millis();
        int updateInterval = std::max(20, 1000 / std::max(1, (speed / 5)));

        if (now - lastUpdate >= updateInterval) {
            lastUpdate = now;

            for (int r = -(HEX_RINGS - 1); r <= (HEX_RINGS - 1); ++r) {
                int q1 = std::max(-(HEX_RINGS - 1), -r - (HEX_RINGS - 1));
            int q2 = std::min(HEX_RINGS - 1, -r + (HEX_RINGS - 1));
            for (int q = q1; q <= q2; ++q) {
                HexCoord hex(q, r);
                int idx = getHexIndex(hex);
                if (idx < 0) continue;

                if (world[idx].brightness > 0 && world[idx].prev == 0)
                    world[idx].brightness *= 0.75f;

                int count = countNeighbors(hex);
                // B2/S23 Hex Life Rules
                if (count == 2 && world[idx].prev == 0) {
                    world[idx].alive = 1;
                    world[idx].hue += 1;
                    world[idx].brightness = 255;
                    world[idx].age = 0;
                } else if ((count < 2 || count > 3) && world[idx].prev == 1) {
                    world[idx].alive = 0;
                    world[idx].brightness = 0;
                    world[idx].age = 0;
                } else if (world[idx].prev == 1 && world[idx].alive == 1) {
                    world[idx].age++;
                    if (world[idx].age > 255) world[idx].age = 255;
                }
            }
        }

            // Copy next generation
            for (int i = 0; i < totalHexes; i++) {
                world[i].prev = world[i].alive;
            }

            generation++;

            // Auto-reset if stagnant (simple check)
            if (generation > 500) {
                Reset();
            }
        }
    }
};
#endif
