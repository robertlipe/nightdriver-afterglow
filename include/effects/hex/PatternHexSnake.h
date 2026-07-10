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
        snake.push_back(HexCoord(0, 0));
        direction = hexGfx->getHexDirection(random(0, 6));
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

            // Calculate new head position
            HexCoord newHead = hexGfx->hexAdd(snake.back(), direction);

            // Check bounds
            if (hexGfx->hexDistance(newHead, HexCoord(0, 0)) > maxRadius) {
                // Hit wall - reset
                ResetGame();
            } else {
                // Check self collision
                bool collision = false;
                for (const auto& segment : snake) {
                    if (segment.q == newHead.q && segment.r == newHead.r) {
                        collision = true;
                        break;
                    }
                }

                if (collision) {
                    ResetGame();
                } else {
                    snake.push_back(newHead);

                    // Check if ate food
                    if (newHead.q == food.q && newHead.r == food.r) {
                        // Ate food - don't remove tail, spawn new food
                        SpawnFood();
                    } else {
                        // Didn't eat - remove tail
                        if (snake.size() > maxLength) {
                            snake.erase(snake.begin());
                        } else {
                            snake.erase(snake.begin());
                        }
                    }
                }
            }

            // Random direction change occasionally
            if (random(0, 100) < 20) {
                int turn = random(0, 2) == 0 ? -1 : 1;
                int newDir = (random(0, 6) + turn + 6) % 6;
                direction = hexGfx->getHexDirection(newDir);
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
