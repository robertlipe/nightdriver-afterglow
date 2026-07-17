#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cmath>
#include <vector>

class PatternHexGeometry : public EffectWithId<PatternHexGeometry>
{
private:
    int speed = 30;
    int shapeType = 0;
    const int maxShapeType = 4;
    int rotation = 0;
    uint8_t hueOffset = 0;

public:
    PatternHexGeometry() : EffectWithId<PatternHexGeometry>("Hex: Geometry") {}
    PatternHexGeometry(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexGeometry>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
        if (jsonObject["shape"].is<int>()) shapeType = jsonObject["shape"].as<int>();
    }
    virtual ~PatternHexGeometry() {}

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back("speed", "Speed", SettingSpec::SettingType::Integer, 10.0, 100.0);
            mySettingSpecs.emplace_back("shape", "Shape Type", SettingSpec::SettingType::Integer, 0.0, 4.0);
        }
        return &mySettingSpecs;
    }

    bool SerializeSettingsToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeSettingsToJSON(root);

        jsonDoc["speed"] = speed;
        jsonDoc["shape"] = shapeType;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, "speed", speed, value);
        RETURN_IF_SET(name, "shape", shapeType, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(220);
        hueOffset += speed / 20;
        rotation = (rotation + speed / 30) % 6;

        HexCoord center(0, 0);
        int maxRadius = HEX_RINGS - 1;

        switch (shapeType) {
            case 0: // Rotating triangle
                {
                    for (int i = 0; i < 3; i++) {
                        int dir = (rotation + i * 2) % 6;
                        HexCoord end = hexGfx->hexAdd(center, hexGfx->hexScale(hexGfx->getHexDirection(dir), maxRadius));
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + i * 85, 255, LINEARBLEND);
                        hexGfx->drawHexLine(center, end, color);
                    }
                }
                break;

            case 1: // Rotating hexagon outline
                {
                    for (int r = 1; r <= maxRadius; r++) {
                        uint8_t hue = (hueOffset + r * 15) % 256;
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, 255, LINEARBLEND);
                        // Use precomputed ring data - no allocation
                        auto ring = hexGfx->getHexRing(r);
                        for (const auto& hex : ring) {
                            HexCoord rotated = hexGfx->hexRotate(hex, rotation);
                            hexGfx->drawHexPixel(rotated, color);
                        }
                    }
                }
                break;

            case 2: // Star pattern
                {
                    for (int i = 0; i < 6; i++) {
                        int dir = (rotation + i) % 6;
                        HexCoord end = hexGfx->hexAdd(center, hexGfx->hexScale(hexGfx->getHexDirection(dir), maxRadius));
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + i * 42, 255, LINEARBLEND);
                        hexGfx->drawHexLine(center, end, color);
                    }
                    // Inner star
                    for (int i = 0; i < 6; i++) {
                        int dir = (rotation + i) % 6;
                        HexCoord mid = hexGfx->hexAdd(center, hexGfx->hexScale(hexGfx->getHexDirection(dir), maxRadius / 2));
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + i * 42 + 128, 200, LINEARBLEND);
                        hexGfx->drawHexLine(center, mid, color);
                    }
                }
                break;

            case 3: // Concentric triangles
                {
                    for (int r = 1; r <= maxRadius; r += 2) {
                        for (int i = 0; i < 3; i++) {
                            int dir = (rotation + i * 2) % 6;
                            HexCoord end = hexGfx->hexAdd(center, hexGfx->hexScale(hexGfx->getHexDirection(dir), r));
                            uint8_t hue = (hueOffset + r * 20) % 256;
                            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, 255, LINEARBLEND);
                            hexGfx->drawHexLine(center, end, color);
                        }
                    }
                }
                break;

            case 4: // Diamond pattern
                {
                    for (int r = 1; r <= maxRadius; r++) {
                        for (int i = 0; i < 6; i++) {
                            int dir = (rotation + i) % 6;
                            HexCoord end = hexGfx->hexAdd(center, hexGfx->hexScale(hexGfx->getHexDirection(dir), r));
                            uint8_t hue = (hueOffset + r * 15 + i * 42) % 256;
                            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, 200, LINEARBLEND);
                            hexGfx->drawHexPixel(end, color);
                        }
                    }
                }
                break;
        }
        EVERY_N_SECONDS(5) {
            shapeType = (shapeType + 1) % maxShapeType;
        }
    }
};
#endif
