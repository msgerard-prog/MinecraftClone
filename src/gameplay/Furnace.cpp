#include "gameplay/Furnace.h"

#include "gameplay/Recipes.h"

#include <algorithm>

namespace mc {

bool tickFurnace(Furnace& f) {
    const bool wasLit = f.lit();
    if (f.burnLeft > 0) --f.burnLeft;
    const auto result = smelt(f.input);
    const auto& items = world::itemRegistry();
    const bool outputFits = result && (f.output.empty() || (f.output.sameKind(*result) &&
                                                          f.output.count + result->count <= items.item(f.output.item).maxStack));
    const bool canSmelt = result && outputFits;
    // Light new fuel when something can smelt (wiki: Furnace › Fuel).
    if (!f.lit() && canSmelt) {
        if (const int ticks = fuelTicks(f.fuel); ticks > 0) {
            f.burnLeft = f.burnDuration = ticks;
            if (--f.fuel.count == 0) f.fuel = {};
        }
    }
    if (f.lit() && canSmelt) {
        if (++f.cookTime >= kFurnaceCookTicks) {
            f.cookTime = 0;
            if (f.output.empty()) f.output = *result;
            else f.output.count = static_cast<uint8_t>(f.output.count + result->count);
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
