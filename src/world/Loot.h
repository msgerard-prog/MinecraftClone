#pragma once

#include "world/Items.h"
#include "world/Random.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace mc::world {

// Chest loot tables of generated structures (M18.3), authored from each structure's
// wiki page ("Loot" tables: pools of rolls, entries with weights and stack sizes).
// Entries for items we don't have yet stay in the pool with their weight and give
// nothing, so the odds of everything else stay vanilla's.
enum class LootTable : uint8_t {
    SimpleDungeon,
    DesertPyramid,
    JunglePyramid,
    Igloo,
    Mineshaft,
    StrongholdCorridor, // the altar chests
    StrongholdCrossing, // storerooms
    StrongholdLibrary,
    VillagePlainsHouse,
    VillageDesertHouse,
    PiglinBartering
};

struct LootEntry {
    std::string_view item; // without "minecraft:"; "" = an empty entry
    uint8_t min = 1, max = 1;
    uint16_t weight = 1;
    bool enchant = false; // an enchanted book / item with one random enchantment
};
struct LootPool {
    uint8_t minRolls, maxRolls;
    std::span<const LootEntry> entries;
};

std::span<const LootPool> lootPools(LootTable table);

// Rolls the table and puts each stack into a random empty slot (vanilla scatters
// the loot through the chest).
void fillChest(LootTable table, Xoroshiro& rng, std::array<ItemStack, 27>& slots);
// One roll of a single-pool table (bartering); empty if it gave nothing.
ItemStack rollOne(LootTable table, Xoroshiro& rng);

} // namespace mc::world
