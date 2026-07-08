#pragma once

#include "globals.h"
#include <array>
#include <optional>
#include "ws281xgfx.h"


#ifndef HEX_RINGS
#define HEX_RINGS 10
#endif

// A standard flat-top hex grid
struct HexCoord {
    int q;
    int r;
    int s; // s = -q - r

    HexCoord(int q_val, int r_val) : q(q_val), r(r_val), s(-q_val - r_val) {}
};

struct OffsetCoord {
    int col;
    int row;
};

struct PixelCoord {
    float x;
    float y;
};

constexpr int TOTAL_HEX_ROWS = (2 * HEX_RINGS) - 1;
constexpr int TOTAL_LEDS_IN_HEX = 3 * HEX_RINGS * (HEX_RINGS - 1) + 1;

constexpr std::array<int, TOTAL_HEX_ROWS> generateRowLengths() {
    std::array<int, TOTAL_HEX_ROWS> data{};
    for (int i = 0; i < TOTAL_HEX_ROWS; ++i) {
        data[i] = (i < HEX_RINGS) ? (HEX_RINGS + i) : (3 * HEX_RINGS - 2 - i);
    }
    return data;
}

constexpr std::array<int, TOTAL_HEX_ROWS + 1> generateCumulativeSums(const std::array<int, TOTAL_HEX_ROWS>& lengths) {
    std::array<int, TOTAL_HEX_ROWS + 1> data{};
    data[0] = 0;
    for (int i = 0; i < TOTAL_HEX_ROWS; ++i) {
        data[i + 1] = data[i] + lengths[i];
    }
    return data;
}

class HexagonGFX : public WS281xGFX
{
public:
    static constexpr std::array<int, TOTAL_HEX_ROWS> ROW_LENGTHS = generateRowLengths();
    static constexpr std::array<int, TOTAL_HEX_ROWS + 1> CUMULATIVE_ROW_SUMS = generateCumulativeSums(ROW_LENGTHS);

    HexagonGFX(size_t numLeds);
    virtual ~HexagonGFX() {}

    static void InitializeHardware(std::vector<std::shared_ptr<GFXBase>>& devices);

    // Override XY mapping to use hexagonal offset logic
    virtual uint16_t xy(uint16_t x, uint16_t y) const noexcept override;

    // Native Hex drawing primitives
    std::optional<int> hexToIndex(HexCoord hex) const;
    void drawHexPixel(int q, int r, CRGB color);
    void drawHexPixel(HexCoord hex, CRGB color);

    // Fills a ring around the hexagon, inset by the indent specified
    virtual void fillHexRing(uint16_t indent, CRGB color);

    // Helpers
    static float hexDistance(HexCoord a, HexCoord b);
    OffsetCoord cubeToOffset(HexCoord hex) const;
    std::optional<int> offsetToLinearIndex(OffsetCoord offset) const;
    HexCoord pixelToHex(PixelCoord pixel, float hex_size, PixelCoord origin_offset_pixels) const;
    PixelCoord hexToPixelFlatTop(HexCoord hex, float hex_size, PixelCoord origin_offset_pixels) const;

private:
    float m_hexSize;
    PixelCoord m_originOffset;
};
