#pragma once

#include "effectmanager.h"
#include <algorithm>

// Inspired by https://editor.soulmatelights.com/gallery/1620-rainbow-tunel
// Like Hypnosis, a swirling radial rainbow, but entering a black hole.

class PatternSMRainbowTunnel : public EffectWithId<PatternSMRainbowTunnel>
{
  public:

    PatternSMRainbowTunnel() : EffectWithId<PatternSMRainbowTunnel>("Colorspin") {}
    PatternSMRainbowTunnel(const JsonObjectConst &jsonObject) : EffectWithId<PatternSMRainbowTunnel>(jsonObject) {}

    void Start() override
    {
        g()->Clear();
    }

    void Draw() override
    {
        static constexpr uint8_t scaleX = 4;
        static constexpr uint8_t scaleY = 4;
        static constexpr uint8_t speed = 2;

        static uint16_t t;

        t += speed;
        const auto& rMap = GFXBase::getPolarMap();

        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            for (int y = 0; y < MATRIX_HEIGHT; y++)
            {
                uint8_t angle = rMap[x][y].angle;
                uint8_t radius = rMap[x][y].scaled_radius;
                g()->leds[XY(x, y)] =
                    CHSV((angle * scaleX) - t + (radius * scaleY), 255, std::clamp(radius * 3, 0, 255));
            }
        }
    }
};
