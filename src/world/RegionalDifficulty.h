#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace mc::world {

// Regional difficulty (M32.2; wiki: Regional difficulty, Java Edition): how dangerous a
// place is, from the difficulty, the world's age (0 until day 3, up to 0.25 after ~63
// days), the chunk's inhabited time (up to 1 after 50 hours of players nearby; x0.75
// below Hard) and the moon (its brightness x 0.25, at most the age factor):
// (0.75 + age + chunk + moon) x 1 / 2 / 3 on Easy / Normal / Hard, 0 on Peaceful.
// `difficulty`: 0 Peaceful .. 3 Hard; `dayTime`: the world's total day ticks.
inline double regionalDifficulty(int difficulty, int64_t dayTime, int64_t inhabited,
                                 float moonBrightness) {
    if (difficulty <= 0) return 0.0;
    const double age = std::clamp(double(dayTime - 72000) / 5760000.0, 0.0, 0.25);
    double chunk =
        std::clamp(double(inhabited) / 3600000.0, 0.0, 1.0) * (difficulty >= 3 ? 1.0 : 0.75);
    chunk += std::min(double(moonBrightness) * 0.25, age);
    if (difficulty == 1) chunk *= 0.5; // (wiki: on Easy the chunk and moon parts halve)
    return (0.75 + age + chunk) * double(std::min(difficulty, 3));
}

// The clamped regional difficulty ("special multiplier") that scales mob gear and
// abilities: 0 below 2, 1 above 4, linear between.
inline double clampedRegionalDifficulty(double regional) {
    return regional < 2.0 ? 0.0 : regional > 4.0 ? 1.0 : (regional - 2.0) / 2.0;
}

// The moon's brightness by phase (wiki: Moon, Regional difficulty):
// full 1, then 0.75, 0.5, 0.25, new 0, and back up.
inline float moonBrightness(int64_t dayTime) {
    static constexpr float kPhase[8] = {1.0f, 0.75f, 0.5f, 0.25f, 0.0f, 0.25f, 0.5f, 0.75f};
    return kPhase[size_t(((dayTime / 24000) % 8 + 8) % 8)];
}

} // namespace mc::world
