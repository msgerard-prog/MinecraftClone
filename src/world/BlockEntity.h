#pragma once

#include "world/Items.h"
#include "world/Mob.h"
#include "world/RecipeIds.h"

#include <array>
#include <climits>

namespace mc::world {

// A furnace's contents and timers (wiki: Furnace › Block data). Plain data: the
// smelting rules live in gameplay/Furnace (tickFurnace).
struct FurnaceData {
    ItemStack input, fuel, output;
    int burnLeft = 0;     // ticks of the current fuel left
    int burnDuration = 0; // of the current fuel (flame gauge)
    int cookTime = 0;     // progress on the current item
    ItemId cooking = 0;   // the input kind being cooked (a different item restarts)
    // Vanilla's RecipesUsed (wiki: Furnace › Block data): how often each recipe was
    // used since the output was last taken. The experience is worked out from these
    // counts when the player takes the output (gameplay/Furnace). A furnace sees only
    // a few recipes between takes; past 16 at once, new recipes aren't counted.
    struct RecipeUse {
        RecipeId recipe = kNoRecipe;
        int32_t count = 0;
    };
    std::array<RecipeUse, 16> recipesUsed{};
    bool lit() const { return burnLeft > 0; }
    void countRecipe(RecipeId recipe, int32_t n = 1) {
        if (recipe == kNoRecipe || n <= 0) return;
        for (RecipeUse& u : recipesUsed)
            if (u.recipe == recipe || u.recipe == kNoRecipe) {
                u.recipe = recipe;
                u.count = u.count > INT32_MAX - n ? INT32_MAX : u.count + n;
                return;
            }
    }
};

// A monster spawner (M18.3; wiki: Monster Spawner › Block data): the mob it spawns
// and the ticks until its next try (vanilla Delay; 200-799 after each spawn).
struct SpawnerData {
    MobType mob = MobType::Zombie;
    int16_t delay = 20;
};

// A brewing stand (M19.4; wiki: Brewing Stand › Block data): three bottles, the
// ingredient, blaze powder fuel; brews left on the fuel and ticks left on the brew.
struct BrewingData {
    std::array<ItemStack, 3> bottles{};
    ItemStack ingredient, fuel;
    int fuelLeft = 0;
    int brewTime = 0;
    ItemId brewing = 0; // the ingredient the running brew started with (not saved: a
                        // loaded brew takes the slot's; vanilla also keeps it in memory)
};

// A chest's 27 slots (wiki: Chest › Block data: Items). A double chest is two chests.
struct ChestData {
    std::array<ItemStack, 27> items{};
};

} // namespace mc::world
