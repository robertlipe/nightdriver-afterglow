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

const float HEX_SQRT3 = std::numbers::sqrt3_v<float>;
const float HEX_SQRT3_OVER_2 = HEX_SQRT3 / 2.0f;
const float HEX_3_OVER_2 = 3.0f / 2.0f;
const float HEX_2_OVER_3 = 2.0f / 3.0f;
const float HEX_1_OVER_3 = 1.0f / 3.0f;
const float HEX_SQRT3_OVER_3 = HEX_SQRT3 / 3.0f;

// Static member initialization - nullptr, allocated in InitializeHardware()
std::unique_ptr<HexCoord[]> HexagonGFX::s_precomputedRingsFlat;
std::unique_ptr<int[]> HexagonGFX::s_ringOffsets;
std::unique_ptr<int[]> HexagonGFX::s_ringSizes;
std::unique_ptr<HexCoord[]> HexagonGFX::s_precomputedSpiral;
std::unique_ptr<HexCoord[]> HexagonGFX::s_indexToHexCoord;
bool HexagonGFX::s_precomputed = false;

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
    // Precompute hex data once at startup
    precomputeHexData();

    // We don't support more than 8 parallel channels
    #if NUM_CHANNELS > 8
        #error The maximum value of NUM_CHANNELS (number of parallel channels) is 8
    #endif

    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        debugW("Allocating HexagonGFX for channel %d", i);
        // Allocate exact number of pixels now that isValidPixel handles out of bounds safely
        devices.push_back(make_shared_psram<HexagonGFX>(TOTAL_LEDS_IN_HEX));
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

    float q_frac = (adjusted_x * HEX_SQRT3_OVER_3 - adjusted_y * HEX_1_OVER_3) / hex_size;
    float r_frac = (adjusted_y * HEX_2_OVER_3) / hex_size;
    float s_frac = -q_frac - r_frac;

    return hexRound(q_frac, r_frac, s_frac);
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

// Direction and neighbor operations
HexCoord HexagonGFX::getHexDirection(int direction) {
    // Wrap direction to 0-5 range
    direction = ((direction % 6) + 6) % 6;
    return HEX_DIRECTIONS[direction];
}

HexCoord HexagonGFX::getHexNeighbor(HexCoord hex, int direction) {
    return hexAdd(hex, getHexDirection(direction));
}

std::array<HexCoord, 6> HexagonGFX::getHexNeighbors(HexCoord hex) {
    std::array<HexCoord, 6> neighbors;
    for (int i = 0; i < 6; i++) {
        neighbors[i] = getHexNeighbor(hex, i);
    }
    return neighbors;
}

HexCoord HexagonGFX::hexAdd(HexCoord a, HexCoord b) {
    return HexCoord(a.q + b.q, a.r + b.r);
}

HexCoord HexagonGFX::hexSubtract(HexCoord a, HexCoord b) {
    return HexCoord(a.q - b.q, a.r - b.r);
}

HexCoord HexagonGFX::hexScale(HexCoord hex, int factor) {
    return HexCoord(hex.q * factor, hex.r * factor);
}

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
    for (int i = 0; i < length; i++) {
        drawHexPixel(current, color);
        // Draw a fan of hexes at this distance
        for (int spread = 0; spread <= i; spread++) {
            int leftDir = (direction - 1 + 6) % 6;
            int rightDir = (direction + 1) % 6;

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
std::vector<HexCoord> HexagonGFX::getHexesInRange(HexCoord center, int radius) {
    std::vector<HexCoord> results;
    for (int q = -radius; q <= radius; q++) {
        for (int r1 = std::max(-radius, -q - radius); r1 <= std::min(radius, -q + radius); r1++) {
            results.push_back(hexAdd(center, HexCoord(q, r1)));
        }
    }
    return results;
}

void HexagonGFX::precomputeHexData()
{
    if (s_precomputed) return;

    // Allocate arrays dynamically after PSRAM is ready
    s_precomputedRingsFlat = std::make_unique<HexCoord[]>(TOTAL_LEDS_IN_HEX);
    s_ringOffsets = std::make_unique<int[]>(HEX_RINGS);
    s_ringSizes = std::make_unique<int[]>(HEX_RINGS);
    s_precomputedSpiral = std::make_unique<HexCoord[]>(TOTAL_LEDS_IN_HEX);
    s_indexToHexCoord = std::make_unique<HexCoord[]>(TOTAL_LEDS_IN_HEX);

    HexCoord center(0, 0);

    // Precompute rings 0 through HEX_RINGS-1 into flat storage
    int flatOffset = 0;
    for (int radius = 0; radius < HEX_RINGS; radius++) {
        s_ringOffsets[radius] = flatOffset;

        if (radius == 0) {
            s_precomputedRingsFlat[flatOffset++] = center;
            s_ringSizes[radius] = 1;
        } else {
            HexCoord hex = hexAdd(center, hexScale(getHexDirection(4), radius));
            int ringSize = 0;
            for (int i = 0; i < 6; i++) {
                for (int j = 0; j < radius; j++) {
                    s_precomputedRingsFlat[flatOffset++] = hex;
                    ringSize++;
                    hex = getHexNeighbor(hex, i);
                }
            }
            s_ringSizes[radius] = ringSize;
        }
    }

    // Precompute spiral (center + all rings in order)
    int spiralOffset = 0;
    for (int radius = 0; radius < HEX_RINGS; radius++) {
        int ringOffset = s_ringOffsets[radius];
        int ringSize = s_ringSizes[radius];
        for (int i = 0; i < ringSize; i++) {
            s_precomputedSpiral[spiralOffset++] = s_precomputedRingsFlat[ringOffset + i];
        }
    }

    // Precompute index -> HexCoord lookup table
    for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
        // Invert the hexToIndex logic
        // Find which row this index belongs to
        int row = 0;
        while (row < TOTAL_HEX_ROWS && CUMULATIVE_ROW_SUMS[row + 1] <= index) {
            row++;
        }

        int indexInRow = index - CUMULATIVE_ROW_SUMS[row];
        int currentRowLength = ROW_LENGTHS[row];

        // Convert back to q coordinate
        int q_min = std::max(-(HEX_RINGS - 1), -(HEX_RINGS - 1) - (row - (HEX_RINGS - 1)));
        int adjustedCol;

        if (row % 2 == 0) {
            // Even row: left to right
            adjustedCol = indexInRow;
        } else {
            // Odd row: right to left
            adjustedCol = currentRowLength - 1 - indexInRow;
        }

        int q = q_min + adjustedCol;
        int r = row - (HEX_RINGS - 1);
        s_indexToHexCoord[index] = HexCoord(q, r);
    }

    s_precomputed = true;
}

void HexagonGFX::cleanupPrecomputedData()
{
    s_precomputedRingsFlat.reset();
    s_ringOffsets.reset();
    s_ringSizes.reset();
    s_precomputedSpiral.reset();
    s_indexToHexCoord.reset();
    s_precomputed = false;
}

HexagonGFX::HexCoordView HexagonGFX::getHexRing(int radius)
{
    if (!s_precomputed) {
        precomputeHexData();
    }

    if (radius < 0 || radius >= HEX_RINGS) {
        return HexCoordView{nullptr, 0};
    }

    return HexCoordView{s_precomputedRingsFlat.get() + s_ringOffsets[radius], s_ringSizes[radius]};
}

HexagonGFX::HexCoordView HexagonGFX::getHexSpiral()
{
    if (!s_precomputed) {
        precomputeHexData();
    }

    return HexCoordView{s_precomputedSpiral.get(), TOTAL_LEDS_IN_HEX};
}

HexCoord HexagonGFX::indexToHexCoord(int index)
{
    if (!s_precomputed) {
        precomputeHexData();
    }

    if (index < 0 || index >= TOTAL_LEDS_IN_HEX) {
        return HexCoord(0, 0);  // Return center for invalid indices
    }

    return s_indexToHexCoord[index];
}

// Legacy allocating version - kept for compatibility but should be avoided
std::vector<HexCoord> HexagonGFX::getHexRing(HexCoord center, int radius) {
    std::vector<HexCoord> results;
    HexCoord hex = hexAdd(center, hexScale(getHexDirection(4), radius));

    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < radius; j++) {
            results.push_back(hex);
            hex = getHexNeighbor(hex, i);
        }
    }
    return results;
}

std::vector<HexCoord> HexagonGFX::getHexSpiral(HexCoord center, int maxRadius) {
    std::vector<HexCoord> results;
    results.push_back(center);

    for (int radius = 1; radius <= maxRadius; radius++) {
        std::vector<HexCoord> ring = getHexRing(center, radius);
        results.insert(results.end(), ring.begin(), ring.end());
    }

    return results;
}

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
