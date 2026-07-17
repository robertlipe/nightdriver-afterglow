#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cmath>
#include <vector>

class PatternHexClock : public EffectWithId<PatternHexClock>
{
private:
    int speed = 30;
    uint8_t hueOffset = 0;
    int maxRadius = HEX_RINGS - 1;

public:
    PatternHexClock() : EffectWithId<PatternHexClock>("Hex: Clock") {}
    PatternHexClock(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexClock>(jsonObject) {
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexClock() {}

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

    // Map a 12-hour time (0.0 to 12.0) to a coordinate on the hex perimeter
    HexCoord getClockHex(float time_12h, int radius) {
        auto hexGfx = hg();
        if (!hexGfx || radius < 1) return HexCoord(0,0);

        // time_12h goes from 0.0 to 12.0
        // 0.0 (12:00) should be straight UP (y < 0 in our coords)
        // 3.0 (3:00) should be straight RIGHT (x > 0)
        float angle = (time_12h / 12.0f) * 2.0f * std::numbers::pi_v<float>;
        
        // Target cartesian vector
        float targetX = std::sin(angle);
        float targetY = -std::cos(angle);
        float targetAtan = std::atan2(targetY, targetX);
        
        // Find the hex in the exact ring that minimizes the angular difference
        std::vector<HexCoord> ring = hexGfx->getHexRing(HexCoord(0,0), radius);
        HexCoord bestHex(0,0);
        float bestDiff = 999.0f;
        
        for (const auto& hex : ring) {
            // Convert HexCoord to Cartesian (flat-top)
            float x = std::numbers::sqrt3_v<float> * hex.q + (std::numbers::sqrt3_v<float> / 2.0f) * hex.r;
            float y = 1.5f * hex.r;

            // Get angle of this pixel (0 is straight right, PI/2 is straight down)
            float hAtan = atan2f(y, x);
            
            float diff = std::abs(targetAtan - hAtan);
            if (diff > std::numbers::pi_v<float>) {
                diff = 2.0f * std::numbers::pi_v<float> - diff;
            }
            
            if (diff < bestDiff) {
                bestDiff = diff;
                bestHex = hex;
            }
        }
        
        return bestHex;
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(230);
        hueOffset += speed / 30;

        HexCoord center(0, 0);

        // Get current local time.
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        int hours   = timeinfo.tm_hour % 12;
        int minutes = timeinfo.tm_min;
        int seconds = timeinfo.tm_sec;

        // Draw clock face rings.
        for (int r = 2; r <= maxRadius; r += 2) {
            std::vector<HexCoord> ring = hexGfx->getHexRing(center, r);
            uint8_t hue = (hueOffset + r * 15) % 256;
            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hue, 64, LINEARBLEND);
            for (const auto& hex : ring) {
                hexGfx->drawHexPixel(hex, color);
            }
        }

        // Draw hour markers (12 positions)
        for (int i = 1; i <= 12; i++) {
            HexCoord marker = getClockHex(i, maxRadius - 1);
            CRGB markerColor = ColorFromPalette(g()->GetCurrentPalette(), hueOffset, 200, LINEARBLEND);
            hexGfx->drawHexPixel(marker, markerColor);
        }

        // Hour hand
        float hourTime = hours + (minutes / 60.0f);
        HexCoord hourEnd = getClockHex(hourTime, maxRadius / 2);
        CRGB hourColor = ColorFromPalette(g()->GetCurrentPalette(), hueOffset, 255, LINEARBLEND);
        hexGfx->drawHexLine(center, hourEnd, hourColor);

        // Minute hand
        float minuteTime = (minutes + (seconds / 60.0f)) / 5.0f; // Scale 60 mins to 12 hours
        HexCoord minuteEnd = getClockHex(minuteTime, maxRadius - 2);
        CRGB minuteColor = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + 85, 255, LINEARBLEND);
        hexGfx->drawHexLine(center, minuteEnd, minuteColor);

        // Second hand
        float secondTime = seconds / 5.0f; // Scale 60 seconds to 12 hours
        HexCoord secondEnd = getClockHex(secondTime, maxRadius - 1);
        CRGB secondColor = CRGB::Red;
        hexGfx->drawHexLine(center, secondEnd, secondColor);

        // Center dot
        CRGB centerColor = CRGB::White;
        hexGfx->drawHexPixel(center, centerColor);
    }

    virtual size_t DesiredFramesPerSecond() const override
    {
        return 25;
    }
};
#endif
