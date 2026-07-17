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
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexSnake() {}

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
        std::vector<HexCoord> ring = hexGfx->getHexRing(center, r);
        if (!ring.empty()) {
            food = ring[random(0, ring.size())];
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
            moveInterval = 400 - speed * 3; // Higher speed = lower interval

            // AI Pathfinding to food
            int bestDir = -1;
            float minDistance = 999999.0f;
            std::vector<int> validDirs;

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

                validDirs.push_back(i);
                float dist = hexGfx->hexDistance(candidateHead, food);
                if (dist < minDistance) {
                    minDistance = dist;
                    bestDir = i;
                }
            }

            if (!validDirs.empty()) {
                // 15% chance to take a random valid turn instead of optimal to make it slither
                if (bestDir == -1 || random(0, 100) < 15) {
                     currentDirIndex = validDirs[random(0, validDirs.size())];
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
