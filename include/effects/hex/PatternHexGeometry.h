//+--------------------------------------------------------------------------
//
// File:        PatternHexGeometry.h
//
// Morphing geometric shapes on the hex grid.
// Expands and contracts interlocking triangles, diamonds, and hexes.
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

#include <algorithm>
#include <cmath>
#include <vector>

class PatternHexGeometry : public EffectWithId<PatternHexGeometry>
{
private:
    int speed = 30;
    int shapeType = 0;
    const int maxShapeType = 4;
    int rotation = 0;
    uint8_t hueOffset = 0;

public:
    PatternHexGeometry() : EffectWithId<PatternHexGeometry>("Hex: Geometry") {}
    PatternHexGeometry(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexGeometry>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
        if (jsonObject[PTY_SHAPE].is<int>()) shapeType = jsonObject[PTY_SHAPE].as<int>();
    }
    virtual ~PatternHexGeometry() {}

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);
    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back(PTY_SPEED, "Speed", SettingSpec::SettingType::Integer, 10.0, 100.0);
            mySettingSpecs.emplace_back(PTY_SHAPE, "Shape Type", SettingSpec::SettingType::Integer, 0.0, 4.0);
        }
        return &mySettingSpecs;
    }

    bool SerializeSettingsToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeSettingsToJSON(root);

        jsonDoc[PTY_SPEED] = speed;
        jsonDoc[PTY_SHAPE] = shapeType;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, PTY_SPEED, speed, value);
        RETURN_IF_SET(name, PTY_SHAPE, shapeType, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    HexCoord getSmoothRotatedPoint(float radius, float angleDegrees) {
        float angleRad = angleDegrees * (std::numbers::pi_v<float> / 180.0f);
        float targetX = std::cos(angleRad) * radius;
        float targetY = std::sin(angleRad) * radius;
        
        // Convert cartesian to axial hex coordinates
        // x = sqrt(3) * q + sqrt(3)/2 * r
        // y = 1.5 * r
        float r = targetY / 1.5f;
        float q = (targetX - (std::numbers::sqrt3_v<float> / 2.0f) * r) / std::numbers::sqrt3_v<float>;
        return HexCoord(std::round(q), std::round(r));
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(200);
        hueOffset += speed / 20;
        
        float smoothRotation = (millis() * speed) / 1000.0f;
        float breath = std::sin(millis() / 500.0f) * 0.5f + 0.5f; // 0.0 to 1.0

        HexCoord center(0, 0);
        float maxRadius = HEX_RINGS - 1;

        switch (shapeType) {
            case 0: // Smooth spinning breathing triangles
                {
                    float r = maxRadius * (0.5f + breath * 0.5f);
                    for (int offset = 0; offset < 360; offset += 120) {
                        HexCoord p1 = getSmoothRotatedPoint(r, smoothRotation + offset);
                        HexCoord p2 = getSmoothRotatedPoint(r, smoothRotation + offset + 120);
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + offset, 255, LINEARBLEND);
                        hexGfx->drawHexLine(p1, p2, color);
                    }
                    
                    // Inverse rotating inner triangle
                    float innerR = maxRadius * (1.0f - breath * 0.5f);
                    for (int offset = 0; offset < 360; offset += 120) {
                        HexCoord p1 = getSmoothRotatedPoint(innerR, -smoothRotation + offset + 60);
                        HexCoord p2 = getSmoothRotatedPoint(innerR, -smoothRotation + offset + 180);
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + 128 + offset, 255, LINEARBLEND);
                        hexGfx->drawHexLine(p1, p2, color);
                    }
                }
                break;

            case 1: // Smooth spinning squares (diamonds)
                {
                    float r = maxRadius * (0.7f + breath * 0.3f);
                    for (int offset = 0; offset < 360; offset += 90) {
                        HexCoord p1 = getSmoothRotatedPoint(r, smoothRotation + offset);
                        HexCoord p2 = getSmoothRotatedPoint(r, smoothRotation + offset + 90);
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + offset, 255, LINEARBLEND);
                        hexGfx->drawHexLine(p1, p2, color);
                    }
                }
                break;

            case 2: // Multi-point star wireframe
                {
                    float innerR = maxRadius * 0.3f;
                    float outerR = maxRadius;
                    for (int offset = 0; offset < 360; offset += 60) {
                        HexCoord pOuter = getSmoothRotatedPoint(outerR, smoothRotation + offset);
                        HexCoord pInner1 = getSmoothRotatedPoint(innerR, smoothRotation + offset + 30);
                        HexCoord pInner2 = getSmoothRotatedPoint(innerR, smoothRotation + offset - 30);
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + offset, 255, LINEARBLEND);
                        hexGfx->drawHexLine(pOuter, pInner1, color);
                        hexGfx->drawHexLine(pOuter, pInner2, color);
                    }
                }
                break;

            case 3: // Spiraling polygon
                {
                    int sides = 6;
                    for (int radiusStep = 1; radiusStep <= maxRadius; radiusStep += 2) {
                        float stepRotation = smoothRotation + (radiusStep * 15);
                        for (int offset = 0; offset < 360; offset += (360 / sides)) {
                            HexCoord p1 = getSmoothRotatedPoint(radiusStep, stepRotation + offset);
                            HexCoord p2 = getSmoothRotatedPoint(radiusStep, stepRotation + offset + (360 / sides));
                            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + radiusStep * 20, 255, LINEARBLEND);
                            hexGfx->drawHexLine(p1, p2, color);
                        }
                    }
                }
                break;

            case 4: // Sine-wave geometric rings
                {
                    for (int i = 0; i < 360; i += 30) {
                        float waveR = maxRadius * (0.5f + std::sin((millis() / 300.0f) + i * 0.1f) * 0.5f);
                        HexCoord p = getSmoothRotatedPoint(waveR, smoothRotation + i);
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + i, 255, LINEARBLEND);
                        hexGfx->drawHexLine(center, p, color);
                    }
                }
                break;
        }
        EVERY_N_SECONDS(10) {
            shapeType = (shapeType + 1) % maxShapeType;
        }
    }
};
#endif
