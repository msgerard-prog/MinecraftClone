#pragma once

#include <cstdint>

namespace mc {

// Particle sprites (M22.3), shared by gameplay/Particles (which picks them) and
// rendering/EntityRenderer (which maps them to atlas cells: block/particle_*.png).
enum class ParticleSprite : uint8_t {
    Generic0, // .. Generic7 (puffs, small to large)
    Flame = 8,
    Lava,
    Crit,
    Effect,
    Splash0, // .. Splash3
    Drip = 16,
    Terrain, // a 4x4-texel piece of a block's texture
    Note,    // (M23.6: note blocks, tinted by pitch)
    Count
};

} // namespace mc
