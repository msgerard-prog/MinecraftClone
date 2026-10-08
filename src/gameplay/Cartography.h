#pragma once

#include "world/Items.h"

#include <optional>
#include <span>

namespace mc {

// Map copying, zooming out and locking (M28.2b; wiki: Map, Cartography Table).
// A filled map's ItemStack::damage is its map id; `state` holds vanilla's
// map_post_processing for a map just made by a table: the new map data (a zoomed-out
// blank map, or a locked copy) is made by main on the next tick (`kMapScale`, `kMapLock`).
inline constexpr world::BlockStateId kMapLock = 1, kMapScale = 2;

// Crafting-grid map recipes: a filled map with empty maps copies it (one copy per empty
// map); a filled map in the middle of 8 paper zooms out. nullopt: not a map recipe.
std::optional<world::ItemStack> craftMap(std::span<const world::ItemStack> grid, int size);
// The cartography table: map + paper zooms out, + an empty map copies, + a glass pane
// locks. Empty: no result.
world::ItemStack cartography(const world::ItemStack& map, const world::ItemStack& other);

// Copying a written book (M28.2c; wiki: Written Book › Copying): the book with book and
// quills makes that many copies, one generation on (original -> copy of original -> copy
// of a copy; those can't be copied); the original stays in the grid. The result carries
// the original's pages until it is taken (`bookCopy` makes the copy's own entry).
std::optional<world::ItemStack> craftBookCopy(std::span<const world::ItemStack> grid);
world::ItemStack bookCopy(const world::ItemStack& original, int count);

} // namespace mc
