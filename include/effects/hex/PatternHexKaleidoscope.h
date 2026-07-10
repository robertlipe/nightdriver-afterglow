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
    int speed = 25;
    int segments = 6;
    uint8_t hueOffset = 0;
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
            mySettingSpecs.emplace_back("speed", "Speed", SettingSpec::SettingType::Integer, 10.0, 100.0);
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

        g()->DimAll(230);
        hueOffset += speed / 15;
        phase += speed / 300.0f;

        HexCoord center(0, 0);
        int maxRadius = HEX_RINGS - 1;

        // Draw one segment, then mirror it
        int segmentAngle = 6 / segments;

        for (int seg = 0; seg < segments; seg++) {
            int baseDir = seg * segmentAngle;

            // Create pattern in this segment
            for (int r = 1; r <= maxRadius; r++) {
                for (int d = 0; d < segmentAngle; d++) {
                    int dir = (baseDir + d) % 6;
                    HexCoord hex = hexGfx->hexAdd(center, hexGfx->hexScale(hexGfx->getHexDirection(dir), r));

                    // Calculate pattern value
                    float pattern = sinf(r * 0.5f + phase) * cosf(d * 1.0f + phase);
                    uint8_t brightness = static_cast<uint8_t>((pattern + 1.0f) * 127.5f);

                    if (brightness > 50) {
                        uint8_t hue = (hueOffset + r * 10 + d * 20) % 256;
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, brightness, LINEARBLEND);
                        hexGfx->drawHexPixel(hex, color);
                    }
                }
            }
        }
    }
};
#endif
