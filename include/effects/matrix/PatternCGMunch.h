#pragma once

#include <bitset>

/**
 * PatternCGMunch: A Pac-Man inspired clock for 64x32 LED matrices.
 * Features grid-locked movement, scent-trail ghost AI, and 3x5 font.
 */

enum class MunchDir : uint8_t { UP, RIGHT, DOWN, LEFT, NONE };

struct MunchEntity {
    int32_t x, y;
    MunchDir dir;
};

class PatternCGMunch : public EffectWithId<PatternCGMunch> {
private:
    MunchEntity munch;
    MunchEntity blinky;

    // Trail of recent logical positions for the ghost to follow
    int16_t trailX[12], trailY[12];
    int trailIdx = 0;
    int lastMin = -1;

    static constexpr int MAZE_W = 28;
    static constexpr int MAZE_H = 31;
    static constexpr int SCALE  = 256;

    // Entity Speeds (scaled)
    static constexpr int32_t kMunchSpeed  = 64;
    static constexpr int32_t kBlinkySpeed = 48;

    // Maze Geometry Constants
    static constexpr int kHouseTop = 12;
    static constexpr int kHouseBot = 16;
    static constexpr int kHouseL   = 10;
    static constexpr int kHouseR   = 17;

    std::bitset<MAZE_W * MAZE_H> pellets;

    static constexpr uint32_t mazeMask[31] = {
        0xFFFFFFF, 0x8000001, 0x8EEEEE1, 0x8EEEEE1, 0x8000001,
        0x8EEEEE1, 0x8E888E1, 0x8E888E1, 0x8000001, 0xFFF8FFF,
        0x0004000, 0x0004000, 0x8000001, 0x8E000E1, 0x8E000E1,
        0x8E000E1, 0x8000001, 0x8EEEE01, 0x800E001, 0x8EEBE01,
        0x8000001, 0x8EEEEE1, 0x800E001, 0x8EEBE01, 0x8000001,
        0x800E001, 0x8EEBE01, 0x8000001, 0x8EEEEE1, 0x8000001,
        0xFFFFFFF
    };

    static constexpr uint8_t font3x5[10][5] = {
        {0b111, 0b101, 0b101, 0b101, 0b111}, {0b010, 0b110, 0b010, 0b010, 0b111},
        {0b111, 0b001, 0b111, 0b100, 0b111}, {0b111, 0b001, 0b111, 0b001, 0b111},
        {0b101, 0b101, 0b111, 0b001, 0b001}, {0b111, 0b100, 0b111, 0b001, 0b111},
        {0b111, 0b100, 0b111, 0b101, 0b111}, {0b111, 0b001, 0b010, 0b100, 0b100},
        {0b111, 0b101, 0b111, 0b101, 0b111}, {0b111, 0b101, 0b111, 0b001, 0b111}
    };

    bool isWall(int x, int y) const {
        if (y < 0 || y >= MAZE_H) return true;
        // The wrap-around tunnel is only allowed at the equator
        bool isTunnelRow = (y >= 9 && y <= 11);
        int lx = (x + MAZE_W) % MAZE_W;
        if (!isTunnelRow && (x < 0 || x >= MAZE_W)) return true;
        return (mazeMask[y] >> (27 - lx)) & 1;
    }

    void ResetPellets() {
        pellets.reset();
        for (unsigned int y = 0; y < MAZE_H; y++) {
            for (unsigned int x = 0; x < MAZE_W; x++) {
                if (!isWall(x, y)) pellets.set(y * MAZE_W + x);
            }
        }
    }

    void DrawDigit(int x, int y, unsigned int digit, uint16_t color) const {
        if (digit > 9) return;
        for (unsigned int r = 0; r < 5; r++) {
            uint8_t bits = font3x5[digit][r];
            for (unsigned int c = 0; c < 3; c++) {
                if ((bits >> (2 - c)) & 1) g()->setPixel(x + c, y + r, color);
            }
        }
    }

public:
    PatternCGMunch() : EffectWithId<PatternCGMunch>("Pac-Clock") { ResetGame(); }
    PatternCGMunch(const JsonObjectConst& json) : EffectWithId<PatternCGMunch>(json) { ResetGame(); }

    void ResetGame() {
        munch.x = 14 * SCALE; munch.y = 23 * SCALE; munch.dir = MunchDir::LEFT;
        blinky.x = 14 * SCALE; blinky.y = 14 * SCALE; blinky.dir = MunchDir::UP;
        for (unsigned int i = 0; i < 12; i++) { trailX[i] = 14; trailY[i] = 23; }
        trailIdx = 0;
        ResetPellets();
    }

    void UpdateMunch() {
        int tx = munch.x / SCALE, ty = munch.y / SCALE;

        if ((munch.x % SCALE == 0) && (munch.y % SCALE == 0)) {
            pellets.reset(ty * MAZE_W + tx);
            trailX[trailIdx] = (int16_t)tx; trailY[trailIdx] = (int16_t)ty;
            trailIdx = (trailIdx + 1) % 12;

            MunchDir options[4]; unsigned int count = 0;
            if (!isWall(tx, ty - 1)) options[count++] = MunchDir::UP;
            if (!isWall(tx + 1, ty)) options[count++] = MunchDir::RIGHT;
            if (!isWall(tx, ty + 1)) options[count++] = MunchDir::DOWN;
            if (!isWall(tx - 1, ty)) options[count++] = MunchDir::LEFT;

            bool straight = false;
            for (unsigned int i = 0; i < count; i++) if (options[i] == munch.dir) straight = true;

            if (straight && count <= 2 && random(100) < 85) {
                // Hallway Persistence
            } else if (count > 0) {
                // Avoid reversing direction unless necessary
                static constexpr MunchDir opposites[] = { MunchDir::DOWN, MunchDir::LEFT, MunchDir::UP, MunchDir::RIGHT, MunchDir::NONE };
                MunchDir choices[4]; unsigned int cCount = 0;
                for (unsigned int i = 0; i < count; i++)
                    if (options[i] != opposites[static_cast<int>(munch.dir)]) choices[cCount++] = options[i];

                if (cCount > 0) munch.dir = choices[random(cCount)];
                else munch.dir = options[0];
            }
        }
        // Convert direction enum to delta movement: clever but needs documentation
        // RIGHT=1 gives dx=1, LEFT=3 gives dx=-1, UP=0/DOWN=2 give dx=0
        int dx = (munch.dir == MunchDir::RIGHT) - (munch.dir == MunchDir::LEFT);
        int dy = (munch.dir == MunchDir::DOWN) - (munch.dir == MunchDir::UP);
        if (!isWall(tx + dx, ty + dy)) { munch.x += dx * kMunchSpeed; munch.y += dy * kMunchSpeed; }
        else { munch.dir = static_cast<MunchDir>(random(4)); }

        if (munch.x < 0) munch.x = (MAZE_W - 1) * SCALE;
        if (munch.x >= MAZE_W * SCALE) munch.x = 0;
    }

    void UpdateBlinky() {
        int tx = blinky.x / SCALE, ty = blinky.y / SCALE;
        // Collision: Return to house if caught
        if (std::abs(blinky.x - munch.x) < 128 && std::abs(blinky.y - munch.y) < 128) {
            blinky.x = 14 * SCALE; blinky.y = 14 * SCALE; return;
        }

        if ((blinky.x % SCALE == 0) && (blinky.y % SCALE == 0)) {
            int tIdx = (trailIdx + 6) % 12;
            int targetTX = trailX[tIdx], targetTY = trailY[tIdx];
            if (std::abs(tx - targetTX) > 10) { targetTX = 14; targetTY = 14; }
            MunchDir options[4]; unsigned int count = 0;
            if (!isWall(tx, ty - 1)) options[count++] = MunchDir::UP;
            if (!isWall(tx + 1, ty)) options[count++] = MunchDir::RIGHT;
            if (!isWall(tx, ty + 1)) options[count++] = MunchDir::DOWN;
            if (!isWall(tx - 1, ty)) options[count++] = MunchDir::LEFT;
            if (count > 0) {
                MunchDir bestDir = options[0]; int32_t minDist = 100000;
                static constexpr MunchDir opposites[] = { MunchDir::DOWN, MunchDir::LEFT, MunchDir::UP, MunchDir::RIGHT, MunchDir::NONE };
                for (unsigned int i = 0; i < count; i++) {
                    if (count > 1 && options[i] == opposites[static_cast<int>(blinky.dir)]) continue;
                    // Convert direction to delta using the same clever trick as UpdateMunch
                    int nx = tx + (options[i] == MunchDir::RIGHT) - (options[i] == MunchDir::LEFT);
                    int ny = ty + (options[i] == MunchDir::DOWN) - (options[i] == MunchDir::UP);
                    int32_t d = std::abs(nx - targetTX) + std::abs(ny - targetTY);
                    if (d < minDist) { minDist = d; bestDir = options[i]; }
                }
                blinky.dir = bestDir;
            }
        }
        int dx = (blinky.dir == MunchDir::RIGHT) - (blinky.dir == MunchDir::LEFT);
        int dy = (blinky.dir == MunchDir::DOWN) - (blinky.dir == MunchDir::UP);
        if (!isWall(tx + dx, ty + dy)) { blinky.x += dx * kBlinkySpeed; blinky.y += dy * kBlinkySpeed; }
        else { blinky.dir = static_cast<MunchDir>(random(4)); }
        if (blinky.x < 0) blinky.x = (MAZE_W - 1) * SCALE;
        if (blinky.x >= MAZE_W * SCALE) blinky.x = 0;
    }

    void Draw() override {
        g()->fillScreen(0);
        auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        struct tm tm_buf;
        struct tm* t = localtime_r(&now_t, &tm_buf);
        if (t && t->tm_min != lastMin) { if (lastMin != -1) ResetGame(); lastMin = t->tm_min; }
        UpdateMunch(); UpdateBlinky(); DrawMaze(t);
        DrawEntity(munch, (uint16_t)0xFFE0, true);  // Yellow
        DrawEntity(blinky, (uint16_t)0xF800, false); // Red
    }

    void DrawMaze(struct tm* t) {
        const unsigned int xOff = 4, yOff = (MATRIX_HEIGHT - MAZE_H) / 2;
        for (unsigned int ty = 0; ty < MAZE_H; ty++) {
            uint32_t row = mazeMask[ty];
            for (unsigned int tx = 0; tx < MAZE_W; tx++) {
                unsigned int px = xOff + (tx * 2);
                bool isHouse = (ty >= kHouseTop && ty <= kHouseBot && tx >= kHouseL && tx <= kHouseR);
                if ((row >> (27 - tx)) & 1) {
                    g()->setPixel(px, yOff + ty, (uint16_t)0x001F); // Blue
                    if (!isHouse) g()->setPixel(px + 1, yOff + ty, (uint16_t)0x001F);
                } else if (!isHouse && pellets.test(ty * MAZE_W + tx) && (tx % 2 == 0) && (ty % 2 == 0)) {
                    g()->setPixel(px + 1, yOff + ty, (uint16_t)0x7BEF); // Gray
                }
            }
        }
        if (t) {
            int h = t->tm_hour % 12; if (h == 0) h = 12;
            int x = (h >= 10) ? 22 : 24; int y = yOff + 12;
            uint16_t cyan = 0x07FF;
            if (h >= 10) { DrawDigit(x, y, 1, cyan); x += 4; }
            DrawDigit(x, y, h % 10, cyan);
            // Colon pulse
            if ((millis() / 500) % 2) {
                g()->setPixel(x + 4, y + 1, (uint16_t)0xFFFF); g()->setPixel(x + 4, y + 3, (uint16_t)0xFFFF);
            }
            DrawDigit(x + 6, y, t->tm_min / 10, cyan); DrawDigit(x + 10, y, t->tm_min % 10, cyan);
        }
    }

    void DrawEntity(MunchEntity& e, uint16_t color, bool animate) {
        unsigned int xOff = 4, yOff = (MATRIX_HEIGHT - MAZE_H) / 2;
        unsigned int sx = xOff + ((e.x / SCALE) * 2), sy = yOff + (e.y / SCALE);
        bool open = animate && ((millis() / 150) % 2 == 0);
        safeSet(sx, sy, color); safeSet(sx+1, sy, color);
        safeSet(sx, sy-1, color); safeSet(sx+1, sy-1, color);
        safeSet(sx, sy+1, color); safeSet(sx+1, sy+1, color);
        if (!open || e.dir != MunchDir::LEFT)  safeSet(sx-1, sy, color);
        if (!open || e.dir != MunchDir::RIGHT) safeSet(sx+2, sy, color);
        if (!open || e.dir != MunchDir::UP)    { safeSet(sx, sy-1, color); safeSet(sx+1, sy-1, color); }
        if (!open || e.dir != MunchDir::DOWN)  { safeSet(sx, sy+1, color); safeSet(sx+1, sy+1, color); }
    }

    void safeSet(unsigned int x, unsigned int y, uint16_t c) const {
        if (x < MATRIX_WIDTH && y < MATRIX_HEIGHT) g()->setPixel(x, y, c);
    }
};
