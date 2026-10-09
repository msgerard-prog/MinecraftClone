#pragma once

#include "world/Enchantments.h"

#include <optional>
#include <string_view>

namespace mc {

// The anvil (M17.5; wiki: Anvil mechanics): repairing with the material (25% of the
// durability per unit), combining two of the same item (their durability + 12%,
// enchantments merged: equal levels go up one, else the higher), or applying an
// enchanted book. Cost in levels: 1 per material unit, 2 for a combining repair,
// each enchantment's level x its rarity multiplier (books: half), plus both items'
// prior-work penalty; the result's penalty becomes 2 x the larger + 1. 40+ is
// "Too Expensive!" in survival.
struct AnvilResult {
    world::ItemStack out;
    int cost = 0;
    int materialUsed = 0; // units taken from the right slot (repairs); 1 otherwise
    bool tooExpensive = false;
};
// `rename`: the name typed in the anvil's field (M29.3b; none: keep the item's name).
AnvilResult anvilCombine(const world::ItemStack& left, const world::ItemStack& right,
                         bool creative, std::optional<std::string_view> rename = std::nullopt);

// The material that repairs an item (iron ingot for iron tools...), or 0.
world::ItemId repairMaterial(world::ItemId item);

} // namespace mc
