#pragma once

#include <array>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>

struct Brick {
    float x, y;
    uint32_t color;
    bool active;
    bool isRect;  // true for decorative bricks (drawn with fillRect), false for time digits (pixel-level)
};

struct Ball {
    float x, y;
    float vx, vy;
};

struct Paddle {
    float x;
};

class PatternCGBreakout : public EffectWithId<PatternCGBreakout> {
private:
    // Named Constants
    static constexpr float kBallSpeed = 1.2f;
    static constexpr float kPaddleSpeed = 1.5f;
    static constexpr float kPaddleWidth = 8.0f;
    static constexpr float kPaddleY = 28.0f;
    static constexpr float kBallRadius = 0.5f;
    static constexpr float kDecorativeBrickWidth = 5.0f;
    static constexpr float kDecorativeBrickHeight = 2.0f;
    static constexpr float kTimeBrickWidth = 1.0f;
    static constexpr float kTimeBrickHeight = 1.0f;
    static constexpr unsigned int kMissChance = 10; // 10% chance to miss

    std::vector<Brick> bricks;
    Ball ball;
    Paddle paddle;
    int lastMin = -1;
    uint32_t lastMissCheck = 0;
    bool shouldMiss = false;
    float xMin = 0.0f;
    float xMax = MATRIX_WIDTH - 1.0f;

    static constexpr uint32_t font3x5[11][5] = {
        {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
        {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
        {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
        {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
        {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
        {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
        {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
        {0b111, 0b001, 0b010, 0b100, 0b100}, // 7
        {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
        {0b111, 0b101, 0b111, 0b001, 0b111}, // 9
        {0b000, 0b010, 0b000, 0b010, 0b000}  // :
    };

public:
    PatternCGBreakout() : EffectWithId<PatternCGBreakout>("CG Breakout") { ResetGame(); }
    PatternCGBreakout(const JsonObjectConst& json) : EffectWithId<PatternCGBreakout>(json) { ResetGame(); }

    void Start() override { ResetGame(); }

    void ResetGame() {
        bricks.clear();
        paddle.x = MATRIX_WIDTH / 2.0f;
        ball.x = MATRIX_WIDTH / 2.0f;
        ball.y = 20.0f;
        ball.vx = 0.8f;
        ball.vy = -1.0f;
        // Normalize ball velocity
        float speed = sqrtf(ball.vx * ball.vx + ball.vy * ball.vy);
        ball.vx = (ball.vx / speed) * kBallSpeed;
        ball.vy = (ball.vy / speed) * kBallSpeed;
        shouldMiss = false;
        SpawnTimeBricks();
    }

    void SpawnTimeBricks() {
        bricks.clear();
        auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        struct tm tm_buf;
        struct tm* t = localtime_r(&now_t, &tm_buf);
        lastMin = t->tm_min;

        // Classic Breakout colors: Red, Orange, Yellow, Green, Cyan, Blue
        static constexpr uint32_t breakoutColors[] = {
            (uint32_t)CRGB::Red,
            (uint32_t)CRGB::OrangeRed,
            (uint32_t)CRGB::Yellow,
            (uint32_t)CRGB::Green,
            (uint32_t)CRGB::Cyan,
            (uint32_t)CRGB::Blue
        };

        // Brick dimensions (classic Breakout style)
        static constexpr unsigned int kDecoWidth = 5;
        static constexpr unsigned int kDecoGap = 1;
        static constexpr unsigned int kDecoHeight = 2;

        // Calculate brick field centering
        unsigned int numBricks = (MATRIX_WIDTH + kDecoGap) / (kDecoWidth + kDecoGap);
        unsigned int fieldWidth = (numBricks * kDecoWidth) + ((numBricks - 1) * kDecoGap);
        unsigned int startOffsetX = (MATRIX_WIDTH - fieldWidth) / 2;

        // Boundaries for the ball behavior (no-fly zone)
        xMin = startOffsetX;
        xMax = startOffsetX + fieldWidth - 1.0f;

        // Add time digits at the very top (like Pong score)
        // Time digits are stored as individual pixels for collision detection
        std::array digits = {
            (unsigned int)(t->tm_hour/10), (unsigned int)(t->tm_hour%10), 10u,
            (unsigned int)(t->tm_min/10), (unsigned int)(t->tm_min%10)
        };
        unsigned int startX = (MATRIX_WIDTH - (5 * 4)) / 2;
        unsigned int timeY = 0; // Time at very top

        for (unsigned int d = 0; d < 5; d++) {
            for (unsigned int row = 0; row < 5; row++) {
                uint32_t rowBits = font3x5[digits[d]][row];
                for (unsigned int col = 0; col < 3; col++) {
                    if ((rowBits >> (2 - col)) & 1) {
                        float px = startX + (d * 4) + col;
                        float py = timeY + row;
                        uint32_t color = (d < 2) ? (uint32_t)CRGB::Cyan :
                                        (d == 2) ? (uint32_t)CRGB::White :
                                        (uint32_t)CRGB::Green;
                        bricks.push_back({px, py, color, true, false});  // false = pixel-level brick
                    }
                }
            }
        }

        // Add decorative brick rows below time
        unsigned int brickStartY = 6; // Start bricks below time
        for (unsigned int row = 0; row < 4; row++) {
            uint32_t rowColor = breakoutColors[row % 6];
            unsigned int yBase = brickStartY + (row * (kDecoHeight + 1));
            if (yBase < kPaddleY - 5) {
                for (unsigned int bx = 0; bx < numBricks; bx++) {
                    float px = startOffsetX + (bx * (kDecoWidth + kDecoGap));
                    if (yBase + kDecoHeight < kPaddleY - 5) {
                        bricks.push_back({px, (float)yBase, rowColor, true, true});
                    }
                }
            }
        }
    }

    void Draw() override {
        fadeAllChannelsToBlackBy(40);

        // Check for minute change
        auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        struct tm tm_buf;
        if (localtime_r(&now_t, &tm_buf)->tm_min != lastMin) {
            SpawnTimeBricks();
        }

        UpdateBall();
        UpdatePaddle();

        float yOffset = (MATRIX_HEIGHT - 32) / 2.0f;

        // Draw bricks
        for (const auto& brick : bricks) {
            if (brick.active) {
                if (brick.isRect) {
                    // Use fillRectangle for decorative bricks (x0, y0, x1, y1)
                    g()->fillRectangle(brick.x, brick.y + yOffset, brick.x + kDecorativeBrickWidth, brick.y + kDecorativeBrickHeight + yOffset, CRGB(brick.color));
                } else {
                    // Use pixel-level drawing for time digits
                    g()->setPixel(brick.x, brick.y + yOffset, CRGB(brick.color));
                }
            }
        }

        // Draw ball with sub-pixel anti-aliasing
        g()->drawPixelXYF_Wu(ball.x, ball.y + yOffset, CRGB::White);

        // Draw paddle
        auto px = (int16_t)lroundf(paddle.x);
        for (int16_t i = -4; i <= 3; i++) {
            g()->setPixel(px + i, kPaddleY + yOffset, CRGB(CRGB::Red));
        }
    }

    void UpdateBall() {
        ball.x += ball.vx;
        ball.y += ball.vy;

        if (fabsf(ball.vx) < 0.001f) {
            ball.x = MATRIX_WIDTH / 2.0f;
            ball.y = 20.0f;
            ball.vx = (random(100) - 50.0f) / 50.0f;
            ball.vy = -1.0f;
            float speed = sqrtf(ball.vx * ball.vx + ball.vy * ball.vy);
            ball.vx = (ball.vx / speed) * kBallSpeed;
            ball.vy = (ball.vy / speed) * kBallSpeed;
            return;
        }

        // Check if ball is too horizontal (boring trajectory)
        // If |vy| is very small compared to |vx|, respawn the ball
        float slope = fabsf(ball.vy / ball.vx);
        if (slope < 0.3f) { // Slope less than ~17 degrees
            // Ball is too horizontal, force respawn
            ball.x = MATRIX_WIDTH / 2.0f;
            ball.y = 20.0f;
            ball.vx = (random(100) - 50.0f) / 50.0f;
            ball.vy = -1.0f;
            float speed = sqrtf(ball.vx * ball.vx + ball.vy * ball.vy);
            ball.vx = (ball.vx / speed) * kBallSpeed;
            ball.vy = (ball.vy / speed) * kBallSpeed;
            return;
        }

        // Wall collisions (constrained to brickyard width)
        if (ball.x <= xMin || ball.x >= xMax) {
            ball.vx = -ball.vx;
            ball.x = std::clamp(ball.x, xMin, xMax);
        }
        if (ball.y <= 0) {
            ball.vy = -ball.vy;
            ball.y = 0;
        }

        // Paddle collision
        if (ball.y >= kPaddleY - 1 && ball.y <= kPaddleY + 1) {
            if (fabsf(ball.x - paddle.x) <= kPaddleWidth / 2.0f) {
                ball.vy = -fabsf(ball.vy); // Always bounce up
                // Steer angle based on hit position (offset is -1.0 to 1.0)
                float offset = (ball.x - paddle.x) / (kPaddleWidth / 2.0f);
                // Mixed momentum: pull current vx toward zero and apply steering nudge.
                // Center hits (offset=0) now actively steepen the trajectory.
                ball.vx = (ball.vx * 0.2f) + (offset * 1.5f);
                // Renormalize to maintain constant speed
                float speed = sqrtf(ball.vx * ball.vx + ball.vy * ball.vy);
                ball.vx = (ball.vx / speed) * kBallSpeed;
                ball.vy = (ball.vy / speed) * kBallSpeed;
            }
        }

        // Brick collisions
        for (auto& brick : bricks) {
            if (!brick.active) continue;
            float bw = brick.isRect ? kDecorativeBrickWidth  : kTimeBrickWidth;
            float bh = brick.isRect ? kDecorativeBrickHeight : kTimeBrickHeight;
            if (fabsf(ball.x - (brick.x + bw/2.0f)) < bw/2.0f + kBallRadius &&
                fabsf(ball.y - (brick.y + bh/2.0f)) < bh/2.0f + kBallRadius) {
                brick.active = false;
                ball.vy = -ball.vy;
                break;
            }
        }

        // Ball missed - respawn
        if (ball.y > 32.0f) {
            ball.x = MATRIX_WIDTH / 2.0f;
            ball.y = 20.0f;
            ball.vx = (random(100) - 50.0f) / 50.0f;
            ball.vy = -1.0f;
            float speed = sqrtf(ball.vx * ball.vx + ball.vy * ball.vy);
            ball.vx = (ball.vx / speed) * kBallSpeed;
            ball.vy = (ball.vy / speed) * kBallSpeed;
        }
    }

    void UpdatePaddle() {
        // Decide if we should miss this ball (10% chance, check every second)
        if (millis() - lastMissCheck > 1000) {
            shouldMiss = (random(100) < kMissChance);
            lastMissCheck = millis();
        }

        if (!shouldMiss && ball.vy > 0) { // Only track if ball is moving down
            // Add intentional error to paddle tracking
            float error = (random(100) - 50.0f) / 25.0f; // -2 to +2
            float targetX = ball.x + error;

            // Move paddle toward target
            float diff = targetX - paddle.x;
            if (fabsf(diff) > 0.5f) {
                paddle.x += std::clamp(diff, -kPaddleSpeed, kPaddleSpeed);
            }
        }

        paddle.x = std::clamp(paddle.x, kPaddleWidth / 2.0f,
                             MATRIX_WIDTH - kPaddleWidth / 2.0f);
    }


};
