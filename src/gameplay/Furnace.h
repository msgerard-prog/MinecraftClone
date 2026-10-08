#pragma once

#include "world/BlockEntity.h"
#include "world/Random.h"

#include <span>

namespace mc {

// Furnace rules (wiki: Furnace): smelts one item every 200 ticks while fuel burns; a
// fuel item starts burning only when something can smelt; progress cools down
// without fuel.
using Furnace = world::FurnaceData;
inline constexpr int kFurnaceCookTicks = 200;

// One tick. Returns true if the lit state changed (block state update).
bool tickFurnace(Furnace& f);

// Experience for recipe uses (a furnace's RecipesUsed): per recipe, uses x its
// experience; the fraction is rolled as the chance of one extra point (wiki: Smelting).
int recipesExperience(std::span<const world::FurnaceData::RecipeUse> used, world::Xoroshiro& rng);
// Pays out and clears a furnace's stored experience (vanilla: when the output is
// taken, or the furnace is broken).
int takeFurnaceExperience(Furnace& f, world::Xoroshiro& rng);

// A lit campfire's tick (M23.4c; wiki: Campfire): each item cooks for 600 ticks, then
// comes off cooked (written to `done`, the count returned; items that don't cook are
// dropped as they are).
int tickCampfire(world::CampfireData& campfire, std::array<world::ItemStack, 4>& done);

} // namespace mc
