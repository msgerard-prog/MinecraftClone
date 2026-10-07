#pragma once

#include "world/Items.h"

namespace mc::world {

// A furnace's contents and timers (wiki: Furnace › Block data). Plain data: the
// smelting rules live in gameplay/Furnace (tickFurnace).
struct FurnaceData {
    ItemStack input, fuel, output;
    int burnLeft = 0;     // ticks of the current fuel left
    int burnDuration = 0; // of the current fuel (flame gauge)
    int cookTime = 0;     // progress on the current item
    ItemId cooking = 0;   // the input kind being cooked (a different item restarts)
    bool lit() const { return burnLeft > 0; }
};

} // namespace mc::world
