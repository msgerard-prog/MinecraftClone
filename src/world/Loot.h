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
    PiglinBartering,
    NetherFortress,
    BastionOther,
    EndCityTreasure, // (M20.4)
    PillagerOutpost, // (M24.4)
    ShipwreckSupply, // (M25.4)
    ShipwreckMap,
    ShipwreckTreasure,
    UnderwaterRuinSmall,
    UnderwaterRuinBig,
    BuriedTreasure,
    AncientCity, // (M27.3b)
    RuinedPortal, // (M27.4b)
    WoodlandMansion, // (M27.4c)
    TrialVault,      // (M27.4d) a vault opened with a trial key
    TrialReward,     // (M27.4d) a trial spawner's reward (beside its key)
    TrialSupply,     // (M27.4d) the chambers' supply chests
    // Archaeology (M27.5): what brushing suspicious sand or gravel turns up.
    ArchaeologyDesertPyramid,
    ArchaeologyDesertWell,
    ArchaeologyOceanRuinCold,
    ArchaeologyOceanRuinWarm,
    ArchaeologyTrailCommon,
    ArchaeologyTrailRare,
    TrialVaultOminous, // (M28.4d) an ominous vault opened with an ominous trial key
    MineshaftSulfur, // (M33.2e; 26.2) a mineshaft chest in the sulfur caves: may hold "Bounce"
    Count
};
// Vanilla's loot table ids ("minecraft:chests/simple_dungeon", "minecraft:archaeology/
// desert_pyramid"...) for saves; and back (nullopt: one we don't have).
std::string_view lootTableName(LootTable table);
std::optional<LootTable> lootTableFromName(std::string_view name);

struct LootEntry {
    std::string_view item; // without "minecraft:"; "" = an empty entry
    uint8_t min = 1, max = 1;
    uint16_t weight = 1;
    bool enchant = false; // an enchanted book / item with one random enchantment
    uint8_t potion = 0;   // potion items: their world::Potion
    uint8_t enchantment = 0; // (M27.3b) this enchantment (world::Enchantment) at a random level
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
