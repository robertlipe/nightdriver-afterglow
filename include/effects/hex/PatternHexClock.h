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
        float T = fmod(time_12h, 12.0f);
        if (T < 0) T += 12.0f;
        
        int segment;
        float frac;
        if (T >= 1.0f && T < 3.0f) { segment = 1; frac = (T - 1.0f) / 2.0f; }      // 1:00 to 3:00 (C1 to C0)
        else if (T >= 3.0f && T < 5.0f) { segment = 0; frac = (T - 3.0f) / 2.0f; } // 3:00 to 5:00 (C0 to C5)
        else if (T >= 5.0f && T < 7.0f) { segment = 5; frac = (T - 5.0f) / 2.0f; } // 5:00 to 7:00 (C5 to C4)
        else if (T >= 7.0f && T < 9.0f) { segment = 4; frac = (T - 7.0f) / 2.0f; } // 7:00 to 9:00 (C4 to C3)
        else if (T >= 9.0f && T < 11.0f) { segment = 3; frac = (T - 9.0f) / 2.0f; } // 9:00 to 11:00 (C3 to C2)
        else { 
            segment = 2; // 11:00 to 1:00 (C2 to C1)
            if (T >= 11.0f) frac = (T - 11.0f) / 2.0f;
            else frac = (T + 1.0f) / 2.0f;
        }
        
        int startCorner = segment;
        int endCorner = (segment + 5) % 6; // Moving clockwise -> mathematically -1 mod 6 for our axes
        
        HexCoord cStart = HexagonGFX::hexScale(HexagonGFX::getHexDirection(startCorner), radius);
        HexCoord cEnd = HexagonGFX::hexScale(HexagonGFX::getHexDirection(endCorner), radius);
        
        return HexagonGFX::hexRound(
            cStart.q + frac * (cEnd.q - cStart.q),
            cStart.r + frac * (cEnd.r - cStart.r),
            cStart.s + frac * (cEnd.s - cStart.s)
        );
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
