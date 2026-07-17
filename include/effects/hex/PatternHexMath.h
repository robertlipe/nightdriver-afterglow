#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

class PatternHexMath : public EffectWithId<PatternHexMath>
{
private:
    int patternType = 0;
    const int maxPatternType = 4;
    int scale = 3;
    int speed = 20;

public:
    PatternHexMath() : EffectWithId<PatternHexMath>("Hex: Math Patterns") {}
    PatternHexMath(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexMath>(jsonObject) {
        if (jsonObject["pattern"].is<int>()) patternType = jsonObject["pattern"].as<int>();
        if (jsonObject["scale"].is<int>()) scale = jsonObject["scale"].as<int>();
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexMath() {}



    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back("pattern", "Pattern Type", SettingSpec::SettingType::Integer, 0.0, 4.0);
            mySettingSpecs.emplace_back("scale", "Scale", SettingSpec::SettingType::Integer, 1.0, 10.0);
            mySettingSpecs.emplace_back("speed", "Speed", SettingSpec::SettingType::Integer, 0.0, 100.0);
        }
        return &mySettingSpecs;
    }

    bool SerializeSettingsToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeSettingsToJSON(root);

        jsonDoc["pattern"] = patternType;
        jsonDoc["scale"] = scale;
        jsonDoc["speed"] = speed;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, "pattern", patternType, value);
        RETURN_IF_SET(name, "scale", scale, value);
        RETURN_IF_SET(name, "speed", speed, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    void Draw() override
    {
        uint32_t ms = millis() * speed / 20;

        auto hexGfx = hg();
        if (!hexGfx) return;

        for (int r = -(HEX_RINGS - 1); r <= (HEX_RINGS - 1); ++r) {
            int q1 = std::max(-(HEX_RINGS - 1), -r - (HEX_RINGS - 1));
            int q2 = std::min(HEX_RINGS - 1, -r + (HEX_RINGS - 1));
            for (int q = q1; q <= q2; ++q) {
                HexCoord hex(q, r);
                int s = hex.s;

                uint8_t index = 0;
                switch (patternType) {
                    case 0: // Stripes Q
                        index = q * scale + ms / 10;
                        break;
                    case 1: // Stripes R
                        index = r * scale + ms / 10;
                        break;
                    case 2: // Stripes S
                        index = s * scale + ms / 10;
                        break;
                    case 3: // Checkerboard
                        index = ((q - r + 300) % 3) * 85 + ms / 20; // Rotate slowly
                        break;
                    case 4: // Concentric Rings
                        index = std::max({std::abs(q), std::abs(r), std::abs(s)}) * scale * 5 - ms / 5;
                        break;
                    default:
                        // put the train back on the rails.
                        patternType = 0;
                        break;
                }

                CRGB color = ColorFromPalette(g()->GetCurrentPalette(), index, 255, LINEARBLEND);
                hexGfx->drawHexPixel(hex, color);
            }
        }

        // Optional small blur to smooth out movement.
        blur1d(g()->leds, TOTAL_LEDS_IN_HEX, 16);

        EVERY_N_SECONDS(5) {
            patternType = (patternType + 1) % maxPatternType;
        }
    }
};
#endif
