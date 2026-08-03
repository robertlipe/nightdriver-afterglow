//+--------------------------------------------------------------------------
//
// File:        PatternHexSpiderweb.h
//
// Pulsing energy web.
// Bright bolts shoot down the 3 primary axes of a faint spiderweb framework.
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

class PatternHexSpiderweb : public EffectWithId<PatternHexSpiderweb>
{
private:
    int speed = 30;

    struct Pulse {
        int radius;
        int maxRadius;
        uint8_t hue;
        bool active;
    };

    std::vector<Pulse> pulses;
    unsigned long lastUpdate = 0;
    uint8_t baseHue = 0;

public:
    PatternHexSpiderweb() : EffectWithId<PatternHexSpiderweb>("Hex: Spiderweb") {}
    PatternHexSpiderweb(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexSpiderweb>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHexSpiderweb() {}

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

        // Base web
        g()->DimAll(200);

        int updateInterval = 100 - speed / 2;
        if (millis() - lastUpdate > updateInterval) {
            lastUpdate = millis();
            baseHue += 2;

            // Advance pulses
            for (auto& p : pulses) {
                if (p.active) {
                    p.radius++;
                    if (p.radius > p.maxRadius) {
                        p.active = false;
                    }
                }
            }

            // Cleanup and spawn
            pulses.erase(std::remove_if(pulses.begin(), pulses.end(), [](const Pulse& p) { return !p.active; }), pulses.end());

            if (random(0, 10) > 6) {
                Pulse p;
                p.radius = 0;
                p.maxRadius = HEX_RINGS - 1;
                p.hue = baseHue + random(0, 64);
                p.active = true;
                pulses.push_back(p);
            }
        }

        HexCoord center(0,0);

        // Draw the web framework faintly
        for (int i = 0; i < TOTAL_LEDS_IN_HEX; i++) {
            HexCoord hex = hexGfx->indexToHexCoord(i);

            bool isSpoke = (hex.q == 0 || hex.r == 0 || hex.s == 0);
            int dist = hexGfx->hexDistance(hex, center);
            bool isRing = (dist % 2 == 0); // Concentric rings every 2 steps

            if (isSpoke || isRing) {
                // Dim static web
                auto idx = hexGfx->hexToIndex(hex);
                if (idx) {
                    CRGB c = g()->leds[*idx];
                    c = blend(c, CHSV(baseHue, 150, 40), 10);
                    hexGfx->drawHexPixel(hex, c);
                }
            }
        }

        // Draw pulses riding the web
        for (const auto& p : pulses) {
            if (p.active) {
                // Pulse forms a ring
                std::vector<HexCoord> ring = hexGfx->getHexRing(center, p.radius);
                CRGB c = CHSV(p.hue, 255, 255);
                for (const auto& hex : ring) {
                    // Only light up if it's on the web structure
                    bool isSpoke = (hex.q == 0 || hex.r == 0 || hex.s == 0);
                    if (isSpoke || (p.radius % 2 == 0)) {
                        hexGfx->drawHexPixel(hex, c);
                    }
                }
            }
        }
    }
};
#endif
