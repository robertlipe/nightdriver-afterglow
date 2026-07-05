#pragma once

#include "effectmanager.h"

#include <algorithm>

// Inspired from https://editor.soulmatelights.com/gallery/843-squares-and-dots
// This looks better on 2812's than on HUB75.

class PatternSMSquaresAndDots : public EffectWithId<PatternSMSquaresAndDots>
{
  private:
    const uint8_t sprites[2][3][3] = {
        1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    };

  public:
    PatternSMSquaresAndDots() : EffectWithId<PatternSMSquaresAndDots>("Squares and Dots")
    {
    }

    PatternSMSquaresAndDots(const JsonObjectConst &jsonObject) : EffectWithId<PatternSMSquaresAndDots>(jsonObject)
    {
    }

    void printSpr(int x, int y, int numSpr)
    {
        int hue = random8();
        int startX = std::clamp(x, 3, MATRIX_WIDTH - 3);
        int startY = std::clamp(y, 3, MATRIX_HEIGHT - 3);
        for (unsigned j = 0; j < 3; j++)
        {
            for (unsigned i = 0; i < 3; i++)
            {
                uint16_t index = XY(startX + i, startY + j);
                if (sprites[numSpr][i][j])
                {
                    g()->leds[index].setHue(hue);
                }
                else
                {
                    g()->leds[index] = 0;
                }
            }
        }
    }

    void Start() override
    {
        g()->Clear();
        for (unsigned x = 0; x < MATRIX_WIDTH / 3 + 1; x++)
        {
            for (unsigned y = 0; y < MATRIX_HEIGHT / 3 + 1; y++)
            {
                printSpr(x * 3, y * 3, random8(2));
            }
        }
    }

    void Draw() override
    {
        EVERY_N_MILLISECONDS(300)
        {
            printSpr((random8(MATRIX_WIDTH) % (MATRIX_WIDTH / 3 + 1)) * 3,
                     (random8(MATRIX_HEIGHT) % (MATRIX_HEIGHT / 3 + 1)) * 3, random8(2));
        }
    }
};
