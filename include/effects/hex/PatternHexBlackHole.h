#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <numbers>

class PatternHexBlackHole : public EffectWithId<PatternHexBlackHole>
{
private:
    int speed = 50;
    float rotationPhase = 0.0f;
    uint8_t hueOffset = 0;

public:
    PatternHexBlackHole() : EffectWithId<PatternHexBlackHole>("Hex: Black Hole") {}
    PatternHexBlackHole(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexBlackHole>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexBlackHole() {}

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

        // Black holes leave no trails! We replace all pixels
        rotationPhase += speed / 200.0f;
        hueOffset += speed / 50;

        constexpr float sqrt3 = std::numbers::sqrt3_v<float>;
        constexpr float pi = std::numbers::pi_v<float>;

        for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
            HexCoord hex = hexGfx->indexToHexCoord(index);
            
            float x = sqrt3 * hex.q + (sqrt3 / 2.0f) * hex.r;
            float y = 1.5f * hex.r;

            float dist = sqrtf(x * x + y * y);
            float angle = atan2f(y, x);

            // Black hole singularity (Event horizon)
            if (dist < 1.8f) {
                hexGfx->drawHexPixel(hex, CRGB::Black);
                continue;
            }

            // Accretion disk math
            // Swirl effect: add to angle based on distance (closer = faster spin)
            float swirlAngle = angle + rotationPhase + (15.0f / dist);

            // Create arms / bands
            float arms = sinf(swirlAngle * 3.0f); // 3-arm spiral

            // Add some noise to the arms
            uint8_t noiseVal = inoise8(static_cast<uint32_t>(dist * 40), static_cast<uint32_t>(swirlAngle * 40 * 256), static_cast<uint32_t>(rotationPhase * 2000));
            float noiseF = (noiseVal - 128.0f) / 128.0f;

            // Combine arms and noise
            float intensity = (arms + noiseF) * 0.5f + 0.5f;

            // Distance falloff (brighter near center, darker at edges)
            float falloff = std::clamp(1.0f - ((dist - 2.0f) / 8.0f), 0.0f, 1.0f);

            intensity *= falloff;

            uint8_t brightness = static_cast<uint8_t>(intensity * 255.0f);
            
            // Color shifts towards white/blue at the center, red at the edges
            uint8_t hue = hueOffset + static_cast<uint8_t>(dist * 10);

            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, brightness, LINEARBLEND);

            // Redshift / Blueshift logic (simulating doppler effect of rotating disk)
            // If the angle (relative to view) is approaching us, shift blue, receding shift red
            float doppler = cosf(angle - rotationPhase); // -1 to 1
            if (doppler > 0.5f) {
                // Blueshift: add a bit of blue
                color.b = std::min(255, color.b + static_cast<int>(doppler * 50));
            } else if (doppler < -0.5f) {
                // Redshift: add a bit of red
                color.r = std::min(255, color.r + static_cast<int>(-doppler * 50));
            }

            hexGfx->drawHexPixel(hex, color);
        }
    }
};
#endif
