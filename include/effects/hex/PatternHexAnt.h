//+--------------------------------------------------------------------------
//
// File:        PatternHexAnt.h
//
// Hexagonal Langton's Ant cellular automaton.
// Ants wander the grid changing colors and turning based on the previous color.
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

class PatternHexAnt : public EffectWithId<PatternHexAnt>
{
private:
    int speed = 50;

    struct Ant {
        HexCoord pos;
        int dir;
        uint8_t hue;
    };

    std::vector<Ant> ants;
    std::vector<uint8_t> gridState; // 0 to numColors-1
    int numColors = 4;
    int rules[4] = {1, -1, -2, 2}; // L60, R60, R120, L120
    unsigned long lastUpdate = 0;

public:
    PatternHexAnt() : EffectWithId<PatternHexAnt>("Hex: Langton's Ant") {}
    PatternHexAnt(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexAnt>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHexAnt() {}

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
        gridState.assign(TOTAL_LEDS_IN_HEX, 0);
        ants.clear();

        // Spawn 3 ants
        for (int i = 0; i < 3; i++) {
            Ant a;
            a.pos = HexCoord(0,0);
            a.dir = i * 2; // Spaced out
            a.hue = i * 85;
            ants.push_back(a);
        }

        // Randomize rules slightly for variety
        for (int i = 0; i < numColors; i++) {
            rules[i] = random(1, 3) * (random(0, 2) == 0 ? 1 : -1);
        }
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        if (gridState.size() != TOTAL_LEDS_IN_HEX) {
            Reset();
        }

        // Throttle updates based on speed
        unsigned long now = millis();
        int updateInterval = std::max(10, 1000 / std::max(1, (speed / 2)));
        
        if (now - lastUpdate >= updateInterval) {
            lastUpdate = now;
            
            for (auto& ant : ants) {
                auto idx = hexGfx->hexToIndex(ant.pos);
                if (idx) {
                    int currentState = gridState[*idx];

                    // 1. Turn
                    ant.dir = (ant.dir + rules[currentState] + 6) % 6;

                    // 2. Flip color
                    gridState[*idx] = (currentState + 1) % numColors;

                    // 3. Move forward
                    ant.pos = hexGfx->hexAdd(ant.pos, hexGfx->getHexDirection(ant.dir));

                    // Wrap or bounce if out of bounds
                    if (hexGfx->hexDistance(ant.pos, HexCoord(0,0)) > (HEX_RINGS - 1)) {
                        // Bounce by reversing direction and taking a step
                        ant.dir = (ant.dir + 3) % 6;
                        ant.pos = hexGfx->hexAdd(ant.pos, hexGfx->getHexDirection(ant.dir));
                    }
                } else {
                    ant.pos = HexCoord(0,0);
                }
            }
            
            // Reset occasionally to prevent complete chaos
            if (random(0, 2000) == 0) Reset();
        }

        // Draw grid
        uint8_t baseHue = millis() / 200; // Cycle very slowly so patterns remain readable
        for (int i = 0; i < TOTAL_LEDS_IN_HEX; i++) {
            if (gridState[i] > 0) {
                HexCoord hex = hexGfx->indexToHexCoord(i);
                uint8_t hue = baseHue + (gridState[i] * (256 / numColors));
                CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, 200, LINEARBLEND);
                hexGfx->drawHexPixel(hex, color);
            } else {
                HexCoord hex = hexGfx->indexToHexCoord(i);
                hexGfx->drawHexPixel(hex, CRGB::Black);
            }
        }

        // Draw ants as bright sparks
        for (const auto& ant : ants) {
            hexGfx->drawHexPixel(ant.pos, CRGB::White);
        }
    }
};
#endif
