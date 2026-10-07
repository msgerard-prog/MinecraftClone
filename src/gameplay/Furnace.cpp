#include "gameplay/Furnace.h"

#include "gameplay/Recipes.h"

#include <algorithm>

namespace mc {

bool tickFurnace(Furnace& f) {
    const bool wasLit = f.lit();
    if (f.burnLeft > 0) --f.burnLeft;
    if (f.input.item != f.cooking) { // a different input starts over (vanilla)
        f.cooking = f.input.item;
        f.cookTime = 0;
    }
    const auto result = smelt(f.input);
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
        if (++f.cookTime >= kFurnaceCookTicks) {
            f.cookTime = 0;
            if (f.output.empty()) f.output = *result;
            else f.output.count = static_cast<uint8_t>(f.output.count + result->count);
            f.experience += smeltExperience(f.input);
            if (--f.input.count == 0) f.input = {};
        }
    } else if (!f.lit() && f.cookTime > 0) {
        f.cookTime = std::max(0, f.cookTime - 2); // cools down without fuel (wiki)
    } else if (!canSmelt) {
        f.cookTime = 0;
    }
    return wasLit != f.lit();
}

} // namespace mc
