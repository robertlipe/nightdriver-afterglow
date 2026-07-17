#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cmath>
#include <vector>

class PatternHexKaleidoscope : public EffectWithId<PatternHexKaleidoscope>
{
private:
    int speed = 8;
    int segments = 6;
    float fHueOffset = 0.0f;
    float phase = 0.0f;

public:
    PatternHexKaleidoscope() : EffectWithId<PatternHexKaleidoscope>("Hex: Kaleidoscope") {}
    PatternHexKaleidoscope(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexKaleidoscope>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
        if (jsonObject["segments"].is<int>()) segments = jsonObject["segments"].as<int>();
    }
    virtual ~PatternHexKaleidoscope() {}

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back("speed", "Speed", SettingSpec::SettingType::Integer, 1.0, 100.0);
            mySettingSpecs.emplace_back("segments", "Segments", SettingSpec::SettingType::Integer, 3.0, 6.0);
        }
        return &mySettingSpecs;
    }

    bool SerializeSettingsToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeSettingsToJSON(root);

        jsonDoc["speed"] = speed;
        jsonDoc["segments"] = segments;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, "speed", speed, value);
        RETURN_IF_SET(name, "segments", segments, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        // Use float math for smooth speed control down to 1
        fHueOffset += speed / 15.0f;
        phase += speed / 300.0f;
        
        uint8_t hueOffset = static_cast<uint8_t>(fHueOffset) % 256;

        constexpr float sqrt3 = std::numbers::sqrt3_v<float>;
        constexpr float pi = std::numbers::pi_v<float>;

        for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
            HexCoord hex = hexGfx->indexToHexCoord(index);
            if (hex.q == 0 && hex.r == 0) {
                 CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset, 255, LINEARBLEND);
                 hexGfx->drawHexPixel(hex, color);
                 continue;
            }

            HexCoord mapped = hex;
            int rotationStep = 6 / segments;
            if (rotationStep < 1) rotationStep = 1;

            for (int rot = 0; rot < 6; rot += rotationStep) {
                float mx = sqrt3 * mapped.q + (sqrt3/2.0f) * mapped.r;
                float my = 1.5f * mapped.r;
                float mAngle = atan2f(my, mx);
                if (mAngle < 0.0f) mAngle += 2.0f * pi;

                if (mAngle <= (2.0f * pi / segments)) {
                    break;
                }
                mapped = hexGfx->hexRotate(mapped, rotationStep);
            }

            float x = sqrt3 * mapped.q + (sqrt3/2.0f) * mapped.r;
            float y = 1.5f * mapped.r;

            float pattern = sinf(x * 0.5f + phase) * cosf(y * 0.5f - phase*0.8f);
            float dist = sqrtf(x*x + y*y);
            pattern += sinf(dist * 0.8f - phase * 2.0f) * 0.5f;

            uint8_t finalPattern = static_cast<uint8_t>(std::clamp((pattern + 1.5f) * 85.0f, 0.0f, 255.0f));
            uint8_t hue = hueOffset + finalPattern;
            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, 255, LINEARBLEND);

            hexGfx->drawHexPixel(hex, color);
        }
    }
};
#endif
