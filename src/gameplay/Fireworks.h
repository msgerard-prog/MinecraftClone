#pragma once

#include "world/ItemExtras.h"
#include "world/Items.h"

#include <optional>
#include <span>

namespace mc {

// Fireworks (M28.4c; wiki: Firework Rocket, Firework Star, Elytra).
//
// Crafting (shapeless): a star is gunpowder, 1-8 dyes and optionally a shape (fire charge:
// large ball, gold nugget: star, a head: creeper, feather: burst), a diamond (trail) and
// glowstone dust (twinkle); a star with dyes gets those as fade colours; a rocket is paper,
// 1-3 gunpowder (flight duration) and up to 7 stars - three rockets.
std::optional<world::ItemStack> craftFirework(std::span<const world::ItemStack> grid);
// Ticks a launched rocket flies: 10 x (flight + 1) + 0..5 + 0..6 (wiki).
int rocketLifetime(int flight, uint32_t r1, uint32_t r2);
// Damage to something `distance` from a rocket with `explosions` stars (none: no harm):
// (5 + 2 x explosions) x sqrt((5 - distance) / 5) within 5 blocks (our reading of the wiki).
float fireworkDamage(int explosions, double distance);
// An elytra boost (wiki: Elytra › Boosting): each tick the glide speeds toward 1.5 blocks
// a tick along the look.
glm::dvec3 boostedVelocity(const glm::dvec3& vel, const glm::dvec3& look);

} // namespace mc
