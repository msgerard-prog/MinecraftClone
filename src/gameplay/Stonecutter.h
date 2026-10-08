#pragma once

#include "world/Items.h"

#include <span>

namespace mc {

// The stonecutter (M23.5; wiki: Stonecutter): one stone-like block becomes one of its
// cut forms - slabs (2), stairs and walls (1), polished/brick/chiseled variants (1),
// and the cut forms of those, so stone also gives stone brick slabs. Block of copper
// gives 4 cut copper (and 8 cut copper slabs). Sorted by item id, as vanilla lists them.
std::span<const world::ItemStack> stonecutterRecipes(world::ItemId input);

} // namespace mc
