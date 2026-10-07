#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace mc::world {

// Daylight cycle (wiki: Daylight cycle). A day is 24000 ticks (20 minutes); time 0 is
// sunrise, 6000 noon, 12000 sunset, 18000 midnight. The game's day time advances by
// one each tick.
inline constexpr int64_t kTicksPerDay = 24000;

// Celestial angle in [0, 1): 0 = sun at the zenith (noon), 0.5 = midnight. The sun
// spends a little longer above the horizon than below it: the linear day fraction is
// eased by a cosine term (days ~ 13000 ticks of sun, nights ~ 11000).
inline double celestialAngle(int64_t dayTime, double partialTick = 0.0) {
    const double t = static_cast<double>(dayTime % kTicksPerDay) + partialTick;
    double f = t / static_cast<double>(kTicksPerDay) - 0.25;
    f -= std::floor(f);
    const double eased = 0.5 - std::cos(f * std::numbers::pi) / 2.0;
    return (f * 2.0 + eased) / 3.0;
}

// Daylight factor 0 (night) .. 1 (day): scales the sky colour.
inline double daylight(double angle) {
    const double v = std::cos(angle * 2.0 * std::numbers::pi) * 2.0 + 0.5;
    return std::clamp(v, 0.0, 1.0);
}

// Sky light lost at this angle: 0 by day .. 11 at night (sky light 15 -> 4).
// Rain and thunder each dim the daylight by up to 5/16 (wiki: Weather; 0..1 strengths).
inline double skyDarken(double angle, double rain = 0.0, double thunder = 0.0) {
    return (1.0 - daylight(angle) * (1.0 - rain * 5.0 / 16.0) * (1.0 - thunder * 5.0 / 16.0)) * 11.0;
}

// Star brightness 0 .. 0.5, only once the sky is dark (our estimate: stars appear
// toward the end of sunset and are brightest at night, wiki: Daylight cycle).
inline double starBrightness(double angle) {
    const double v = 1.0 - (std::cos(angle * 2.0 * std::numbers::pi) * 2.0 + 0.25);
    const double c = std::clamp(v, 0.0, 1.0);
    return c * c * 0.5;
}

// Internal (integer) sky light 4..15 used by game logic (mob spawning, daylight
// sensors; wiki: Light › Internal sky light table).
inline int internalSkyLight(int64_t dayTime) {
    return 15 - static_cast<int>(skyDarken(celestialAngle(dayTime)));
}

// Direction to the sun (unit vector; the moon is opposite): it rises in the east
// (+X), crosses the zenith at noon and sets in the west (-X), circling about the
// north-south axis.
struct SkyDirection {
    double x, y, z;
};
inline SkyDirection sunDirection(double angle) {
    const double t = angle * 2.0 * std::numbers::pi;
    return {-std::sin(t), std::cos(t), 0.0};
}

// The sunrise/sunset glow (wiki: Daylight cycle; vanilla's sunrise colour curve): only
// while the sun is within cos(angle) of +-0.4 of the horizon. r, g, b and an alpha
// that peaks as the sun crosses the horizon; alpha 0 = no glow.
struct SunriseColor {
    float r = 0, g = 0, b = 0, a = 0;
};
inline SunriseColor sunriseColor(double angle) {
    const double c = std::cos(angle * 2.0 * std::numbers::pi);
    if (c < -0.4 || c > 0.4) return {};
    const double f = c / 0.4 * 0.5 + 0.5; // 0 below the horizon .. 1 above
    double a = 1.0 - (1.0 - std::sin(f * std::numbers::pi)) * 0.99;
    a *= a;
    return {float(f * 0.3 + 0.7), float(f * f * 0.7 + 0.2), 0.2f, float(a)};
}

// Moon phase 0..7 (0 = full moon), one step per day.
inline int moonPhase(int64_t dayTime) {
    return static_cast<int>(((dayTime / kTicksPerDay) % 8 + 8) % 8);
}

} // namespace mc::world
