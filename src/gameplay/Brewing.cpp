#include "gameplay/Brewing.h"

#include "world/Potions.h"

#include <array>
#include <string_view>

namespace mc {

using namespace world;

namespace {

ItemId item(std::string_view name) { return itemRegistry().find(name).value_or(kNoItem); }

struct Step {
    std::string_view ingredient;
    Potion from, to;
};
using P = Potion;
// wiki: Brewing › Brewing recipes (the ones whose ingredients exist here).
constexpr Step kSteps[] = {
    {"nether_wart", P::Water, P::Awkward},
    {"redstone", P::Water, P::Mundane},
    {"glowstone_dust", P::Water, P::Thick},
    {"sugar", P::Water, P::Mundane},
    {"ghast_tear", P::Water, P::Mundane},
    {"spider_eye", P::Water, P::Mundane},
    {"blaze_powder", P::Water, P::Mundane},
    {"magma_cream", P::Water, P::Mundane},
    {"fermented_spider_eye", P::Water, P::Weakness},
    {"sugar", P::Awkward, P::Swiftness},
    {"spider_eye", P::Awkward, P::Poison},
    {"ghast_tear", P::Awkward, P::Regeneration},
    {"blaze_powder", P::Awkward, P::Strength},
    {"magma_cream", P::Awkward, P::FireResistance},
    {"golden_carrot", P::Awkward, P::NightVision},
    {"pufferfish", P::Awkward, P::WaterBreathing}, // (M25 review: pufferfish now exist)
    {"pufferfish", P::Water, P::Mundane},
    {"phantom_membrane", P::Awkward, P::SlowFalling}, // (M26.4a)
    // (M29.2a; wiki: Brewing) a turtle shell, a breeze rod, cobweb, a slime block, stone.
    {"turtle_helmet", P::Awkward, P::TurtleMaster},
    {"breeze_rod", P::Awkward, P::WindCharging},
    {"cobweb", P::Awkward, P::Weaving},
    {"slime_block", P::Awkward, P::Oozing},
    {"stone", P::Awkward, P::Infestation},
    {"redstone", P::TurtleMaster, P::LongTurtleMaster},
    {"glowstone_dust", P::TurtleMaster, P::StrongTurtleMaster},
    // Corruption.
    {"fermented_spider_eye", P::Swiftness, P::Slowness},
    {"fermented_spider_eye", P::LongSwiftness, P::LongSlowness},
    {"fermented_spider_eye", P::Leaping, P::Slowness},
    {"fermented_spider_eye", P::LongLeaping, P::LongSlowness},
    {"fermented_spider_eye", P::Healing, P::Harming},
    {"fermented_spider_eye", P::StrongHealing, P::StrongHarming},
    {"fermented_spider_eye", P::Poison, P::Harming},
    {"fermented_spider_eye", P::LongPoison, P::Harming},
    {"fermented_spider_eye", P::StrongPoison, P::StrongHarming},
    {"fermented_spider_eye", P::NightVision, P::Invisibility},
    {"fermented_spider_eye", P::LongNightVision, P::LongInvisibility},
    // Extending (redstone).
    {"redstone", P::NightVision, P::LongNightVision},
    {"redstone", P::Invisibility, P::LongInvisibility},
    {"redstone", P::Leaping, P::LongLeaping},
    {"redstone", P::FireResistance, P::LongFireResistance},
    {"redstone", P::Swiftness, P::LongSwiftness},
    {"redstone", P::Slowness, P::LongSlowness},
    {"redstone", P::WaterBreathing, P::LongWaterBreathing},
    {"redstone", P::Poison, P::LongPoison},
    {"redstone", P::Regeneration, P::LongRegeneration},
    {"redstone", P::Strength, P::LongStrength},
    {"redstone", P::Weakness, P::LongWeakness},
    {"redstone", P::SlowFalling, P::LongSlowFalling},
    // Strengthening (glowstone dust).
    {"glowstone_dust", P::Leaping, P::StrongLeaping},
    {"glowstone_dust", P::Swiftness, P::StrongSwiftness},
    {"glowstone_dust", P::Slowness, P::StrongSlowness},
    {"glowstone_dust", P::Healing, P::StrongHealing},
    {"glowstone_dust", P::Harming, P::StrongHarming},
    {"glowstone_dust", P::Poison, P::StrongPoison},
    {"glowstone_dust", P::Regeneration, P::StrongRegeneration},
    {"glowstone_dust", P::Strength, P::StrongStrength},
};

} // namespace

std::optional<ItemStack> brewResult(const ItemStack& ingredient, const ItemStack& bottle) {
    static const ItemId potion = item("potion"), splash = item("splash_potion"),
                        gunpowder = item("gunpowder"), lingering = item("lingering_potion"),
                        breath = item("dragon_breath");
    if (ingredient.empty() || bottle.empty() ||
        (bottle.item != potion && bottle.item != splash && bottle.item != lingering) ||
        !bottle.potion)
        return std::nullopt;
    if (ingredient.item == breath) { // (M28.4b) a splash potion to its lingering form
        if (bottle.item != splash) return std::nullopt;
        ItemStack s = bottle;
        s.item = lingering;
        return s;
    }
    if (ingredient.item == gunpowder) { // any potion to its splash form
        if (bottle.item == splash) return std::nullopt;
        ItemStack s = bottle;
        s.item = splash;
        return s;
    }
    const std::string_view id = itemRegistry().item(ingredient.item).id;
    for (const Step& st : kSteps)
        if (static_cast<uint8_t>(st.from) == bottle.potion && id.substr(10) == st.ingredient) {
            ItemStack s = bottle;
            s.potion = static_cast<uint8_t>(st.to);
            return s;
        }
    return std::nullopt;
}

bool isBrewingIngredient(const ItemStack& itemStack) {
    if (itemStack.empty()) return false;
    const std::string_view id = itemRegistry().item(itemStack.item).id;
    if (id == "minecraft:gunpowder" || id == "minecraft:dragon_breath") return true;
    for (const Step& st : kSteps)
        if (id.substr(10) == st.ingredient) return true;
    return false;
}

bool tickBrewing(BrewingData& b) {
    static const ItemId blazePowder = item("blaze_powder");
    bool changed = false;
    // Refuel: blaze powder gives 20 brews (wiki: Brewing Stand › Fuel).
    if (b.fuelLeft <= 0 && !b.fuel.empty() && b.fuel.item == blazePowder) {
        b.fuelLeft = kBrewFuel;
        if (--b.fuel.count == 0) b.fuel = {};
        changed = true;
    }
    bool canBrew = false;
    for (const ItemStack& bottle : b.bottles)
        canBrew = canBrew || brewResult(b.ingredient, bottle).has_value();
    if (b.brewTime > 0) {
        if (b.brewing == 0) b.brewing = b.ingredient.item; // (after loading)
        if (!canBrew ||
            b.ingredient.item != b.brewing) { // ingredient changed or bottles taken: the brew stops
            b.brewTime = 0;
            return true;
        }
        if (--b.brewTime == 0) {
            for (ItemStack& bottle : b.bottles)
                if (const auto r = brewResult(b.ingredient, bottle)) bottle = *r;
            if (--b.ingredient.count == 0) b.ingredient = {};
        }
        return true;
    }
    if (canBrew && b.fuelLeft > 0) {
        b.brewTime = kBrewTicks;
        b.brewing = b.ingredient.item;
        --b.fuelLeft;
        changed = true;
    }
    return changed;
}

} // namespace mc
