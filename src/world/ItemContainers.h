#pragma once

#include "world/Items.h"

#include <array>
#include <cstdint>

namespace mc::world {

// The contents an item carries (M23.6; vanilla's minecraft:container component: a
// broken shulker box keeps its 27 slots). ItemStack stays a small value type, so the
// slots live in this table and the stack holds an id. Entries never change once added
// and are never freed (copies of a stack may share one); a save writes the slots out
// and a load adds a new entry. Thread-safe: chunk loading and saving read and add
// entries on worker threads. Rare (a box broken or loaded), so a lock is fine.
using ItemContents = std::array<ItemStack, 27>;

// Adds the slots and returns their id; 0 if every slot is empty (nothing to carry).
uint32_t addItemContents(const ItemContents& slots);
// A copy of the slots (all empty for 0 or an unknown id).
ItemContents itemContents(uint32_t id);

} // namespace mc::world
