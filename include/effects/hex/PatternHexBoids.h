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
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexBoids() {}

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

    void InitBoids()
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        HexCoord center(0, 0);
        boids.clear();

        for (int i = 0; i < numBoids; i++) {
            HexBoid b;
            // Pick random LED index and convert to HexCoord - no allocation
            int randomIndex = random(1, TOTAL_LEDS_IN_HEX);  // Skip center (index 0)
            HexCoord pos = hexGfx->indexToHexCoord(randomIndex);

            // Verify position is within bounds
            if (hexGfx->hexDistance(pos, center) > maxRadius - 2) {
                b.position = center;
            } else {
                b.position = pos;
            }

            // Random velocity
            int dir = random(0, 6);
            b.velocity = hexGfx->getHexDirection(dir);

            b.color = ColorFromPalette(g()->GetCurrentPalette(), hueOffset + i * 20, 255, LINEARBLEND);
            b.trail.clear(); // Initialize empty trail
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

        // Update and draw boids
        for (auto& boid : boids) {
            // Add current position to trail
            boid.trail.push_back(boid.position);
            if (boid.trail.size() > HexBoid::TRAIL_LENGTH) {
                boid.trail.erase(boid.trail.begin());
            }

            // Simple flocking: move towards center with some randomness
            float dist = hexGfx->hexDistance(boid.position, center);

            if (dist > maxRadius - 2) {
                // Steer towards center
                HexCoord toCenter = hexGfx->hexSubtract(center, boid.position);
                if (toCenter.q != 0 || toCenter.r != 0) {
                    // Normalize and apply
                    if (toCenter.q > 0) boid.velocity.q = 1;
                    else if (toCenter.q < 0) boid.velocity.q = -1;
                    else boid.velocity.q = 0;

                    if (toCenter.r > 0) boid.velocity.r = 1;
                    else if (toCenter.r < 0) boid.velocity.r = -1;
                    else boid.velocity.r = 0;
                }
            } else {
                // Random direction change occasionally
                if (random(0, 100) < 10) {
                    int newDir = random(0, 6);
                    boid.velocity = hexGfx->getHexDirection(newDir);
                }
            }

            // Move
            boid.position = hexGfx->hexAdd(boid.position, boid.velocity);

            // Keep in bounds
            if (hexGfx->hexDistance(boid.position, center) > maxRadius) {
                boid.position = center;
                boid.trail.clear(); // Clear trail when resetting position
            }

            // Draw trail with fading brightness
            for (size_t i = 0; i < boid.trail.size(); i++) {
                uint8_t brightness = (i + 1) * 255 / boid.trail.size();
                CRGB trailColor = boid.color;
                trailColor.nscale8(brightness / 2); // Dim trail
                hexGfx->drawHexPixel(boid.trail[i], trailColor);
            }

            // Draw boid head (brighter)
            hexGfx->drawHexPixel(boid.position, boid.color);
        }
    }
};
#endif
