#pragma once

#ifndef PATTERN_CG_KONG_H
#define PATTERN_CG_KONG_H

#include "globals.h"
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>

// Debug Flags
#define LOG_MASK_AI       0x01
#define LOG_MASK_PHYSICS  0x02
#define LOG_MASK_THREATS  0x04
#define LOG_MASK_OBJECTS  0x08
#define LOG_MASK_ALL      0xFF

// Current Debug Level
#define LOG_CURRENT_MASK  (LOG_MASK_ALL)

#define DEBUG_EFFECT false
#define debugEffect(mask, ...) if (DEBUG_EFFECT && ((mask) & LOG_CURRENT_MASK)) debugA(__VA_ARGS__)


class PatternCGKong : public EffectWithId<PatternCGKong>
{
    struct Barrel {
        float x, y;
        float vx, vy;
        bool active;
        bool isBlue;
    };

    // Physics & Game Constants
    static constexpr float kGravity = 0.15f;
    static constexpr float kJumpStrength = -1.4f; // v38.9: Higher jump for better clearance
    static constexpr float kWalkSpeed = 2.4f;   // v38.9: Aligned with 60Hz barrels (approx. 24 px/sec)
    static constexpr float kClimbSpeed = 0.4f;

    // Layout & Threshold Constants
    static constexpr float kGroundY = 31.0f;
    static constexpr float kT1Y = 27.0f;
    static constexpr float kT2Y = 23.0f;
    static constexpr float kT3Y = 17.0f;
    static constexpr float kDkPlatformY = 13.0f;

    // v18: Derived Thresholds (No Magic Numbers)
    // Used to determine if Mario "fell" below a tier.
    static constexpr float kThreshG = kT1Y - 1.0f; // > 26
    static constexpr float kThreshT1 = kT2Y - 3.0f; // > 20
    static constexpr float kThreshT2 = kT3Y - 2.0f; // > 15

    // Ladder positions
    static constexpr float kLadder1X = 8.0f;
    static constexpr float kLadder2X = MATRIX_WIDTH / 2.0f;
    static constexpr float kLadder3X = MATRIX_WIDTH - 8.0f;

    // Safety & Tolerance Constants
    static constexpr float kLadderSnapDist = 2.0f;
    static constexpr float kLadderProximity = 6.0f; // v39.29: Increased reach so Mario can climb from safe offset
    static constexpr float kBailProximity = 6.0f;
    static constexpr float kWaitOffset = 10.0f;
    static constexpr float kWaitOffsetLarge = 12.0f;
    static constexpr float kSafetyRadSmall = 6.0f;
    static constexpr float kSafetyRadLarge = 12.0f;
    static constexpr float kSafetyRadHuge = 20.0f;

    static constexpr int kLadderWidth = 4;

    enum EntityState { WALKING, CLIMBING, JUMPING };
    struct Entity {
        float x = 0.0f;
        float y = 0.0f;
        uint8_t frame = 0;
        uint32_t lastAnimTime = 0;
        EntityState state = WALKING;
        float vx = 0.0f;
        float vy = 0.0f;
        float targetX = 0.0f;
        bool faceLeft = false;
        float jumpFloorY = 28.0f;
        float climbTargetY = 0.0f;
        int tier = 0; // v30: Explicit tier tracking
        uint32_t lastClimbTime = 0; // v32: Cooldown to prevent ladder jitter
        uint32_t panicTime = 0; // v34: Retreat persistence
        uint32_t targetLockTime = 0; // v39.0: Decision persistence
        float lastTargetX = 0.0f;
        uint32_t _squashTime = 0;
        uint32_t ladderBusyTime = 0; // v39.17: Ladder busy hysteresis
        uint32_t lastJumpTime = 0; // v39.29: Prevent pogo-sticking
    };

    // ------------------------------------------------------------------
    // CGKong palette expressed as RGB565 *intent*.
    // Conversion to CRGB is gamma-corrected at draw time.
    // ------------------------------------------------------------------
    static constexpr uint16_t kDKFur565     = 0x9A44;
    static constexpr uint16_t kDKTan565     = 0xF6B2;
    static constexpr uint16_t kMarioSkin565 = 0xFBA0;
    static constexpr uint16_t kBarrel565    = 0xD2A0; // Burnt orange
    static constexpr uint16_t kGirder565    = 0xB104; // Rusty steel red
    static constexpr uint16_t kRed565       = 0xF800; // Mario Red

    std::vector<Barrel> _barrels;
    Entity _dk;
    Entity _mario;
    float _flashAmount = 0.0f;
    bool _gameActive = true;
    uint32_t _winTime = 0; // v16: Time of victory
    uint32_t _resetTime = 0; // v37: Fade reset timer
    uint32_t _tier0Time = 0; // v37.2: Track time on ground floor
    float _dkBreath = 0.0f;
    uint32_t frameTime = 0;
    uint32_t _dkShockTime = 0; // v38: DK stomp shockwave trigger

    // Palette members (computed per frame)
    CRGB _palFur;
    CRGB _palTan;
    CRGB _palMarioSkin;
    CRGB _palBarrel;
    CRGB _palGirder;
    CRGB _palRed;
    CRGB _palBlue;

    // 7x12 font (scaffolding style)
    static constexpr uint16_t font7x12[11][12] = {
        {0x1C, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C}, // 0
        {0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C}, // 1
        {0x1C, 0x22, 0x22, 0x02, 0x02, 0x1C, 0x20, 0x20, 0x20, 0x20, 0x22, 0x3E}, // 2
        {0x1C, 0x22, 0x22, 0x02, 0x02, 0x1C, 0x02, 0x02, 0x02, 0x02, 0x22, 0x1C}, // 3
        {0x04, 0x0C, 0x1C, 0x24, 0x44, 0x44, 0x3F, 0x04, 0x04, 0x04, 0x04, 0x04}, // 4
        {0x3E, 0x20, 0x20, 0x20, 0x20, 0x3C, 0x02, 0x02, 0x02, 0x02, 0x22, 0x1C}, // 5
        {0x1C, 0x22, 0x20, 0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C}, // 6
        {0x3E, 0x22, 0x02, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}, // 7
        {0x1C, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C}, // 8
        {0x1C, 0x22, 0x22, 0x22, 0x22, 0x1E, 0x02, 0x02, 0x02, 0x02, 0x22, 0x1C}, // 9
        {0x00, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x00, 0x00}  // :
    };

    // DK Fur (Dark Brown) 12x12 - Gorilla silhouette v9 (Arm gaps for cleaner look)
    static constexpr uint16_t dkFur[3][12] = {
        {0x000, 0x0E0, 0x1F0, 0x3F8, 0x7FC, 0x7FC, 0x7FC, 0xFFF, 0xFFF, 0x7FC, 0x3F8, 0x228}, // Standing (hunch)
        {0x000, 0x8E1, 0x9F1, 0x7FC, 0xFFF, 0xC3F, 0xC3F, 0xC3F, 0xFFF, 0x7FC, 0x666, 0x666}, // Pounding A (Arms out, wide stance)
        {0x9F9, 0xFFF, 0xC3F, 0xC3F, 0xC3F, 0xFFF, 0x7FC, 0x7FC, 0x7FC, 0x3F8, 0x228, 0x666}  // Pounding B (Arms up, wide stance - Feet fixed!)
    };
    // DK Skin (Tan) - Facial features v7
    static constexpr uint16_t dkTan[3][12] = {
        {0x000, 0x000, 0x0A0, 0x000, 0x1F0, 0x1F0, 0x000, 0x1F0, 0x1F0, 0x0C0, 0x000, 0x000},
        {0x000, 0x000, 0x0A0, 0x000, 0x0E0, 0x0E0, 0x000, 0x1F0, 0x1F0, 0x0C0, 0x000, 0x000},
        {0x000, 0x000, 0x0A0, 0x000, 0x0E0, 0x0E0, 0x000, 0x3F0, 0x3F0, 0x1E0, 0x000, 0x000}
    };

    // Mario Layers (8x8)
    static constexpr uint8_t marioRed[4][8] = {
        {0x38, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Hat/Shirt Stand
        {0x38, 0x7C, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00}, // Run A
        {0x38, 0x7C, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00}, // Run B
        {0x38, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // Climb
    };
    static constexpr uint8_t marioBlue[4][8] = {
        {0x00, 0x00, 0x00, 0x00, 0x18, 0x3C, 0x24, 0x00}, // Overalls
        {0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x1C, 0x00}, // Run A
        {0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x38, 0x00}, // Run B
        {0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x3C, 0x42}  // Climb
    };
    static constexpr uint8_t marioSkin[4][8] = {
        {0x00, 0x00, 0x3C, 0x7E, 0x00, 0x00, 0x00, 0x00}, // Face/Hands
        {0x00, 0x00, 0x3C, 0x7E, 0x42, 0x42, 0x00, 0x00}, // Run A
        {0x00, 0x00, 0x3C, 0x7E, 0x42, 0x42, 0x00, 0x00}, // Run B
        {0x00, 0x00, 0x3C, 0x7E, 0x00, 0x00, 0x00, 0x00}  // Climb
    };

public:
    // v39.13: Compile-time checks - this effect requires specific dimensions
    static_assert(MATRIX_WIDTH >= 64, "PatternCGKong requires MATRIX_WIDTH >= 64");
    static_assert(MATRIX_HEIGHT >= 32, "PatternCGKong requires MATRIX_HEIGHT >= 32");

    PatternCGKong() : EffectWithId<PatternCGKong>("CG Kong") { ResetGame(); }
    PatternCGKong(const JsonObjectConst& json) : EffectWithId<PatternCGKong>(json) { ResetGame(); }

    void Start() override {
        // Initialize palette once
        _palFur       = GFXBase::from16Bit(kDKFur565);
        _palTan       = GFXBase::from16Bit(kDKTan565);
        _palMarioSkin = GFXBase::from16Bit(kMarioSkin565);
        _palBarrel    = GFXBase::from16Bit(kBarrel565);
        _palGirder    = GFXBase::from16Bit(kGirder565);
        _palRed       = GFXBase::from16Bit(kRed565);
        _palBlue      = GFXBase::from16Bit(0x001F);
        ResetGame();
    }

    void ResetGame() {
        if (_resetTime == 0) {
            _resetTime = millis();
            debugEffect(LOG_MASK_AI, "ResetGame triggered. Fading out...\n");
            return;
        }
        _resetTime = 0;
        _barrels.clear();
        _mario.state = WALKING;
        _mario.x = 4.0f;
        _mario.tier = 0; // v31: Explicit tier initialization
        _mario.y = GetFloorY(4.0f, 0) - 3.0f;
        _mario.vx = kWalkSpeed;
        _mario.vy = 0;
        _mario.targetX = 64.0f;
        _mario.faceLeft = false;
        _mario.lastClimbTime = 0;
        _mario.panicTime = 0;
        _mario.lastJumpTime = 0;
        _mario.lastAnimTime = millis(); // v38.6: Initialize timer

        _dk.x = 12.0f;
        _dk.y = 7.0f;
        _dk.frame = 0;

        _gameActive = true;
        _winTime = 0;
        _tier0Time = millis(); // v37.2: Start ground floor timer
        _barrels.clear();
        _flashAmount = 0.0f;
        debugEffect(LOG_MASK_AI, "ResetGame: Mario @ %.1f,%.1f (Tier %d). DK @ %.1f,%.1f\n", _mario.x, _mario.y, _mario.tier, _dk.x, _dk.y);
    }

    void Draw() override {
        frameTime = millis();
        // Calculate DK breathing (Sub-pixel breathing: < 0.5px keeps DK heavy, not floaty)
        _dkBreath = sinf(frameTime * 0.002f) * 0.3f;

        // v37: Visual Fade Reset
        if (_resetTime > 0) {
            uint32_t elapsed = millis() - _resetTime;
            if (elapsed > 600) {
                ResetGame(); // Second call finishes reset
            } else {
                g()->DimAll(255 - uint8_t(elapsed * 255 / 600)); // Standard blend out
                return;
            }
        }

        g()->DimAll(160);
        if (_flashAmount > 0.1f) g()->DimAll(uint8_t(160 * (1.0f - _flashAmount))); // Pulse background

        DrawGirders();
        DrawGhostClock();

        // v38: DK stomp shockwave (palette-only radial pulse)
        if (_dkShockTime > 0) {
            // perceptual tuning for shockwave
            const float kRingSpeed = 16.0f; // slightly slower
            // If too subtle, try 0.45f;
            const float kRingThickness = 1.4f; // ticker for smoother ring
            const float kBrightness = 0.40f; // perceptually punchy
            const float kDuration = 180.0f;

            uint32_t age = millis() - _dkShockTime;
            if (age < kDuration) {
                float t = age / kDuration;        // 0..1
                float radius = 3.0f + t * kRingSpeed;
                float strength = (1.0f - t) * kBrightness;

                // v37.11: Optimized Primitive Circle
                int cx = (int)_dk.x;
                float yOffset = (MATRIX_HEIGHT - 32) / 2.0f;
                int cy = (int)(_dk.y + _dkBreath + yOffset);
                uint8_t v = uint8_t(255 * strength);
                CRGB c(v, v, v);

                g()->DrawSafeCircle(cx, cy, (int)radius, c);
                if (kRingThickness > 1.2f) {
                    g()->DrawSafeCircle(cx, cy, (int)radius + 1, c);
                }
            } else {
                _dkShockTime = 0;
            }
        }

        if (_winTime > 0) {
            UpdateDK(millis()); // Keep DK breathing on win screen
            if (millis() - _winTime > 3000) ResetGame();
        } else {
            UpdateEntities();
        }

        DrawEntities();
        DrawBarrels();

        if (_flashAmount > 0) _flashAmount *= 0.92f; // Fade out flash
    }

    // Helper: Calculate Floor Y for any X and Tier
    // tier: 0=Ground, 1=T1, 2=T2, 3=T3
    float GetFloorY(float x, int tier) {
        if (tier == 0) return kGroundY; // Flat ground

        if (tier == 1) { // T1: (0, 27) -> (Max, 29)
             return 27.0f + (x / MATRIX_WIDTH) * 2.0f;
        }
        if (tier == 2) { // T2: (Max, 21) -> (0, 23)
             return 21.0f + ((MATRIX_WIDTH - x) / MATRIX_WIDTH) * 2.0f;
        }
        if (tier == 3) { // T3: (DKEdge+1, 13) -> (Max, 17)
             float dkEdge = MATRIX_WIDTH / 4.0f + 8.0f;
             if (x < dkEdge) return kDkPlatformY; // On Platform
             return 13.0f + (x - dkEdge) * (17.0f - 13.0f) / (MATRIX_WIDTH - dkEdge);
        }
        return 32.0f; // Fallback
    }

    // v33: Lookahead Helper
    // v38.1: Added ignoreBarrel to prevent "false positives" when checking landing zone for a jump over a specific barrel
    bool IsAreaSafe(float x, int tier, float radius, const Barrel* ignoreBarrel = nullptr) {
        for (const auto& b : _barrels) {
            if (!b.active) continue;
            if (ignoreBarrel && &b == ignoreBarrel) continue; // Skip the barrel we are effectively engaging/jumping

            // Barrel Y is roughly FloorY - 2.0. Mario Y is FloorY - 3.0.
            // Check if barrel is on the same tier
            float floorY = GetFloorY(x, tier);

            // v35.1: Detect falling barrels near this X coordinate (Vertical Safety)
            if (b.vy > 0 && fabsf(b.x - x) < 4.0f) {
                 // v39.5: Paranoid vertical window (12px) to catch ladder-drop hazards
                 if (b.y < floorY && b.y > floorY - 12.0f) return false;
            }

            // v39.7: Strict tier check to prevent false positives (T3 barrels flagging T2 Mario)
            if (fabsf(b.y - (floorY - 2.0f)) > 2.5f) continue;

            // Horizontal Danger
            if (fabsf(b.x - x) < radius) return false;
        }
        return true;
    }

    // v37.6 Ladder Safety: Check if a ladder is currently dangerous (descending OR approaching)
    // v39.17: Added hysteresis - once busy, stay busy for 500ms
    bool IsLadderBusy(float ladderX, int tier) {
        uint32_t nowTime = millis();
        // If ladder was recently marked busy, keep it busy (hysteresis)
        if (_mario.ladderBusyTime > 0 && (nowTime - _mario.ladderBusyTime < 500)) {
            return true;
        }

        for (const auto& b : _barrels) {
            if (!b.active) continue;

            // Allow loose Y matching for tier check
            float floorY = GetFloorY(b.x, tier);
            if (fabsf(b.y - (floorY - 2.0f)) > 4.0f) continue; // Not on this tier

            // Check approach
            bool incoming = false;
            if (tier == 2 && b.x > ladderX && b.x < ladderX + 20.0f && b.vx < 0) incoming = true; // L4: From Right
            if (tier == 1 && b.x < ladderX && b.x > ladderX - 20.0f && b.vx > 0) incoming = true; // L3: From Left
            if (tier == 0 && b.x > ladderX && b.x < ladderX + 20.0f && b.vx < 0) incoming = true; // T0? Barrels roll Left.

            if (incoming) {
                _mario.ladderBusyTime = nowTime;
                return true;
            }
        }
        return false;
    }



private:
    void DrawGirders() {
        int dkPlatformEdge = MATRIX_WIDTH / 4 + 8;
        float yOffset = (MATRIX_HEIGHT - 32) / 2.0f;
        g()->drawLine(0, 13 + yOffset, dkPlatformEdge, 13 + yOffset, _palGirder); // DK platform
        g()->drawLine(0, 14 + yOffset, dkPlatformEdge, 14 + yOffset, _palGirder); // Thicken

        // Use a loop to draw 2-pixel thick slanted girders for solidity
        auto drawThickLine = [&](int x1, int y1, int x2, int y2) {
            g()->drawLine(x1, y1 + yOffset, x2, y2 + yOffset, _palGirder);
            g()->drawLine(x1, y1 + 1 + yOffset, x2, y2 + 1 + yOffset, _palGirder);
        };

        // v13: T3 slope starts at y=13 (DK Edge) down to y=17
        drawThickLine(dkPlatformEdge + 1, 13, MATRIX_WIDTH - 1, 17); // T3
        drawThickLine(MATRIX_WIDTH - 1, 21, 0, 23); // T2
        drawThickLine(0, 27, MATRIX_WIDTH - 1, 29); // T1
        g()->drawLine(0, 31 + yOffset, MATRIX_WIDTH - 1, 31 + yOffset, _palGirder); // Ground

        DrawLadder(kLadder3X, 17, 4); // T3 to T2
        DrawLadder(kLadder1X, 23, 4); // T2 to T1
        DrawLadder(kLadder2X, 29, 2); // T1 to Ground
    }

    void DrawLadder(int x, int y, int h) {
        float yOffset = (MATRIX_HEIGHT - 32) / 2.0f;
        for (int i = 0; i < h; i++) {
            g()->setPixel(x - 1, y + i + yOffset, _palBlue);
            g()->setPixel(x + 1, y + i + yOffset, _palBlue);
            if (i % 2 == 0) g()->setPixel(x, y + i + yOffset, _palBlue);
        }
    }

    void DrawGhostClock() {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        struct tm tm_buf;
        struct tm* t = localtime_r(&now, &tm_buf);
        int hours = t->tm_hour;
        int mins = t->tm_min;
        int digits[5] = {hours / 10, hours % 10, 10, mins / 10, mins % 10};

        if (_winTime > 0) { // v20: Spin digits on win!
             uint8_t spin = (millis() / 50) % 10;
             for(int& d : digits) d = (d + spin) % 10;
        }
        int clockWidth = (5 * 6) + 4;
        int startX = (MATRIX_WIDTH - clockWidth) / 2;
        int startY = 10;

        CRGB ghostBase(30, 30, 30);

        for (int d = 0; d < 5; d++) {
            float highlight = 0.0f;
            int dx = startX + (d * 6);
            int dy = (d == 2) ? startY + 2 : startY;

            for (const auto& b : _barrels) {
                if (!b.active) continue;
                float distSq = powf(b.x - (dx + 3), 2) + powf(b.y - (dy + 6), 2);
                if (distSq < 100.0f) { // 10 pixel radius
                    float h = (10.0f - sqrtf(distSq)) / 10.0f;
                    highlight = std::max(highlight, h);
                }
                if (b.isBlue) highlight = std::max(highlight, 0.4f);
            }

            CRGB color = ghostBase;
            if (highlight > 0 || _flashAmount > 0) {
                float totalHighlight = std::max(highlight, _flashAmount);
                uint8_t val = (uint8_t)std::min(225.0f, 180.0f * totalHighlight + (_flashAmount > 0.1f ? 50.0f : 0.0f));
                color += CRGB(val, val, val);
            }

            DrawDigit(dx, dy, digits[d], color);
        }
    }

    void DrawDigit(int x, int y, int digit, CRGB color) {
        float yOffset = (MATRIX_HEIGHT - 32) / 2.0f;
        for (int row = 0; row < 12; row++) {
            uint16_t rowBits = font7x12[digit][row];
            for (int col = 0; col < 6; col++) {
                if ((rowBits >> (5 - col)) & 1) {
                    g()->setPixel(x + col, y + row + yOffset, color);
                }
            }
        }
    }

    void UpdateDK(uint32_t nowTime) {
        // DK Pounding logic - switch between check "white static" pixels.
        if (nowTime - _dk.lastAnimTime > 400) {
            if (_dk.frame == 0) _dk.frame = 1;
            else if (_dk.frame == 1) _dk.frame = 2;
            else if (_dk.frame == 2) {
                _dk.frame = 0;
		// v38: DK stomp shockwave (visual only)
                 _dkShockTime = millis();

                // Spawn barrel on the pounding frame return
                // Spawn barrel on the pounding frame return
                // v24: User requested higher frequency ("bring that back"). Bump to 40%.
                // v25: Balance Pass. User felt 40% was "rain". Reduced to 20%.
                // v33.3: Balanced chaos
                if (random_range(0, 100) < 60) {
                    _barrels.push_back({25.0f, 9.5f, kClimbSpeed + (random_range(0, 100)/500.0f), 0.0f, true, false});
                }
            }
            _dk.lastAnimTime = nowTime;
        }
    }

    void UpdateMario(uint32_t nowTime) {
        if (_mario.state == WALKING) {

            // v100: Behavioral Logic (10Hz Timer)
            if (nowTime - _mario.lastAnimTime > 100) {
                _mario.lastAnimTime = nowTime;

                // 1. Behavioral Targeting - v39.0: Added Hysteresis
                float preferredTargetX = _mario.targetX;
                if (_mario.tier == 0) preferredTargetX = kLadder2X;
                else if (_mario.tier == 1) preferredTargetX = IsLadderBusy(kLadder1X, 1) ? kLadder2X : (kLadder1X - 3.0f); // v39.29: Stand Left of drop (x=5)
                else if (_mario.tier == 2) preferredTargetX = IsLadderBusy(kLadder3X, 2) ? 60.0f : (kLadder3X + 3.0f); // v39.29: Stand Right of drop (x=59)
                else preferredTargetX = 0.0f; // Walk to DK

                // Lock the target for 300ms to prevent jitter and respect overrides
                if (nowTime - _mario.targetLockTime > 300) {
                    _mario.targetX = preferredTargetX;
                    _mario.lastTargetX = preferredTargetX;
                    _mario.targetLockTime = nowTime;
                }

                // 2. Behavioral Movement Direction
                float dist = _mario.targetX - _mario.x;
                if (fabsf(dist) < 1.0f) {
                     _mario.vx = 0;
                } else {
                     float dir = (dist > 0) ? 1.0f : -1.0f;
                     _mario.vx = dir * kWalkSpeed;
                     _mario.faceLeft = (_mario.vx < 0);
                }

                // v39.15: Disable ALL safety overrides when going for the win!
                // Widened to x<25 to give Mario room to approach DK without oscillation
                bool goingForWin = (_mario.tier == 3 && _mario.x < 25.0f);

                // 3. AI Refinement: Safety Overrides (skip if going for win)
                if (!goingForWin) {
                    // a) Multiple barrels ahead? Move BACKWARDS (True Retreat)
                    // v39.6: Only consider barrels between Mario and his target
                    int approaching = 0;
                    float toTarget = (_mario.targetX - _mario.x);
                    for (const auto& b : _barrels) {
                        if (!b.active) continue;
                        float dx = b.x - _mario.x;
                        float dy = b.y - _mario.y;
                        if (fabsf(dy - 2.0f) > 3.0f) continue;
                        bool towardsTarget = (dx * toTarget > 0);
                        if (towardsTarget && fabsf(dx) < 25.0f) approaching++;
                    }
                    if (approaching > 1) {
                        float retreatDir = (toTarget > 0) ? -1.0f : 1.0f;
                        if (IsAreaSafe(_mario.x + retreatDir * 6.0f, _mario.tier, 4.0f)) {
                            _mario.vx = retreatDir * kWalkSpeed * 1.2f; // v39.7: Faster retreat to outpace barrels
                            debugEffect(LOG_MASK_THREATS, "[SAFETY] Mario Retreat (Sandwich Avoidance). vx:%.1f\n", _mario.vx);
                        } else {
                            _mario.vx = 0; // Wait
                            debugEffect(LOG_MASK_THREATS, "[SAFETY] Mario Stop (Sandwich Avoidance - Rear Blocked). x:%.1f\n", _mario.x);
                        }
                    }

                    // b) T3 Spawn Safety: Retreat if AT spawn point while DK is winding up
                    // v39.9: Only if Mario is close to spawn (x>23) OR moving away from DK
                    // Don't interfere if he's already past spawn and going for the win!
                    if (_mario.tier == 3 && _mario.x > 23.0f && _mario.x < 30.0f && (_dk.frame == 1 || _dk.frame == 2)) {
                        // Move AWAY from DK (DK is at x~12, spawn x=25). Retreat direction is RIGHT.
                        if (IsAreaSafe(_mario.x + 6.0f, 3, 4.0f)) {
                            _mario.vx = kWalkSpeed * 1.2f; // v39.7: Faster retreat
                            debugEffect(LOG_MASK_THREATS, "[SAFETY] Mario Retreat (Spawn Hazard). x:%.1f\n", _mario.x);
                        } else {
                            _mario.vx = 0;
                            debugEffect(LOG_MASK_THREATS, "[SAFETY] Mario Stop (Spawn Hazard - Rear Blocked). x:%.1f\n", _mario.x);
                        }
                    }

                    // c) Safety Stop: Inhibit forward movement into any immediate hazard (projected)
                    // v39.26: ALWAYS check safety, even if standing still (vx=0)!
                    if (!IsAreaSafe(_mario.x + _mario.vx * 2.5f, _mario.tier, 5.0f)) {
                        // v39.21: If near unsafe ladder (x>52), SPRINT THROUGH to hidey hole
                        if (_mario.tier == 2 && _mario.x > 52.0f) {
                            _mario.vx = kWalkSpeed * 1.5f;
                            _mario.targetX = 60.0f; _mario.lastTargetX = 60.0f;
                            _mario.targetLockTime = millis();
                            debugEffect(LOG_MASK_THREATS, "[FORCE-HIDEY] SPRINTING through danger. x:%.1f\n", _mario.x);
                        } else {
                            // v39.31: Dynamic Retreat Direction. Odd Tiers (1,3) Flow Right -> Retreat Right. Even (0,2) Flow Left -> Retreat Left.
                            float retreatDir = (_mario.tier & 1) ? 1.0f : -1.0f;
                            _mario.vx = retreatDir * kWalkSpeed * 1.7f;
                            debugEffect(LOG_MASK_THREATS, "[SAFETY-RETREAT] Hazard Ahead! Backing off. vx:%.1f\n", _mario.vx);
                        }
                    }
                } // End safety overrides

                // 4. Animation
                if (_mario.vx != 0) _mario.frame = (_mario.frame == 1) ? 2 : 1;
                else _mario.frame = 1;

                // 5. Ladder Entry Check
                bool nearL1 = fabsf(_mario.x - kLadder1X) < kLadderProximity;
                bool nearL2 = fabsf(_mario.x - kLadder2X) < kLadderProximity;
                bool nearL3 = fabsf(_mario.x - kLadder3X) < kLadderProximity;
                bool climbCooldown = (nowTime - _mario.lastClimbTime < 1500);

                if (!climbCooldown) {
                    auto startClimb = [&](float ladderX, float targetY) {
                         _mario.state = CLIMBING; _mario.x = ladderX;
                         _mario.vy = (targetY < _mario.y) ? -kClimbSpeed : kClimbSpeed;
                         _mario.vx = 0; _mario.climbTargetY = targetY;
                         debugEffect(LOG_MASK_AI, "[CLIMB] Mario committing to climb. TargetY:%.1f Tier:%d x:%.1f\n", targetY, _mario.tier, _mario.x);
                    };
                    auto checkL = [&](float lx, int targetT, float rBase, float rDest) {
                         return IsAreaSafe(lx, targetT, rDest) && IsAreaSafe(lx, _mario.tier, rBase);
                    };
                    if (_mario.tier == 0 && nearL2 && checkL(kLadder2X, 1, kSafetyRadSmall, kSafetyRadLarge)) startClimb(kLadder2X, 25.0f);
                    else if (_mario.tier == 1 && nearL1 && checkL(kLadder1X, 2, kSafetyRadSmall, kSafetyRadHuge)) startClimb(kLadder1X, 19.5f);
                    else if (_mario.tier == 2 && nearL3 && checkL(kLadder3X, 3, kSafetyRadSmall, kSafetyRadLarge)) startClimb(kLadder3X, 13.0f);
                }

                const char* stNames[] = {"WALK", "CLIMB", "JUMP"};
                debugEffect(LOG_MASK_AI, "[%p] Mario: x%.1f y%.1f vx%.1f Tier:%d Target:%.1f State:%s\n",
                    this, _mario.x, _mario.y, _mario.vx, _mario.tier, _mario.targetX, stNames[_mario.state]);
            } // End 10Hz Timer

            // Frame-Rate Reflexes (60Hz)
            if (_mario.state == WALKING) {
                // v39.28: Unified Physics Scaling (Inertia Fix)
                // Was 6.0f (Ground) vs 4.5f (Air) -> 25% "friction" on landing.
                // Standardized to 5.0f for consistent momentum.
                _mario.x += (_mario.vx / 5.0f);
                _mario.y = GetFloorY(_mario.x, _mario.tier) - 3.0f;
            }

            if (_mario.tier == 3) {
                 float dkEdge = MATRIX_WIDTH / 4.0f + 8.0f;
                 if (_mario.x < dkEdge + 2.0f) { // Robust Win
                      _flashAmount = 1.0f; _winTime = millis(); return;
                 }
            }
            // v39.7: Win-state invulnerability
            if (_winTime > 0) return;

            for (const auto& b : _barrels) {
                if (!b.active) continue;
                float dx = b.x - _mario.x;
                float dy = b.y - _mario.y;
                if (fabsf(dx) < 3.2f && (dy > -1.0f && dy < 2.5f)) { ResetGame(); return; }

                // v39.18: Disable jumping if going for win (prevents hopping near DK)
                bool goingForWin = (_mario.tier == 3 && _mario.x < 25.0f);

                if (fabsf(dy - 2.0f) < 3.0f && _mario.state == WALKING && !goingForWin) { // Same tier barrels
                    // v39.29: Jump Cooldown (500ms) to prevent "Pogo Stick" behavior
                    if (nowTime - _mario.lastJumpTime < 500) continue;

                    float jumpDist = 12.0f;
                    // v39.2: Only jump if barrel is IN FRONT and CLOSING
                    float relVx = b.vx - (_mario.vx / 6.0f);
                    bool incoming = (dx * _mario.vx > 0.1f);
                    bool closing = (dx * relVx < 0);

                    // v39.20: Disable jumping if forcing hidey hole (x=60 target) to prevent stall cycle
                    bool forcingHidey = (_mario.tier == 2 && _mario.targetX == 60.0f);

                    // v39.25: Disable jumping if RETREATING (Panic Jump Fix)
                    bool retreating = (fabsf(_mario.vx) > kWalkSpeed * 1.3f);

                    if (fabsf(dx) < jumpDist && incoming && closing && !forcingHidey && !retreating) {
                        // v39.28: Prediction synced with unified scaling (5.0f)
                        float landingX = _mario.x + (_mario.vx * 4.0f); // approx 20 frames air time?
                        if (IsAreaSafe(landingX, _mario.tier, 6.0f, &b)) {
                             _mario.state = JUMPING; _mario.vy = kJumpStrength; _mario.jumpFloorY = _mario.y;
                             debugEffect(LOG_MASK_PHYSICS, "[%p] Mario Jump Start! dx:%.1f vx:%.1f\n", this, dx, _mario.vx);
                             break;
                        }
                    }
                }
            }
        } else if (_mario.state == CLIMBING) {
            _mario.y += _mario.vy;
            _mario.frame = 3;
            if (_mario.vy < 0 && _mario.y <= _mario.climbTargetY) {
                _mario.state = WALKING; _mario.y = _mario.climbTargetY; _mario.tier++;
                _mario.lastClimbTime = millis();
            } else if (_mario.vy > 0 && _mario.y >= _mario.climbTargetY) {
                _mario.state = WALKING; _mario.y = _mario.climbTargetY; _mario.tier--;
                _mario.lastClimbTime = millis();
            }
        } else if (_mario.state == JUMPING) {
            // Horizontal speed scaled for 60Hz jump arc. v39.28: Unified Scaling
            _mario.x += (_mario.vx / 5.0f);
            _mario.y += _mario.vy; _mario.vy += kGravity;
            if (_mario.y >= _mario.jumpFloorY && _mario.vy > 0) {
                 _mario.y = _mario.jumpFloorY; _mario.vy = 0; _mario.state = WALKING;
                 _mario._squashTime = millis();
                 _mario.lastJumpTime = millis(); // v39.29: Cooldown start
                 debugEffect(LOG_MASK_PHYSICS, "[%p] Mario Jump Landed. x:%.1f\n", this, _mario.x);

                  // v39.13: Landing Re-Evaluation - check for immediate threats
                  // Fixed: Check threats in BOTH directions, not just current vx direction
                  // v39.20: Disable retreat if Going For Win or Forcing Hidey
                  bool goingForWin = (_mario.tier == 3 && _mario.x < 25.0f);
                  bool forcingHidey = (_mario.tier == 2 && _mario.targetX == 60.0f);

                  int threatsLeft = 0;
                  int threatsRight = 0;
                  float nearestLeft = -999.0f;
                  float nearestRight = 999.0f;

                  if (!goingForWin && !forcingHidey) {
                      for (const auto& b : _barrels) {
                          if (!b.active) continue;
                          float dx = b.x - _mario.x;
                          float dy = b.y - _mario.y;
                          if (fabsf(dy - 2.0f) > 3.0f) continue;

                          // Check for threats within ~20px
                          if (dx < 0 && dx > -20.0f) {
                              threatsLeft++;
                              if (dx > nearestLeft) nearestLeft = dx;
                          }
                          if (dx >= 0 && dx < 20.0f) {
                              threatsRight++;
                              if (dx < nearestRight) nearestRight = dx;
                          }
                      }
                  }

                  if (threatsLeft > 0 && threatsRight > 0) {
                      // v39.27: SANDWICHED! Don't retreat into the one we cleared.
                      // Vertical Jump to dodge the incoming threat.
                      _mario.state = JUMPING; _mario.vy = kJumpStrength; _mario.jumpFloorY = _mario.y;
                      _mario.vx = 0; // Straight up
                      debugEffect(LOG_MASK_THREATS, "[LANDING] SANDWICHED! Vertical Jump. L:%.1f R:%.1f\n", nearestLeft, nearestRight);

                  } else if (threatsLeft > 0 || threatsRight > 0) {
                       // Single threat direction - Retreat away
                       // v39.24: Panic Jump Fix - Lock Target!
                       float retreatDir = (threatsRight > 0) ? -1.0f : 1.0f; // If threat Right, Go Left

                       _mario.vx = retreatDir * kWalkSpeed * 1.7f;
                       _mario.targetX = _mario.x + (retreatDir * 50.0f);
                       _mario.lastTargetX = _mario.targetX;
                       _mario.targetLockTime = millis();

                       debugEffect(LOG_MASK_THREATS, "[LANDING] Threat! Retreating. x:%.1f vx:%.1f Target:%.1f\n", _mario.x, _mario.vx, _mario.targetX);
                  }
            }
        }
        if (_mario.x < 4.0f) _mario.x = 4.0f;
        if (_mario.x > MATRIX_WIDTH + 4.0f) _mario.x = MATRIX_WIDTH + 4.0f;
    }

    void UpdateEntities() {
        uint32_t nowTime = millis();
        UpdateDK(nowTime);
        UpdateMario(nowTime);
        UpdateBarrels();
    }

    void DrawEntities() {
        float yOffset = (MATRIX_HEIGHT - 32) / 2.0f;
        // DK - Draw palette layers
        for (int r = 0; r < 12; r++) {
            uint16_t furBits = dkFur[_dk.frame][r];
            uint16_t dkTanBits = dkTan[_dk.frame][r];
            for (int c = 0; c < 12; c++) {
                if ((dkTanBits >> (11 - c)) & 1) g()->drawPixelXYF_Wu(_dk.x + c - 6, _dk.y + r - 6 + _dkBreath + yOffset,  _palTan); // Tan skin
                else if ((furBits >> (11 - c)) & 1) g()->drawPixelXYF_Wu(_dk.x + c - 6, _dk.y + r - 6 + _dkBreath + yOffset, _palFur); // Darker Brown
            }
        }
        // Mario - Draw multi-color layers
        // Compress (squash) when grounded if the jump just ended
        float yoff = 0.0f;
        float xscale = 1.0f;
        uint32_t squashAge = millis() - _mario._squashTime;
        if (_mario.state == WALKING && squashAge < 120) {
            if (squashAge < 60) {
                yoff = 1.2f;      // thud
                xscale = 1.3f;    // compress wide
            } else {
                yoff = 0.6f;      // recovery
                xscale = 1.15f;
            }
        }

        for (int r = 0; r < 8; r++) {
            uint8_t rBits = marioRed[_mario.frame][r];
            uint8_t bBits = marioBlue[_mario.frame][r];
            uint8_t sBits = marioSkin[_mario.frame][r];
            for (int c = 0; c < 8; c++) {
                CRGB col = CRGB::Black;
                // Sprite Source assumed LEFT-FACING based on user feedback.
                // If vx > 0 (Right), faceLeft=false. We want RIGHT. If source is LEFT, we must FLIP (7-c).
                // If vx < 0 (Left), faceLeft=true. We want LEFT. If source is LEFT, we use NORMAL (c).
                int colIdx = _mario.faceLeft ? c : (7 - c);
                if ((rBits >> (7 - colIdx)) & 1) col = _palRed;
                else if ((bBits >> (7 - colIdx)) & 1) col = _palBlue;
                else if ((sBits >> (7 - colIdx)) & 1) col = _palMarioSkin;

                if (col != CRGB::Black) {
                    g()->drawPixelXYF_Wu(_mario.x + (c - 4) * xscale,
                                         _mario.y + (r - 4) + yoff + yOffset, col);
                }
            }
        }

    }

    void UpdateBarrels() {
        for (auto& b : _barrels) {
            if (!b.active) continue;
            // v22: Verbose Logging as requested
            debugEffect(LOG_MASK_OBJECTS, "B: x%.1f y%.1f vx%.1f vy%.1f\n", b.x, b.y, b.vx, b.vy);

            b.x += b.vx;
            b.y += b.vy;

            // v14 Physics: Use GetFloorY helpers for slope targets
            // To roll ON top of line: Target Y = FloorY - 2.0f. (Bottom row at FloorY-1, Line at FloorY)

            // Current Tier Logic (simplified vs FSM for brevity, but kept structure)
            if (b.y < kT3Y + 1.0f && b.vx > 0) { // T3 Slope
                 b.y = GetFloorY(b.x, 3) - 2.0f;
                 if (b.x >= MATRIX_WIDTH - 8) { b.x = MATRIX_WIDTH - 8; b.vx = 0; b.vy = 0.5f; }
            }
            else if (b.vx == 0 && b.vy > 0 && b.y < 22) { // Ladder T3->T2
                 // v23: Add X-bound (>32) to prevent catching L1 drops (x=8)!
                 if (b.x > kLadder2X && b.y >= 20.0f) { b.y = 20.0f; b.vx = -0.5f; b.vy = 0; }
            }
            else if (b.vx < 0 && b.y < 24) { // T2 Slope
                 b.y = GetFloorY(b.x, 2) - 2.0f;
                 if (b.x <= 10) {
                     b.x = 8.0f; b.vx = 0; b.vy = 0.5f;
                     // debugEffect("Barrel hitting L1 descent! Snap to 8.\n"); // v19: Silenced
                 }
            }
            else if (b.vx == 0 && b.vy > 0 && b.y < 28) { // Ladder T2->T1
                 // Target T1 Top. v20: Add X Check to prevent catching T1->Gnd drops!
                 if (b.x < 16 && b.y >= 26.0f) { b.y = 26.0f; b.vx = 0.5f; b.vy = 0; }
            }
            else if (b.vx > 0 && b.y < 30) { // T1 Slope
                 b.y = GetFloorY(b.x, 1) - 2.0f;
                 if (b.x >= MATRIX_WIDTH - 16) { b.x = MATRIX_WIDTH - 16; b.vx = 0; b.vy = 0.5f; }
            }
            else if (b.vx == 0 && b.vy > 0 && b.y >= kT1Y + 0.5f) { // Ladder T1->Ground
                 if (b.x > kLadder2X && b.y >= 30.0f) {
                     b.y = 30.0f;
                     b.vx = -0.6f; // v15: Always roll LEFT on ground to use full floor
                     b.vy = 0;
                 }
            } else if (b.y >= 30.0f) { // Ground Roll
                 b.y = GetFloorY(b.x, 0) - 2.0f;
                 if (b.x < 2) b.active = false; // Despawn at left edge
            }

            if (b.x < -10 || b.x > MATRIX_WIDTH + 10) b.active = false;
        }
    }

    void DrawBarrels() {
        // Simple rolling phase: offset rows to simulate rotation
        // v39.4: Freeze animation on win
        int barrelPhase = (_winTime > 0) ? 0 : (frameTime >> 5) & 1;
        const int phase = barrelPhase ? 1 : 0;
        float yOffset = (MATRIX_HEIGHT - 32) / 2.0f;

        for (const auto& b : _barrels) {
            if (!b.active) continue;
            // Draw 4x3 bitmap centered on b.x, b.y
            // 4x3 bitmap:
            // .XX.
            // XXXX
            // .XX.
            CRGB col = b.isBlue ? CRGB::Blue : _palBarrel;

            // Row 0 (.XX.) - biased by phase
            g()->drawPixelXYF_Wu(b.x - (1 + barrelPhase), b.y - 1 + yOffset, col);
            g()->drawPixelXYF_Wu(b.x      + barrelPhase,  b.y - 1 + yOffset, col);

            // Row 1 (XXXX) - static (axle)
            g()->drawPixelXYF_Wu(b.x - 2, b.y + yOffset,     col);
            g()->drawPixelXYF_Wu(b.x - 1, b.y + yOffset,     col);
            g()->drawPixelXYF_Wu(b.x,     b.y + yOffset,     col);
            g()->drawPixelXYF_Wu(b.x + 1, b.y + yOffset,     col);

            // Row 2 (.XX.) - biased counter-phase
            g()->drawPixelXYF_Wu(b.x - (1 + (1 - barrelPhase)), b.y + 1 + yOffset, col);
            g()->drawPixelXYF_Wu(b.x      + (1 - barrelPhase),  b.y + 1 + yOffset, col);
        }
    }
};

#endif // PATTERN_CG_KONG_H
