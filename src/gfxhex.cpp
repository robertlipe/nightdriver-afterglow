#include "globals.h"
#include <algorithm>
#include <cmath>

#if HEXAGON

#include "gfxhex.h"

#include <numbers>

const float HEX_SQRT3 = std::numbers::sqrt3_v<float>;
const float HEX_SQRT3_OVER_2 = HEX_SQRT3 / 2.0f;
const float HEX_3_OVER_2 = 3.0f / 2.0f;
const float HEX_2_OVER_3 = 2.0f / 3.0f;
const float HEX_1_OVER_3 = 1.0f / 3.0f;

HexagonGFX::HexagonGFX(size_t numLeds) : WS281xGFX(numLeds, 1), m_hexSize(1.0f), m_originOffset({0.0f, 0.0f}) {
    // Width and height in GFXBase for generic effects that expect a rectangular bounding box
    _width = HEX_RINGS * 2 - 1;
    _height = HEX_RINGS * 2 - 1;
    Adafruit_GFX::_width = _width;
    Adafruit_GFX::WIDTH = _width;
    Adafruit_GFX::_height = _height;
    Adafruit_GFX::HEIGHT = _height;
}

void HexagonGFX::InitializeHardware(std::vector<std::shared_ptr<GFXBase>>& devices)
{
    // We don't support more than 8 parallel channels
    #if NUM_CHANNELS > 8
        #error The maximum value of NUM_CHANNELS (number of parallel channels) is 8
    #endif

    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        debugW("Allocating HexagonGFX for channel %d", i);
        // Allocate one extra "trash" pixel for out-of-bounds mapping to prevent heap corruption
        devices.push_back(make_shared_psram<HexagonGFX>(TOTAL_LEDS_IN_HEX + 1));
    }

    AddLEDs(devices);
}

std::optional<int> HexagonGFX::hexToIndex(HexCoord hex) const {
    // 1. Check bounds
    if (std::abs(hex.q) >= HEX_RINGS || std::abs(hex.r) >= HEX_RINGS || std::abs(hex.s) >= HEX_RINGS) {
        return std::nullopt;
    }

    // 2. Find row (0 to TOTAL_HEX_ROWS - 1)
    // r goes from -(HEX_RINGS - 1) at the top to (HEX_RINGS - 1) at the bottom
    int row = hex.r + (HEX_RINGS - 1);

    // 3. Find the minimum q for this row to determine the left-most hex
    int q_min = std::max(-(HEX_RINGS - 1), -(HEX_RINGS - 1) - hex.r);

    // 4. Find 0-based column index from the left of the row
    int adjustedCol = hex.q - q_min;

    // 5. Calculate serpentine index
    int index = CUMULATIVE_ROW_SUMS[row];
    int currentRowLength = ROW_LENGTHS[row];

    if (row % 2 == 0) {
        // Even row: left to right (since first LED is upper-left)
        index += adjustedCol;
    } else {
        // Odd row: right to left
        index += (currentRowLength - 1 - adjustedCol);
    }

    return index;
}

float HexagonGFX::hexDistance(HexCoord a, HexCoord b) {
    return (fabsf(a.q - b.q) + fabsf(a.r - b.r) + fabsf(a.s - b.s)) / 2.0f;
}

HexCoord HexagonGFX::pixelToHex(PixelCoord pixel, float hex_size, PixelCoord origin_offset_pixels) const {
    float adjusted_x = pixel.x - origin_offset_pixels.x;
    float adjusted_y = pixel.y - origin_offset_pixels.y;

    float q_frac = (adjusted_x * HEX_2_OVER_3 - adjusted_y * HEX_1_OVER_3) / hex_size;
    float r_frac = (adjusted_y * HEX_2_OVER_3) / hex_size;
    float s_frac = -q_frac - r_frac;

    int rx = roundf(q_frac);
    int ry = roundf(r_frac);
    int rs = roundf(s_frac);

    float x_diff = fabsf(rx - q_frac);
    float y_diff = fabsf(ry - r_frac);
    float z_diff = fabsf(rs - s_frac);

    if (x_diff > y_diff && x_diff > z_diff) {
        rx = -ry - rs;
    } else if (y_diff > z_diff) {
        ry = -rx - rs;
    } else {
        rs = -rx - ry;
    }

    return HexCoord(rx, ry);
}

PixelCoord HexagonGFX::hexToPixelFlatTop(HexCoord hex, float hex_size, PixelCoord origin_offset_pixels) const {
    float pixel_x_float = hex_size * (HEX_SQRT3 * hex.q + HEX_SQRT3_OVER_2 * hex.r);
    float pixel_y_float = hex_size * (HEX_3_OVER_2 * hex.r);
    return {pixel_x_float + origin_offset_pixels.x, pixel_y_float + origin_offset_pixels.y};
}

uint16_t HexagonGFX::xy(uint16_t x, uint16_t y) const noexcept {
    // Legacy support for basic x/y cartesian drawing
    // Map rectangular coordinate space to closest hex index
    // Using a simple scaling where (x, y) acts as pixel coordinates
    PixelCoord p = {(float)x, (float)y};
    HexCoord hex = pixelToHex(p, m_hexSize, m_originOffset);
    std::optional<int> idx = hexToIndex(hex);
    if (idx.has_value()) {
        return idx.value();
    }
    // Return an out-of-bounds index so the base class ignores it,
    // or map to 0 with a risk of artifacting
    return TOTAL_LEDS_IN_HEX;
}

void HexagonGFX::drawHexPixel(int q, int r, CRGB color) {
    drawHexPixel(HexCoord(q, r), color);
}

void HexagonGFX::drawHexPixel(HexCoord hex, CRGB color) {
    std::optional<int> idx = hexToIndex(hex);
    if (idx.has_value()) {
        leds[idx.value()] = color;
    }
}

void HexagonGFX::fillHexRing(uint16_t indent, CRGB color) {
    int radius = (HEX_RINGS - 1) - indent;
    if (radius < 0) return;

    for (int q = -radius; q <= radius; q++) {
        for (int r = -radius; r <= radius; r++) {
            HexCoord hex(q, r);
            if (std::max({std::abs(hex.q), std::abs(hex.r), std::abs(hex.s)}) == radius) {
                drawHexPixel(hex, color);
            }
        }
    }
}

#endif // HEXAGON
