#pragma once

#include "world/Items.h"

#include <array>

namespace mc::world {

// A furnace's contents and timers (wiki: Furnace › Block data). Plain data: the
// smelting rules live in gameplay/Furnace (tickFurnace).
struct FurnaceData {
    ItemStack input, fuel, output;
    int burnLeft = 0;     // ticks of the current fuel left
    int burnDuration = 0; // of the current fuel (flame gauge)
    int cookTime = 0;     // progress on the current item
    ItemId cooking = 0;   // the input kind being cooked (a different item restarts)
    float experience = 0.0f; // stored by smelting, paid out when the output is taken
    bool lit() const { return burnLeft > 0; }
};

// A chest's 27 slots (wiki: Chest › Block data: Items). A double chest is two chests.
struct ChestData {
    std::array<ItemStack, 27> items{};
};

} // namespace mc::world
