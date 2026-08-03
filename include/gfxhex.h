//+--------------------------------------------------------------------------
//
// File:        gfxhex.h
//
// Hexagonal grid graphics driver definitions.
// Defines HexCoord math, neighbor generation, and matrix bounds checking.
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

#pragma once

#include "globals.h"
#include <algorithm>
#include <array>
#include <optional>
#include <vector>
#include "ws281xgfx.h"


#ifndef HEX_RINGS
#define HEX_RINGS 10
#endif

// A standard flat-top hex grid
struct HexCoord {
    int q;
    int r;
    int s; // s = -q - r

    constexpr HexCoord(int q_val, int r_val) : q(q_val), r(r_val), s(-q_val - r_val) {}
    constexpr HexCoord() : q(0), r(0), s(0) {}
};

// Hex direction vectors (flat-top)
constexpr std::array<HexCoord, 6> HEX_DIRECTIONS = {
    HexCoord(1, 0), HexCoord(1, -1), HexCoord(0, -1),
    HexCoord(-1, 0), HexCoord(-1, 1), HexCoord(0, 1)
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

// Precomputes the number of LEDs in each horizontal row of a flat-topped hexagon.
// The middle row is the widest, and rows taper symmetrically towards the top and bottom.
constexpr std::array<int, TOTAL_HEX_ROWS> generateRowLengths() {
    std::array<int, TOTAL_HEX_ROWS> data{};
    for (int i = 0; i < TOTAL_HEX_ROWS; ++i) {
        data[i] = (i < HEX_RINGS) ? (HEX_RINGS + i) : (3 * HEX_RINGS - 2 - i);
    }
    return data;
}

// Precomputes the starting LED index (offset) for each row.
// Note the goofy array sizes: an N-element array of row lengths produces an (N+1)-element array
// of starting offsets. The first row starts at index 0, the second at length[0], and the
// final (N+1)th element holds the total sum of all LEDs. This avoids O(N) loops in `hexToIndex()`.
constexpr std::array<int, TOTAL_HEX_ROWS + 1> generateCumulativeSums(const std::array<int, TOTAL_HEX_ROWS>& lengths) {
    std::array<int, TOTAL_HEX_ROWS + 1> data{};
    data[0] = 0;
    for (int i = 0; i < TOTAL_HEX_ROWS; ++i) {
        data[i + 1] = data[i] + lengths[i];
    }
    return data;
}

class HexagonGFX final : public WS281xGFX
{
public:
    static constexpr std::array<int, TOTAL_HEX_ROWS> ROW_LENGTHS = generateRowLengths();
    static constexpr std::array<int, TOTAL_HEX_ROWS + 1> CUMULATIVE_ROW_SUMS = generateCumulativeSums(ROW_LENGTHS);

    HexagonGFX(size_t numLeds);
    ~HexagonGFX() override = default;

    static void InitializeHardware(std::vector<std::shared_ptr<GFXBase>>& devices);

    // Provides a fallback compatibility layer for 2D Cartesian effects (e.g. moving lines).
    // This maps standard (X,Y) logic onto the closest hex index so legacy effects don't
    // crash, though they will appear optically skewed due to the non-square grid.
    uint16_t xy(uint16_t x, uint16_t y) const noexcept override;

    bool isValidPixel(uint x, uint y) const noexcept override {
        return xy(x, y) < _ledcount;
    }

    // --- Native Hex Drawing Primitives ---

    // The core math engine of the hex grid. Maps a virtual Q/R/S axial coordinate
    // to the physical 1D hardware index of the LED in the serpentine string.
    std::optional<int> hexToIndex(HexCoord hex) const;

    // Primary rendering methods for hex-native effects. Converts coordinates and applies color.
    void drawHexPixel(int q, int r, CRGB color);
    void drawHexPixel(HexCoord hex, CRGB color);

    // Fills a ring around the hexagon, inset by the indent specified
    void fillHexRing(uint16_t indent, CRGB color);

    // --- Sub-Pixel / Float Coordinate Helpers ---
    // These bridge the gap between continuous floating-point math (used for smooth
    // physical motion, rotation, and gravity) and the discrete, snapped axial coordinates.
    static float hexDistance(HexCoord a, HexCoord b);
    OffsetCoord cubeToOffset(HexCoord hex) const;
    std::optional<int> offsetToLinearIndex(OffsetCoord offset) const;
    HexCoord pixelToHex(PixelCoord pixel, float hex_size, PixelCoord origin_offset_pixels) const;
    PixelCoord hexToPixelFlatTop(HexCoord hex, float hex_size, PixelCoord origin_offset_pixels) const;

    // Direction and neighbor operations
    static constexpr HexCoord getHexDirection(int direction) {
        return HEX_DIRECTIONS[((direction % 6) + 6) % 6];
    }
    static constexpr HexCoord hexAdd(HexCoord a, HexCoord b) {
        return HexCoord(a.q + b.q, a.r + b.r);
    }
    static constexpr HexCoord hexSubtract(HexCoord a, HexCoord b) {
        return HexCoord(a.q - b.q, a.r - b.r);
    }
    static constexpr HexCoord hexScale(HexCoord hex, int factor) {
        return HexCoord(hex.q * factor, hex.r * factor);
    }
    static constexpr HexCoord getHexNeighbor(HexCoord hex, int direction) {
        return hexAdd(hex, getHexDirection(direction));
    }
    static constexpr std::array<HexCoord, 6> getHexNeighbors(HexCoord hex) {
        return {getHexNeighbor(hex, 0), getHexNeighbor(hex, 1), getHexNeighbor(hex, 2),
                getHexNeighbor(hex, 3), getHexNeighbor(hex, 4), getHexNeighbor(hex, 5)};
    }

    // Shape drawing
    // WARNING: fillHexagon and drawHexSpiral allocate internally. Use sparingly in hot paths.
    void fillHexagon(HexCoord center, int radius, CRGB color);
    void drawHexLine(HexCoord start, HexCoord end, CRGB color);
    void drawHexSpiral(HexCoord center, int maxRadius, CRGB color);
    void drawHexCone(HexCoord center, int direction, int length, CRGB color);
    void drawHexWedge(HexCoord center, int startDir, int endDir, int radius, CRGB color);

    // Range and area operations
    // Precomputed data for performance - returns lightweight views into static data
    // These are centered at (0,0) and cover the entire hex grid
    struct HexCoordView {
        const HexCoord* data;
        int size;

        const HexCoord* begin() const { return data; }
        const HexCoord* end() const { return data + size; }
    };

    HexCoordView getHexRing(int radius) const;
    HexCoordView getHexSpiral() const;
    HexCoord indexToHexCoord(int index) const;  // Convert LED index to HexCoord

    // Interpolation and transformation
    static HexCoord hexLerp(HexCoord a, HexCoord b, float t);
    static HexCoord hexRound(float q_frac, float r_frac, float s_frac);
    static HexCoord hexRotate(HexCoord hex, int rotations);

private:
    float m_hexSize;
    PixelCoord m_originOffset;

    // Precomputed static data for performance - stored entirely in Flash!
    struct PrecomputedRingsData {
        std::array<HexCoord, TOTAL_LEDS_IN_HEX> flat;
        std::array<int, HEX_RINGS> offsets;
        std::array<int, HEX_RINGS> sizes;
    };
    static consteval PrecomputedRingsData generatePrecomputedRings();
    static const PrecomputedRingsData RINGS_DATA;

    static consteval std::array<HexCoord, TOTAL_LEDS_IN_HEX> generateSpiral();
    static const std::array<HexCoord, TOTAL_LEDS_IN_HEX> PRECOMPUTED_SPIRAL;

    static consteval std::array<HexCoord, TOTAL_LEDS_IN_HEX> generateIndexToHex();
    static const std::array<HexCoord, TOTAL_LEDS_IN_HEX> INDEX_TO_HEX_COORD;
};

inline consteval HexagonGFX::PrecomputedRingsData HexagonGFX::generatePrecomputedRings() {
    PrecomputedRingsData d{};
    int flatOffset = 0;
    HexCoord center(0, 0);
    for (int radius = 0; radius < HEX_RINGS; radius++) {
        d.offsets[radius] = flatOffset;
        if (radius == 0) {
            d.flat[flatOffset++] = center;
            d.sizes[radius] = 1;
        } else {
            HexCoord hex = hexAdd(center, hexScale(getHexDirection(4), radius));
            int ringSize = 0;
            for (int i = 0; i < 6; i++) {
                for (int j = 0; j < radius; j++) {
                    d.flat[flatOffset++] = hex;
                    ringSize++;
                    hex = getHexNeighbor(hex, i);
                }
            }
            d.sizes[radius] = ringSize;
        }
    }
    return d;
}
inline constexpr HexagonGFX::PrecomputedRingsData HexagonGFX::RINGS_DATA = HexagonGFX::generatePrecomputedRings();

inline consteval std::array<HexCoord, TOTAL_LEDS_IN_HEX> HexagonGFX::generateSpiral() {
    std::array<HexCoord, TOTAL_LEDS_IN_HEX> d{};
    int spiralOffset = 0;
    for (int radius = 0; radius < HEX_RINGS; radius++) {
        int ringOffset = RINGS_DATA.offsets[radius];
        int ringSize = RINGS_DATA.sizes[radius];
        for (int i = 0; i < ringSize; i++) {
            d[spiralOffset++] = RINGS_DATA.flat[ringOffset + i];
        }
    }
    return d;
}
inline constexpr std::array<HexCoord, TOTAL_LEDS_IN_HEX> HexagonGFX::PRECOMPUTED_SPIRAL = HexagonGFX::generateSpiral();

inline consteval std::array<HexCoord, TOTAL_LEDS_IN_HEX> HexagonGFX::generateIndexToHex() {
    std::array<HexCoord, TOTAL_LEDS_IN_HEX> d{};
    for (int index = 0; index < TOTAL_LEDS_IN_HEX; index++) {
        int row = 0;
        while (row < TOTAL_HEX_ROWS && CUMULATIVE_ROW_SUMS[row + 1] <= index) {
            row++;
        }
        int indexInRow = index - CUMULATIVE_ROW_SUMS[row];
        int currentRowLength = ROW_LENGTHS[row];
        int q_min = std::max(-(HEX_RINGS - 1), -(HEX_RINGS - 1) - (row - (HEX_RINGS - 1)));

        int adjustedCol;
        if (row % 2 == 0) {
            adjustedCol = indexInRow;
        } else {
            adjustedCol = currentRowLength - 1 - indexInRow;
        }
        int q = q_min + adjustedCol;
        int r = row - (HEX_RINGS - 1);
        d[index] = HexCoord(q, r);
    }
    return d;
}
inline constexpr std::array<HexCoord, TOTAL_LEDS_IN_HEX> HexagonGFX::INDEX_TO_HEX_COORD = HexagonGFX::generateIndexToHex();
