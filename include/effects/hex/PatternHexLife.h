#pragma once

#include "globals.h"

#if HEXAGON
#include "ledstripeffect.h"
#include "gfxhex.h"
#include "systemcontainer.h"

#include <algorithm>
#include <cmath>
#include <vector>

class HexCell
{
public:
    uint8_t alive : 1;
    uint8_t prev : 1;
    uint8_t hue;
    uint8_t brightness;
    uint8_t age; // Track how many generations cell has been alive
};

class PatternHexLife : public EffectWithId<PatternHexLife>
{
private:
    std::unique_ptr<HexCell[]> world;
    int generation = 0;
    unsigned int density = 30;
    unsigned long seed;
    int maxRadius = HEX_RINGS - 1;
    int totalHexes;

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);

    int getHexIndex(HexCoord hex)
    {
        auto hexGfx = hg();
        if (!hexGfx) return -1;
        auto idx = hexGfx->hexToIndex(hex);
        return idx.has_value() ? idx.value() : -1;
    }

    int countNeighbors(HexCoord hex)
    {
        auto hexGfx = hg();
        if (!hexGfx) return 0;

        int count = 0;
        // Manually iterate the 6 hex directions to avoid vector allocation
        for (int i = 0; i < 6; i++) {
            HexCoord neighbor = hexGfx->getHexNeighbor(hex, i);
            int idx = getHexIndex(neighbor);
            if (idx >= 0 && world[idx].prev) {
                count++;
            }
        }
        return count;
    }

    void randomFillWorld()
    {
        seed = random();
        srand(seed);

        for (int i = 0; i < totalHexes; i++) {
            if ((rand() % 100) < density) {
                world[i].alive = 1;
                world[i].brightness = 128;
                world[i].age = 0;
            } else {
                world[i].alive = 0;
                world[i].brightness = 0;
                world[i].age = 0;
            }
            world[i].prev = world[i].alive;
            world[i].hue = 0;
        }
    }

public:
    PatternHexLife() : EffectWithId<PatternHexLife>("Hex: Life") {}
    PatternHexLife(const JsonObjectConst& jsonObject) : EffectWithId<PatternHexLife>(jsonObject) {}
    virtual ~PatternHexLife() {}

    EffectSettingSpecs* FillSettingSpecs() override
    {
        return &mySettingSpecs;
    }

    bool Init(std::vector<std::shared_ptr<GFXBase>>& gfx) override
    {
        LEDStripEffect::Init(gfx);

        totalHexes = TOTAL_LEDS_IN_HEX;
        world = make_unique_psram<HexCell[]>(totalHexes);

        return true;
    }

    bool RequiresDoubleBuffering() const override
    {
        return true;
    }

    void Reset()
    {
        randomFillWorld();
        generation = 0;
    }

    void Draw() override
    {
        auto hexGfx = hg();
        if (!hexGfx) return;

        if (generation == 0)
            Reset();

        // Display current generation with age-based coloring
        for (int r = -(HEX_RINGS - 1); r <= (HEX_RINGS - 1); ++r) {
            int q1 = std::max(-(HEX_RINGS - 1), -r - (HEX_RINGS - 1));
            int q2 = std::min(HEX_RINGS - 1, -r + (HEX_RINGS - 1));
            for (int q = q1; q <= q2; ++q) {
                HexCoord hex(q, r);
                int idx = getHexIndex(hex);
                if (idx >= 0 && world[idx].brightness > 0) {
                    // Age-based hue shift: older cells shift through the palette
                    uint8_t ageHue = (world[idx].hue + world[idx].age * 8) % 256;
                    // Brightness based on age (fade in, stay bright, fade out)
                    uint8_t ageBrightness = world[idx].brightness;
                    if (world[idx].age < 5) {
                        ageBrightness = world[idx].brightness * world[idx].age / 5;
                    } else if (world[idx].age > 50) {
                        ageBrightness = world[idx].brightness * (60 - world[idx].age) / 10;
                    }
                    CRGB color = ColorFromPalette(g()->GetCurrentPalette(), ageHue, ageBrightness, LINEARBLEND);
                    hexGfx->drawHexPixel(hex, color);
                } else if (idx >= 0) {
                    hexGfx->drawHexPixel(hex, CRGB::Black);
                }
            }
        }

        // Birth and death cycle
        for (int r = -(HEX_RINGS - 1); r <= (HEX_RINGS - 1); ++r) {
            int q1 = std::max(-(HEX_RINGS - 1), -r - (HEX_RINGS - 1));
            int q2 = std::min(HEX_RINGS - 1, -r + (HEX_RINGS - 1));
            for (int q = q1; q <= q2; ++q) {
                HexCoord hex(q, r);
                int idx = getHexIndex(hex);
                if (idx < 0) continue;

                if (world[idx].brightness > 0 && world[idx].prev == 0)
                    world[idx].brightness *= 0.75;

                int count = countNeighbors(hex);
                // B2/S23 Hex Life Rules
                if (count == 2 && world[idx].prev == 0) {
                    world[idx].alive = 1;
                    world[idx].hue += 1;
                    world[idx].brightness = 255;
                    world[idx].age = 0;
                } else if ((count < 2 || count > 3) && world[idx].prev == 1) {
                    world[idx].alive = 0;
                    world[idx].brightness = 0;
                    world[idx].age = 0;
                } else if (world[idx].prev == 1 && world[idx].alive == 1) {
                    world[idx].age++;
                    if (world[idx].age > 255) world[idx].age = 255;
                }
            }
        }

        // Copy next generation
        for (int i = 0; i < totalHexes; i++) {
            world[i].prev = world[i].alive;
        }

        generation++;

        // Auto-reset if stagnant (simple check)
        if (generation > 500) {
            Reset();
        }
    }
};
#endif
