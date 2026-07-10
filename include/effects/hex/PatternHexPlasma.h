#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cmath>
#include <vector>

class PatternHexPlasma : public EffectWithId<PatternHexPlasma>
{
private:
    int speed = 25;
    float time = 0.0f;
    uint8_t hueOffset = 0;

public:
    PatternHexPlasma() : EffectWithId<PatternHexPlasma>("Hex: Plasma") {}
    PatternHexPlasma(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexPlasma>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexPlasma() {}

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

        g()->DimAll(220);
        hueOffset += speed / 20;
        time += speed / 400.0f;

        for (int r = -(HEX_RINGS - 1); r <= (HEX_RINGS - 1); ++r) {
            int q1 = std::max(-(HEX_RINGS - 1), -r - (HEX_RINGS - 1));
            int q2 = std::min(HEX_RINGS - 1, -r + (HEX_RINGS - 1));
            for (int q = q1; q <= q2; ++q) {
                HexCoord hex(q, r);

                // Get pixel coordinates
                PixelCoord pixel = hexGfx->hexToPixelFlatTop(hex, 1.0f, {0.0f, 0.0f});

                // Plasma calculation using multiple sine waves
                float v1 = sinf(pixel.x * 0.1f + time);
                float v2 = sinf(pixel.y * 0.1f + time);
                float v3 = sinf((pixel.x + pixel.y) * 0.1f + time);
                float v4 = sinf(sqrtf(pixel.x * pixel.x + pixel.y * pixel.y) * 0.1f + time);

                float value = (v1 + v2 + v3 + v4) / 4.0f;

                // Map to color
                uint8_t hue = (hueOffset + static_cast<uint8_t>((value + 1.0f) * 127.5f)) % 256;
                uint8_t brightness = static_cast<uint8_t>((value + 1.0f) * 127.5f);

                CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, brightness, LINEARBLEND);
                hexGfx->drawHexPixel(hex, color);
            }
        }
    }
};
#endif
