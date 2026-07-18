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

    HexCoord getSmoothRotatedPoint(float hexRadius, float angleDegrees) {
        float angleRad = angleDegrees * (std::numbers::pi_v<float> / 180.0f);
        // Convert hex rings to physical cartesian radius so we actually reach the edges
        float cartesianRadius = hexRadius * std::numbers::sqrt3_v<float>;
        float targetX = std::cos(angleRad) * cartesianRadius;
        float targetY = std::sin(angleRad) * cartesianRadius;
        
        // Convert cartesian to axial hex coordinates
        // x = sqrt(3) * q + sqrt(3)/2 * r
        // y = 1.5 * r
        float r = targetY / 1.5f;
        float q = (targetX - (std::numbers::sqrt3_v<float> / 2.0f) * r) / std::numbers::sqrt3_v<float>;
        return HexCoord(std::round(q), std::round(r));
    }

    void drawCurvedArc(std::shared_ptr<GFXHex> hexGfx, float startRadius, float endRadius, float startAngle, float endAngle, CRGB color) {
        int steps = std::max(5.0f, std::abs(endRadius - startRadius) * 2.0f);
        HexCoord lastP = getSmoothRotatedPoint(startRadius, startAngle);
        for (int i = 1; i <= steps; i++) {
            float t = (float)i / steps;
            float r = startRadius + t * (endRadius - startRadius);
            float a = startAngle + t * (endAngle - startAngle);
            HexCoord p = getSmoothRotatedPoint(r, a);
            hexGfx->drawHexLine(lastP, p, color);
            lastP = p;
        }
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

            case 1: // Pulsing Curved Atom / Fan Leaves
                {
                    float outerR = maxRadius * (0.8f + breath * 0.2f);
                    for (int offset = 0; offset < 360; offset += 60) {
                        // Draw sweeping curves outwards from the center
                        float twist = 60.0f * std::sin(millis() / 1000.0f); // dynamic twisting arc
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + offset, 255, LINEARBLEND);
                        drawCurvedArc(hexGfx, 0.0f, outerR, smoothRotation + offset, smoothRotation + offset + twist, color);
                        
                        // Cross-link them like fan blades
                        HexCoord p1 = getSmoothRotatedPoint(outerR, smoothRotation + offset + twist);
                        HexCoord p2 = getSmoothRotatedPoint(outerR * 0.5f, smoothRotation + offset + twist + 30);
                        hexGfx->drawHexLine(p1, p2, color);
                    }
                }
                break;

            case 2: // Multi-point star wireframe (now with breathing curves)
                {
                    float innerR = maxRadius * (0.2f + breath * 0.2f);
                    float outerR = maxRadius;
                    for (int offset = 0; offset < 360; offset += 60) {
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + offset, 255, LINEARBLEND);
                        // Draw curved lines instead of straight lines to the star points
                        drawCurvedArc(hexGfx, innerR, outerR, smoothRotation + offset + 30, smoothRotation + offset, color);
                        drawCurvedArc(hexGfx, innerR, outerR, smoothRotation + offset - 30, smoothRotation + offset, color);
                    }
                }
                break;

            case 3: // Spiraling polygon webs
                {
                    int sides = 6;
                    for (int radiusStep = 1; radiusStep <= maxRadius; radiusStep += 2) {
                        // The rotation increases with radius, creating a true spiral
                        float stepRotation = smoothRotation + (radiusStep * (15.0f + 10.0f * breath));
                        for (int offset = 0; offset < 360; offset += (360 / sides)) {
                            HexCoord p1 = getSmoothRotatedPoint(radiusStep, stepRotation + offset);
                            HexCoord p2 = getSmoothRotatedPoint(radiusStep, stepRotation + offset + (360 / sides));
                            CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + radiusStep * 20, 255, LINEARBLEND);
                            hexGfx->drawHexLine(p1, p2, color);
                        }
                    }
                }
                break;

            case 4: // Electron Arcs / Sine-wave geometric rings
                {
                    for (int i = 0; i < 360; i += 45) {
                        float waveStartR = maxRadius * 0.2f;
                        float waveEndR = maxRadius * (0.6f + std::sin((millis() / 300.0f) + i * 0.1f) * 0.4f);
                        CRGB color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + i, 255, LINEARBLEND);
                        // Draw curving "electron orbits" that sweep out and back
                        drawCurvedArc(hexGfx, waveStartR, waveEndR, smoothRotation + i, smoothRotation + i + 90, color);
                        drawCurvedArc(hexGfx, waveEndR, waveStartR, smoothRotation + i + 90, smoothRotation + i + 180, color);
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
