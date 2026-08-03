//+--------------------------------------------------------------------------
//
// File:        PatternHexSnake.h
//
// A wandering snake.
// A long glowing line wanders the grid smoothly turning 60 degrees.
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
#include "systemcontainer.h"
#include "gfxhex.h"
#include <algorithm>
#include <cmath>
#include <vector>

class PatternHexSnake : public EffectWithId<PatternHexSnake>
{
private:
    int speed = 50;
    std::vector<HexCoord> snake;
    HexCoord direction;
    int currentDirIndex = 0;
    HexCoord food;
    uint8_t hueOffset = 0;
    int maxRadius = HEX_RINGS - 1;
    int maxLength = 30;
    unsigned long lastMove = 0;
    int moveInterval = 200;

public:
    PatternHexSnake() : EffectWithId<PatternHexSnake>("Hex: Snake") {}
    PatternHexSnake(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexSnake>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHexSnake() {}

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

    void ResetGame()
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        snake.clear();
        currentDirIndex = random(0, 6);
        direction = hexGfx->getHexDirection(currentDirIndex);

        // Start with a small body
        HexCoord start(0, 0);
        for(int i = 0; i < 4; i++) {
            snake.push_back(start);
            start = hexGfx->hexAdd(start, direction);
        }
        SpawnFood();
    }

    void SpawnFood()
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        HexCoord center(0, 0);
        int r = random(1, maxRadius - 1);
        auto ring = hexGfx->getHexRing(r);
        if (ring.size > 0) {
            food = ring.data[random(0, ring.size)];
        } else {
            food = center;
        }
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(230);
        hueOffset += speed / 25;

        if (snake.empty()) {
            ResetGame();
        }

        unsigned long now = millis();
        if (now - lastMove > moveInterval) {
            lastMove = now;
            // Non-linear inverse scaling: speed 10 = 400ms, speed 100 = 40ms. Naturally asymptotes to prevent negative intervals.
            moveInterval = 4000 / std::max(1, speed);

            // AI Pathfinding to food
            int bestDir = -1;
            float minDistance = 999999.0f;
            std::array<int, 6> validDirs;
            int numValidDirs = 0;
            for (int i = 0; i < 6; i++) {
                // Don't reverse direction 180 degrees
                if (i == (currentDirIndex + 3) % 6 && snake.size() > 1) continue;

                HexCoord candidateHead = hexGfx->hexAdd(snake.back(), hexGfx->getHexDirection(i));

                // Check bounds
                if (hexGfx->hexDistance(candidateHead, HexCoord(0, 0)) > maxRadius) continue;

                // Check self collision
                bool collision = false;
                // Exclude the very tip of the tail since it will move out of the way
                for (size_t s = 1; s < snake.size(); s++) {
                    if (snake[s].q == candidateHead.q && snake[s].r == candidateHead.r) {
                        collision = true;
                        break;
                    }
                }
                if (collision) continue;

                float dist = hexGfx->hexDistance(candidateHead, food);
                validDirs[numValidDirs++] = i;
                if (dist < minDistance) {
                    minDistance = dist;
                    bestDir = i;
                }
            }

            if (numValidDirs > 0) {
                // 15% chance to take a random valid turn instead of optimal to make it slither
                if (bestDir == -1 || random(0, 100) < 15) {
                    currentDirIndex = validDirs[random(0, numValidDirs)];
                } else {
                    currentDirIndex = bestDir;
                }
                direction = hexGfx->getHexDirection(currentDirIndex);

                HexCoord newHead = hexGfx->hexAdd(snake.back(), direction);
                snake.push_back(newHead);

                if (newHead.q == food.q && newHead.r == food.r) {
                    SpawnFood();
                    // Optional: limit max length
                    if (snake.size() > maxLength) {
                        snake.erase(snake.begin());
                    }
                } else {
                    snake.erase(snake.begin());
                }
            } else {
                // Trapped!
                ResetGame();
            }
        }

        // Draw snake
        for (size_t i = 0; i < snake.size(); i++) {
            uint8_t hue = (hueOffset + i * 10) % 256;
            uint8_t brightness = (i == snake.size() - 1) ? 255 : 200 - (i * 5);
            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, brightness, LINEARBLEND);
            hexGfx->drawHexPixel(snake[i], color);
        }

        // Draw food
        CRGB foodColor = CRGB::Red;
        hexGfx->drawHexPixel(food, foodColor);
    }
};
#endif
