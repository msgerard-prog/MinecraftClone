#pragma once

#include "world/ItemContainers.h"
#include "world/Items.h"

namespace mc {

// Bundles (M29.3f; wiki: Bundle): an item that carries a mix of items up to a weight of
// 64 - each item counts 64 / its stack size (so 64 of a 64-stacker, 16 of a 16-stacker,
// one unstackable item), a bundle inside 4 plus what it holds. The stacks live in the
// item-contents table (ItemStack::contents; ours: 27 different stacks at most), the last
// one put in comes out first.
bool isBundle(world::ItemId item);
int bundleWeight(const world::ItemStack& bundle);
// The weight `stack` adds per item (0: it can't go in - a full shulker box... ours: shulker
// boxes holding items don't go in, as vanilla).
int bundleItemWeight(const world::ItemStack& stack);
// Puts as many of `in` as fit into `bundle` (a new contents id); returns how many went in.
int addToBundle(world::ItemStack& bundle, const world::ItemStack& in);
// Takes the last stack out (empty if it holds nothing).
world::ItemStack takeFromBundle(world::ItemStack& bundle);

} // namespace mc
