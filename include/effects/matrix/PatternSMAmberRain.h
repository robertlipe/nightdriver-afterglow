#pragma once

#include "effectmanager.h"

// Derived from https://editor.soulmatelights.com/gallery/2007-amber-rain

struct Circle
{
    float thickness = 3.0;
    long startTime;
    uint16_t offset;
    int centerX;
    int centerY;
    int hue;
    int bpm = 10;

    void move()
    {
        centerX = random(0, MATRIX_WIDTH);
        centerY = random(0, MATRIX_HEIGHT);
    }

    void reset()
    {
        startTime = millis();
        centerX = random(0, MATRIX_WIDTH);
        centerY = random(0, MATRIX_HEIGHT);
        hue = random(0, 255);
        offset = random(0, 60000 / bpm);
    }

    float radius()
    {
        float radius = beatsin16(30, 0, 500, offset) / 100.0f;
        return radius;
    }
};

const int NUMBER_OF_CIRCLES = 20;

class PatternSMAmberRain : public EffectWithId<PatternSMAmberRain>
{
  private:

    Circle circles[NUMBER_OF_CIRCLES] = {};

    void drawCircle(Circle circle)
    {
        int centerX = circle.centerX;
        int centerY = circle.centerY;
        int hue = circle.hue;
        float radius = circle.radius();

        int startX = centerX - std::ceil(radius);
        int endX = centerX + std::ceil(radius);
        int startY = centerY - std::ceil(radius);
        int endY = centerY + std::ceil(radius);

        for (int x = startX; x < endX; x++)
        {
            for (int y = startY; y < endY; y++)
            {
                if (!g()->isValidPixel(x, y))
                    continue;

                float distance = sqrtf(sq(x - centerX) + sq(y - centerY));
                if (distance > radius)
                    continue;

                uint16_t brightness;
                if (radius < 1.0f)
                { // last pixel
                    brightness = 255.0f * radius;
                }
                else
                {
                    float percentage = distance / radius;
                    float fraction = 1.0f - percentage;
                    brightness = 255.0f * fraction;
                }

                g()->leds[XY(x, y)] += CHSV(hue, 255, brightness);
            }
        }
    }

  public:

    PatternSMAmberRain() : EffectWithId<PatternSMAmberRain>("Color Rain") {}
    PatternSMAmberRain(const JsonObjectConst &jsonObject) : EffectWithId<PatternSMAmberRain>(jsonObject) {}

    void Start() override
    {
        g()->Clear();
        for (auto & circle : circles)
        {
            circle.reset();
        }
    }

    void Draw() override
    {
        fadeAllChannelsToBlackBy(32);

        for (auto & circle : circles)
        {
            if (circle.radius() < 0.001)
            {
                circle.move();
            }
            drawCircle(circle);
        }
    }
};
