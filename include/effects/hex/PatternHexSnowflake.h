//+--------------------------------------------------------------------------
//
// File:        PatternHexSnowflake.h
//
// Vapor diffusion snowflake growth.
// Cellular automaton that grows a symmetrical 6-sided crystal and melts.
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

class PatternHexSnowflake : public EffectWithId<PatternHexSnowflake>
{
private:
    int speed = 50;

    struct Crystal {
        bool active;
        int age;
    };

    std::vector<Crystal> grid;
    bool melting = false;
    unsigned long lastUpdate = 0;
    uint8_t baseHue = 130; // Ice blue

public:
    PatternHexSnowflake() : EffectWithId<PatternHexSnowflake>("Hex: Snowflake") {}
    PatternHexSnowflake(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexSnowflake>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexSnowflake() {}

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

    void Reset() {
        grid.assign(TOTAL_LEDS_IN_HEX, {false, 0});
        auto hexGfx = hg();
        if (hexGfx) {
            auto idx = hexGfx->hexToIndex(HexCoord(0,0));
            if (idx) {
                grid[*idx].active = true;
                grid[*idx].age = 255;
            }
        }
        melting = false;
        baseHue = random(100, 170); // Variety of cool winter colors
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        if (grid.size() != TOTAL_LEDS_IN_HEX) {
            Reset();
        }

        int updateInterval = 200 - speed;
        if (millis() - lastUpdate > updateInterval) {
            lastUpdate = millis();

            std::vector<Crystal> nextGrid = grid;
            bool changed = false;
            int activeCount = 0;

            for (int i = 0; i < TOTAL_LEDS_IN_HEX; i++) {
                if (grid[i].active) {
                    activeCount++;
                    if (melting) {
                        nextGrid[i].age -= 10;
                        if (nextGrid[i].age <= 0) {
                            nextGrid[i].active = false;
                            nextGrid[i].age = 0;
                            changed = true;
                        }
                    } else {
                        // Increase age to solidify
                        if (nextGrid[i].age < 255) nextGrid[i].age += 10;
                    }
                } else if (!melting) {
                    // Cellular automaton rule for vapor diffusion snowflake growth:
                    // A cell becomes active if it has exactly 1 active neighbor.
                    HexCoord hex = hexGfx->indexToHexCoord(i);
                    int neighbors = 0;
                    for (int dir = 0; dir < 6; dir++) {
                        HexCoord n = hexGfx->getHexNeighbor(hex, dir);
                        auto nIdx = hexGfx->hexToIndex(n);
                        if (nIdx && grid[*nIdx].active) {
                            neighbors++;
                        }
                    }
                    if (neighbors == 1) {
                        nextGrid[i].active = true;
                        nextGrid[i].age = 50; // Start dim
                        changed = true;
                    }
                }
            }

            grid = nextGrid;

            // State machine transitions
            if (!melting && !changed) {
                // Fully grown, wait a bit then melt
                if (random(0, 10) == 0) melting = true;
            } else if (melting && activeCount == 0) {
                // Fully melted, restart
                Reset();
            }
        }

        // Draw phase
        for (int i = 0; i < TOTAL_LEDS_IN_HEX; i++) {
            if (grid[i].active) {
                HexCoord hex = hexGfx->indexToHexCoord(i);
                CRGB color = CHSV(baseHue, 255 - grid[i].age / 2, grid[i].age); // Newer = more saturated, older = whiter
                hexGfx->drawHexPixel(hex, color);
            } else {
                HexCoord hex = hexGfx->indexToHexCoord(i);
                hexGfx->drawHexPixel(hex, CRGB::Black);
            }
        }
    }
};
#endif
