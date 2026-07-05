#pragma once

#include "effectmanager.h"
#include "effects/matrix/Boid.h"
#include "effects/matrix/Vector.h"
#include <span>

// Inspired by
// https://editor.soulmatelights.com/gallery/2252-overcomplicatedbubbles The
// flocking code gives the bubbles a bit of a swarming effect

class PatternSMBubbles : public EffectWithId<PatternSMBubbles>
{
  private:
    static constexpr int NUM_PARTICLES = 80;
    struct Bubble
    {
        Boid boid;
        float scale{1};
        float mass{1};
        int scaledir{1};
        int bbrightness{0};
    };
    Bubble bubbles[NUM_PARTICLES];
    uint16_t x;
    uint16_t y;
    uint16_t z;
    int speed = 1;
    int count = NUM_PARTICLES;

  public:
    PatternSMBubbles() : EffectWithId<PatternSMBubbles>("Bubbles")
    {
    }

    PatternSMBubbles(const JsonObjectConst &jsonObject) : EffectWithId<PatternSMBubbles>(jsonObject)
    {
    }

    void Start() override
    {
        g()->Clear();
        x = random16();
        y = random16();
        z = random16();

        for (auto& bubble : bubbles)
        {
            bubble.boid = Boid(random(MATRIX_WIDTH), 0);
            bubble.boid.hue = random(40, 255);
            bubble.mass = random(1.0f, 2.5f);
            bubble.scale = random(0.5f, 68.5f);
        }
    }

    void Draw() override
    {
        EVERY_N_MILLISECONDS(2000)
        {
            if (count <= 5)
            {
                speed = 1;
            }
            else if (count >= 80)
            {
                speed = -1;
            }
            count += speed;
        }

        std::span<Bubble> activeBubbles(bubbles + 5, count - 5);
        for (auto& bubble : activeBubbles)
        {
            bubble.scale += bubble.scaledir;
            bubble.bbrightness = ::map((long)bubble.boid.location.y, 0L, (long)MATRIX_HEIGHT, 65L, 255L);
            if (bubble.scale > 200 || bubble.scale < 10)
            {
                bubble.scaledir = -bubble.scaledir;
            }
            int ioffset = bubble.scale * bubble.boid.location.x;
            int joffset = bubble.scale * bubble.boid.location.y;
            float angle = inoise8(x + ioffset, y + joffset, z);

            bubble.boid.velocity.x = 0.2f * ((float)sin8(angle) * 0.0078125f - 1.0f);
            bubble.boid.velocity.y = -((float)cos8(angle) * 0.0078125f - 1.0f);
            bubble.boid.update();

            g()->drawPixelXYF_Wu(bubble.boid.location.x, MATRIX_HEIGHT - 1 - bubble.boid.location.y,
                         ColorFromPalette(OceanColors_p, bubble.boid.hue * 15, bubble.bbrightness, LINEARBLEND));

            if (bubble.boid.location.x < 0 || bubble.boid.location.x >= MATRIX_WIDTH || bubble.boid.location.y < 0 ||
                bubble.boid.location.y >= MATRIX_HEIGHT)
            {
                bubble.boid.location.x = random(MATRIX_WIDTH);
                bubble.boid.location.y = 0;
            }
        }
        fadeAllChannelsToBlackBy(105);
        x += speed;
        y += speed;
        z += speed;
    }
};
