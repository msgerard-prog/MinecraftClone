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

} // namespace mc::gfx
