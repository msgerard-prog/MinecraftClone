#include "world/Loot.h"

#include "world/Enchantments.h"

#include <string>

namespace mc::world {

namespace {

// wiki: Monster Room › Loot (Java Edition).
constexpr LootEntry kDungeon1[] = {
    {"leather", 1, 5, 20},       {"name_tag", 1, 1, 20},        {"copper_horse_armor", 1, 1, 15},
    {"music_disc_13", 1, 1, 15}, {"music_disc_cat", 1, 1, 15},  {"iron_horse_armor", 1, 1, 15},
    {"golden_apple", 1, 1, 15},  {"enchanted_book", 1, 1, 10, true}, {"golden_horse_armor", 1, 1, 10},
    {"diamond_horse_armor", 1, 1, 5}, {"music_disc_otherside", 1, 1, 2}, {"enchanted_golden_apple", 1, 1, 2}};
constexpr LootEntry kDungeon2[] = {
    {"wheat", 1, 4, 20},         {"bread", 1, 1, 20},           {"coal", 1, 4, 15},
    {"redstone", 1, 4, 15},      {"beetroot_seeds", 2, 4, 10},  {"melon_seeds", 2, 4, 10},
    {"pumpkin_seeds", 2, 4, 10}, {"iron_ingot", 1, 4, 10},      {"bucket", 1, 1, 10},
    {"gold_ingot", 1, 4, 5}};
constexpr LootEntry kDungeon3[] = {
    {"bone", 1, 8, 10}, {"gunpowder", 1, 8, 10}, {"rotten_flesh", 1, 8, 10}, {"string", 1, 8, 10}};
constexpr LootPool kDungeon[] = {{1, 3, kDungeon1}, {1, 4, kDungeon2}, {3, 3, kDungeon3}};

// wiki: Desert Pyramid › Loot (Java Edition).
constexpr LootEntry kDesert1[] = {
    {"", 1, 1, 15},                {"bone", 4, 6, 25},            {"rotten_flesh", 3, 7, 25},
    {"spider_eye", 1, 3, 25},      {"leather", 1, 5, 20},         {"enchanted_book", 1, 1, 20, true},
    {"golden_apple", 1, 1, 20},    {"gold_ingot", 2, 7, 15},      {"iron_ingot", 1, 5, 15},
    {"emerald", 1, 3, 15},         {"copper_horse_armor", 1, 1, 15}, {"iron_horse_armor", 1, 1, 15},
    {"golden_horse_armor", 1, 1, 10}, {"diamond", 1, 3, 5},       {"diamond_horse_armor", 1, 1, 5},
    {"enchanted_golden_apple", 1, 1, 2}};
constexpr LootEntry kDesert2[] = {
    {"string", 1, 8, 10}, {"bone", 1, 8, 10}, {"sand", 1, 8, 10}, {"rotten_flesh", 1, 8, 10}, {"gunpowder", 1, 8, 10}};
constexpr LootEntry kDesert3[] = {{"", 1, 1, 6}, {"dune_armor_trim_smithing_template", 2, 2, 1}};
constexpr LootPool kDesert[] = {{2, 4, kDesert1}, {4, 4, kDesert2}, {1, 1, kDesert3}};

// wiki: Jungle Pyramid › Loot (Java Edition).
constexpr LootEntry kJungle1[] = {
    {"bone", 4, 6, 20},       {"rotten_flesh", 3, 7, 16},  {"gold_ingot", 2, 7, 15},
    {"bamboo", 1, 3, 15},     {"iron_ingot", 1, 5, 10},    {"leather", 1, 5, 3},
    {"diamond", 1, 3, 3},     {"emerald", 1, 3, 2},        {"copper_horse_armor", 1, 1, 1},
    {"enchanted_book", 1, 1, 1, true}, {"iron_horse_armor", 1, 1, 1}, {"golden_horse_armor", 1, 1, 1},
    {"diamond_horse_armor", 1, 1, 1}};
constexpr LootEntry kJungle2[] = {{"", 1, 1, 2}, {"wild_armor_trim_smithing_template", 2, 2, 1}};
constexpr LootPool kJungle[] = {{2, 6, kJungle1}, {1, 1, kJungle2}};

// wiki: Igloo › Loot (the basement chest; Java Edition).
constexpr LootEntry kIgloo1[] = {{"wheat", 2, 3, 10}, {"gold_nugget", 1, 3, 10}, {"rotten_flesh", 1, 1, 10},
                                 {"apple", 1, 3, 15}, {"coal", 1, 4, 15},        {"stone_axe", 1, 1, 2},
                                 {"emerald", 1, 1, 1}};
constexpr LootEntry kIgloo2[] = {{"golden_apple", 1, 1, 1}};
constexpr LootPool kIgloo[] = {{2, 8, kIgloo1}, {1, 1, kIgloo2}};

} // namespace

std::span<const LootPool> lootPools(LootTable table) {
    switch (table) {
    case LootTable::SimpleDungeon: return kDungeon;
    case LootTable::DesertPyramid: return kDesert;
    case LootTable::JunglePyramid: return kJungle;
    case LootTable::Igloo: return kIgloo;
    default: return {}; // (filled in as their structures arrive)
    }
}

void fillChest(LootTable table, Xoroshiro& rng, std::array<ItemStack, 27>& slots) {
    const auto& items = itemRegistry();
    std::array<ItemStack, 27> rolled{};
    int count = 0;
    for (const LootPool& pool : lootPools(table)) {
        int total = 0;
        for (const LootEntry& e : pool.entries)
            total += e.weight;
        if (total <= 0) continue;
        const int rolls = pool.minRolls + static_cast<int>(rng.nextInt(uint32_t(pool.maxRolls - pool.minRolls + 1)));
        for (int r = 0; r < rolls; ++r) {
            int pick = static_cast<int>(rng.nextInt(uint32_t(total)));
            const LootEntry* chosen = &pool.entries.front();
            for (const LootEntry& e : pool.entries) {
                if (pick < e.weight) {
                    chosen = &e;
                    break;
                }
                pick -= e.weight;
            }
            const int n = chosen->min + static_cast<int>(rng.nextInt(uint32_t(chosen->max - chosen->min + 1)));
            const auto id = chosen->item.empty() ? std::nullopt : items.find(chosen->item);
            if (!id || count >= 27) continue; // an item we don't have yet: nothing
            ItemStack s{*id, static_cast<uint8_t>(std::min<int>(n, items.item(*id).maxStack))};
            if (chosen->enchant) { // one random enchantment at a random level (vanilla enchant_randomly)
                const auto e = static_cast<Enchantment>(1 + rng.nextInt(uint32_t(Enchantment::Count) - 1));
                if (e != Enchantment::Thorns && canEnchant(*id, e))
                    setEnchantment(s, e, 1 + static_cast<int>(rng.nextInt(uint32_t(enchantmentInfo(e).maxLevel))));
            }
            rolled[size_t(count++)] = s;
        }
    }
    // Scatter into random empty slots.
    for (int i = 0; i < count; ++i) {
        int free = 0;
        for (const ItemStack& s : slots)
            free += s.empty();
        if (free == 0) break;
        int target = static_cast<int>(rng.nextInt(uint32_t(free)));
        for (ItemStack& s : slots)
            if (s.empty() && target-- == 0) {
                s = rolled[size_t(i)];
                break;
            }
    }
}

} // namespace mc::world
