#pragma once

#include "world/BlockEntity.h"

namespace mc {

// Furnace rules (wiki: Furnace): smelts one item every 200 ticks while fuel burns; a
// fuel item starts burning only when something can smelt; progress cools down
// without fuel.
using Furnace = world::FurnaceData;
inline constexpr int kFurnaceCookTicks = 200;

// One tick. Returns true if the lit state changed (block state update).
bool tickFurnace(Furnace& f);

} // namespace mc
