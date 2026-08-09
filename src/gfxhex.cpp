//+--------------------------------------------------------------------------
//
// File:        gfxhex.cpp
//
// Hexagonal grid graphics driver implementation.
// Maps 2D Cartesian and Hex axes to the physical 1D LED strip layout.
//
// NightDriverStrip - (c) 2026 Robert Lipe.  All Rights Reserved.
//
// This file is part of the NightDriver software project.
//
//    NightDriver is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    NightDriver is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Nightdriver.  It is normally found in copying.txt
//+--------------------------------------------------------------------------

#include "globals.h"
#include <algorithm>
#include <cmath>

#if HEXAGON

#include "gfxhex.h"

#include <numbers>

constexpr float HEX_SQRT3 = std::numbers::sqrt3_v<float>;
constexpr float HEX_SQRT3_OVER_2 = HEX_SQRT3 / 2.0f;
constexpr float HEX_3_OVER_2 = 3.0f / 2.0f;
constexpr float HEX_2_OVER_3 = 2.0f / 3.0f;
constexpr float HEX_1_OVER_3 = 1.0f / 3.0f;
constexpr float HEX_SQRT3_OVER_3 = HEX_SQRT3 / 3.0f;

// Static member initialization - nullptr, allocated in InitializeHardware()


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


    // Modern PSRAM boards can handle massive parallel strip allocations
    #if NUM_CHANNELS > 16
        #error The maximum value of NUM_CHANNELS (number of parallel channels) is 16
    #endif

    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        debugW("Allocating HexagonGFX for channel %d", i);
        // Allocate exact number of pixels now that isValidPixel handles out of bounds safely
        devices.push_back(std::make_shared<HexagonGFX>(TOTAL_LEDS_IN_HEX));
    }

    AddLEDs(devices);
}

// Maps a virtual Q/R/S axial coordinate to the physical 1D hardware index.
// The math is slightly gnarly to handle the "boustrophedon" (serpentine) wiring
// typical of these flat-topped LED hex panels.
std::optional<int> HexagonGFX::hexToIndex(HexCoord hex) const {
    // 1. Check bounds (axial coordinates naturally define a hexagon when constrained)
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

// Converts a smooth 2D Cartesian floating-point coordinate (like a physics particle)
// into the nearest discrete hex axial coordinate using standard hex rotation matrices.
HexCoord HexagonGFX::pixelToHex(PixelCoord pixel, float hex_size, PixelCoord origin_offset_pixels) const {
    float adjusted_x = pixel.x - origin_offset_pixels.x;
    float adjusted_y = pixel.y - origin_offset_pixels.y;

    // Inverse flat-top basis matrix
    float q_frac = (adjusted_x * HEX_SQRT3_OVER_3 - adjusted_y * HEX_1_OVER_3) / hex_size;
    float r_frac = (adjusted_y * HEX_2_OVER_3) / hex_size;
    float s_frac = -q_frac - r_frac;

    return hexRound(q_frac, r_frac, s_frac);
}

// Converts a discrete hex axial coordinate back to the exact center point
// of that hexagon in smooth 2D Cartesian floating-point space.
PixelCoord HexagonGFX::hexToPixelFlatTop(HexCoord hex, float hex_size, PixelCoord origin_offset_pixels) const {
    // Forward flat-top basis matrix
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

// Direction and neighbor operations


// Shape drawing
void HexagonGFX::fillHexagon(HexCoord center, int radius, CRGB color) {
    // Use precomputed ring data - no allocation
    for (int r = 0; r <= radius; r++) {
        auto ring = getHexRing(r);
        for (const auto& hex : ring) {
            HexCoord offsetHex = hexAdd(center, hex);
            drawHexPixel(offsetHex, color);
        }
    }
}

void HexagonGFX::drawHexLine(HexCoord start, HexCoord end, CRGB color) {
    int distance = static_cast<int>(hexDistance(start, end));
    for (int i = 0; i <= distance; i++) {
        float t = (distance == 0) ? 0.0f : static_cast<float>(i) / distance;
        HexCoord lerp = hexLerp(start, end, t);
        drawHexPixel(lerp, color);
    }
}

void HexagonGFX::drawHexSpiral(HexCoord center, int maxRadius, CRGB color) {
    // Use precomputed spiral data - no allocation
    auto spiral = getHexSpiral();
    for (const auto& hex : spiral) {
        // Only draw if within requested radius
        if (hexDistance(hex, HexCoord(0, 0)) <= maxRadius) {
            HexCoord offsetHex = hexAdd(center, hex);
            drawHexPixel(offsetHex, color);
        }
    }
}

void HexagonGFX::drawHexCone(HexCoord center, int direction, int length, CRGB color) {
    HexCoord current = center;
    const int leftDir = (direction - 1 + 6) % 6;
    const int rightDir = (direction + 1) % 6;

    for (int i = 0; i < length; i++) {
        drawHexPixel(current, color);
        // Draw a fan of hexes at this distance
        for (int spread = 0; spread <= i; spread++) {
            HexCoord leftHex = current;
            HexCoord rightHex = current;

            for (int j = 0; j < spread; j++) {
                leftHex = getHexNeighbor(leftHex, leftDir);
                rightHex = getHexNeighbor(rightHex, rightDir);
            }

            if (spread > 0) {
                drawHexPixel(leftHex, color);
                drawHexPixel(rightHex, color);
            }
        }
        current = getHexNeighbor(current, direction);
    }
}

void HexagonGFX::drawHexWedge(HexCoord center, int startDir, int endDir, int radius, CRGB color) {
    // Normalize directions
    startDir = ((startDir % 6) + 6) % 6;
    endDir = ((endDir % 6) + 6) % 6;

    for (int r = 0; r <= radius; r++) {
        int dir = startDir;
        do {
            HexCoord hex = hexAdd(center, hexScale(getHexDirection(dir), r));
            drawHexPixel(hex, color);
            dir = (dir + 1) % 6;
        } while (dir != (endDir + 1) % 6);
    }
}

// Range and area operations




HexagonGFX::HexCoordView HexagonGFX::getHexRing(int radius) const {
    if (radius < 0 || radius >= HEX_RINGS) {
        return HexCoordView{nullptr, 0};
    }
    return HexCoordView{RINGS_DATA.flat.data() + RINGS_DATA.offsets[radius], RINGS_DATA.sizes[radius]};
}

HexagonGFX::HexCoordView HexagonGFX::getHexSpiral() const {
    return HexCoordView{RINGS_DATA.flat.data(), TOTAL_LEDS_IN_HEX};
}

HexCoord HexagonGFX::indexToHexCoord(int index) const {
    if (index < 0 || index >= TOTAL_LEDS_IN_HEX) {
        return HexCoord(0, 0);
    }
    return INDEX_TO_HEX_COORD[index];
}

// Legacy allocating version - kept for compatibility but should be avoided
// Interpolation and transformation
HexCoord HexagonGFX::hexLerp(HexCoord a, HexCoord b, float t) {
    float q_frac = a.q + (b.q - a.q) * t;
    float r_frac = a.r + (b.r - a.r) * t;
    float s_frac = a.s + (b.s - a.s) * t;
    return hexRound(q_frac, r_frac, s_frac);
}

HexCoord HexagonGFX::hexRound(float q_frac, float r_frac, float s_frac) {
    int rx = static_cast<int>(roundf(q_frac));
    int ry = static_cast<int>(roundf(r_frac));
    int rs = static_cast<int>(roundf(s_frac));

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

HexCoord HexagonGFX::hexRotate(HexCoord hex, int rotations) {
    // Normalize rotations to 0-5 range
    rotations = ((rotations % 6) + 6) % 6;

    for (int i = 0; i < rotations; i++) {
        // 60-degree rotation: (q, r, s) -> (-r, -s, -q)
        int new_q = -hex.r;
        int new_r = -hex.s;
        hex = HexCoord(new_q, new_r);
    }

    return hex;
}

#endif // HEXAGON
