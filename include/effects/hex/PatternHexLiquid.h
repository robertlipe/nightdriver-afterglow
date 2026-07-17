#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cmath>
#include <numbers>

class PatternHexLiquid : public EffectWithId<PatternHexLiquid>
{
private:
    int speed = 30;
    int scale = 40;
    uint8_t hueOffset = 0;
    uint16_t z_offset = 0;

public:
    PatternHexLiquid() : EffectWithId<PatternHexLiquid>("Hex: Liquid") {}
    PatternHexLiquid(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexLiquid>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
        if (jsonObject["scale"].is<int>()) scale = jsonObject["scale"].as<int>();
    }
    virtual ~PatternHexLiquid() {}

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back("speed", "Speed", SettingSpec::SettingType::Integer, 1.0, 100.0);
            mySettingSpecs.emplace_back("scale", "Scale", SettingSpec::SettingType::Integer, 10.0, 100.0);
        }
        return &mySettingSpecs;
    }

    bool SerializeSettingsToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeSettingsToJSON(root);

        jsonDoc["speed"] = speed;
        jsonDoc["scale"] = scale;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, "speed", speed, value);
        RETURN_IF_SET(name, "scale", scale, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        z_offset += speed;
        hueOffset += speed / 20;

        constexpr float sqrt3 = std::numbers::sqrt3_v<float>;

        for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
            HexCoord hex = hexGfx->indexToHexCoord(index);
            
            // Convert to pseudo-cartesian for noise sampling
            float x = sqrt3 * hex.q + (sqrt3 / 2.0f) * hex.r;
            float y = 1.5f * hex.r;

            // Scale coordinates
            uint32_t nx = static_cast<uint32_t>((x + 20.0f) * scale * 256);
            uint32_t ny = static_cast<uint32_t>((y + 20.0f) * scale * 256);

            // Get 3D noise
            uint8_t noiseVal = inoise8(nx, ny, z_offset);

            // Create a sharp threshold for the lava-lamp "blobs"
            // Smoothstep-like transition between blobs and background
            uint8_t brightness = 0;
            uint8_t hue = hueOffset;

            if (noiseVal > 140) {
                brightness = 255;
                hue += 40; // Blobs are a different color
            } else if (noiseVal > 120) {
                // Anti-aliased edge
                brightness = (noiseVal - 120) * 12;
                hue += (noiseVal - 120) * 2;
            } else {
                // Background
                brightness = 40;
                hue += 128; // Complementary background color
            }

            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, brightness, LINEARBLEND);
            hexGfx->drawHexPixel(hex, color);
        }
    }
};
#endif
