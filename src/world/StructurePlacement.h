#pragma once

#include "world/Chunk.h"
#include "world/Random.h"

#include <cstdint>

namespace mc::world {

// Where structures may start (M18.3; wiki: Structure set). "random_spread": the
// world is cut into square regions of `spacing` chunks; each region holds one start
// candidate at a random offset in 0 .. spacing - separation - 1 on each axis (or the
// average of two such numbers with `triangular`), so two candidates are at least
// `separation` chunks apart. The structure's own checks (biome, terrain) then decide
// whether it really generates. Spacing, separation and salt are vanilla's (wiki); the
// random numbers come from our own seed mixing, so positions differ from vanilla's.
struct RandomSpread {
    int spacing, separation;
    uint32_t salt;
    bool triangular = false;
};

inline constexpr RandomSpread kVillages{34, 8, 10387312};
inline constexpr RandomSpread kDesertPyramids{32, 8, 14357617};
inline constexpr RandomSpread kIgloos{32, 8, 14357618};
inline constexpr RandomSpread kJunglePyramids{32, 8, 14357619};
inline constexpr RandomSpread kSwampHuts{32, 8, 14357620};
inline constexpr RandomSpread kOutposts{32, 8, 165745296}; // (M24.4; wiki: Pillager Outpost)
inline constexpr RandomSpread kAncientCities{24, 8, 20083232}; // (M27.3b; wiki: Ancient City)
inline constexpr RandomSpread kRuinedPortals{40, 15, 34222645}; // (M27.4b; wiki: Ruined Portal)
inline constexpr RandomSpread kMansions{80, 20, 10387319};       // (M27.4c; wiki: Woodland Mansion)
inline constexpr RandomSpread kTrialChambers{34, 12, 94251327};  // (M27.4d; wiki: Trial Chambers)
inline constexpr RandomSpread kTrailRuins{34, 8, 83469867};      // (M27.5b; wiki: Trail Ruins)
inline constexpr RandomSpread kShipwrecks{24, 4, 165745295}; // (M25.4; wiki: Shipwreck, Structure set)
inline constexpr RandomSpread kOceanRuins{20, 8, 14357621};  // (M25.4; wiki: Ocean Ruins)
inline constexpr RandomSpread kMonuments{32, 5, 10387313, true}; // (M25.5; wiki: Ocean Monument - triangular spread)

inline int32_t floorDivChunks(int32_t a, int32_t b) { return (a >= 0 ? a : a - b + 1) / b; }

// The start candidate of the region containing `chunk`.
inline ChunkPos spreadCandidate(uint64_t seed, const RandomSpread& s, ChunkPos chunk) {
    const int32_t rx = floorDivChunks(chunk.x, s.spacing), rz = floorDivChunks(chunk.z, s.spacing);
    Xoroshiro r(mixSeed(mixSeed(mixSeed(seed, s.salt), static_cast<uint32_t>(rx)), static_cast<uint32_t>(rz)));
    const uint32_t n = static_cast<uint32_t>(s.spacing - s.separation);
    auto offset = [&] {
        return s.triangular ? static_cast<int32_t>((r.nextInt(n) + r.nextInt(n)) / 2) : static_cast<int32_t>(r.nextInt(n));
    };
    const int32_t ox = offset(), oz = offset();
    return {rx * s.spacing + ox, rz * s.spacing + oz};
}

inline bool isSpreadCandidate(uint64_t seed, const RandomSpread& s, ChunkPos chunk) {
    return spreadCandidate(seed, s, chunk) == chunk;
}

// Mineshafts (wiki: Structure set): any chunk, with probability 0.4%.
inline bool isMineshaftCandidate(uint64_t seed, ChunkPos chunk) {
    Xoroshiro r(mixSeed(mixSeed(mixSeed(seed, 0x4d494e45u), static_cast<uint32_t>(chunk.x)), static_cast<uint32_t>(chunk.z)));
    return r.nextDouble() < 0.004;
}

} // namespace mc::world
