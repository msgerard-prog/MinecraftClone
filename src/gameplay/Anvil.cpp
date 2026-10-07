#include "gameplay/Anvil.h"

#include <algorithm>
#include <string_view>

namespace mc {

using namespace world;

ItemId repairMaterial(ItemId item) {
    const ItemDef& d = itemRegistry().item(item);
    const std::string_view id = d.id;
    auto find = [](const char* n) { return *itemRegistry().find(n); };
    if (id == "minecraft:shield") return find("oak_planks");
    if (d.armorSlot) {
        if (id.find("leather") != std::string_view::npos) return find("leather");
        if (id.find("copper") != std::string_view::npos) return find("copper_ingot");
        if (id.find("golden") != std::string_view::npos) return find("gold_ingot");
        if (id.find("iron") != std::string_view::npos) return find("iron_ingot");
        if (id.find("diamond") != std::string_view::npos) return find("diamond");
        return 0;
    }
    switch (d.tier) {
    case ToolTier::Wood: return find("oak_planks");
    case ToolTier::Stone: return find("cobblestone");
    case ToolTier::Iron: return find("iron_ingot");
    case ToolTier::Gold: return find("gold_ingot");
    case ToolTier::Diamond: return find("diamond");
    case ToolTier::Copper: return find("copper_ingot");
    default: return 0;
    }
}

namespace {
// Rarity multiplier by weight (wiki: Anvil mechanics): common 1, uncommon 2, rare 4,
// very rare 8.
int multiplier(Enchantment e) {
    const int w = enchantmentInfo(e).weight;
    return w >= 10 ? 1 : w >= 5 ? 2 : w >= 2 ? 4 : 8;
}
} // namespace

AnvilResult anvilCombine(const ItemStack& left, const ItemStack& right, bool creative) {
    AnvilResult r;
    if (left.empty() || right.empty()) return r;
    const ItemDef& ld = itemRegistry().item(left.item);
    const bool book = itemRegistry().item(right.item).id == "minecraft:enchanted_book";
    // A plain book can't take enchantments on the anvil (vanilla: only enchanted books
    // combine with each other or onto gear).
    if (ld.id == "minecraft:book") return r;
    ItemStack out = left;
    out.count = 1;
    int cost = 0;
    r.materialUsed = 1;
    if (ld.durability > 0 && right.item == repairMaterial(left.item)) {
        // Repair: 25% of the durability per unit, as many units as needed.
        if (left.damage == 0) return r;
        const int quarter = std::max(1, ld.durability / 4);
        int used = 0, damage = left.damage;
        while (damage > 0 && used < right.count) {
            damage = std::max(0, damage - quarter);
            ++used;
        }
        out.damage = uint16_t(damage);
        cost += used;
        r.materialUsed = used;
    } else if (right.item == left.item || book) {
        if (!book && ld.durability > 0 && left.damage > 0) { // durability + 12%
            const int remaining = (ld.durability - left.damage) + (ld.durability - right.damage) + ld.durability * 12 / 100;
            out.damage = uint16_t(std::max(0, ld.durability - remaining));
            cost += 2;
        }
        for (const uint16_t v : right.enchantments) {
            if (!v) continue;
            const Enchantment e = Enchantment(v >> 8);
            const int rl = v & 0xFF;
            if (!canEnchant(left.item, e)) continue;
            // Clashes with one already on the item (same group): skipped, costs 1.
            bool clash = false;
            for (const uint16_t lv : out.enchantments)
                if (lv && Enchantment(lv >> 8) != e && enchantmentInfo(e).group &&
                    enchantmentInfo(Enchantment(lv >> 8)).group == enchantmentInfo(e).group)
                    clash = true;
            if (clash) {
                cost += 1;
                continue;
            }
            const int ll = enchantLevel(out, e);
            const int level = ll == rl ? std::min(ll + 1, enchantmentInfo(e).maxLevel) : std::max(ll, rl);
            setEnchantment(out, e, level);
            cost += level * std::max(1, multiplier(e) / (book ? 2 : 1));
        }
        if (out.enchantments == left.enchantments && out.damage == left.damage) return r; // nothing changed
    } else {
        return r;
    }
    cost += left.repairCost + right.repairCost;
    out.repairCost = uint8_t(std::min(255, std::max<int>(left.repairCost, right.repairCost) * 2 + 1));
    r.out = out;
    r.cost = cost;
    r.tooExpensive = !creative && cost >= 40;
    return r;
}

} // namespace mc
