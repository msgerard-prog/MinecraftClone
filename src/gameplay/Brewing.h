#pragma once

#include "world/BlockEntity.h"

#include <optional>

namespace mc {

// Brewing (M19.4; wiki: Brewing, Brewing Stand). A brew takes 400 ticks (20 s) and one
// unit of fuel (blaze powder gives 20); the ingredient is used up once for all three
// bottles. Recipes: water + nether wart = awkward; awkward + an effect ingredient =
// that potion; redstone extends, glowstone strengthens, a fermented spider eye
// corrupts, gunpowder makes it a splash potion.
inline constexpr int kBrewTicks = 400;
inline constexpr int kBrewFuel = 20;

// What `bottle` becomes with `ingredient` (nullopt: no recipe).
std::optional<world::ItemStack> brewResult(const world::ItemStack& ingredient,
                                           const world::ItemStack& bottle);
// Whether `item` is something that brews with at least one bottle kind.
bool isBrewingIngredient(const world::ItemStack& item);

// One tick of a brewing stand; true if its saved state changed.
bool tickBrewing(world::BrewingData& b);

} // namespace mc
