#pragma once

#include "effectmanager.h"
#include "colordata.h"
#include <numeric>

// Derived from https://wokwi.com/projects/289218075224441356
// N Glowing balls in orbit around each other around a rotating plane.

class PatternSMMetaBalls : public EffectWithId<PatternSMMetaBalls>
{
  private:

    int bx[5];
    int by[5];

    uint8_t dist(int x1, int y1, int x2, int y2)
    {
        int a = y2 - y1;
        int b = x2 - x1;
        a *= a;
        b *= b;
        //    uint8_t dist = 220 / (sqrt16(a + b) + 1);
        // Avoid a div/0 crash.
        uint8_t dist = 220 / (sqrt16(a + b + 1));
        return dist;
    }

  public:

    PatternSMMetaBalls() : EffectWithId<PatternSMMetaBalls>("MetaBalls") {}
    PatternSMMetaBalls(const JsonObjectConst &jsonObject) : EffectWithId<PatternSMMetaBalls>(jsonObject) {}

    void Start() override
    {
        g()->Clear();
    }

    void Draw() override
    {
        for (int a = 0; a < 5; a++)
        {
            bx[a] = beatsin16(15 + a * 2, 0, MATRIX_WIDTH - 1, 0, a * 32);
            by[a] = beatsin16(18 + a * 2, 0, MATRIX_HEIGHT - 1, 0, a * 32);
        }
        for (unsigned i = 0; i < MATRIX_WIDTH - 1; i++)
        {
            for (unsigned j = 0; j < MATRIX_HEIGHT - 1; j++)
            {
                uint8_t sum = dist(i, j, bx[0], by[0]);
                for (int a = 1; a < 5; a++)
                {
                    sum = std::add_sat<uint8_t>(sum, dist(i, j, bx[a], by[a]));
                }
                // HeatColors2_p peaks with blue instead of white and looks nicer for this effect
                g()->leds[XY(i, j)] = ColorFromPalette(HeatColors2_p, sum + 220, 254, LINEARBLEND);
            }
        }

        g()->blur2d(g()->leds, MATRIX_WIDTH - 1, 0, MATRIX_HEIGHT - 1, 0, 32);
        fadeAllChannelsToBlackBy(10);
    }
};
