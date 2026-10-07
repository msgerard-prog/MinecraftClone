#pragma once

#include "world/Random.h"
#include "world/World.h"

#include <cstdint>

namespace mc::world {

// Weather (M22.1; wiki: Weather). Vanilla's cycle: each of rain and thunder has a
// countdown; when it runs out the state flips and a new one starts - rain lasts
// 12,000-23,999 ticks and comes back after 12,000-179,999; thunder lasts 3,600-15,599
// and comes back after 12,000-179,999 (it only shows while it rains). /weather sets a
// state for a time (clearWeatherTime holds clear skies). Rain and thunder fade in and
// out by 0.01 a tick. Saved in level.dat (raining, rainTime, thundering, thunderTime,
// clearWeatherTime).
struct Weather {
    bool raining = false, thundering = false;
    int rainTime = 0, thunderTime = 0, clearTime = 0;
    float rain = 0.0f, prevRain = 0.0f;       // 0..1 strength
    float thunder = 0.0f, prevThunder = 0.0f; // 0..1 (times rain)

    void tick(Xoroshiro& rng);
    enum class Kind { Clear, Rain, Thunder };
    void set(Kind kind, int duration);
    float rainAt(float alpha) const { return prevRain + (rain - prevRain) * alpha; }
    float thunderAt(float alpha) const { return prevThunder + (thunder - prevThunder) * alpha; }
};

// What falls at a block (wiki: Weather › Precipitation): nothing in hot dry biomes
// (deserts, savannas, badlands; temperature 1.5+), snow where the biome temperature,
// lowered by 0.00125 a block above Y 80 (our reading), is below 0.15, else rain.
enum class Precipitation { None, Rain, Snow };
Precipitation precipitationAt(const World& world, const BlockPos& p);
// The first block from the top a raindrop stops on (collides, or a fluid) + 1: rain and
// snow reach down to here. Unloaded columns give the world's bottom.
int rainHeight(const World& world, int32_t x, int32_t z);
// Is rain falling on this exact spot (raining, open to the sky, a rainy biome)?
bool rainingAt(const World& world, const Weather& weather, const BlockPos& p);

} // namespace mc::world
