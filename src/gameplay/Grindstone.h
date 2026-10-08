#pragma once

#include "world/Items.h"
#include "world/Random.h"

namespace mc {

// The grindstone (M23.5; wiki: Grindstone): one enchanted item loses its enchantments
// (curses stay - we have none yet) and its prior-work penalty; an enchanted book
// becomes a book. Two of the same damageable item merge: their remaining durability
// plus 5% of the maximum, without enchantments. `xpCost` is the sum of the removed
// enchantments' minimum costs; taking the result pays experience for it.
struct GrindResult {
    world::ItemStack out;
    int xpCost = 0;
};
GrindResult grind(const world::ItemStack& top, const world::ItemStack& bottom);
// Vanilla: ceil(sum / 2) + a random 0..ceil(sum / 2) - 1 points (0 for 0).
int grindExperience(int xpCost, world::Xoroshiro& rng);

} // namespace mc
