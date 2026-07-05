#pragma once

#include "effectmanager.h"
#include "effects/strip/musiceffect.h"

// Derived from https://editor.soulmatelights.com/gallery/1118-snakes
// Not quite the Nokia classic - no collision detection, but bright colors.

#if ENABLE_AUDIO
class PatternSMSnakes : public BeatEffectBase, public EffectWithId<PatternSMSnakes>
#else
class PatternSMSnakes : public EffectWithId<PatternSMSnakes>
#endif
{
  private:
    uint8_t Speed = 250; // 1-255 Setting
    uint8_t Scale = 20;  // 1-100 Setting

    const int HEIGHT = MATRIX_HEIGHT;
    const int WIDTH = MATRIX_WIDTH;

    const int SNAKES_LENGTH = (8U); // длина червяка от 2 до 15 (+ 1 пиксель голова/хвостик),
                                    // ограничена размером переменной для хранения трактории тела
                                    // червяка
    static constexpr int trackingOBJECT_MAX_COUNT = (100U); // максимальное количество отслеживаемых объектов (очень
                                                            // влияет на расход памяти)

    float speedfactor; // регулятор скорости в эффектах реального времени

    struct TrackingObject {
        float posX;
        float posY;
        float speedX;
        float speedY;
        float shift;
        long time;
        uint8_t hue;
        uint8_t state;
        bool isShift;
    };
    TrackingObject trackingObjects[trackingOBJECT_MAX_COUNT];

    int enlargedObjectNUM; // используемое в эффекте количество объектов

  public:
    PatternSMSnakes()
        :
#if ENABLE_AUDIO
          BeatEffectBase(1.50, 0.05),
#endif
          EffectWithId<PatternSMSnakes>("Snakes")
    {
    }

    PatternSMSnakes(const JsonObjectConst &jsonObject)
        :
#if ENABLE_AUDIO
          BeatEffectBase(1.50, 0.05),
#endif
          EffectWithId<PatternSMSnakes>(jsonObject)
    {
    }

    void Start() override
    {
        g()->Clear();
        speedfactor = (float)Speed / 555.0f + 0.001f;

        enlargedObjectNUM = (Scale - 1U) / 99.0f * (trackingOBJECT_MAX_COUNT - 1U) + 1.0f;
        if (enlargedObjectNUM > trackingOBJECT_MAX_COUNT)
            enlargedObjectNUM = trackingOBJECT_MAX_COUNT;
        for (int i = 0; i < enlargedObjectNUM; i++)
        {
            trackingObjects[i].time = 0;
            trackingObjects[i].posX = random8(WIDTH);
            trackingObjects[i].posY = random8(HEIGHT);
            trackingObjects[i].speedX = (255.0f + random8()) / 255.0f;
            trackingObjects[i].speedY = 0.0f;
            trackingObjects[i].shift = 0.0f;
            trackingObjects[i].hue = random8();
            trackingObjects[i].state = random8(4); //     0b00           направление головы змейки
                                                 // 0b10     0b11
                                                 //     0b01
        }
    }

    void Draw() override
    {
#if ENABLE_AUDIO
        ProcessAudio();
#endif

        // g()->Clear(); // If you prefer your tails more crispy-edged.
        fadeAllChannelsToBlackBy(32); // more rattly-tailed snakes. 16 makes really long tails

        for (int i = 0; i < enlargedObjectNUM; i++)
        {
            trackingObjects[i].speedY += trackingObjects[i].speedX * speedfactor;
            if (trackingObjects[i].speedY >= 1.0f)
            {
                trackingObjects[i].speedY = trackingObjects[i].speedY - (int)trackingObjects[i].speedY;
                if (random8(9U) == 0U) // вероятность поворота
                {
                    if (random8(2U))
                    {                                                               // <- поворот налево
                        trackingObjects[i].time = (trackingObjects[i].time << 2) | 0b01; // младший бит = поворот
                        switch (trackingObjects[i].state)
                        {
                        case 0b10:
                            trackingObjects[i].state = 0b01;
                            if (trackingObjects[i].posY == 0U)
                                trackingObjects[i].posY = HEIGHT - 1U;
                            else
                                trackingObjects[i].posY--;
                            break;
                        case 0b11:
                            trackingObjects[i].state = 0b00;
                            if (trackingObjects[i].posY >= HEIGHT - 1U)
                                trackingObjects[i].posY = 0U;
                            else
                                trackingObjects[i].posY++;
                            break;
                        case 0b00:
                            trackingObjects[i].state = 0b10;
                            if (trackingObjects[i].posX == 0U)
                                trackingObjects[i].posX = WIDTH - 1U;
                            else
                                trackingObjects[i].posX--;
                            break;
                        case 0b01:
                            trackingObjects[i].state = 0b11;
                            if (trackingObjects[i].posX >= WIDTH - 1U)
                                trackingObjects[i].posX = 0U;
                            else
                                trackingObjects[i].posX++;
                            break;
                        }
                    }
                    else
                    { // -> поворот направо
                        trackingObjects[i].time = (trackingObjects[i].time << 2) | 0b11; // младший бит = поворот, старший = направо
                        switch (trackingObjects[i].state)
                        {
                        case 0b11:
                            trackingObjects[i].state = 0b01;
                            if (trackingObjects[i].posY == 0U)
                                trackingObjects[i].posY = HEIGHT - 1U;
                            else
                                trackingObjects[i].posY--;
                            break;
                        case 0b10:
                            trackingObjects[i].state = 0b00;
                            if (trackingObjects[i].posY >= HEIGHT - 1U)
                                trackingObjects[i].posY = 0U;
                            else
                                trackingObjects[i].posY++;
                            break;
                        case 0b01:
                            trackingObjects[i].state = 0b10;
                            if (trackingObjects[i].posX == 0U)
                                trackingObjects[i].posX = WIDTH - 1U;
                            else
                                trackingObjects[i].posX--;
                            break;
                        case 0b00:
                            trackingObjects[i].state = 0b11;
                            if (trackingObjects[i].posX >= WIDTH - 1U)
                                trackingObjects[i].posX = 0U;
                            else
                                trackingObjects[i].posX++;
                            break;
                        }
                    }
                }
                else
                { // двигаем без поворота
                    trackingObjects[i].time = (trackingObjects[i].time << 2);
                    switch (trackingObjects[i].state)
                    {
                    case 0b01:
                        if (trackingObjects[i].posY == 0U)
                            trackingObjects[i].posY = HEIGHT - 1U;
                        else
                            trackingObjects[i].posY--;
                        break;
                    case 0b00:
                        if (trackingObjects[i].posY >= HEIGHT - 1U)
                            trackingObjects[i].posY = 0U;
                        else
                            trackingObjects[i].posY++;
                        break;
                    case 0b10:
                        if (trackingObjects[i].posX == 0U)
                            trackingObjects[i].posX = WIDTH - 1U;
                        else
                            trackingObjects[i].posX--;
                        break;
                    case 0b11:
                        if (trackingObjects[i].posX >= WIDTH - 1U)
                            trackingObjects[i].posX = 0U;
                        else
                            trackingObjects[i].posX++;
                        break;
                    }
                }
            }

            int8_t dx, dy;
            switch (trackingObjects[i].state)
            {
            case 0b01:
                dy = 1;
                dx = 0;
                break;
            case 0b00:
                dy = -1;
                dx = 0;
                break;
            case 0b10:
                dy = 0;
                dx = 1;
                break;
            case 0b11:
                dy = 0;
                dx = -1;
                break;
            default:
                dy = 0;
                dx = 0;
                break;
            }
            long temp = trackingObjects[i].time;
            uint8_t x = trackingObjects[i].posX;
            uint8_t y = trackingObjects[i].posY;
            g()->leds[XY(x, y)] += CHSV(trackingObjects[i].hue, 255U,
                                        trackingObjects[i].speedY * 255); // тут рисуется голова

            for (int m = 0; m < SNAKES_LENGTH; m++)
            { // 16 бит распаковываем, 14 ещё остаётся без дела в запасе, 2 на хвостик
                x = (WIDTH + x + dx) % WIDTH;
                y = (HEIGHT + y + dy) % HEIGHT;
                g()->leds[XY(x, y)] +=
                    CHSV(trackingObjects[i].hue + (m + trackingObjects[i].speedY) * 4U, 255U, 255U); // тут рисуется тело

                if (temp & 0b01)
                { // младший бит = поворот, старший = направо
                    temp = temp >> 1;
                    if (temp & 0b01)
                    { // старший бит = направо
                        if (dx == 0)
                        {
                            dx = 0 - dy;
                            dy = 0;
                        }
                        else
                        {
                            dy = dx;
                            dx = 0;
                        }
                    }
                    else
                    { // иначе налево
                        if (dx == 0)
                        {
                            dx = dy;
                            dy = 0;
                        }
                        else
                        {
                            dy = 0 - dx;
                            dx = 0;
                        }
                    }
                    temp = temp >> 1;
                }
                else
                { // если без поворота
                    temp = temp >> 2;
                }
            }
            x = (WIDTH + x + dx) % WIDTH;
            y = (HEIGHT + y + dy) % HEIGHT;
            g()->leds[XY(x, y)] += CHSV(trackingObjects[i].hue + (SNAKES_LENGTH + trackingObjects[i].speedY) * 4U, 255U,
                                        (1.0f - trackingObjects[i].speedY) * 255); // хвостик
        }
    }

#if ENABLE_AUDIO
    void HandleBeat(bool bMajor, float elapsed, float span) override
    {
        for (int i = 0; i < enlargedObjectNUM; i++)
        {
            trackingObjects[i].hue = random8();
        }
        Speed = 150 + random8(75) - 72;               // 1-255 Setting
        speedfactor = (float)Speed / 555.0f + 0.001f; // The actually used class member.
    }
#endif
};
