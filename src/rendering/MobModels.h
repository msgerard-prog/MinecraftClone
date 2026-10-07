#pragma once

#include "world/Mob.h"

#include <span>

namespace mc::gfx {

// Cuboid mob models (our own, matching tools/textures/gen_entities.py's box-UV
// layouts). Units: pixels (1/16 block), origin at the feet centre, +Y up, the mob
// facing +Z. Parts rotate about their pivot for animation.
struct MobPart {
    enum class Anim : uint8_t { None, Head, LegA, LegB, ArmForward }; // LegA/B swing opposite
    float from[3], to[3];
    float pivot[3];
    int u, v; // box-UV origin in the mob's 64x64 texture
    Anim anim;
};

std::span<const MobPart> mobModel(world::MobType type);
// Row of the mob's 64x64 texture in the stacked mob atlas (64 x 64*Count).
inline int mobTextureRow(world::MobType type) { return static_cast<int>(type); }
inline const char* mobTexturePath(world::MobType type) {
    return type == world::MobType::Zombie ? "assets/minecraft/textures/entity/clone/zombie.png"
                                          : "assets/minecraft/textures/entity/clone/cow.png";
}

} // namespace mc::gfx
