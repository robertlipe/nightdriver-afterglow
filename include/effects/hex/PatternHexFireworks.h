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
    HexCoord position;
    HexCoord velocity;
    CRGB color;
    uint8_t life;
    uint8_t maxLife;
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
        if (jsonObject["speed"].is<int>()) speed = jsonObject["speed"].as<int>();
    }
    virtual ~PatternHexFireworks() {}

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

            HexCoord dir = hexGfx->getHexDirection(random(0, 6));
            int speed = 1 + random(0, 2);

            HexParticle p;
            p.position = origin;
            p.velocity = hexGfx->hexScale(dir, speed);
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
            p.position = hexGfx->hexAdd(p.position, p.velocity);

            // Fade out
            p.life -= speed / 10;

            if (p.life <= 0) {
                it = particles.erase(it);
                continue;
            }

            // Draw particle with fade
            uint8_t brightness = (p.life * 255) / p.maxLife;
            CRGB fadedColor = p.color;
            fadedColor.nscale8(brightness);
            hexGfx->drawHexPixel(p.position, fadedColor);

            ++it;
        }
    }
};
#endif
