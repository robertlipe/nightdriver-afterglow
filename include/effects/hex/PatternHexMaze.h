#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "systemcontainer.h"
#include "gfxhex.h"
#include <algorithm>
#include <cmath>
#include <vector>

class PatternHexMaze : public EffectWithId<PatternHexMaze>
{
private:
    int speed = 20;
    std::vector<HexCoord> path;
    HexCoord currentPos;
    HexCoord targetPos;
    uint8_t hueOffset = 0;
    int maxRadius = HEX_RINGS - 1;
    bool generating = true;

public:
    PatternHexMaze() : EffectWithId<PatternHexMaze>("Hex: Maze") {}
    PatternHexMaze(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexMaze>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexMaze() {}

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

    void ResetMaze()
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        path.clear();
        currentPos = HexCoord(0, 0);
        targetPos = HexCoord(0, 0);
        generating = true;
        path.push_back(currentPos);
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(235);
        hueOffset += speed / 30;

        if (path.empty()) {
            ResetMaze();
        }

        // Draw existing path
        for (size_t i = 0; i < path.size(); i++) {
            uint8_t hue = (hueOffset + i * 5) % 256;
            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, 200, LINEARBLEND);
            hexGfx->drawHexPixel(path[i], color);
        }

        // Generate new path segment
        if (generating) {
            // Pick a random neighbor that's not already in path
            // Manually iterate neighbors to avoid vector allocation
            std::vector<HexCoord> validNeighbors;
            validNeighbors.reserve(6); // Max 6 neighbors

            for (int i = 0; i < 6; i++) {
                HexCoord neighbor = hexGfx->getHexNeighbor(currentPos, i);
                // Check if neighbor is in bounds and not in path
                if (hexGfx->hexDistance(neighbor, HexCoord(0, 0)) <= maxRadius) {
                    bool inPath = false;
                    for (const auto& p : path) {
                        if (p.q == neighbor.q && p.r == neighbor.r) {
                            inPath = true;
                            break;
                        }
                    }
                    if (!inPath) {
                        validNeighbors.push_back(neighbor);
                    }
                }
            }

            if (!validNeighbors.empty()) {
                // Move to random valid neighbor
                currentPos = validNeighbors[random(0, validNeighbors.size())];
                path.push_back(currentPos);

                // Check if we've reached the edge
                if (hexGfx->hexDistance(currentPos, HexCoord(0, 0)) >= maxRadius) {
                    generating = false;
                }
            } else {
                // Backtrack - remove last position
                if (path.size() > 1) {
                    path.pop_back();
                    currentPos = path.back();
                } else {
                    // Restart if stuck at center
                    ResetMaze();
                }
            }

            // Limit path length
            if (path.size() > 100) {
                generating = false;
            }
        }

        // Draw current position head
        CRGB headColor = CRGB::White;
        hexGfx->drawHexPixel(currentPos, headColor);
    }
};
#endif
