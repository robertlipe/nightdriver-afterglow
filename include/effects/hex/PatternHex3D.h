//+--------------------------------------------------------------------------
//
// File:        PatternHex3D.h
//
// Isometric 3D tumbling blocks effect.
// Shades hexagons to create a rolling 3D landscape optical illusion.
//
// NightDriverStrip - (c) 2026 Robert Lipe.  All Rights Reserved.
//
// This file is part of the NightDriver software project.
//
//    NightDriver is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    NightDriver is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Nightdriver.  It is normally found in copying.txt
//+--------------------------------------------------------------------------

#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <cmath>

class PatternHex3D : public EffectWithId<PatternHex3D>
{
private:
    int speed = 30;
    float phase = 0;
    uint8_t hueOffset = 0;

public:
    PatternHex3D() : EffectWithId<PatternHex3D>("Hex: Isometric 3D") {}
    PatternHex3D(const JsonObjectConst& jsonObject) : EffectWithId<PatternHex3D>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHex3D() {}

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back(PTY_SPEED, "Speed", SettingSpec::SettingType::Integer, 10.0, 100.0);
        }
        return &mySettingSpecs;
    }

    bool SerializeSettingsToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeSettingsToJSON(root);

        jsonDoc[PTY_SPEED] = speed;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, PTY_SPEED, speed, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        phase += speed / 200.0f;
        hueOffset += speed / 30;

        for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
            HexCoord hex = hexGfx->indexToHexCoord(index);

            // Map hex grid into 3 interlocking sets to form isometric cubes
            int face = ((hex.q - hex.r) % 3 + 3) % 3;

            // Flat-top cartesian for wave math
            float x = std::numbers::sqrt3_v<float> * hex.q + (std::numbers::sqrt3_v<float> / 2.0f) * hex.r;
            float y = 1.5f * hex.r;

            // Generate a wavy height map
            float height = sinf(x * 0.3f + phase) * cosf(y * 0.3f - phase * 0.8f);

            // Base hue for this cube (cube centers share roughly the same coordinates)
            uint8_t cubeHue = hueOffset + static_cast<uint8_t>((x + y) * 5.0f);

            // Adjust brightness and saturation based on the face of the cube
            uint8_t brightness = 255;
            uint8_t sat = 255;

            if (face == 0) {
                // Top face: brightest, slightly desaturated (like catching a highlight)
                brightness = 255;
                sat = 200;
            } else if (face == 1) {
                // Left face: medium brightness
                brightness = 180;
                sat = 255;
            } else {
                // Right face: darkest shadow
                brightness = 90;
                sat = 255;
            }

            // Pulse the face based on the height map
            brightness = static_cast<uint8_t>(brightness * (0.5f + (height + 1.0f) * 0.25f));

            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), cubeHue, brightness, LINEARBLEND);

            // Desaturate slightly to give it a solid "block" feel rather than pure neon
            color = blend(color, CRGB::White, 255 - sat);

            hexGfx->drawHexPixel(hex, color);
        }
    }
};
#endif
