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

    virtual bool isValidPixel(uint x, uint y) const noexcept override {
        return xy(x, y) < _ledcount;
    }

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

    // Direction and neighbor operations
    static HexCoord getHexDirection(int direction);
    static HexCoord getHexNeighbor(HexCoord hex, int direction);
    static std::array<HexCoord, 6> getHexNeighbors(HexCoord hex);
    static HexCoord hexAdd(HexCoord a, HexCoord b);
    static HexCoord hexSubtract(HexCoord a, HexCoord b);
    static HexCoord hexScale(HexCoord hex, int factor);

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

    static HexCoordView getHexRing(int radius);
    static HexCoordView getHexSpiral();
    static HexCoord indexToHexCoord(int index);  // Convert LED index to HexCoord

    // Legacy allocating versions - avoid using these in hot paths
    // These support arbitrary centers and radii
    std::vector<HexCoord> getHexRing(HexCoord center, int radius);
    std::vector<HexCoord> getHexSpiral(HexCoord center, int maxRadius);
    std::vector<HexCoord> getHexesInRange(HexCoord center, int radius);

    // Interpolation and transformation
    static HexCoord hexLerp(HexCoord a, HexCoord b, float t);
    static HexCoord hexRound(float q_frac, float r_frac, float s_frac);
    static HexCoord hexRotate(HexCoord hex, int rotations);

private:
    float m_hexSize;
    PixelCoord m_originOffset;

    // Precomputed static data for performance
    // Using fixed-size arrays to avoid Static Initialization Order Fiasco
    // No heap allocation - all storage is in BSS/initialized data sections
    static std::unique_ptr<HexCoord[]> s_precomputedRingsFlat;  // Flat storage for all rings
    static std::unique_ptr<int[]> s_ringOffsets;  // Offset of each ring in flat storage
    static std::unique_ptr<int[]> s_ringSizes;    // Size of each ring
    static std::unique_ptr<HexCoord[]> s_precomputedSpiral;
    static std::unique_ptr<HexCoord[]> s_indexToHexCoord;  // LED index -> HexCoord lookup
    static bool s_precomputed;

    static void precomputeHexData();
    static void cleanupPrecomputedData();
};
