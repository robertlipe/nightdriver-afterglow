#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cmath>
#include <vector>

class PatternHexRadar : public EffectWithId<PatternHexRadar>
{
private:
    int speed = 40;
    int maxRadius = HEX_RINGS - 1;
    float angle = 0.0f;
    uint8_t hueOffset = 0;

public:
    PatternHexRadar() : EffectWithId<PatternHexRadar>("Hex: Radar") {}
    PatternHexRadar(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexRadar>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexRadar() {}

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

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(200);
        hueOffset += speed / 30;
        angle += speed / 200.0f;

        HexCoord center(0, 0);

        // Convert angle to hex direction (0-5)
        int direction = static_cast<int>((angle / (2.0f * 3.14159f)) * 6.0f) % 6;

        // Draw scanning wedge
        int wedgeSize = 2; // Size of wedge in directions
        int startDir = direction - wedgeSize;
        int endDir = direction + wedgeSize;

        hexGfx->drawHexWedge(center, startDir, endDir, maxRadius,
            ColorFromPalette(g()->GetCurrentPalette(), hueOffset, 128, LINEARBLEND));

        // Draw range rings
        for (int r = 2; r <= maxRadius; r += 2) {
            // Use precomputed ring data - no allocation
            auto ring = hexGfx->getHexRing(r);
            CRGB ringColor = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + r * 10, 64, LINEARBLEND);
            for (const auto& hex : ring) {
                hexGfx->drawHexPixel(hex, ringColor);
            }
        }

        // Draw center blip
        CRGB centerColor = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + 128, 255, LINEARBLEND);
        hexGfx->drawHexPixel(center, centerColor);
    }
};
#endif
