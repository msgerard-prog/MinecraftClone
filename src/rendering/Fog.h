#pragma once

namespace mc::gfx {

struct FogRange {
    float start; // blocks: fog begins
    float end;   // blocks: fully fogged (nothing visible beyond)
};

// Vanilla terrain fog (wiki: Fog): visibility ends at the render distance and fog
// begins at 92% of it. Distance is cylindrical (shader), the ramp linear.
constexpr FogRange terrainFog(int renderDistanceChunks) {
    const float end = static_cast<float>(renderDistanceChunks * 16);
    return {end * 0.92f, end};
}

// The Nether's thick fog (wiki: Fog › Nether): since 1.21.11 fixed from 10 to 96
// blocks, but never past the render distance.
constexpr FogRange netherFog(int renderDistanceChunks) {
    const float rd = static_cast<float>(renderDistanceChunks * 16);
    return {10.0f, rd < 96.0f ? rd : 96.0f};
}

// Under water (wiki: Fog): from 8 blocks behind the eye to 96 ahead (vanilla eases the
// distance in over ~30 s after diving; ours is at once), never past the render distance.
constexpr FogRange waterFog(int renderDistanceChunks) {
    const float rd = static_cast<float>(renderDistanceChunks * 16);
    return {-8.0f, rd < 96.0f ? rd : 96.0f};
}

} // namespace mc::gfx
