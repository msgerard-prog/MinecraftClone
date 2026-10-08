#pragma once

#include "world/Items.h"

namespace mc {

// The smithing table (M23.6; wiki: Smithing Table, Smithing Template). A template, a
// base item and an addition:
//   netherite upgrade + a diamond tool or armor piece + a netherite ingot -> the
//     netherite version, keeping its damage, enchantments, repair cost and trim;
//   an armor trim template + an armor piece + a trim material -> the piece with that
//     trim (replacing another; nothing if it already has exactly that one).
// Taking the result uses one of each input.
world::ItemStack smith(const world::ItemStack& templ, const world::ItemStack& base,
                       const world::ItemStack& addition);

} // namespace mc
