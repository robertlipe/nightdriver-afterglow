#pragma once

#include "effectmanager.h"
#include <cmath>
#include <algorithm>

// Derived from
// https://editor.soulmatelights.com/gallery/2269-aaron-gotwalts-unknown-pleasure
//
// Colored, accelerated, gravity popcorn balls.

class PatternSMColorPopcorn : public EffectWithId<PatternSMColorPopcorn>
{
  private:
    CRGBPalette16 currentPalette = RainbowColors_p;

    uint8_t gravity = 16;
    static constexpr int NUM_ROCKETS = 8;

    struct Rocket
    {
        int32_t x, y, xd, yd;
    };

    Rocket rockets[NUM_ROCKETS];

    void restart_rocket(Rocket& rocket)
    {
        rocket.xd = random8() + 32;
        if (rocket.x > (MATRIX_WIDTH / 2 * 256))
        {
            // leap towards the centre of the screen
            rocket.xd = -rocket.xd;
        }
        // controls the leap height
        rocket.yd = random8() * 5 + (MATRIX_HEIGHT - 1) * 1;
    }

    void move()
    {
        for (auto& rocket : rockets)
        {
            // add the X & Y velocities to the positions
            rocket.x += rocket.xd;
            rocket.y += rocket.yd;

            // bounce off the floor?
            if (rocket.y < 0)
            {
                rocket.yd = (-rocket.yd * 240) >> 8;
                rocket.y = rocket.yd;
                // settled on the floor?
                if (rocket.y <= 200)
                { // if you change gravity, this will probably need changing too
                    restart_rocket(rocket);
                }
            }

            // bounce off the sides of the screen?
            if (rocket.x < 0 || rocket.x > MATRIX_WIDTH * 256)
            {
                rocket.xd = (-rocket.xd * 248) >> 8;
                // force back onto the screen, otherwise they eventually sneak away
                if (rocket.x < 0)
                {
                    rocket.x = rocket.xd;
                    rocket.yd += rocket.xd;
                }
                else
                {
                    rocket.x = (MATRIX_WIDTH * 256) - rocket.xd;
                }
            }

            // gravity
            rocket.yd -= gravity;

            // viscosity
            rocket.xd = (rocket.xd * 224) >> 8;
            rocket.yd = (rocket.yd * 224) >> 8;
        }
    }

    void paint()
    {
        for (int r = 0; r < NUM_ROCKETS; r++)
        {
            auto& rocket = rockets[r];
            CRGB rgb = ColorFromPalette(currentPalette, r * (256 / NUM_ROCKETS), 255, LINEARBLEND);

            // make the acme white, because why not (peak velocity near 0)
            if (std::abs(rocket.yd) < 256)
                rgb = CRGB::White;

            float x = rocket.x / 256.0f;
            float y = rocket.y / 256.0f;
            g()->drawPixelXYF_Wu(x, MATRIX_HEIGHT - 1 - y, rgb);
        }
    }

  public:
    PatternSMColorPopcorn() : EffectWithId<PatternSMColorPopcorn>("Color Popcorn")
    {
    }

    PatternSMColorPopcorn(const JsonObjectConst &jsonObject) : EffectWithId<PatternSMColorPopcorn>(jsonObject)
    {
    }

    void Start() override
    {
        g()->Clear();
        for (auto& rocket : rockets)
        {
            rocket.x = random8() * MATRIX_WIDTH - 1;
            rocket.y = random8() * MATRIX_HEIGHT - 1;
            rocket.xd = 0;
            rocket.yd = 0;
        }
    }

    void Draw() override
    {
        fadeToBlackBy(g()->leds, NUM_LEDS, 60);

        EVERY_N_MILLISECONDS(16)
        {
            move();
            paint();
        }
    }
};
