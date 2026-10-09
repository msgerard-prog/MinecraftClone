#include "world/Loot.h"

#include "world/Enchantments.h"
#include "world/Potions.h"

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

// wiki: Mineshaft › Loot (the chest minecarts; Java Edition).
constexpr LootEntry kMine1[] = {{"name_tag", 1, 1, 30},       {"golden_apple", 1, 1, 20},
                                {"enchanted_book", 1, 1, 10, true}, {"iron_pickaxe", 1, 1, 5},
                                {"enchanted_golden_apple", 1, 1, 1}, {"", 1, 1, 5}};
constexpr LootEntry kMine2[] = {{"glow_berries", 3, 6, 15},  {"bread", 1, 3, 15},          {"beetroot_seeds", 2, 4, 10},
                                {"coal", 3, 8, 10},          {"melon_seeds", 2, 4, 10},    {"pumpkin_seeds", 2, 4, 10},
                                {"iron_ingot", 1, 5, 10},    {"lapis_lazuli", 4, 9, 5},    {"redstone", 4, 9, 5},
                                {"gold_ingot", 1, 3, 5},     {"diamond", 1, 2, 3}};
constexpr LootEntry kMine3[] = {{"rail", 4, 8, 20},
                                {"torch", 1, 16, 15},
                                {"activator_rail", 1, 4, 5},
                                {"detector_rail", 1, 4, 5},
                                {"powered_rail", 1, 4, 5}};
constexpr LootPool kMine[] = {{1, 1, kMine1}, {2, 4, kMine2}, {3, 3, kMine3}};
// (M33.2e; wiki: Music Disc - "Bounce" in sulfur cave mineshafts; our odds: 1 chest in 3)
constexpr LootEntry kMineSulfur4[] = {{"music_disc_bounce", 1, 1, 1}, {"", 1, 1, 2}};
constexpr LootPool kMineSulfur[] = {{1, 1, kMine1}, {2, 4, kMine2}, {3, 3, kMine3}, {1, 1, kMineSulfur4}};
// (M33.3e; 26.3 abandoned camps - the wiki names the chests, not their odds: ours, a camper's
// supplies and, buried, their savings; explorer maps are left out - not in ours yet)
constexpr LootEntry kCamp1[] = {{"bread", 1, 3, 15},   {"apple", 1, 3, 10},  {"torch", 2, 8, 12},
                                {"string", 1, 4, 10},  {"leather", 1, 3, 8}, {"stick", 2, 6, 10},
                                {"wheat", 2, 5, 8},    {"coal", 1, 4, 8},    {"map", 1, 1, 4},
                                {"cooked_beef", 1, 2, 6}, {"red_shrub", 1, 2, 4}, {"poplar_sapling", 1, 2, 4}};
constexpr LootPool kCampCommon[] = {{3, 6, kCamp1}};
constexpr LootEntry kCamp2[] = {{"emerald", 1, 4, 12}, {"gold_ingot", 1, 3, 10}, {"iron_ingot", 1, 4, 12},
                                {"diamond", 1, 1, 2},  {"name_tag", 1, 1, 5},   {"golden_apple", 1, 1, 4},
                                {"book", 1, 2, 6},     {"compass", 1, 1, 4},    {"music_disc_bounce", 1, 1, 1}};
constexpr LootPool kCampSecret[] = {{2, 4, kCamp2}};

// wiki: Stronghold › Loot (Java Edition).
constexpr LootEntry kAltar1[] = {
    {"diamond_horse_armor", 1, 1, 1}, {"music_disc_otherside", 1, 1, 1}, {"gold_ingot", 1, 3, 5},
    {"iron_boots", 1, 1, 5},          {"iron_horse_armor", 1, 1, 1},     {"iron_pickaxe", 1, 1, 5},
    {"apple", 1, 3, 15},              {"iron_ingot", 1, 5, 10},          {"enchanted_book", 1, 1, 1, true},
    {"diamond", 1, 3, 3},             {"copper_horse_armor", 1, 1, 1},   {"golden_apple", 1, 1, 1},
    {"iron_helmet", 1, 1, 5},         {"iron_sword", 1, 1, 5},           {"leather", 1, 5, 1},
    {"ender_pearl", 1, 1, 10},        {"iron_leggings", 1, 1, 5},        {"redstone", 4, 9, 5},
    {"golden_horse_armor", 1, 1, 1},  {"iron_chestplate", 1, 1, 5},      {"bread", 1, 3, 15}};
constexpr LootEntry kAltar2[] = {{"eye_armor_trim_smithing_template", 1, 1, 1}, {"", 1, 1, 9}};
constexpr LootPool kAltar[] = {{2, 3, kAltar1}, {1, 1, kAltar2}};
constexpr LootEntry kStore1[] = {{"apple", 1, 3, 15},     {"bread", 1, 3, 15},      {"coal", 3, 8, 10},
                                 {"iron_ingot", 1, 5, 10}, {"redstone", 4, 9, 5},    {"gold_ingot", 1, 3, 5},
                                 {"enchanted_book", 1, 1, 1, true}, {"iron_pickaxe", 1, 1, 1}};
constexpr LootPool kStore[] = {{1, 4, kStore1}};
constexpr LootEntry kLibrary1[] = {{"map", 1, 1, 1},   {"enchanted_book", 1, 1, 10, true}, {"paper", 2, 7, 20},
                                   {"compass", 1, 1, 1}, {"book", 1, 3, 20}};
constexpr LootEntry kLibrary2[] = {{"eye_armor_trim_smithing_template", 1, 1, 1}};
constexpr LootPool kLibrary[] = {{2, 10, kLibrary1}, {1, 1, kLibrary2}};

// wiki: Village/Loot (Java Edition): plains and desert house chests.
constexpr LootEntry kPlainsHouse1[] = {{"potato", 1, 7, 10},     {"apple", 1, 5, 10},    {"bread", 1, 4, 10},
                                       {"oak_sapling", 1, 2, 5}, {"emerald", 1, 4, 2},   {"dandelion", 1, 1, 2},
                                       {"gold_nugget", 1, 3, 1}, {"book", 1, 1, 1},      {"feather", 1, 1, 1},
                                       {"poppy", 1, 1, 1}};
constexpr LootEntry kHouse2[] = {{"", 1, 1, 2}, {"bundle", 1, 1, 1}};
constexpr LootPool kPlainsHouse[] = {{3, 8, kPlainsHouse1}, {1, 1, kHouse2}};
constexpr LootEntry kDesertHouse1[] = {{"wheat", 1, 7, 10},   {"bread", 1, 4, 10},  {"cactus", 1, 4, 10},
                                       {"dead_bush", 1, 3, 2}, {"emerald", 1, 3, 1}, {"book", 1, 1, 1},
                                       {"clay_ball", 1, 1, 1}, {"green_dye", 1, 1, 1}};
constexpr LootPool kDesertHouse[] = {{3, 8, kDesertHouse1}, {1, 1, kHouse2}};

// wiki: Bartering (Java Edition; total 469 - the 1.21.6 dried ghast included). The
// potions are Fire Resistance, the bottle water. Items we don't have yet (iron nuggets,
// crying obsidian, nether bricks, spectral arrows, dried ghasts) give nothing; the
// book and boots come without Soul Speed (no such enchantment yet).
constexpr uint8_t kFireRes = static_cast<uint8_t>(Potion::FireResistance);
constexpr LootEntry kBarter1[] = {
    {"enchanted_book", 1, 1, 5},     {"iron_boots", 1, 1, 8},       {"potion", 1, 1, 8, false, kFireRes},
    {"splash_potion", 1, 1, 8, false, kFireRes}, {"potion", 1, 1, 10, false, static_cast<uint8_t>(Potion::Water)},    {"iron_nugget", 10, 36, 10},
    {"ender_pearl", 2, 4, 10},       {"string", 3, 9, 20},          {"quartz", 5, 12, 20},
    {"obsidian", 1, 1, 40},          {"crying_obsidian", 1, 3, 40}, {"fire_charge", 1, 1, 40},
    {"leather", 2, 4, 40},           {"soul_sand", 2, 8, 40},       {"nether_brick", 2, 8, 40},
    {"spectral_arrow", 6, 12, 40},   {"gravel", 8, 16, 40},         {"blackstone", 8, 16, 40},
    {"dried_ghast", 1, 1, 10}};
constexpr LootPool kBarter[] = {{1, 1, kBarter1}};

// wiki: Nether Fortress › Loot (Java Edition).
constexpr LootEntry kFortress1[] = {
    {"gold_ingot", 1, 3, 15},          {"saddle", 1, 1, 10},           {"golden_horse_armor", 1, 1, 8},
    {"nether_wart", 3, 7, 5},          {"iron_ingot", 1, 5, 5},        {"diamond", 1, 3, 5},
    {"copper_horse_armor", 1, 1, 5},   {"flint_and_steel", 1, 1, 5},   {"iron_horse_armor", 1, 1, 5},
    {"golden_sword", 1, 1, 5},         {"golden_chestplate", 1, 1, 5}, {"diamond_horse_armor", 1, 1, 3},
    {"obsidian", 2, 4, 2}};
constexpr LootEntry kFortress2[] = {{"", 1, 1, 14}, {"rib_armor_trim_smithing_template", 1, 1, 1}};
constexpr LootPool kFortress[] = {{2, 4, kFortress1}, {1, 1, kFortress2}};

// wiki: End City › Loot (end_city_treasure, 2-6 rolls; the gear enchanted at levels
// 20-39 in vanilla, ours with one random enchantment; the spear, copper horse armor
// and spire armor trim give nothing until those items exist).
constexpr LootEntry kEndCity1[] = {
    {"diamond", 2, 7, 5},          {"iron_ingot", 4, 8, 10},         {"gold_ingot", 2, 7, 15},
    {"emerald", 2, 6, 2},          {"beetroot_seeds", 1, 10, 5},     {"saddle", 1, 1, 3},
    {"iron_horse_armor", 1, 1, 1}, {"golden_horse_armor", 1, 1, 1},  {"diamond_horse_armor", 1, 1, 1},
    {"diamond_sword", 1, 1, 3, true},      {"diamond_boots", 1, 1, 3, true},     {"diamond_chestplate", 1, 1, 3, true},
    {"diamond_leggings", 1, 1, 3, true},   {"diamond_helmet", 1, 1, 3, true},    {"diamond_pickaxe", 1, 1, 3, true},
    {"diamond_shovel", 1, 1, 3, true},     {"iron_sword", 1, 1, 3, true},        {"iron_boots", 1, 1, 3, true},
    {"iron_chestplate", 1, 1, 3, true},    {"iron_leggings", 1, 1, 3, true},     {"iron_helmet", 1, 1, 3, true},
    {"iron_pickaxe", 1, 1, 3, true},       {"iron_shovel", 1, 1, 3, true},
    {"diamond_spear", 1, 1, 3, true},      {"copper_horse_armor", 1, 1, 1}};
constexpr LootEntry kEndCity2[] = {{"", 1, 1, 14}, {"spire_armor_trim_smithing_template", 1, 1, 1}};
constexpr LootPool kEndCity[] = {{2, 6, kEndCity1}, {1, 1, kEndCity2}};

// wiki: Pillager Outpost › Loot (authored from the wiki's table; goat horns and
// banner patterns left out).
constexpr LootEntry kOutpost1[] = {{"crossbow", 1, 1, 1}};
constexpr LootEntry kOutpost2[] = {{"wheat", 3, 5, 7}, {"potato", 2, 5, 5}, {"carrot", 3, 5, 5}};
constexpr LootEntry kOutpost3[] = {{"dark_oak_log", 2, 3, 1}};
constexpr LootEntry kOutpost4[] = {{"experience_bottle", 1, 1, 7}, {"string", 1, 6, 4}, {"arrow", 2, 7, 4},
                                   {"tripwire_hook", 1, 3, 3}, {"iron_ingot", 1, 3, 3}, {"book", 1, 1, 1, true}};
constexpr LootEntry kOutpost5[] = {{"", 1, 1, 3}, {"sentry_armor_trim_smithing_template", 2, 2, 1}};
constexpr LootPool kOutpost[] = {{0, 1, kOutpost1}, {2, 3, kOutpost2}, {1, 3, kOutpost3}, {2, 3, kOutpost4}, {1, 1, kOutpost5}};

// wiki: Shipwreck › Loot (Java Edition): supply, map and treasure chests. Maps and
// buried treasure maps don't exist yet: their slots stay empty (M28).
constexpr LootEntry kShipSupply1[] = {
    {"paper", 1, 12, 8},         {"potato", 2, 6, 7},        {"poisonous_potato", 2, 6, 7}, {"carrot", 4, 8, 7},
    {"wheat", 8, 21, 7},         {"coal", 2, 8, 6},          {"rotten_flesh", 5, 24, 5},    {"gunpowder", 1, 5, 3},
    {"pumpkin", 1, 3, 2},        {"bamboo", 1, 3, 2},        {"tnt", 1, 2, 1},              {"leather_helmet", 1, 1, 3, true},
    {"leather_chestplate", 1, 1, 3, true}, {"leather_leggings", 1, 1, 3, true}, {"leather_boots", 1, 1, 3, true}};
// (every shipwreck chest: 1 in 6, two coast armor trim templates - wiki)
constexpr LootEntry kShipTrim[] = {{"coast_armor_trim_smithing_template", 2, 2, 1}, {"", 1, 1, 5}};
constexpr LootPool kShipSupply[] = {{3, 10, kShipSupply1}, {1, 1, kShipTrim}};
constexpr LootEntry kShipMap1[] = {{"paper", 1, 10, 20}, {"feather", 1, 5, 10}, {"book", 1, 5, 5}};
constexpr LootPool kShipMap[] = {{3, 3, kShipMap1}, {1, 1, kShipTrim}};
constexpr LootEntry kShipTreasure1[] = {{"iron_ingot", 1, 5, 90}, {"gold_ingot", 1, 5, 10}, {"emerald", 1, 5, 40},
                                        {"diamond", 1, 1, 5}, {"experience_bottle", 1, 1, 5}};
constexpr LootEntry kShipTreasure2[] = {{"iron_nugget", 1, 10, 50}, {"gold_nugget", 1, 10, 10}, {"lapis_lazuli", 1, 10, 20}};
constexpr LootPool kShipTreasure[] = {{3, 6, kShipTreasure1}, {2, 5, kShipTreasure2}, {1, 1, kShipTrim}};
// wiki: Ocean Ruins › Loot (small and big ruin chests).
constexpr LootEntry kRuinSmall1[] = {{"coal", 1, 4, 10}, {"stone_axe", 1, 1, 2}, {"rotten_flesh", 1, 1, 5},
                                     {"emerald", 1, 1, 1}, {"wheat", 2, 3, 10}};
constexpr LootEntry kRuinSmall2[] = {{"leather_chestplate", 1, 1, 1}, {"golden_helmet", 1, 1, 1},
                                     {"fishing_rod", 1, 1, 5, true}};
constexpr LootPool kRuinSmall[] = {{2, 8, kRuinSmall1}, {1, 1, kRuinSmall2}};
constexpr LootEntry kRuinBig1[] = {{"coal", 1, 4, 10}, {"gold_nugget", 1, 3, 10}, {"emerald", 1, 1, 1}, {"wheat", 2, 3, 10}};
constexpr LootEntry kRuinBig2[] = {{"golden_apple", 1, 1, 1}, {"enchanted_book", 1, 1, 5, true},
                                   {"leather_chestplate", 1, 1, 1}, {"golden_helmet", 1, 1, 1},
                                   {"fishing_rod", 1, 1, 5, true}};
constexpr LootPool kRuinBig[] = {{2, 8, kRuinBig1}, {1, 1, kRuinBig2}};
// wiki: Buried Treasure › Loot - always a heart of the sea.
constexpr LootEntry kBuried1[] = {{"heart_of_the_sea", 1, 1, 1}};
constexpr LootEntry kBuried2[] = {{"iron_ingot", 1, 4, 20}, {"gold_ingot", 1, 4, 10}, {"tnt", 1, 2, 5}};
constexpr LootEntry kBuried3[] = {{"emerald", 4, 8, 5}, {"diamond", 1, 2, 5}, {"prismarine_crystals", 1, 5, 5}};
constexpr LootEntry kBuried4[] = {{"leather_chestplate", 1, 1, 1}, {"iron_sword", 1, 1, 1}};
constexpr LootEntry kBuried5[] = {{"cooked_cod", 2, 4, 1}, {"cooked_salmon", 2, 4, 1}};
constexpr LootEntry kBuried6[] = {{"potion", 1, 1, 1, false, uint8_t(Potion::WaterBreathing)}}; // (0-2, wiki)
constexpr LootPool kBuried[] = {{1, 1, kBuried1}, {5, 8, kBuried2}, {1, 3, kBuried3}, {0, 1, kBuried4}, {2, 2, kBuried5},
                                {0, 2, kBuried6}};

// wiki: Ancient City › Loot (Java 1.21.11; the compass, disc fragments and disc 5 aren't
// in the game yet: those entries give nothing, keeping the others' odds).
constexpr LootEntry kAncient1[] = {
    {"coal", 6, 15, 7},             {"bone", 1, 15, 5},            {"soul_torch", 1, 15, 5},
    {"book", 3, 10, 5},             {"potion", 1, 3, 5, false, uint8_t(Potion::Regeneration)},
    {"enchanted_book", 1, 1, 5, true}, {"echo_shard", 1, 3, 4},    {"amethyst_shard", 1, 15, 3},
    {"glow_berries", 1, 15, 3},     {"sculk", 4, 10, 3},           {"candle", 1, 4, 3},
    {"experience_bottle", 1, 3, 3}, {"sculk_sensor", 1, 3, 3},
    {"enchanted_book", 1, 1, 3, false, 0, uint8_t(Enchantment::SwiftSneak)}, {"iron_leggings", 1, 1, 3, true},
    {"leather", 1, 5, 2},           {"sculk_catalyst", 1, 2, 2},   {"compass", 1, 1, 2},
    {"name_tag", 1, 1, 2},          {"music_disc_13", 1, 1, 2},    {"music_disc_cat", 1, 1, 2},
    {"lead", 1, 1, 2},              {"diamond_hoe", 1, 1, 2, true}, {"diamond_horse_armor", 1, 1, 2},
    {"diamond_leggings", 1, 1, 2, true}, {"disc_fragment_5", 1, 3, 4},
    {"enchanted_golden_apple", 1, 2, 1}, {"music_disc_otherside", 1, 1, 1}};
constexpr LootEntry kAncient2[] = {{"ward_armor_trim_smithing_template", 1, 1, 4},
                                   {"silence_armor_trim_smithing_template", 1, 1, 1},
                                   {"", 1, 1, 75}};
constexpr LootPool kAncientCity[] = {{5, 10, kAncient1}, {1, 1, kAncient2}};

// wiki: Ruined Portal › Loot (Java).
constexpr LootEntry kRuined1[] = {
    {"obsidian", 1, 2, 40},        {"flint", 1, 4, 40},          {"iron_nugget", 9, 18, 40},
    {"flint_and_steel", 1, 1, 40}, {"fire_charge", 1, 1, 40},    {"golden_apple", 1, 1, 15},
    {"gold_nugget", 4, 24, 15},    {"golden_sword", 1, 1, 15, true}, {"golden_axe", 1, 1, 15, true},
    {"golden_hoe", 1, 1, 15, true}, {"golden_shovel", 1, 1, 15, true}, {"golden_pickaxe", 1, 1, 15, true},
    {"golden_boots", 1, 1, 15, true}, {"golden_chestplate", 1, 1, 15, true}, {"golden_helmet", 1, 1, 15, true},
    {"golden_leggings", 1, 1, 15, true}, {"glistering_melon_slice", 4, 12, 5}, {"golden_horse_armor", 1, 1, 5},
    {"light_weighted_pressure_plate", 1, 1, 5}, {"golden_carrot", 4, 12, 5}, {"clock", 1, 1, 5},
    {"gold_ingot", 2, 8, 5},       {"bell", 1, 1, 1},            {"enchanted_golden_apple", 1, 1, 1},
    {"gold_block", 1, 2, 1}};
constexpr LootEntry kRuined2[] = {{"lodestone", 1, 2, 2}, {"", 1, 1, 1}}; // (since 1.21.5; no lodestone yet)
constexpr LootPool kRuinedPortal[] = {{4, 8, kRuined1}, {1, 1, kRuined2}};

// wiki: Woodland Mansion › Loot (Java).
constexpr LootEntry kMansion1[] = {
    {"lead", 1, 1, 20},           {"golden_apple", 1, 1, 15},     {"music_disc_13", 1, 1, 15},
    {"music_disc_cat", 1, 1, 15}, {"name_tag", 1, 1, 20},         {"chainmail_chestplate", 1, 1, 10},
    {"diamond_hoe", 1, 1, 15},    {"diamond_chestplate", 1, 1, 5}, {"enchanted_book", 1, 1, 10, true},
    {"enchanted_golden_apple", 1, 1, 2}};
constexpr LootEntry kMansion2[] = {
    {"iron_ingot", 1, 4, 10},   {"gold_ingot", 1, 4, 5},      {"bread", 1, 1, 20},       {"wheat", 1, 4, 20},
    {"bucket", 1, 1, 10},       {"redstone", 1, 4, 15},       {"coal", 1, 4, 15},        {"melon_seeds", 2, 4, 10},
    {"pumpkin_seeds", 2, 4, 10}, {"beetroot_seeds", 2, 4, 10}, {"resin_clump", 2, 4, 50}};
constexpr LootEntry kMansion3[] = {{"bone", 1, 8, 10}, {"gunpowder", 1, 8, 10}, {"rotten_flesh", 1, 8, 10}, {"string", 1, 8, 10}};
constexpr LootEntry kMansion4[] = {{"vex_armor_trim_smithing_template", 1, 1, 1}, {"", 1, 1, 1}};
constexpr LootPool kMansion[] = {{1, 3, kMansion1}, {1, 4, kMansion2}, {3, 3, kMansion3}, {1, 1, kMansion4}};

// wiki: Vault › Loot, Trial Spawner › Loot, Trial Chambers › Loot (Java; ours: without
// ominous bottles, guster banner patterns, heavy cores and the mace - M28 - and without
// tipped arrows).
constexpr LootEntry kVault1[] = {
    {"emerald", 2, 4, 5},        {"arrow", 2, 8, 4},      {"iron_ingot", 1, 2, 4},       {"wind_charge", 1, 3, 4},
    {"honey_bottle", 1, 2, 4},   {"shield", 1, 1, 3},     {"bow", 1, 1, 3, true},        {"diamond", 1, 2, 2},
    {"golden_apple", 1, 1, 2},   {"golden_carrot", 1, 2, 2}, {"enchanted_book", 1, 1, 2, true},
    {"crossbow", 1, 1, 2, true}, {"iron_axe", 1, 1, 2, true}, {"iron_chestplate", 1, 1, 2, true},
    {"diamond_axe", 1, 1, 1, true}, {"enchanted_golden_apple", 1, 1, 1}};
constexpr LootPool kVault[] = {{1, 3, kVault1}};
// (M28.4d; wiki: Vault › Ominous loot - ours: the common and rare items together, then a
// quarter of the time a unique: the heavy core, the flow banner pattern or an enchanted
// golden apple)
constexpr LootEntry kVaultOminous1[] = {
    {"emerald", 4, 10, 5},       {"wind_charge", 8, 12, 4},  {"diamond", 2, 3, 3},       {"ominous_bottle", 1, 1, 2},
    {"emerald_block", 1, 1, 2},  {"iron_block", 1, 1, 2},    {"golden_apple", 1, 1, 2},  {"crossbow", 1, 1, 2, true},
    {"diamond_axe", 1, 1, 1, true}, {"diamond_chestplate", 1, 1, 1, true}, {"enchanted_book", 1, 1, 2, true},
    {"diamond_block", 1, 1, 1}};
constexpr LootEntry kVaultOminous2[] = {{"heavy_core", 1, 1, 1}, {"flow_banner_pattern", 1, 1, 2},
                                        {"enchanted_golden_apple", 1, 1, 3}, {"", 1, 1, 18}};
constexpr LootPool kVaultOminous[] = {{1, 3, kVaultOminous1}, {1, 1, kVaultOminous2}};
// (a trial spawner's consumable - given instead of the key half the time)
constexpr LootEntry kTrialReward1[] = {
    {"cooked_chicken", 1, 1, 3}, {"bread", 1, 3, 3}, {"baked_potato", 1, 3, 2},
    {"potion", 1, 1, 1, false, uint8_t(Potion::Regeneration)}, {"potion", 1, 1, 1, false, uint8_t(Potion::Swiftness)}};
constexpr LootPool kTrialReward[] = {{1, 1, kTrialReward1}};
constexpr LootEntry kTrialSupply1[] = {
    {"arrow", 4, 14, 2},     {"glow_berries", 2, 10, 2}, {"baked_potato", 2, 4, 2}, {"stone_pickaxe", 1, 1, 2},
    {"tuff", 8, 20, 1},      {"", 1, 1, 1}, // (tipped arrows: not in the game yet)
    {"acacia_planks", 3, 6, 1}, {"torch", 3, 6, 1}, {"bone_meal", 2, 5, 1},    {"moss_block", 2, 5, 1},
    {"potion", 1, 1, 1, false, uint8_t(Potion::Strength)}, {"potion", 1, 1, 1, false, uint8_t(Potion::Regeneration)},
    {"milk_bucket", 1, 1, 1}};
constexpr LootPool kTrialSupply[] = {{3, 5, kTrialSupply1}};

// wiki: Archaeology (Java) - one item from each table when brushed.
constexpr LootEntry kArchPyramid1[] = {{"archer_pottery_sherd"}, {"miner_pottery_sherd"}, {"prize_pottery_sherd"},
                                       {"skull_pottery_sherd"},  {"diamond"},             {"tnt"},
                                       {"gunpowder"},            {"emerald"}};
constexpr LootPool kArchPyramid[] = {{1, 1, kArchPyramid1}};
constexpr LootEntry kArchWell1[] = {{"arms_up_pottery_sherd", 1, 1, 2}, {"brewer_pottery_sherd", 1, 1, 2},
                                    {"brick"}, {"emerald"}, {"stick"}, {"suspicious_stew"}};
constexpr LootPool kArchWell[] = {{1, 1, kArchWell1}};
constexpr LootEntry kArchCold1[] = {
    {"blade_pottery_sherd"}, {"explorer_pottery_sherd"}, {"mourner_pottery_sherd"}, {"plenty_pottery_sherd"},
    {"iron_axe"},            {"emerald", 1, 1, 2},       {"wheat", 1, 1, 2},         {"wooden_hoe", 1, 1, 2},
    {"coal", 1, 1, 2},       {"gold_nugget", 1, 1, 2}};
constexpr LootPool kArchCold[] = {{1, 1, kArchCold1}};
constexpr LootEntry kArchWarm1[] = {
    {"angler_pottery_sherd"}, {"shelter_pottery_sherd"}, {"snort_pottery_sherd"}, {"sniffer_egg"},
    {"iron_axe"},             {"emerald", 1, 1, 2},      {"wheat", 1, 1, 2},      {"wooden_hoe", 1, 1, 2},
    {"coal", 1, 1, 2},        {"gold_nugget", 1, 1, 2}};
constexpr LootPool kArchWarm[] = {{1, 1, kArchWarm1}};
constexpr LootEntry kArchTrailCommon1[] = {
    {"emerald", 1, 1, 2},        {"wheat", 1, 1, 2},           {"wooden_hoe", 1, 1, 2},   {"clay", 1, 1, 2},
    {"brick", 1, 1, 2},          {"yellow_dye", 1, 1, 2},      {"blue_dye", 1, 1, 2},     {"light_blue_dye", 1, 1, 2},
    {"white_dye", 1, 1, 2},      {"orange_dye", 1, 1, 2},      {"red_candle", 1, 1, 2},   {"green_candle", 1, 1, 2},
    {"purple_candle", 1, 1, 2},  {"brown_candle", 1, 1, 2},    {"magenta_stained_glass_pane"}, {"pink_stained_glass_pane"},
    {"blue_stained_glass_pane"}, {"light_blue_stained_glass_pane"}, {"red_stained_glass_pane"},
    {"yellow_stained_glass_pane"}, {"purple_stained_glass_pane"}, {"spruce_hanging_sign"}, {"oak_hanging_sign"},
    {"gold_nugget"},             {"coal"},                     {"wheat_seeds"},           {"beetroot_seeds"},
    {"dead_bush"},               {"flower_pot"},               {"string"},                {"lead"}};
constexpr LootPool kArchTrailCommon[] = {{1, 1, kArchTrailCommon1}};
constexpr LootEntry kArchTrailRare1[] = {
    {"burn_pottery_sherd"}, {"danger_pottery_sherd"}, {"friend_pottery_sherd"}, {"heart_pottery_sherd"},
    {"heartbreak_pottery_sherd"}, {"howl_pottery_sherd"}, {"sheaf_pottery_sherd"},
    {"wayfinder_armor_trim_smithing_template"}, {"raiser_armor_trim_smithing_template"},
    {"shaper_armor_trim_smithing_template"}, {"host_armor_trim_smithing_template"}, {"music_disc_relic"}};
constexpr LootPool kArchTrailRare[] = {{1, 1, kArchTrailRare1}};

// wiki: Bastion Remnant › Loot, the generic chests (Java Edition). Enchanted/damaged
// gear comes plain or with one random enchantment here.
constexpr LootEntry kBastion1[] = {
    {"spectral_arrow", 10, 22, 10}, {"golden_carrot", 6, 17, 12},   {"ancient_debris", 1, 1, 12},
    {"enchanted_book", 1, 1, 10, true}, {"snout_banner_pattern", 1, 1, 9}, {"golden_apple", 1, 1, 9},
    {"crossbow", 1, 1, 6},          {"diamond_shovel", 1, 1, 6},    {"diamond_pickaxe", 1, 1, 6, true},
    {"music_disc_pigstep", 1, 1, 5}, {"netherite_scrap", 1, 1, 4}};
constexpr LootEntry kBastion2[] = {
    {"iron_ingot", 1, 6, 2},   {"gold_ingot", 1, 6, 2},         {"crying_obsidian", 1, 5, 2}, {"iron_block", 1, 1, 2},
    {"iron_sword", 1, 1, 2, true}, {"gold_block", 1, 1, 2},     {"crossbow", 1, 1, 1},        {"golden_sword", 1, 1, 1},
    {"golden_axe", 1, 1, 1, true}, {"golden_helmet", 1, 1, 1},  {"golden_chestplate", 1, 1, 1},
    {"golden_leggings", 1, 1, 1}, {"golden_boots", 1, 1, 1},    {"golden_boots", 1, 1, 1, true}};
constexpr LootEntry kBastion3[] = {
    {"arrow", 5, 17, 2},  {"magma_cream", 2, 6, 2},  {"gilded_blackstone", 1, 5, 2}, {"iron_chain", 2, 10, 1},
    {"obsidian", 4, 6, 1}, {"string", 4, 6, 1},      {"iron_nugget", 2, 8, 1},       {"gold_nugget", 2, 8, 1},
    {"bone_block", 3, 6, 1}, {"cooked_porkchop", 1, 1, 1}};
constexpr LootEntry kBastion4[] = {{"snout_armor_trim_smithing_template", 1, 1, 1}, {"", 1, 1, 11}};
constexpr LootEntry kBastion5[] = {{"netherite_upgrade_smithing_template", 1, 1, 1}, {"", 1, 1, 9}};
constexpr LootPool kBastion[] = {{1, 1, kBastion1}, {2, 2, kBastion2}, {3, 4, kBastion3}, {1, 1, kBastion4}, {1, 1, kBastion5}};

} // namespace

std::span<const LootPool> lootPools(LootTable table) {
    switch (table) {
    case LootTable::SimpleDungeon: return kDungeon;
    case LootTable::DesertPyramid: return kDesert;
    case LootTable::JunglePyramid: return kJungle;
    case LootTable::Igloo: return kIgloo;
    case LootTable::Mineshaft: return kMine;
    case LootTable::MineshaftSulfur: return kMineSulfur;
    case LootTable::CampCommon: return kCampCommon;
    case LootTable::CampSecret: return kCampSecret;
    case LootTable::StrongholdCorridor: return kAltar;
    case LootTable::StrongholdCrossing: return kStore;
    case LootTable::StrongholdLibrary: return kLibrary;
    case LootTable::VillagePlainsHouse: return kPlainsHouse;
    case LootTable::VillageDesertHouse: return kDesertHouse;
    case LootTable::PiglinBartering: return kBarter;
    case LootTable::NetherFortress: return kFortress;
    case LootTable::BastionOther: return kBastion;
    case LootTable::EndCityTreasure: return kEndCity;
    case LootTable::PillagerOutpost: return kOutpost;
    case LootTable::ShipwreckSupply: return kShipSupply;
    case LootTable::ShipwreckMap: return kShipMap;
    case LootTable::ShipwreckTreasure: return kShipTreasure;
    case LootTable::UnderwaterRuinSmall: return kRuinSmall;
    case LootTable::UnderwaterRuinBig: return kRuinBig;
    case LootTable::BuriedTreasure: return kBuried;
    case LootTable::AncientCity: return kAncientCity;
    case LootTable::RuinedPortal: return kRuinedPortal;
    case LootTable::WoodlandMansion: return kMansion;
    case LootTable::TrialVault: return kVault;
    case LootTable::TrialReward: return kTrialReward;
    case LootTable::TrialSupply: return kTrialSupply;
    case LootTable::ArchaeologyDesertPyramid: return kArchPyramid;
    case LootTable::ArchaeologyDesertWell: return kArchWell;
    case LootTable::ArchaeologyOceanRuinCold: return kArchCold;
    case LootTable::ArchaeologyOceanRuinWarm: return kArchWarm;
    case LootTable::ArchaeologyTrailCommon: return kArchTrailCommon;
    case LootTable::ArchaeologyTrailRare: return kArchTrailRare;
    case LootTable::TrialVaultOminous: return kVaultOminous;
    default: return {}; // (filled in as their structures arrive)
    }
}

ItemStack rollOne(LootTable table, Xoroshiro& rng) {
    const auto pools = lootPools(table);
    if (pools.empty()) return {};
    int total = 0;
    for (const LootEntry& e : pools[0].entries)
        total += e.weight;
    int pick = static_cast<int>(rng.nextInt(uint32_t(total)));
    for (const LootEntry& e : pools[0].entries) {
        if (pick >= e.weight) {
            pick -= e.weight;
            continue;
        }
        const int n = e.min + static_cast<int>(rng.nextInt(uint32_t(e.max - e.min + 1)));
        const auto id = e.item.empty() ? std::nullopt : itemRegistry().find(e.item);
        if (!id) return {};
        ItemStack s{*id, static_cast<uint8_t>(std::min<int>(n, itemRegistry().item(*id).maxStack))};
        s.potion = e.potion;
        return s;
    }
    return {};
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
            s.potion = chosen->potion;
            if (chosen->enchantment != 0) { // a given enchantment at a random level
                const auto e = static_cast<Enchantment>(chosen->enchantment);
                setEnchantment(s, e, 1 + static_cast<int>(rng.nextInt(uint32_t(enchantmentInfo(e).maxLevel))));
            } else if (chosen->enchant) { // one random enchantment at a random level (vanilla enchant_randomly)
                for (int tries = 0; tries < 64; ++tries) { // (Thorns has no effect yet: never handed out)
                    const auto e = static_cast<Enchantment>(1 + rng.nextInt(kRandomEnchantments));
                    if (e == Enchantment::Thorns || !canEnchant(*id, e)) continue;
                    setEnchantment(s, e, 1 + static_cast<int>(rng.nextInt(uint32_t(enchantmentInfo(e).maxLevel))));
                    break;
                }
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

namespace {
constexpr std::string_view kTableNames[] = {
    "chests/simple_dungeon",     "chests/desert_pyramid",       "chests/jungle_temple",
    "chests/igloo_chest",        "chests/abandoned_mineshaft",  "chests/stronghold_corridor",
    "chests/stronghold_crossing", "chests/stronghold_library",  "chests/village/village_plains_house",
    "chests/village/village_desert_house", "gameplay/piglin_bartering", "chests/nether_bridge",
    "chests/bastion_other",      "chests/end_city_treasure",    "chests/pillager_outpost",
    "chests/shipwreck_supply",   "chests/shipwreck_map",        "chests/shipwreck_treasure",
    "chests/underwater_ruin_small", "chests/underwater_ruin_big", "chests/buried_treasure",
    "chests/ancient_city",       "chests/ruined_portal",        "chests/woodland_mansion",
    "chests/trial_chambers/reward", "spawners/trial_chamber/consumables", "chests/trial_chambers/supply",
    "archaeology/desert_pyramid", "archaeology/desert_well",    "archaeology/ocean_ruin_cold",
    "archaeology/ocean_ruin_warm", "archaeology/trail_ruins_common", "archaeology/trail_ruins_rare",
    "chests/trial_chambers/reward_ominous", "chests/abandoned_mineshaft_sulfur_caves",
    "chests/abandoned_camp/common", "chests/abandoned_camp/secret"}; // (ours: names after the wiki's chest kinds) // (ours: vanilla rolls the disc
                                                                                       //  in its mineshaft table)
static_assert(std::size(kTableNames) == size_t(LootTable::Count));
} // namespace

std::string_view lootTableName(LootTable table) {
    static const auto names = [] {
        std::array<std::string, size_t(LootTable::Count)> n;
        for (size_t i = 0; i < n.size(); ++i) n[i] = "minecraft:" + std::string(kTableNames[i]);
        return n;
    }();
    return names[size_t(table)];
}

std::optional<LootTable> lootTableFromName(std::string_view name) {
    if (name.starts_with("minecraft:")) name.remove_prefix(10);
    for (size_t i = 0; i < std::size(kTableNames); ++i)
        if (kTableNames[i] == name) return LootTable(i);
    return std::nullopt;
}

} // namespace mc::world
