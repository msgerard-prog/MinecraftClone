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
    // 0 furnace, 1 smoker (food only), 2 blast furnace (ores and metal): the last two
    // cook in half the time and burn fuel twice as fast (M23.5; wiki: Smoker, Blast Furnace).
    uint8_t kind = 0;
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

// A sign's text (M23.3c; wiki: Sign › Block data): 4 lines a side, each up to 24
// ASCII characters (vanilla allows any text that fits 90 font pixels), a dye colour
// (vanilla default black) and whether wax locked it.
struct SignData {
    static constexpr int kLines = 4, kChars = 24;
    struct Side {
        std::array<std::array<char, kChars + 1>, kLines> lines{}; // NUL-terminated
        uint8_t colour = 15;  // dye index (15 black)
        bool glowing = false;
    } front, back;
    bool waxed = false;
    bool hanging = false; // saved as minecraft:hanging_sign
};

// A campfire's food (M23.4c; wiki: Campfire › Block data): up to 4 items, each with its
// cooking time so far (done at 600 ticks).
struct CampfireData {
    std::array<ItemStack, 4> items{};
    std::array<int16_t, 4> cookTime{};
};

// A redstone comparator's output strength (M21.2; wiki: Redstone Comparator › Block
// data: OutputSignal) - its block state only says whether it is on.
struct ComparatorData {
    int output = 0;
};

// A hopper's 5 slots and its transfer cooldown (M21.3; wiki: Hopper › Block data:
// Items, TransferCooldown).
struct HopperData {
    std::array<ItemStack, 5> items{};
    int cooldown = 0;
};
// A dispenser's or dropper's 9 slots (wiki: Dispenser, Dropper › Block data: Items).
struct DispenserData {
    std::array<ItemStack, 9> items{};
    bool dropper = false; // (saved as minecraft:dropper)
};

// A chest's 27 slots (wiki: Chest › Block data: Items). A double chest is two chests.
struct ChestData {
    std::array<ItemStack, 27> items{};
    bool barrel = false; // (M23.5: a barrel's contents, saved as minecraft:barrel)
    bool shulker = false; // (M23.6: a shulker box's, saved as minecraft:shulker_box)
    bool ender = false;   // (M23.6: the player's ender chest slots, saved in level.dat)
};

} // namespace mc::world
