#pragma once

#include "gameplay/ItemEntities.h"
#include "world/Direction.h"
#include "world/World.h"

namespace mc::world {
class BlockUpdates;
}

namespace mc {

// Containers as hoppers and droppers see them (M21.3; wiki: Hopper › Behavior): which
// slots take an item put in from a side, and which give one up.
//   chests (double: both halves), hoppers, dispensers, droppers: any slot;
//   furnaces: from above into the input, from a side the fuel (fuel only); taken from
//     below: the output;
//   brewing stands: from above the ingredient, from a side bottles (potions) or blaze
//     powder (fuel); taken from below: the bottles.
// `from`: the side of the container the item comes in through (or goes out of).
bool insertOne(world::World& world, const world::BlockPos& container, world::Direction from,
               const world::ItemStack& one);
// Takes one item out of the container through its side `from` into `out` (count 1).
// `fromSlot`: where it came from (to put it back if it doesn't fit after all).
bool extractOne(world::World& world, const world::BlockPos& container, world::Direction from,
                world::ItemStack& out, world::ItemStack** fromSlot = nullptr);
bool isContainer(const world::World& world, const world::BlockPos& p);
// Composters (M23.5) take compostables from above and give bone meal below through the
// block updates (chances, the ready tick); main sets them once (null: composters ignored).
void setHopperBlockUpdates(world::BlockUpdates* updates);

// Every hopper in the ticking chunks, each game tick (wiki: Hopper): while enabled
// (not powered) and its 8-tick cooldown is over, it pushes one item into the container
// it points into, then pulls one from the container above - or picks up dropped items
// over it; either resets the cooldown.
void tickHoppers(world::World& world, ItemEntities& items);

} // namespace mc
