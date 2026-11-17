#pragma once

#include <algorithm>
#include <cmath>

#include "effectmanager.h"
#include "effects/strip/musiceffect.h"
#include "systemhardwarehub.h"

// Derived from https://editor.soulmatelights.com/gallery/1127-sand
// This looks better on a square display than a landscape one, but it's
// stil cool enough to include, IMO.

#if ENABLE_AUDIO
class PatternSMSand : public BeatEffectBase, public EffectWithId<PatternSMSand>
#else
class PatternSMSand : public EffectWithId<PatternSMSand>
#endif
{
  private:
    uint8_t Scale = 85; // 1-100 limit Should be a setting. % of screen height

    static constexpr int WIDTH = MATRIX_WIDTH;
    static constexpr int HEIGHT = MATRIX_HEIGHT;

#if ENABLE_AUDIO
    static constexpr uint8_t top_reserve = 1; // I'm not sure that every place that needs it has it.
#else
    static constexpr uint8_t top_reserve = 0;
#endif

    uint8_t pcnt{0}; // какой-то счётчик какого-то прогресса

  public:
    PatternSMSand()
        :
#if ENABLE_AUDIO
          BeatEffectBase(1.50, 0.05),
#endif
          EffectWithId<PatternSMSand>("Sand")
    {
    }

    PatternSMSand(const JsonObjectConst &jsonObject)
        :
#if ENABLE_AUDIO
          BeatEffectBase(1.50, 0.05),
#endif
          EffectWithId<PatternSMSand>(jsonObject)
    {
    }

    void Start() override
    {
        g()->Clear();
    }



    void Draw() override
    {
#if ENABLE_AUDIO
        ProcessAudio();
#endif

        float tilt_x = 0.0f;
#if HAS_SYSTEM_HARDWARE_HUB
        auto imu = SystemHardwareHub::Instance().GetInertial();
        if (imu.valid)
        {
            // Waveshare panel: ay is the horizontal axis.
            // Negate ay so sand flows downhill towards real gravity
            tilt_x = -std::clamp(imu.ay / 9.8f, -1.0f, 1.0f);
            if (g()->getRotation() == 2)
            {
                tilt_x = -tilt_x; // Keep physical gravity aligned when inverted
            }
        }
#endif

        // if enough has already been poured, bang random grains of sand
        uint8_t temp = map8(random8(), Scale * 2.55, 255U);
        if (pcnt >= map8(temp, 2U, HEIGHT - 3U))
        {
            temp = HEIGHT + 1U - pcnt;
            if (!random8(4U)) // sometimes sand crumbles up to half at once
                if (random8(2U))
                    temp = 2U;
                else
                    temp = 3U;
            for (int y = 1; y < pcnt; y++)
                for (int x = 0; x < WIDTH; x++)
                    if (!random8(temp))
                        g()->drawPixel(x, MATRIX_HEIGHT - 1 - y, CRGB::Black);
        }

        pcnt = 0U;
        // In this effect, 0, 0 is our lower left corner. g()->getPixel and g()->drawPixel handle
        // our coordinate inversion.
        //
        // 1 skips the bottom line, so sand will accumulate.
        // HEIGHT - top_reserve == our top drawable line, respecting VU meter.
        for (int y = 1; y < HEIGHT - top_reserve; y++) // Skip audio line
        {
            for (int i = 0; i < WIDTH; i++)
            {
                // Scan in the direction of tilt to prevent grains jumping multiple steps in one frame
                int x = (tilt_x > 0.2f) ? (WIDTH - 1 - i) : i;

                CRGB me = g()->getPixel(x, MATRIX_HEIGHT - 1 - y);
                if (me)
                { // checking for every grain of sand
                    if (!g()->getPixel(x, MATRIX_HEIGHT - 1 - (y - 1)))
                    {                                   // if it's empty below us, we just fall
                        g()->drawPixel(x, MATRIX_HEIGHT - 1 - (y - 1), me);      // draw one pixel down
                        g()->drawPixel(x, MATRIX_HEIGHT - 1 - y, CRGB::Black); //  blank our own
                    }
                    else if (x > 0U && !g()->getPixel(x - 1, MATRIX_HEIGHT - 1 - (y - 1)) && x < WIDTH - 1 && !g()->getPixel(x + 1, MATRIX_HEIGHT - 1 - (y - 1)))
                    {
                        // Peak: scatter left or right, biased by lateral tilt
                        int right_chance = std::clamp(128 + static_cast<int>(tilt_x * 120.0f), 10, 245);
                        bool go_right = (random8() < static_cast<uint8_t>(right_chance));
                        int target_x = go_right ? (x + 1) : (x - 1);
                        g()->drawPixel(target_x, MATRIX_HEIGHT - 1 - (y - 1), me);
                        g()->drawPixel(x, MATRIX_HEIGHT - 1 - y, CRGB::Black);
                        pcnt = y - 1;
                    }
                    else if (tilt_x > 0.10f && x < WIDTH - 1 && !g()->getPixel(x + 1, MATRIX_HEIGHT - 1 - (y - 1)))
                    { // Tilted right: slope to the right downhill first
                        g()->drawPixel(x + 1, MATRIX_HEIGHT - 1 - (y - 1), me);
                        g()->drawPixel(x, MATRIX_HEIGHT - 1 - y, CRGB::Black);
                        pcnt = y - 1;
                    }
                    else if (x > 0U && !g()->getPixel(x - 1, MATRIX_HEIGHT - 1 - (y - 1)))
                    { // slope to the left
                        g()->drawPixel(x - 1, MATRIX_HEIGHT - 1 - (y - 1), me);
                        g()->drawPixel(x, MATRIX_HEIGHT - 1 - y, CRGB::Black);
                        pcnt = y - 1;
                    }
                    else if (x < WIDTH - 1 && !g()->getPixel(x + 1, MATRIX_HEIGHT - 1 - (y - 1)))
                    { // if below us slopes to the right
                        g()->drawPixel(x + 1, MATRIX_HEIGHT - 1 - (y - 1), me);
                        g()->drawPixel(x, MATRIX_HEIGHT - 1 - y, CRGB::Black);
                        pcnt = y - 1;
                    }
                    else // if there is a plateau below us
                    {
                        // Under lateral tilt, sand avalanches downhill across plateaus
                        if (tilt_x < -0.20f && x > 0 && !g()->getPixel(x - 1, MATRIX_HEIGHT - 1 - y))
                        {
                            uint8_t slide_chance = static_cast<uint8_t>(std::min(255.0f, -tilt_x * 255.0f));
                            if (random8() < slide_chance)
                            {
                                g()->drawPixel(x - 1, MATRIX_HEIGHT - 1 - y, me);
                                g()->drawPixel(x, MATRIX_HEIGHT - 1 - y, CRGB::Black);
                            }
                        }
                        else if (tilt_x > 0.20f && x < WIDTH - 1 && !g()->getPixel(x + 1, MATRIX_HEIGHT - 1 - y))
                        {
                            uint8_t slide_chance = static_cast<uint8_t>(std::min(255.0f, tilt_x * 255.0f));
                            if (random8() < slide_chance)
                            {
                                g()->drawPixel(x + 1, MATRIX_HEIGHT - 1 - y, me);
                                g()->drawPixel(x, MATRIX_HEIGHT - 1 - y, CRGB::Black);
                            }
                        }
                        pcnt = y;
                    }
                }
            }
        }

        // Emit randomly colored new grains of sand at top.
        // HEIGHT - 1 == top row, occupied by VU.
        // HEIGHT - top_reserve - 1 == our top drawable row
        // Bias emitter position towards the elevated side under tilt
        int emitter_center = MATRIX_CENTER_X;
        if (std::abs(tilt_x) > 0.10f)
        {
            emitter_center = std::clamp(MATRIX_CENTER_X - static_cast<int>(tilt_x * (WIDTH / 3)), 2, WIDTH - 3);
        }

        if (!g()->getPixel(emitter_center, MATRIX_HEIGHT - 1 - (HEIGHT - top_reserve - 1)) &&
            !g()->getPixel(emitter_center + 1, MATRIX_HEIGHT - 1 - (HEIGHT - top_reserve - 1)) && !random8(3))
        {
            int emit_pos = random8(2) ? emitter_center : (emitter_center + 1);
            g()->drawPixel(emit_pos, MATRIX_HEIGHT - 1 - (HEIGHT - top_reserve - 1), CHSV(random8(), 255U, 255U));
        }
    }

#if ENABLE_AUDIO
    void HandleBeat(bool bMajor, float elapsed, float span) override
    {
        // Force deletion (on the next Draw()) of the bottom row. It's simple,
        // but it's enough to make the pyramid visibly respond to ambient sounds.
        for (int x = 0; x < WIDTH; x++)
        {
            g()->drawPixel(x, MATRIX_HEIGHT - 1 - 0, CRGB::Black);
        }
    }
#endif
};
