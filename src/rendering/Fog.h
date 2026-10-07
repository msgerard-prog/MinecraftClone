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

// The Nether's thick fog (wiki: Fog › Nether): visibility ends at half the render
// distance, at most 96 blocks, and starts almost at the camera.
constexpr FogRange netherFog(int renderDistanceChunks) {
    const float rd = static_cast<float>(renderDistanceChunks * 16);
    const float end = (rd < 192.0f ? rd : 192.0f) * 0.5f;
    return {end * 0.1f, end};
}

} // namespace mc::gfx
