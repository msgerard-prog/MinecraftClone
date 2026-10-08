#include "gameplay/Furnace.h"

#include "gameplay/Recipes.h"

#include <algorithm>
#include <vector>
#include <climits>
#include <cmath>

namespace mc {

bool tickFurnace(Furnace& f) {
    const bool wasLit = f.lit();
    // Smokers and blast furnaces (M23.5) burn fuel twice as fast and cook in 100 ticks.
    const bool fast = f.kind != 0;
    if (f.burnLeft > 0) f.burnLeft = std::max(0, f.burnLeft - (fast ? 2 : 1));
    if (f.input.item != f.cooking) { // a different input starts over (vanilla)
        f.cooking = f.input.item;
        f.cookTime = 0;
    }
    auto result = smelt(f.input);
    // A smoker cooks only food, a blast furnace only ores, raw metal and metal gear
    // (wiki: Smoker, Blast Furnace).
    if (result && f.kind == 1 && world::itemRegistry().item(result->item).food == 0) result.reset();
    if (result && f.kind == 2) {
        // Which inputs a blast furnace takes, by name once per item (not every tick).
        static const std::vector<uint8_t> blastable = [] {
            const auto& items = world::itemRegistry();
            std::vector<uint8_t> t(items.count());
            for (size_t i = 0; i < t.size(); ++i) {
                const std::string_view in = items.item(static_cast<world::ItemId>(i)).id;
                t[i] = in.find("_ore") != std::string_view::npos || in.find("raw_") != std::string_view::npos ||
                       in == "minecraft:ancient_debris" || in.find("iron_") != std::string_view::npos ||
                       in.find("golden_") != std::string_view::npos;
            }
            return t;
        }();
        if (f.input.item >= blastable.size() || !blastable[f.input.item]) result.reset();
    }
    const auto& items = world::itemRegistry();
    const bool outputFits = result && (f.output.empty() || (f.output.sameKind(*result) &&
                                                          f.output.count + result->count <= items.item(f.output.item).maxStack));
    const bool canSmelt = result && outputFits;
    // Light new fuel when something can smelt (wiki: Furnace › Fuel).
    if (!f.lit() && canSmelt) {
        if (const int ticks = fuelTicks(f.fuel); ticks > 0) {
            f.burnLeft = f.burnDuration = ticks;
            // A lava bucket leaves its empty bucket behind (wiki: Fuel).
            if (world::itemRegistry().item(f.fuel.item).id == "minecraft:lava_bucket")
                f.fuel = {*world::itemRegistry().find("bucket"), 1};
            else if (--f.fuel.count == 0) f.fuel = {};
        }
    }
    if (f.lit() && canSmelt) {
        if (++f.cookTime >= (fast ? kFurnaceCookTicks / 2 : kFurnaceCookTicks)) {
            f.cookTime = 0;
            if (f.output.empty()) f.output = *result;
            else f.output.count = static_cast<uint8_t>(f.output.count + result->count);
            f.countRecipe(smeltRecipe(f.input)); // RecipesUsed: the experience is paid on taking
            if (--f.input.count == 0) f.input = {};
        }
    } else if (!f.lit() && f.cookTime > 0) {
        f.cookTime = std::max(0, f.cookTime - 2); // cools down without fuel (wiki)
    } else if (!canSmelt) {
        f.cookTime = 0;
    }
    return wasLit != f.lit();
}

int recipesExperience(std::span<const world::FurnaceData::RecipeUse> used, world::Xoroshiro& rng) {
    // Each recipe pays its experience times the number of uses; the whole part is
    // given, the fraction is the chance of one more point (wiki: Smelting).
    int64_t total = 0;
    for (const world::FurnaceData::RecipeUse& u : used) {
        if (u.recipe == world::kNoRecipe || u.count <= 0) continue;
        const double xp = double(u.count) * double(recipeExperience(u.recipe));
        const double whole = std::floor(xp);
        total += static_cast<int64_t>(whole);
        if (xp > whole && rng.nextFloat() < float(xp - whole)) ++total;
    }
    return static_cast<int>(std::min<int64_t>(total, INT32_MAX));
}

int takeFurnaceExperience(Furnace& f, world::Xoroshiro& rng) {
    const int xp = recipesExperience(f.recipesUsed, rng);
    f.recipesUsed = {};
    return xp;
}

int tickCampfire(world::CampfireData& campfire, std::array<world::ItemStack, 4>& done) {
    int n = 0;
    for (size_t i = 0; i < 4; ++i) {
        world::ItemStack& it = campfire.items[i];
        if (it.empty() || ++campfire.cookTime[i] < 600) continue;
        const auto cooked = smelt(it);
        done[size_t(n++)] = cooked ? *cooked : it;
        it = {};
        campfire.cookTime[i] = 0;
    }
    return n;
}

} // namespace mc
