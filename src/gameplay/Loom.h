#pragma once

#include "world/Banners.h"
#include "world/Items.h"

#include <array>

namespace mc {

// The loom (M28.3d; wiki: Loom): a banner, a dye and (optionally) a banner pattern item;
// the player picks a pattern and gets the banner with one more layer in the dye's colour
// (6 layers at most). The pattern item stays in the loom.
inline constexpr int kMaxLoomLayers = 6;
// The patterns the loom offers with `patternItem` in its slot (indices into
// world::kBannerPatterns, in order); returns how many.
int loomPatterns(const world::ItemStack& patternItem, std::array<int, 48>& out);
// The woven banner (empty: no result).
world::ItemStack loomResult(const world::ItemStack& banner, const world::ItemStack& dye, int pattern);
// The dye colour (0..15) of a dye item, or -1.
int dyeColour(world::ItemId item);

} // namespace mc
