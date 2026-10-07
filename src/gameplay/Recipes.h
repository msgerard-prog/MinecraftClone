#pragma once

#include "world/Items.h"

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace mc {

// Crafting and smelting recipes, authored here from the wiki's recipe pages
// (vanilla ships them as data-pack JSON we don't copy, ADR 0004).
struct Ingredient {
    enum class Kind { Empty, Item, Planks, Logs, Coal, StoneTool } kind = Kind::Empty;
    world::ItemId item = world::kNoItem;
    bool matches(const world::ItemStack& s) const;
};

struct Recipe {
    int width = 0, height = 0;        // shaped: pattern size; shapeless: 0
    std::vector<Ingredient> pattern;  // shaped: width x height, row-major; shapeless: the list
    world::ItemStack result;
};

// The result of the grid (`size` x `size`, row-major), or nullopt. Shaped recipes may
// sit anywhere in the grid and match mirrored left-right (vanilla).
std::optional<world::ItemStack> craft(std::span<const world::ItemStack> grid, int size);
const std::vector<Recipe>& craftingRecipes();

// Smelting (wiki: Smelting): input -> output, 200 ticks each.
std::optional<world::ItemStack> smelt(const world::ItemStack& input);
// Fuel burn time in ticks (wiki: Fuel), 0 if not a fuel.
int fuelTicks(const world::ItemStack& fuel);

} // namespace mc
