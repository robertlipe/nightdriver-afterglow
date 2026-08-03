//+--------------------------------------------------------------------------
//
// File:        PatternHexFireworks.h
//
// Fireworks explosions with gravity and trails.
// Rockets launch and detonate into colorful expanding hex rings.
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

struct HexParticle {
    float q;
    float r;
    float vq;
    float vr;
    CRGB color;
    int life;
    int maxLife;
};

class PatternHexFireworks : public EffectWithId<PatternHexFireworks>
{
private:
    int speed = 40;
    std::vector<HexParticle> particles;
    uint8_t hueOffset = 0;
    unsigned long lastLaunch = 0;
    int launchInterval = 800;

public:
    PatternHexFireworks() : EffectWithId<PatternHexFireworks>("Hex: Fireworks") {}
    PatternHexFireworks(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexFireworks>(jsonObject) {
        if (jsonObject[PTY_SPEED].is<int>()) speed = jsonObject[PTY_SPEED].as<int>();
    }
    virtual ~PatternHexFireworks() {}

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

    void LaunchFirework(HexCoord origin)
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        // Cap total particles to prevent memory exhaustion
        static const int MAX_PARTICLES = 500;
        if (particles.size() >= MAX_PARTICLES) {
            return; // Skip launch if too many particles
        }

        uint8_t baseHue = hueOffset + random(0, 64);
        int particleCount = 12 + random(0, 8);

        for (int i = 0; i < particleCount; i++) {
            if (particles.size() >= MAX_PARTICLES) break;

            float angle = random(0, 360) * (std::numbers::pi_v<float> / 180.0f);
            float vel = (random(30, 120) / 1000.0f) * (this->speed / 50.0f);

            HexParticle p;
            p.q = origin.q;
            p.r = origin.r;
            p.vq = std::cos(angle) * vel;
            p.vr = std::sin(angle) * vel;
            p.color = ColorFromPalette(g()->GetCurrentPalette(), baseHue + random(0, 32), 255, LINEARBLEND);
            p.life = 100 + random(0, 50);
            p.maxLife = p.life;

            particles.push_back(p);
        }
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        g()->DimAll(240);
        hueOffset += speed / 20;

        // Reserve capacity for particles to avoid repeated reallocations
        static const int MAX_PARTICLES = 500;
        if (particles.capacity() < MAX_PARTICLES) {
            particles.reserve(MAX_PARTICLES);
        }

        unsigned long now = millis();
        if (now - lastLaunch > launchInterval) {
            // Launch multiple simultaneous fireworks for better effect
            int numLaunches = 1 + random(0, 3); // 1-3 simultaneous launches

            for (int i = 0; i < numLaunches; i++) {
                // Pick random LED index and convert to HexCoord - no allocation
                int randomIndex = random(1, TOTAL_LEDS_IN_HEX);  // Skip center (index 0)
                HexCoord launchPos = hexGfx->indexToHexCoord(randomIndex);
                LaunchFirework(launchPos);
            }
            lastLaunch = now;
            launchInterval = 300 + random(0, 400); // More frequent launches
        }

        // Update and draw particles
        for (auto it = particles.begin(); it != particles.end(); ) {
            HexParticle& p = *it;

            // Update position
            p.q += p.vq;
            p.r += p.vr;
            
            // Fake gravity pulling towards positive r (downwards in hex)
            p.vr += 0.005f * (this->speed / 50.0f);

            // Fade out
            p.life -= std::max(1, this->speed / 10);

            if (p.life <= 0) {
                it = particles.erase(it);
                continue;
            }

            // Draw particle with fade
            uint8_t brightness = (p.life * 255) / p.maxLife;
            CRGB fadedColor = p.color;
            fadedColor.nscale8(brightness);
            HexCoord pos(std::round(p.q), std::round(p.r));
            hexGfx->drawHexPixel(pos, fadedColor);

            ++it;
        }
    }
};
#endif
