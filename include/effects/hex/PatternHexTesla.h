//+--------------------------------------------------------------------------
//
// File:        PatternHexTesla.h
//
// Beat-reactive Tesla Coil lightning.
// Branching, forking lightning bolts randomly walk from the core to the edges.
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

#if ENABLE_AUDIO
#include "effects/strip/musiceffect.h"
#endif

#include <vector>

#if ENABLE_AUDIO
class PatternHexTesla : public BeatEffectBase, public EffectWithId<PatternHexTesla>
#else
class PatternHexTesla : public EffectWithId<PatternHexTesla>
#endif
{
private:
    int speed = 50;
    uint8_t baseHue = 160; // Blue/Purple for electricity
    int maxRadius = HEX_RINGS - 1;
    unsigned long lastAutoBeat = 0;

    struct Branch {
        HexCoord head;
        int direction;
        int life;
        uint8_t hue;
        bool active;
    };

    std::vector<Branch> branches;

public:
#if ENABLE_AUDIO
    PatternHexTesla() : BeatEffectBase(1.2f, 0.2f), EffectWithId<PatternHexTesla>("Hex: Tesla Coil") {}
    PatternHexTesla(const JsonObjectConst& jsonObject) : BeatEffectBase(1.2f, 0.2f), EffectWithId<PatternHexTesla>(jsonObject) {
#else
    PatternHexTesla() : EffectWithId<PatternHexTesla>("Hex: Tesla Coil") {}
    PatternHexTesla(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexTesla>(jsonObject) {
#endif
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHexTesla() {}

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

    void SpawnBranch(bool major) {
        int count = major ? 6 : random(1, 4);
        int startDir = random(0, 6);

        for (int i = 0; i < count; i++) {
            Branch b;
            b.head = HexCoord(0, 0); // Center
            b.direction = major ? i : (startDir + i) % 6;
            b.life = random(maxRadius, maxRadius * 2);
            b.hue = baseHue + random(0, 40); // Variation in color
            b.active = true;
            branches.push_back(b);
        }
    }

#if ENABLE_AUDIO
    void HandleBeat(bool bMajor, float elapsed, float span) override
    {
        if (span > 1.2f) {
            SpawnBranch(bMajor || span > 1.8f);
        }
    }
#endif

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

#if ENABLE_AUDIO
        ProcessAudio();
#else
        // Simulate beats if no audio
        if (millis() - lastAutoBeat > (unsigned long)(1000 - speed * 8)) {
            SpawnBranch(random(0, 10) > 7);
            lastAutoBeat = millis();
        }
#endif

        // Fade existing pixels to create trails
        g()->DimAll(200);
        baseHue += speed / 50;

        HexCoord center(0, 0);

        // Draw bright center core
        hexGfx->fillHexagon(center, 1, CHSV(baseHue, 100, 255)); // White/blue hot core
        hexGfx->drawHexPixel(center, CRGB::White);

        // Update and draw active branches
        std::vector<Branch> newForks;
        for (auto it = branches.begin(); it != branches.end();) {
            if (!it->active) {
                it = branches.erase(it);
                continue;
            }

            // Draw current head
            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), it->hue, 255, LINEARBLEND);
            hexGfx->drawHexPixel(it->head, color);

            // Move head
            // 70% chance to go straight, 15% left, 15% right
            int randTurn = random(0, 100);
            if (randTurn < 15) {
                it->direction = (it->direction + 5) % 6; // Turn left
            } else if (randTurn > 85) {
                it->direction = (it->direction + 1) % 6; // Turn right
            }

            it->head = hexGfx->hexAdd(it->head, hexGfx->getHexDirection(it->direction));
            it->life--;

            // Check bounds and life
            if (it->life <= 0 || hexGfx->hexDistance(it->head, center) > maxRadius) {
                it->active = false;

                // Spawn a small explosion/spark at the end
                if (random(0, 2) == 0) {
                    for (int i = 0; i < 6; i++) {
                        HexCoord spark = hexGfx->hexAdd(it->head, hexGfx->getHexDirection(i));
                        if (hexGfx->hexDistance(spark, center) <= maxRadius) {
                            hexGfx->drawHexPixel(spark, CHSV(it->hue + 128, 200, 255));
                        }
                    }
                }
            }

            // Sometimes fork
            if (it->active && random(0, 100) < 5) {
                Branch fork = *it;
                fork.direction = (fork.direction + (random(0, 2) == 0 ? 1 : 5)) % 6;
                fork.life /= 2;
                newForks.push_back(fork);
            }

            ++it;
        }

        // Add forks
        if (!newForks.empty()) {
            branches.insert(branches.end(), newForks.begin(), newForks.end());
        }
    }
};
#endif
