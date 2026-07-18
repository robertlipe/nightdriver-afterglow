//+--------------------------------------------------------------------------
//
// File:        PatternHexBoids.h
//
// Boids flocking simulation on a hex grid.
// Particles exhibit separation, alignment, and cohesion behaviors.
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
#include "systemcontainer.h"
#include "gfxhex.h"
#include <algorithm>
#include <cmath>
#include <vector>

struct HexBoid {
    HexCoord position;
    HexCoord velocity;
    CRGB color;
    std::vector<HexCoord> trail; // Store recent positions for trail effect
    static const int TRAIL_LENGTH = 8;
};

class PatternHexBoids : public EffectWithId<PatternHexBoids>
{
private:
    int speed = 40;
    std::vector<HexBoid> boids;
    uint8_t hueOffset = 0;
    int maxRadius = HEX_RINGS - 1;
    int numBoids = 15;

public:
    PatternHexBoids() : EffectWithId<PatternHexBoids>("Hex: Boids") {}
    PatternHexBoids(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexBoids>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHexBoids() {}

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

    void InitBoids()
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        boids.clear();

        for (int i = 0; i < numBoids; i++) {
            HexBoid b;
            int randomIndex = random(1, TOTAL_LEDS_IN_HEX);
            b.position = hexGfx->indexToHexCoord(randomIndex);

            // Start with random velocity
            b.velocity = hexGfx->getHexDirection(random(0, 6));

            b.color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + i * 20, 255, LINEARBLEND);
            b.trail.clear();
            boids.push_back(b);
        }
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(220);
        hueOffset += speed / 25;

        if (boids.empty()) {
            InitBoids();
        }

        HexCoord center(0, 0);

        // Flocking parameters
        float desiredSeparation = 3.5f; // Was 2.0f
        float neighborDist = 6.0f;      // Was 4.0f

        // Update and draw boids
        std::vector<HexBoid> nextBoids = boids;

        for (size_t i = 0; i < boids.size(); i++) {
            auto& boid = boids[i];
            auto& nextBoid = nextBoids[i];

            // Add current position to trail
            nextBoid.trail.push_back(nextBoid.position);
            if (nextBoid.trail.size() > HexBoid::TRAIL_LENGTH) {
                nextBoid.trail.erase(nextBoid.trail.begin());
            }

            // Calculate Flocking Forces
            float sepQ = 0, sepR = 0, sepS = 0;
            float aliQ = 0, aliR = 0, aliS = 0;
            float cohQ = 0, cohR = 0, cohS = 0;
            int sepCount = 0, aliCount = 0, cohCount = 0;

            for (size_t j = 0; j < boids.size(); j++) {
                if (i == j) continue;
                auto& other = boids[j];
                float d = hexGfx->hexDistance(boid.position, other.position);

                if (d > 0 && d < desiredSeparation) {
                    sepQ += (boid.position.q - other.position.q) / d;
                    sepR += (boid.position.r - other.position.r) / d;
                    sepS += (boid.position.s - other.position.s) / d;
                    sepCount++;
                }
                if (d > 0 && d < neighborDist) {
                    aliQ += other.velocity.q;
                    aliR += other.velocity.r;
                    aliS += other.velocity.s;
                    aliCount++;

                    cohQ += other.position.q;
                    cohR += other.position.r;
                    cohS += other.position.s;
                    cohCount++;
                }
            }

            float vQ = boid.velocity.q;
            float vR = boid.velocity.r;
            float vS = boid.velocity.s;

            // Apply Separation
            if (sepCount > 0) {
                vQ += (sepQ / sepCount) * 2.5f;
                vR += (sepR / sepCount) * 2.5f;
                vS += (sepS / sepCount) * 2.5f;
            }
            // Apply Alignment
            if (aliCount > 0) {
                vQ += (aliQ / aliCount) * 1.5f;
                vR += (aliR / aliCount) * 1.5f;
                vS += (aliS / aliCount) * 1.5f;
            }
            // Apply Cohesion
            if (cohCount > 0) {
                vQ += ((cohQ / cohCount) - boid.position.q) * 0.8f;
                vR += ((cohR / cohCount) - boid.position.r) * 0.8f;
                vS += ((cohS / cohCount) - boid.position.s) * 0.8f;
            }

            // Boundary avoidance (steer to center gently)
            float distToCenter = hexGfx->hexDistance(boid.position, center);
            if (distToCenter > maxRadius - 2) {
                vQ += (center.q - boid.position.q) * 0.4f;
                vR += (center.r - boid.position.r) * 0.4f;
                vS += (center.s - boid.position.s) * 0.4f;
            }

            // Occasionally add random noise to prevent locking up
            if (random(0, 100) < 25) {
                HexCoord randDir = hexGfx->getHexDirection(random(0, 6));
                vQ += randDir.q * 1.0f;
                vR += randDir.r * 1.0f;
                vS += randDir.s * 1.0f;
            }

            // Convert accumulated float vectors to nearest hex direction
            int bestDir = 0;
            float bestDot = -9999.0f;
            for (int dir = 0; dir < 6; dir++) {
                HexCoord hDir = hexGfx->getHexDirection(dir);
                float dot = (vQ * hDir.q) + (vR * hDir.r) + (vS * hDir.s);
                if (dot > bestDot) {
                    bestDot = dot;
                    bestDir = dir;
                }
            }

            nextBoid.velocity = hexGfx->getHexDirection(bestDir);
            nextBoid.position = hexGfx->hexAdd(nextBoid.position, nextBoid.velocity);

            // Bounds failsafe
            if (hexGfx->hexDistance(nextBoid.position, center) > maxRadius) {
                nextBoid.position = center;
                nextBoid.trail.clear();
            }
        }

        boids = nextBoids;

        // Draw phase
        for (auto& boid : boids) {
            for (size_t i = 0; i < boid.trail.size(); i++) {
                uint8_t brightness = (i + 1) * 255 / boid.trail.size();
                CRGB trailColor = boid.color;
                trailColor.nscale8(brightness / 2);
                hexGfx->drawHexPixel(boid.trail[i], trailColor);
            }
            hexGfx->drawHexPixel(boid.position, boid.color);
        }
    }
};
#endif
