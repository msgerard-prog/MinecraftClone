// The grindstone (M23.5; wiki: Grindstone).
#include "gameplay/Grindstone.h"

#include "world/Enchantments.h"

#include <algorithm>

namespace mc {

namespace {

// Each enchantment's minimum cost at its level (the table's lower bound, wiki:
// Enchanting mechanics), summed.
int minCosts(const world::ItemStack& s) {
    int sum = 0;
    for (const uint16_t e : s.enchantments) {
        if (e == 0) continue;
        const auto& info = world::enchantmentInfo(static_cast<world::Enchantment>(e >> 8));
        sum += info.minBase + info.minPerLevel * ((e & 0xFF) - 1);
    }
    return sum;
}

world::ItemStack stripped(world::ItemStack s) {
    s.enchantments = {};
    s.repairCost = 0;
    s.count = 1;
    static const world::ItemId book = *world::itemRegistry().find("book");
    static const world::ItemId enchantedBook = *world::itemRegistry().find("enchanted_book");
    if (s.item == enchantedBook) s.item = book;
    return s;
}

} // namespace

GrindResult grind(const world::ItemStack& top, const world::ItemStack& bottom) {
    if (top.empty() && bottom.empty()) return {};
    if (top.empty() || bottom.empty()) {
        const world::ItemStack& one = top.empty() ? bottom : top;
        if (!world::isEnchanted(one)) return {}; // nothing to take off
        return {stripped(one), minCosts(one)};
    }
    const int maxDurability = world::itemRegistry().item(top.item).durability;
    if (top.item != bottom.item || maxDurability <= 0) return {};
    world::ItemStack out = stripped(top);
    const int remaining =
        (maxDurability - top.damage) + (maxDurability - bottom.damage) + maxDurability * 5 / 100;
    out.damage = static_cast<uint16_t>(maxDurability - std::min(remaining, maxDurability));
    return {out, minCosts(top) + minCosts(bottom)};
}

int grindExperience(int xpCost, world::Xoroshiro& rng) {
    if (xpCost <= 0) return 0;
    const int half = (xpCost + 1) / 2;
    return half + static_cast<int>(rng.nextInt(static_cast<uint32_t>(half)));
}

} // namespace mc
