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
    std::vector<HexCoord> visitedList;
    std::vector<bool> visitedMap;
    HexCoord currentPos;
    uint8_t hueOffset = 0;
    int maxRadius = HEX_RINGS - 1;
    bool generating = true;
    unsigned long waitStart = 0;

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
        visitedList.clear();
        visitedMap.assign(TOTAL_LEDS_IN_HEX + 1, false);
        currentPos = HexCoord(0, 0);
        generating = true;
        waitStart = 0;
        
        path.push_back(currentPos);
        visitedList.push_back(currentPos);
        auto idx = hexGfx->hexToIndex(currentPos);
        if (idx) visitedMap[*idx] = true;
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(235);
        hueOffset += speed / 30;

        if (path.empty() && visitedList.empty()) {
            ResetMaze();
        }

        // Draw visited maze
        for (size_t i = 0; i < visitedList.size(); i++) {
            uint8_t hue = (hueOffset + i * 2) % 256;
            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, 180, LINEARBLEND);
            hexGfx->drawHexPixel(visitedList[i], color);
        }

        // Generate new path segment
        if (generating) {
            // How many steps to advance per frame
            int steps = std::max(1, speed / 20);

            for (int s = 0; s < steps && generating; s++) {
                std::vector<HexCoord> validNeighbors;
                validNeighbors.reserve(6);

                for (int i = 0; i < 6; i++) {
                    HexCoord neighbor = hexGfx->getHexNeighbor(currentPos, i);
                    if (hexGfx->hexDistance(neighbor, HexCoord(0, 0)) <= maxRadius) {
                        auto idx = hexGfx->hexToIndex(neighbor);
                        if (idx && !visitedMap[*idx]) {
                            validNeighbors.push_back(neighbor);
                        }
                    }
                }

                if (!validNeighbors.empty()) {
                    // Move to random unvisited neighbor
                    currentPos = validNeighbors[random(0, validNeighbors.size())];
                    path.push_back(currentPos);
                    visitedList.push_back(currentPos);
                    auto idx = hexGfx->hexToIndex(currentPos);
                    if (idx) visitedMap[*idx] = true;
                } else {
                    // Backtrack efficiently: skip dead nodes in one frame
                    bool foundBranch = false;
                    while (path.size() > 1) {
                        path.pop_back();
                        currentPos = path.back();
                        
                        // Check if this new pos has unvisited neighbors
                        for (int i = 0; i < 6; i++) {
                            HexCoord neighbor = hexGfx->getHexNeighbor(currentPos, i);
                            if (hexGfx->hexDistance(neighbor, HexCoord(0, 0)) <= maxRadius) {
                                auto idx = hexGfx->hexToIndex(neighbor);
                                if (idx && !visitedMap[*idx]) {
                                    foundBranch = true;
                                    break;
                                }
                            }
                        }
                        if (foundBranch) break;
                    }

                    if (!foundBranch && path.size() <= 1) {
                        // Maze fully generated
                        generating = false;
                        waitStart = millis();
                    }
                }
            }

            // Draw current path active head
            CRGB headColor = CRGB::White;
            hexGfx->drawHexPixel(currentPos, headColor);
        } else {
            // Finished generation. Wait a few seconds then reset.
            if (millis() - waitStart > 3000) {
                ResetMaze();
            }
        }
    }
};
#endif
