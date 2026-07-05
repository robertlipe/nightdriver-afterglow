#pragma once

#include <deque>
#include <vector>
#include <ctime>
#include <array>

#include "globals.h"
#include "ledstripeffect.h"

// Tetris Clock for 64x32 Matrix
// Architecture: "The Assembler"
// Uses "Construction Primitives" shapes to build legible digits.

#define MAX_GRID_W 64
#define MAX_GRID_H 32
#define BLOCK_SIZE 3

// Tetromino Types
enum TetrominoType { T_I=0, T_J, T_L, T_O, T_S, T_T, T_Z, T_NONE };

// Colors (Standard Tetris Colors)
static const CRGB TETRIS_COLORS[] = {
    CRGB::Cyan,
    CRGB::Blue,
    CRGB::Orange,
    CRGB::Yellow,
    CRGB::Green,
    CRGB::Purple,
    CRGB::Red,
    CRGB::Black // None
};

struct Tetromino {
    TetrominoType type;
    int x;          // Grid X
    float y;        // Grid Y
    int rotation;   // Current Rotation
    int targetRotation; // Final Rotation
    int targetY;    // Stop Y
    bool settled;   // Locked?
};

struct TetrisPoint { int x, y; };

// Tetromino Definitions: "Construction Set" Mode
// To ensure legibility, we redefine standard shapes into useful primitives for digit construction.
// Colors correspond to TetrominoType indices: 0=Cyan, 1=Blue, 2=Orange, 3=Yellow, 4=Green, 5=Purple, 6=Red.
static const TetrisPoint TetrominoDefinitions[7][4][4] = {
    // 0: Cyan (I) -> Line 4
    {
        {{0,0}, {1,0}, {2,0}, {3,0}}, // Horiz
        {{0,0}, {0,1}, {0,2}, {0,3}}, // Vert
        {{0,0}, {1,0}, {2,0}, {3,0}},
        {{0,0}, {0,1}, {0,2}, {0,3}}
    },
    // 1: Blue (J) -> Line 3
    {
        {{0,0}, {1,0}, {2,0}, {0,0}}, // Horiz 3
        {{0,0}, {0,1}, {0,2}, {0,0}}, // Vert 3
        {{0,0}, {1,0}, {2,0}, {0,0}},
        {{0,0}, {0,1}, {0,2}, {0,0}}
    },
    // 2: Orange (L) -> Line 2
    {
        {{0,0}, {1,0}, {0,0}, {0,0}}, // Horiz 2
        {{0,0}, {0,1}, {0,0}, {0,0}}, // Vert 2
        {{0,0}, {1,0}, {0,0}, {0,0}},
        {{0,0}, {0,1}, {0,0}, {0,0}}
    },
    // 3: Yellow (O) -> Box 2x2
    {
        {{0,0}, {1,0}, {0,1}, {1,1}},
        {{0,0}, {1,0}, {0,1}, {1,1}},
        {{0,0}, {1,0}, {0,1}, {1,1}},
        {{0,0}, {1,0}, {0,1}, {1,1}}
    },
    // 4: Green (S) -> Dot 1x1
    {
        {{0,0}, {0,0}, {0,0}, {0,0}}, // Just 0,0 repeated
        {{0,0}, {0,0}, {0,0}, {0,0}},
        {{0,0}, {0,0}, {0,0}, {0,0}},
        {{0,0}, {0,0}, {0,0}, {0,0}}
    },
    // 5: Purple (T) -> L-Corner 3-blocks (2x2 minus 1)
    {
        {{0,0}, {1,0}, {0,1}, {0,0}}, // Top-Left + Right + Down (Corner at 0,0)
        {{0,0}, {1,0}, {0,1}, {0,0}},
        {{0,0}, {1,0}, {0,1}, {0,0}},
        {{0,0}, {1,0}, {0,1}, {0,0}}
    },
    // 6: Red (Z) -> Line 2 (Same as Orange, for variety)
    {
        {{0,0}, {1,0}, {0,0}, {0,0}},
        {{0,0}, {0,1}, {0,0}, {0,0}},
        {{0,0}, {1,0}, {0,0}, {0,0}},
        {{0,0}, {0,1}, {0,0}, {0,0}}
    }
};

struct BlueprintStep {
    TetrominoType type; // 0=I(4), 1=J(3), 2=L(2), 3=O(2x2), 4=S(1), 5=T(Corn), 6=Z(2)
    int targetX;
    int targetY;
    int rotation; // 0=Horiz, 1=Vert
};

class PatternCGTetris : public EffectWithId<PatternCGTetris> {
private:
    uint8_t grid[MAX_GRID_W][MAX_GRID_H]; // Stores ColorIndex + 1 (0=Empty)
    int gridW; // Calculated at runtime
    int gridH;

    std::vector<Tetromino> fallingPieces;
    int lastMinute = -1;

    // State
    enum State { IDLE, CLEARING, SPAWNING };
    State currentState = IDLE;
    int stateTimer = 0;

    // Digit Construction Queue
    std::deque<BlueprintStep> buildQueue;

    struct BlueprintSpan {
        const BlueprintStep* ptr;
        size_t count;
        const BlueprintStep* begin() const { return ptr; }
        const BlueprintStep* end() const { return ptr + count; }
    };

    // Blueprint definitions (Clean, legible digits 3 wide x 7 high)
    // Uses simplified shapes for construction.
    static BlueprintSpan GetBlueprint(int digit) {
        static const BlueprintStep d0[] = {
            {T_J, 0, 0, 1}, {T_J, 0, 4, 1}, {T_S, 0, 3, 0}, {T_J, 2, 0, 1},
            {T_J, 2, 4, 1}, {T_S, 2, 3, 0}, {T_S, 1, 0, 0}, {T_S, 1, 6, 0}
        };
        static const BlueprintStep d1[] = {
            {T_I, 1, 0, 1}, {T_J, 1, 4, 1}
        };
        static const BlueprintStep d2[] = {
            {T_L, 0, 0, 0}, {T_S, 2, 0, 0}, {T_Z, 2, 1, 1}, {T_L, 0, 3, 0},
            {T_S, 2, 3, 0}, {T_Z, 0, 4, 1}, {T_J, 0, 6, 0}
        };
        static const BlueprintStep d3[] = {
            {T_J, 2, 0, 1}, {T_J, 2, 4, 1}, {T_S, 2, 3, 0}, {T_L, 0, 0, 0},
            {T_Z, 0, 3, 0}, {T_L, 0, 6, 0}
        };
        static const BlueprintStep d4[] = {
            {T_J, 0, 0, 1}, {T_S, 1, 3, 0}, {T_I, 2, 0, 1}, {T_J, 2, 4, 1}
        };
        static const BlueprintStep d5[] = {
            {T_J, 0, 0, 0}, {T_Z, 0, 1, 1}, {T_J, 0, 3, 0}, {T_Z, 2, 4, 1}, {T_J, 0, 6, 0}
        };
        static const BlueprintStep d6[] = {
            {T_I, 0, 0, 1}, {T_J, 0, 4, 1}, {T_L, 1, 0, 0}, {T_Z, 1, 3, 0},
            {T_L, 1, 6, 0}, {T_Z, 2, 4, 1}
        };
        static const BlueprintStep d7[] = {
            {T_J, 0, 0, 0}, {T_I, 2, 1, 1}, {T_Z, 2, 5, 1}
        };
        static const BlueprintStep d8[] = {
            {T_J, 0, 0, 1}, {T_J, 0, 4, 1}, {T_J, 2, 0, 1}, {T_J, 2, 4, 1},
            {T_S, 1, 0, 0}, {T_S, 1, 3, 0}, {T_S, 1, 6, 0}
        };
        static const BlueprintStep d9[] = {
            {T_J, 0, 0, 0}, {T_L, 0, 1, 1}, {T_Z, 2, 1, 1}, {T_J, 0, 3, 0}, {T_I, 2, 3, 1}
        };

        static const BlueprintSpan blueprints[] = {
            {d0, sizeof(d0)/sizeof(d0[0])}, {d1, sizeof(d1)/sizeof(d1[0])},
            {d2, sizeof(d2)/sizeof(d2[0])}, {d3, sizeof(d3)/sizeof(d3[0])},
            {d4, sizeof(d4)/sizeof(d4[0])}, {d5, sizeof(d5)/sizeof(d5[0])},
            {d6, sizeof(d6)/sizeof(d6[0])}, {d7, sizeof(d7)/sizeof(d7[0])},
            {d8, sizeof(d8)/sizeof(d8[0])}, {d9, sizeof(d9)/sizeof(d9[0])}
        };

        if (digit < 0 || digit > 9) return blueprints[0];
        return blueprints[digit];
    }

    int GetDigitOffset(int pos) {
        int totalWidth = 16;
        int startX = (gridW - totalWidth) / 2;
        if (startX < 0) startX = 0;

        switch(pos) {
            case 0: return startX + 0;
            case 1: return startX + 4;
            case 2: return startX + 9;
            case 3: return startX + 13;
        }
        return 0;
    }

    // Bottom alignment for max fall height
    int GetDigitOffsetY() {
        int contentH = 7;
        int y = gridH - contentH; // Flush Bottom
        return (y < 0) ? 0 : y;
    }

    // Helper: Get shapes (4 coordinates for each rotation)
    std::array<TetrisPoint, 4> GetPoints(const Tetromino& p) {
         std::array<TetrisPoint, 4> points;

         const auto& shape = TetrominoDefinitions[p.type][p.rotation % 4];
         for (int i = 0; i < 4; i++) {
             points[i] = {p.x + shape[i].x, (int)p.y + shape[i].y};
         }
         return points;
    }

    void ClearGrid() {
        memset(grid, 0, sizeof(grid));
    }

    // Optimized bounds check
    void DrawBlock(int gx, int gy, uint8_t colorIdx) {
        if ((unsigned)gx >= gridW || (unsigned)gy >= gridH) return;

        int yOffset = (MATRIX_HEIGHT - 32) / 2;
        int x = gx * BLOCK_SIZE;
        int y = gy * BLOCK_SIZE + yOffset;

        // Block Size 3: Draws pixels 0, 1. Gap at 2.
        int w = BLOCK_SIZE;
        int h = BLOCK_SIZE;

        g()->fillRectangle(x, y, x + w, y + h, TETRIS_COLORS[colorIdx]);
    }

    int currentMinute() {
        time_t now;
        time(&now);
        struct tm local_buf;
        struct tm* local = localtime_r(&now, &local_buf);
        return local->tm_min;
    }

public:
    PatternCGTetris() : EffectWithId<PatternCGTetris>("Tetris Clock") {}
    PatternCGTetris(const JsonObjectConst& json) : EffectWithId<PatternCGTetris>(json) {}

    void Start() override {
        // Calculate Grid Definition from Matrix Size
        if (g()->width() > 0 && BLOCK_SIZE > 0) {
            gridW = g()->width() / BLOCK_SIZE;
            gridH = 32 / BLOCK_SIZE;
        }
        if (gridW > MAX_GRID_W) gridW = MAX_GRID_W;
        if (gridH > MAX_GRID_H) gridH = MAX_GRID_H;

        ClearGrid();
        fallingPieces.clear();
        currentState = IDLE;
        lastMinute = -1;
    }

    // Animation Constants
    static constexpr float kFallSpeed = 0.20f; // Grid units per frame
    static constexpr int kSpawnDelay = 20;     // Frames between piece spawns
    static constexpr int kMeltInterval = 2;    // Frames between grid melt shifts
    static constexpr int kSpinInterval = 20;   // Frames between rotation ticks

    void QueueDigit(int digit, int pos) {
        int offsetX = GetDigitOffset(pos);
        int offsetY = GetDigitOffsetY();
        const auto& steps = GetBlueprint(digit);
        for (const auto& step : steps) {
            BlueprintStep s = step;
            s.targetX += offsetX;
            s.targetY += offsetY;
            buildQueue.push_back(s);
        }
    }

    void Draw() override {
        // Time Check
        int min = currentMinute();
        if (min != lastMinute) {
            currentState = CLEARING;
            stateTimer = 0;
            fallingPieces.clear(); // Remove falling pieces so we only animate the grid
            lastMinute = min;
        }

        // Logic
        switch (currentState) {
            case CLEARING:
                // Animation: Melt/Fall through floor
                // Shift grid down every kMeltInterval frames
                if (stateTimer++ % kMeltInterval == 0) {
                     for (int x = 0; x < gridW; x++) {
                         for (int y = gridH - 1; y > 0; y--) {
                             grid[x][y] = grid[x][y-1];
                         }
                         grid[x][0] = 0; // Clear top
                     }
                }

                // Done when enough time passed for full height to clear
                if (stateTimer > (gridH * kMeltInterval + 10)) {
                    ClearGrid(); // Ensure perfectly clean
                    currentState = SPAWNING;
		    stateTimer = 0;

                    // Queue up the time
                    time_t now;
                    time(&now);
                    struct tm local_buf;
                    struct tm* local = localtime_r(&now, &local_buf);
                    int h = local->tm_hour;
                    int m = local->tm_min;

                    QueueDigit(h / 10, 0);
                    QueueDigit(h % 10, 1);
                    QueueDigit(m / 10, 2);
                    QueueDigit(m % 10, 3);
                }
                break;

            case SPAWNING:
                // Drop one piece every N frames
                if (stateTimer++ % kSpawnDelay == 0 && !buildQueue.empty()) {
                    BlueprintStep step = buildQueue.front();
                    buildQueue.pop_front();
                    // Random start rotation
                    int startRot = random(4);
                    Tetromino newPiece = { step.type, step.targetX, -4.0f, startRot, step.rotation, step.targetY, false };
                    fallingPieces.push_back(newPiece);
                }
                if (fallingPieces.empty() && buildQueue.empty()) currentState = IDLE;
                break;

            case IDLE:
                break;
        }

        // Physics (Gravity & Rotation)
        for (auto& p : fallingPieces) {
            if (p.settled) continue;

            float newY = p.y + kFallSpeed;

            // Spin animation: Rotate towards target
            float dist = p.targetY - p.y;
            if (dist > 2.0f) {
                // Spin towards target every few frames
                if ((int)(p.y * 10) % kSpinInterval == 0) {
                     if (p.rotation != p.targetRotation) {
                         p.rotation = (p.rotation + 1) % 4;
                     }
                }
            } else {
                p.rotation = p.targetRotation; // Snap
            }

            // Check Collision (Floor mostly)
            if (newY >= p.targetY) {
                p.y = (float)p.targetY;
                p.rotation = p.targetRotation;
                p.settled = true;

                // Lock into grid
                auto pts = GetPoints(p);
                for (const auto& pt : pts) {
                    if (pt.x >= 0 && pt.x < gridW && pt.y >= 0 && pt.y < gridH) {
                        grid[pt.x][pt.y] = p.type + 1;
                    }
                }
            } else {
                p.y = newY;
            }
        }

        // Render
        g()->fillScreen(0);

        // Draw Grid
        for (int x = 0; x < gridW; x++) {
            for (int y = 0; y < gridH; y++) {
                if (grid[x][y] > 0) {
                    DrawBlock(x, y, grid[x][y] - 1);
                }
            }
        }

        // Draw Falling
        for (const auto& p : fallingPieces) {
            if (!p.settled) {
                auto pts = GetPoints(p);
                for (const auto& pt : pts) {
                    DrawBlock(pt.x, pt.y, p.type);
                }
            }
        }
    }
};
