#pragma once

#include "world/Enchantments.h"
#include "world/World.h"

#include <array>
#include <cstdint>

namespace mc {

// The enchanting table (M17.5; wiki: Enchanting Table, Enchanting mechanics).
// Bookshelves 2 blocks out (with air between) raise the three offers' level costs;
// each offer's enchantments come from the player's enchantment seed, so they stay
// the same until the player enchants something.
int countBookshelves(const world::World& world, const world::BlockPos& table);

struct EnchantOffer {
    int cost = 0; // level requirement (0: no offer)
    world::Enchantment hint = world::Enchantment::None; // shown on the button
    int hintLevel = 0;
};
std::array<EnchantOffer, 3> enchantOffers(const world::ItemStack& item, int bookshelves, uint64_t seed);

// The enchantments of offer `slot` (0..2) at `cost` (deterministic for the seed).
struct EnchantPick {
    std::array<std::pair<world::Enchantment, int>, 4> list{};
    int count = 0;
};
EnchantPick pickEnchantments(const world::ItemStack& item, int cost, uint64_t seed, int slot);

// Applies offer `slot`: a book becomes an enchanted book. Returns the new stack.
world::ItemStack applyEnchantments(const world::ItemStack& item, const EnchantPick& pick);

} // namespace mc
