#include "gameplay/Recipes.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <algorithm>
#include <array>
#include <string>

namespace mc {

using namespace world;

namespace {

ItemId id(std::string_view name) { return *itemRegistry().find(name); }
std::string_view nameOf(ItemId i) {
    std::string_view n = itemRegistry().item(i).id;
    if (n.starts_with("minecraft:")) n.remove_prefix(10);
    return n;
}

Ingredient item(std::string_view name) { return {Ingredient::Kind::Item, id(name)}; }
const Ingredient kNone{};
const Ingredient kPlanks{Ingredient::Kind::Planks};
const Ingredient kLogs{Ingredient::Kind::Logs};
const Ingredient kWoodenSlab{Ingredient::Kind::WoodenSlab};
const Ingredient kCoal{Ingredient::Kind::Coal};
const Ingredient kStoneTool{Ingredient::Kind::StoneTool};

// A shaped recipe from rows of keys ('.' = empty).
Recipe shaped(std::initializer_list<std::string_view> rows,
              std::initializer_list<std::pair<char, Ingredient>> keys, std::string_view result, int count = 1) {
    Recipe r;
    r.height = static_cast<int>(rows.size());
    r.width = static_cast<int>(rows.begin()->size());
    for (std::string_view row : rows)
        for (char c : row) {
            Ingredient in = kNone;
            for (const auto& [k, v] : keys)
                if (k == c) in = v;
            r.pattern.push_back(in);
        }
    r.result = {id(result), static_cast<uint8_t>(count)};
    return r;
}

Recipe shapeless(std::initializer_list<Ingredient> list, std::string_view result, int count = 1) {
    Recipe r;
    r.pattern.assign(list);
    r.result = {id(result), static_cast<uint8_t>(count)};
    return r;
}

std::vector<Recipe> build() {
    std::vector<Recipe> r;
    // Wood (wiki: Planks, Stick, Crafting Table).
    // Every log, wood and stripped variant makes 4 planks; 4 logs make 3 wood (wiki:
    // Planks, Wood; M23.3b adds wood, stripped logs, mangrove, pale oak, the stems).
    for (std::string_view w : {"oak", "birch", "spruce", "acacia", "jungle", "dark_oak", "cherry", "mangrove", "pale_oak",
                               "crimson", "warped"}) {
        const std::string wood(w);
        const bool nether = wood == "crimson" || wood == "warped";
        const std::string log = nether ? "_stem" : "_log", bark = nether ? "_hyphae" : "_wood";
        for (const std::string& in : {wood + log, wood + bark, "stripped_" + wood + log, "stripped_" + wood + bark})
            r.push_back(shapeless({item(in)}, wood + "_planks", 4));
        r.push_back(shaped({"##", "##"}, {{'#', item(wood + log)}}, wood + bark, 3));
        r.push_back(shaped({"##", "##"}, {{'#', item("stripped_" + wood + log)}}, "stripped_" + wood + bark, 3));
    }
    // Bamboo (wiki: Bamboo, Block of Bamboo, Bamboo Mosaic): 9 make a block, a block 2
    // planks, 2 bamboo a stick.
    r.push_back(shaped({"###", "###", "###"}, {{'#', item("bamboo")}}, "bamboo_block"));
    r.push_back(shapeless({item("bamboo_block")}, "bamboo_planks", 2));
    r.push_back(shapeless({item("stripped_bamboo_block")}, "bamboo_planks", 2));
    r.push_back(shaped({"#", "#"}, {{'#', item("bamboo")}}, "stick"));
    r.push_back(shaped({"#", "#"}, {{'#', item("bamboo_slab")}}, "bamboo_mosaic"));
    r.push_back(shaped({"#", "#"}, {{'#', kPlanks}}, "stick", 4));
    r.push_back(shaped({"##", "##"}, {{'#', kPlanks}}, "crafting_table"));
    // Light and stations (wiki: Torch, Furnace).
    r.push_back(shaped({"C", "S"}, {{'C', kCoal}, {'S', item("stick")}}, "torch", 4));
    r.push_back(shaped({"###", "#.#", "###"}, {{'#', kStoneTool}}, "furnace"));
    r.push_back(shaped({"###", "#.#", "###"}, {{'#', kPlanks}}, "chest")); // wiki: Chest
    // Workstations 1 (M23.5; wiki: Smoker, Blast Furnace, Barrel).
    r.push_back(shaped({".L.", "LFL", ".L."}, {{'L', kLogs}, {'F', item("furnace")}}, "smoker"));
    r.push_back(shaped({"III", "IFI", "SSS"},
                       {{'I', item("iron_ingot")}, {'F', item("furnace")}, {'S', item("smooth_stone")}}, "blast_furnace"));
    r.push_back(shaped({"PSP", "P.P", "PSP"}, {{'P', kPlanks}, {'S', kWoodenSlab}}, "barrel"));
    r.push_back(shaped({"S.S", "S.S", "SSS"}, {{'S', kWoodenSlab}}, "composter")); // wiki: Composter
    r.push_back(shaped({"I.I", "I.I", "III"}, {{'I', item("iron_ingot")}}, "cauldron")); // wiki: Cauldron
    // M23.6 (wiki: Ender Chest, Shulker Box): 8 obsidian around an eye; shells over a
    // chest; any shulker box with a dye becomes that colour (contents kept: craft()).
    r.push_back(shaped({"###", "#E#", "###"}, {{'#', item("obsidian")}, {'E', item("ender_eye")}}, "ender_chest"));
    // Netherite (wiki: Netherite Ingot - 4 scrap and 4 gold; Smithing Template - copied
    // with 7 diamonds and its material; Smithing Table).
    r.push_back(shapeless({item("netherite_scrap"), item("netherite_scrap"), item("netherite_scrap"),
                           item("netherite_scrap"), item("gold_ingot"), item("gold_ingot"), item("gold_ingot"),
                           item("gold_ingot")},
                          "netherite_ingot"));
    r.push_back(shaped({"###", "###", "###"}, {{'#', item("netherite_ingot")}}, "netherite_block"));
    r.push_back(shapeless({item("netherite_block")}, "netherite_ingot", 9));
    r.push_back(shaped({"DTD", "DND", "DDD"},
                       {{'D', item("diamond")}, {'T', item("netherite_upgrade_smithing_template")}, {'N', item("netherrack")}},
                       "netherite_upgrade_smithing_template", 2));
    // Armor trim templates copy like the netherite one, each with its own block (wiki:
    // Smithing Template; flow needs breeze rods - not in the game yet).
    for (const auto& [pattern, block] :
         {std::pair{"sentry", "cobblestone"}, std::pair{"dune", "sandstone"}, std::pair{"coast", "cobblestone"},
          std::pair{"wild", "mossy_cobblestone"}, std::pair{"ward", "cobbled_deepslate"}, std::pair{"eye", "end_stone"},
          std::pair{"vex", "cobblestone"}, std::pair{"tide", "prismarine"}, std::pair{"snout", "blackstone"},
          std::pair{"rib", "netherrack"}, std::pair{"spire", "purpur_block"}, std::pair{"wayfinder", "terracotta"},
          std::pair{"shaper", "terracotta"}, std::pair{"silence", "cobbled_deepslate"}, std::pair{"raiser", "terracotta"},
          std::pair{"host", "terracotta"}, std::pair{"bolt", "copper_block"}}) {
        const std::string t = std::string(pattern) + "_armor_trim_smithing_template";
        if (!itemRegistry().find(block)) continue;
        r.push_back(shaped({"DTD", "DBD", "DDD"}, {{'D', item("diamond")}, {'T', item(t)}, {'B', item(block)}}, t, 2));
    }
    r.push_back(shaped({"II", "PP", "PP"}, {{'I', item("iron_ingot")}, {'P', kPlanks}}, "smithing_table"));
    r.push_back(shaped({"GGG", "GAG", "GGG"}, {{'G', item("gold_ingot")}, {'A', item("apple")}}, "golden_apple")); // wiki
    r.push_back(shaped({"PPP", "PRP", "PPP"}, {{'P', kPlanks}, {'R', item("redstone")}}, "note_block")); // wiki
    // Job sites (M24.1; wiki: Lectern - wooden slabs over a bookshelf; Fletching Table).
    r.push_back(shaped({"SSS", ".B.", ".S."}, {{'S', kWoodenSlab}, {'B', item("bookshelf")}}, "lectern"));
    r.push_back(shaped({"FF", "PP", "PP"}, {{'F', item("flint")}, {'P', kPlanks}}, "fletching_table"));
    r.push_back(shaped({"PPP", "PDP", "PPP"}, {{'P', kPlanks}, {'D', item("diamond")}}, "jukebox"));     // wiki
    r.push_back(shaped({"SS", "PP"}, {{'S', item("string")}, {'P', kPlanks}}, "loom"));                   // wiki: Loom
    r.push_back(shaped({"AA", "PP", "PP"}, {{'A', item("paper")}, {'P', kPlanks}}, "cartography_table")); // wiki

    r.push_back(shaped({"S", "C", "S"}, {{'S', item("shulker_shell")}, {'C', item("chest")}}, "shulker_box"));
    for (const char* colour : kDyeColours) {
        const std::string c(colour);
        for (const char* from : {"shulker_box"})
            r.push_back(shapeless({item(from), item(c + "_dye")}, c + "_shulker_box"));
        for (const char* other : kDyeColours)
            if (std::string(other) != c)
                r.push_back(shapeless({item(std::string(other) + "_shulker_box"), item(c + "_dye")}, c + "_shulker_box"));
    }
    r.push_back(shaped({".I.", "SSS"}, {{'I', item("iron_ingot")}, {'S', item("stone")}}, "stonecutter"));
    r.push_back(shaped({"T#T", "P.P"}, {{'T', item("stick")}, {'#', item("stone_slab")}, {'P', kPlanks}}, "grindstone"));
    r.push_back(shaped({"WWW", "PPP"}, {{'W', item("red_wool")}, {'P', kPlanks}}, "red_bed")); // wiki: Bed
    // Tools (wiki: Pickaxe, Axe, Shovel, Hoe, Sword), per material.
    const std::pair<const char*, Ingredient> materials[] = {{"wooden", kPlanks},
                                                            {"stone", kStoneTool},
                                                            {"iron", item("iron_ingot")},
                                                            {"golden", item("gold_ingot")},
                                                            {"diamond", item("diamond")},
                                                            {"copper", item("copper_ingot")}};
    const Ingredient stick = item("stick");
    for (const auto& [mat, m] : materials) {
        const std::string p(mat);
        r.push_back(shaped({"###", ".S.", ".S."}, {{'#', m}, {'S', stick}}, p + "_pickaxe"));
        r.push_back(shaped({"##", "#S", ".S"}, {{'#', m}, {'S', stick}}, p + "_axe"));
        r.push_back(shaped({"#", "S", "S"}, {{'#', m}, {'S', stick}}, p + "_shovel"));
        r.push_back(shaped({"##", ".S", ".S"}, {{'#', m}, {'S', stick}}, p + "_hoe"));
        r.push_back(shaped({"#", "#", "S"}, {{'#', m}, {'S', stick}}, p + "_sword"));
    }
    // Blocks (wiki: Sandstone, Red Sandstone).
    r.push_back(shaped({"##", "##"}, {{'#', item("sand")}}, "sandstone"));
    r.push_back(shaped({"##", "##"}, {{'#', item("red_sand")}}, "red_sandstone"));
    // (wiki: Coarse Dirt, Andesite, Packed Ice)
    r.push_back(shaped({"DG", "GD"}, {{'D', item("dirt")}, {'G', item("gravel")}}, "coarse_dirt", 4));
    r.push_back(shapeless({item("diorite"), item("cobblestone")}, "andesite", 2));
    r.push_back(shaped({"###", "###", "###"}, {{'#', item("ice")}}, "packed_ice"));
    // Redstone (wiki: Redstone Torch, Redstone Repeater, Lever, Button, Block of
    // Redstone, Redstone Lamp, Piston). Sticky pistons need slime balls: not yet.
    const Ingredient redstone = item("redstone");
    r.push_back(shaped({"R", "S"}, {{'R', redstone}, {'S', stick}}, "redstone_torch"));
    r.push_back(shaped({"TRT", "SSS"}, {{'T', item("redstone_torch")}, {'R', redstone}, {'S', item("stone")}},
                       "repeater"));
    r.push_back(shaped({"S", "C"}, {{'S', stick}, {'C', item("cobblestone")}}, "lever"));
    r.push_back(shapeless({item("stone")}, "stone_button"));
    r.push_back(shapeless({item("oak_planks")}, "oak_button"));
    r.push_back(shaped({"###", "###", "###"}, {{'#', redstone}}, "redstone_block"));
    r.push_back(shapeless({item("redstone_block")}, "redstone", 9));
    r.push_back(shaped({".R.", "RGR", ".R."}, {{'R', redstone}, {'G', item("glowstone")}}, "redstone_lamp"));
    r.push_back(shaped({"PPP", "CIC", "CRC"},
                       {{'P', kPlanks}, {'C', item("cobblestone")}, {'I', item("iron_ingot")}, {'R', redstone}},
                       "piston"));
    // (wiki: Bucket)
    r.push_back(shaped({"#.#", ".#."}, {{'#', item("iron_ingot")}}, "bucket"));
    // (wiki: Flint and Steel)
    r.push_back(shapeless({item("iron_ingot"), item("flint")}, "flint_and_steel"));
    // (wiki: Shears - two iron ingots diagonally)
    r.push_back(shaped({".#", "#."}, {{'#', item("iron_ingot")}}, "shears"));
    // (wiki: Bow - 3 sticks and 3 string; White Wool - 4 string)
    r.push_back(shaped({".SX", "S.X", ".SX"}, {{'S', stick}, {'X', item("string")}}, "bow"));
    r.push_back(shaped({"..S", ".SX", "S.X"}, {{'S', stick}, {'X', item("string")}}, "fishing_rod")); // (M25.2)
    r.push_back(shaped({"##", "##"}, {{'#', item("string")}}, "white_wool"));
    // (wiki: Bread - 3 wheat in a row; Bone Meal - a bone makes 3)
    r.push_back(shaped({"###"}, {{'#', item("wheat")}}, "bread"));
    r.push_back(shapeless({item("bone")}, "bone_meal", 3));
    // Armor (wiki: Armor - helmet 5, chestplate 8, leggings 7, boots 4 of the material)
    // and the shield (planks around an iron ingot).
    for (const auto& [mat, ing] : {std::pair{"leather", "leather"}, std::pair{"copper", "copper_ingot"},
                                   std::pair{"golden", "gold_ingot"}, std::pair{"iron", "iron_ingot"},
                                   std::pair{"diamond", "diamond"}}) {
        const std::string m = mat;
        const Ingredient x = item(ing);
        r.push_back(shaped({"###", "#.#"}, {{'#', x}}, m + "_helmet"));
        if (m == "iron") // (once: M25.3b, wiki: Turtle Shell from 5 scutes)
            r.push_back(shaped({"###", "#.#"}, {{'#', item("turtle_scute")}}, "turtle_helmet"));
        r.push_back(shaped({"#.#", "###", "###"}, {{'#', x}}, m + "_chestplate"));
        r.push_back(shaped({"###", "#.#", "#.#"}, {{'#', x}}, m + "_leggings"));
        r.push_back(shaped({"#.#", "#.#"}, {{'#', x}}, m + "_boots"));
    }
    r.push_back(shaped({"WIW", "WWW", ".W."}, {{'W', kPlanks}, {'I', item("iron_ingot")}}, "shield"));
    // Books and enchanting (wiki: Paper, Book, Bookshelf, Enchanting Table, Anvil,
    // Block of Iron).
    r.push_back(shaped({"###"}, {{'#', item("sugar_cane")}}, "paper", 3));
    r.push_back(shapeless({item("paper"), item("paper"), item("paper"), item("leather")}, "book"));
    // Mount gear (M26.2; wiki: Leather Horse Armor - 7 leather in an H; Saddle - craftable
    // since 1.21.6 from 3 leather and an iron ingot: our layout assumption).
    r.push_back(shaped({"#.#", "###", "#.#"}, {{'#', item("leather")}}, "leather_horse_armor"));
    r.push_back(shaped({"###", ".I."}, {{'#', item("leather")}, {'I', item("iron_ingot")}}, "saddle"));
    // (M26.3; wiki: Leather - 4 rabbit hides; Wolf Armor - 6 armadillo scutes)
    r.push_back(shaped({"##", "##"}, {{'#', item("rabbit_hide")}}, "leather"));
    r.push_back(shaped({"#..", "###", "#.#"}, {{'#', item("armadillo_scute")}}, "wolf_armor"));
    // Nether (M19.2; wiki: Blaze Powder, Eye of Ender, Gold Nugget, Fire Charge).
    r.push_back(shapeless({item("blaze_rod")}, "blaze_powder", 2));
    r.push_back(shapeless({item("ender_pearl"), item("blaze_powder")}, "ender_eye"));
    r.push_back(shapeless({item("gold_ingot")}, "gold_nugget", 9));
    r.push_back(shaped({"###", "###", "###"}, {{'#', item("gold_nugget")}}, "gold_ingot"));
    r.push_back(shapeless({item("gunpowder"), item("blaze_powder"), item("coal")}, "fire_charge", 3));
    // Brewing (M19.4; wiki: Glass Bottle, Sugar, Fermented Spider Eye, Golden Carrot,
    // Glowstone).
    r.push_back(shaped({"#.#", ".#."}, {{'#', item("glass")}}, "glass_bottle", 3));
    r.push_back(shapeless({item("sugar_cane")}, "sugar"));
    r.push_back(shapeless({item("spider_eye"), item("brown_mushroom"), item("sugar")}, "fermented_spider_eye"));
    r.push_back(shaped({"###", "#C#", "###"}, {{'#', item("gold_nugget")}, {'C', item("carrot")}}, "golden_carrot"));
    r.push_back(shaped({"##", "##"}, {{'#', item("glowstone_dust")}}, "glowstone"));
    // The End (wiki: End Stone Bricks, Purpur Block, Iron Bars).
    r.push_back(shaped({"##", "##"}, {{'#', item("end_stone")}}, "end_stone_bricks", 4));
    r.push_back(shaped({"##", "##"}, {{'#', item("popped_chorus_fruit")}}, "purpur_block", 4));
    r.push_back(shaped({"###", "###"}, {{'#', item("iron_ingot")}}, "iron_bars", 16));
    // Redstone 2 (M21.1; wiki: Door 3 from 6 planks/ingots, Trapdoor 2 from 6 planks /
    // 1 from 4 iron, Fence 3, Fence Gate 1, Pressure Plates from 2 of their material).
    {
        // (each wood's planks make its own set: M23.3)
        for (const char* w : {"oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "cherry", "crimson", "warped",
                              "mangrove", "bamboo", "pale_oak"}) {
            const std::string wood(w);
            const Ingredient planks = item(wood + "_planks");
            r.push_back(shaped({"##", "##", "##"}, {{'#', planks}}, wood + "_door", 3));
            r.push_back(shaped({"###", "###"}, {{'#', planks}}, wood + "_trapdoor", 2));
            r.push_back(shaped({"#S#", "#S#"}, {{'#', planks}, {'S', stick}}, wood + "_fence", 3));
            r.push_back(shaped({"S#S", "S#S"}, {{'#', planks}, {'S', stick}}, wood + "_fence_gate"));
            r.push_back(shaped({"##"}, {{'#', planks}}, wood + "_pressure_plate"));
            if (wood != "oak") r.push_back(shapeless({planks}, wood + "_button"));
            // Boats (M25.2b; wiki: Boat): 5 planks in a U; bamboo makes a raft.
            if (wood != "crimson" && wood != "warped")
                r.push_back(shaped({"#.#", "###"}, {{'#', planks}}, wood == "bamboo" ? "bamboo_raft" : wood + "_boat"));
            // Chest boats (M26.2; wiki: Boat with Chest): a boat and a chest.
            if (wood != "crimson" && wood != "warped")
                r.push_back(shapeless({item(wood == "bamboo" ? "bamboo_raft" : wood + "_boat"), item("chest")},
                                      wood == "bamboo" ? "bamboo_chest_raft" : wood + "_chest_boat"));
            // Signs: 6 planks and a stick make 3; hanging signs: 2 chains over 6 stripped
            // logs make 6 (wiki: Sign, Hanging Sign).
            r.push_back(shaped({"###", "###", ".S."}, {{'#', planks}, {'S', stick}}, wood + "_sign", 3));
            const bool nether = wood == "crimson" || wood == "warped";
            const std::string stripped = wood == "bamboo" ? "stripped_bamboo_block"
                                                          : "stripped_" + wood + (nether ? "_stem" : "_log");
            r.push_back(shaped({"C.C", "###", "###"}, {{'C', item("chain")}, {'#', item(stripped)}}, wood + "_hanging_sign", 6));
        }
        r.push_back(shaped({"##", "##", "##"}, {{'#', item("iron_ingot")}}, "iron_door", 3));
        r.push_back(shaped({"##", "##"}, {{'#', item("iron_ingot")}}, "iron_trapdoor"));
        r.push_back(shaped({"##"}, {{'#', item("stone")}}, "stone_pressure_plate"));
        r.push_back(shaped({"##"}, {{'#', item("polished_blackstone")}}, "polished_blackstone_pressure_plate"));
        r.push_back(shapeless({item("polished_blackstone")}, "polished_blackstone_button"));
        r.push_back(shaped({"##"}, {{'#', item("gold_ingot")}}, "light_weighted_pressure_plate"));
        r.push_back(shaped({"##"}, {{'#', item("iron_ingot")}}, "heavy_weighted_pressure_plate"));
    }
    // (wiki: Redstone Comparator - 3 redstone torches round a quartz over 3 stone;
    // Observer - cobblestone, 2 redstone dust and a quartz)
    r.push_back(shaped({".T.", "TQT", "SSS"},
                       {{'T', item("redstone_torch")}, {'Q', item("quartz")}, {'S', item("stone")}}, "comparator"));
    r.push_back(shaped({"CCC", "RRQ", "CCC"},
                       {{'C', item("cobblestone")}, {'R', item("redstone")}, {'Q', item("quartz")}}, "observer"));
    // (wiki: Hopper - 5 iron ingots round a chest; Dispenser - cobblestone round a bow
    // over redstone; Dropper - cobblestone with redstone)
    r.push_back(shaped({"I.I", "ICI", ".I."}, {{'I', item("iron_ingot")}, {'C', item("chest")}}, "hopper"));
    r.push_back(shaped({"###", "#B#", "#R#"}, {{'#', item("cobblestone")}, {'B', item("bow")}, {'R', item("redstone")}},
                       "dispenser"));
    r.push_back(shaped({"###", "#.#", "#R#"}, {{'#', item("cobblestone")}, {'R', item("redstone")}}, "dropper"));
    r.push_back(shaped({"I.I", "III"}, {{'I', item("iron_ingot")}}, "minecart")); // (wiki: Minecart)
    // (wiki: Slime Block - 9 slime balls, and back; Sticky Piston - a slime ball over a piston)
    r.push_back(shaped({"###", "###", "###"}, {{'#', item("slime_ball")}}, "slime_block"));
    r.push_back(shapeless({item("slime_block")}, "slime_ball", 9));
    r.push_back(shaped({"S", "P"}, {{'S', item("slime_ball")}, {'P', item("piston")}}, "sticky_piston"));
    // Rails (wiki: Rail 16, Powered Rail 6, Detector Rail 6, Activator Rail 6)
    r.push_back(shaped({"I.I", "ISI", "I.I"}, {{'I', item("iron_ingot")}, {'S', stick}}, "rail", 16));
    r.push_back(shaped({"G.G", "GSG", "GRG"}, {{'G', item("gold_ingot")}, {'S', stick}, {'R', item("redstone")}},
                       "powered_rail", 6));
    r.push_back(shaped({"I.I", "IPI", "IRI"},
                       {{'I', item("iron_ingot")}, {'P', item("stone_pressure_plate")}, {'R', item("redstone")}},
                       "detector_rail", 6));
    r.push_back(shaped({"ISI", "ITI", "ISI"}, {{'I', item("iron_ingot")}, {'S', stick}, {'T', item("redstone_torch")}},
                       "activator_rail", 6));
    // (wiki: End Rod - a blaze rod over popped chorus fruit makes 4)
    r.push_back(shaped({"B", "P"}, {{'B', item("blaze_rod")}, {'P', item("popped_chorus_fruit")}}, "end_rod", 4));
    // (wiki: End Crystal - glass around an eye of ender over a ghast tear)
    r.push_back(shaped({"GGG", "GEG", "GTG"}, {{'G', item("glass")}, {'E', item("ender_eye")}, {'T', item("ghast_tear")}},
                       "end_crystal"));
    r.push_back(shaped({".B.", "###"}, {{'B', item("blaze_rod")}, {'#', kStoneTool}}, "brewing_stand"));
    r.push_back(shaped({"###", "BBB", "###"}, {{'#', kPlanks}, {'B', item("book")}}, "bookshelf"));
    r.push_back(shaped({".B.", "DOD", "OOO"}, {{'B', item("book")}, {'D', item("diamond")}, {'O', item("obsidian")}},
                       "enchanting_table"));
    r.push_back(shaped({"###", "###", "###"}, {{'#', item("iron_ingot")}}, "iron_block"));
    for (const auto& [unit, block] : {std::pair{"diamond", "diamond_block"}, std::pair{"emerald", "emerald_block"},
                                      std::pair{"lapis_lazuli", "lapis_block"}, std::pair{"coal", "coal_block"}}) {
        r.push_back(shaped({"###", "###", "###"}, {{'#', item(unit)}}, block)); // (M23.6; wiki: each block)
        r.push_back(shapeless({item(block)}, unit, 9));
    }
    // Beacon (wiki: 5 glass, a nether star, 3 obsidian), conduit (a heart of the sea in 8
    // nautilus shells), sea lantern (4 prismarine shards, 5 crystals).
    r.push_back(shaped({"GGG", "GNG", "OOO"}, {{'G', item("glass")}, {'N', item("nether_star")}, {'O', item("obsidian")}},
                       "beacon"));
    r.push_back(shaped({"NNN", "NHN", "NNN"}, {{'N', item("nautilus_shell")}, {'H', item("heart_of_the_sea")}}, "conduit"));
    r.push_back(shaped({"SCS", "CCC", "SCS"}, {{'S', item("prismarine_shard")}, {'C', item("prismarine_crystals")}},
                       "sea_lantern"));
    r.push_back(shapeless({item("iron_block")}, "iron_ingot", 9));
    r.push_back(shaped({"BBB", ".I.", "III"}, {{'B', item("iron_block")}, {'I', item("iron_ingot")}}, "anvil"));
    // (wiki: Arrow - flint, stick, feather -> 4)
    r.push_back(shaped({"F", "S", "E"}, {{'F', item("flint")}, {'S', stick}, {'E', item("feather")}}, "arrow", 4));
    // Building blocks (M23.1; wiki: Slab, Stairs, Wall and each block's page): every
    // slab is 3 of its block for 6, stairs 6 for 4, walls 6 for 6.
    {
        const auto& reg = blockRegistry();
        for (BlockId b = 1; b < reg.blockCount(); ++b) {
            const BlockSettings& st = reg.block(b).settings;
            if (st.kind == BlockKind::Plain) continue;
            const std::string result(std::string_view(reg.block(b).id).substr(10));
            const Ingredient base = item(std::string_view(reg.block(st.base).id).substr(10));
            switch (st.kind) {
            case BlockKind::Slab: r.push_back(shaped({"###"}, {{'#', base}}, result, 6)); break;
            case BlockKind::Stairs: r.push_back(shaped({"#..", "##.", "###"}, {{'#', base}}, result, 4)); break;
            case BlockKind::Wall: r.push_back(shaped({"###", "###"}, {{'#', base}}, result, 6)); break;
            case BlockKind::Plain: break;
            }
        }
        auto square = [&](std::string_view from, std::string_view to, int count) {
            r.push_back(shaped({"##", "##"}, {{'#', item(from)}}, to, count));
        };
        auto pillarOf = [&](std::string_view from, std::string_view to, int count) {
            r.push_back(shaped({"#", "#"}, {{'#', item(from)}}, to, count));
        };
        square("stone", "stone_bricks", 4);
        square("brick", "bricks", 1);
        square("nether_brick", "nether_bricks", 1);
        square("granite", "polished_granite", 4);
        square("diorite", "polished_diorite", 4);
        square("andesite", "polished_andesite", 4);
        square("cobbled_deepslate", "polished_deepslate", 4);
        square("polished_deepslate", "deepslate_bricks", 4);
        square("deepslate_bricks", "deepslate_tiles", 4);
        square("blackstone", "polished_blackstone", 4);
        square("polished_blackstone", "polished_blackstone_bricks", 4);
        square("quartz", "quartz_block", 1);
        square("quartz_block", "quartz_bricks", 4);
        square("tuff", "polished_tuff", 4);
        square("polished_tuff", "tuff_bricks", 4);
        square("sandstone", "cut_sandstone", 4);
        square("red_sandstone", "cut_red_sandstone", 4);
        pillarOf("quartz_block", "quartz_pillar", 2);
        pillarOf("cobbled_deepslate_slab", "chiseled_deepslate", 1);
        pillarOf("quartz_slab", "chiseled_quartz_block", 1);
        pillarOf("stone_brick_slab", "chiseled_stone_bricks", 1);
        pillarOf("sandstone_slab", "chiseled_sandstone", 1);
        pillarOf("red_sandstone_slab", "chiseled_red_sandstone", 1);
        pillarOf("tuff_slab", "chiseled_tuff", 1);
        pillarOf("tuff_brick_slab", "chiseled_tuff_bricks", 1);
        pillarOf("polished_blackstone_slab", "chiseled_polished_blackstone", 1);
        r.push_back(shaped({"NW", "WN"}, {{'N', item("nether_brick")}, {'W', item("nether_wart")}}, "red_nether_bricks"));
    }
    // Copper (M23.4b; wiki: Block of Copper, Cut Copper, Chiseled Copper, Copper Grate,
    // Copper Door, Copper Trapdoor, Copper Bulb, Honeycomb).
    {
        static constexpr const char* kStages[4] = {"", "exposed_", "weathered_", "oxidized_"};
        const Ingredient ingot = item("copper_ingot");
        r.push_back(shaped({"###", "###", "###"}, {{'#', ingot}}, "copper_block"));
        r.push_back(shapeless({item("copper_block")}, "copper_ingot", 9));
        r.push_back(shapeless({item("waxed_copper_block")}, "copper_ingot", 9));
        r.push_back(shaped({"##", "##", "##"}, {{'#', ingot}}, "copper_door", 3));
        r.push_back(shaped({"##", "##"}, {{'#', ingot}}, "copper_trapdoor"));
        for (const char* wx : {"", "waxed_"})
            for (int i = 0; i < 4; ++i) {
                const std::string w(wx), st(kStages[i]);
                const std::string block = w + (i == 0 ? "copper_block" : st + "copper");
                r.push_back(shaped({"##", "##"}, {{'#', item(block)}}, w + st + "cut_copper", 4));
                r.push_back(shaped({"#", "#"}, {{'#', item(w + st + "cut_copper_slab")}}, w + st + "chiseled_copper"));
                r.push_back(shaped({".#.", "#.#", ".#."}, {{'#', item(block)}}, w + st + "copper_grate", 4));
                r.push_back(shaped({".C.", "CBC", ".R."},
                                   {{'C', item(block)}, {'B', item("blaze_rod")}, {'R', item("redstone")}},
                                   w + st + "copper_bulb", 4));
            }
        // Honeycomb waxes any unwaxed copper block in the grid (shapeless).
        const auto& reg = blockRegistry();
        for (BlockId b = 1; b < reg.blockCount(); ++b) {
            const std::string_view id = std::string_view(reg.block(b).id).substr(10);
            if (!BlockUpdates::isCopper(b) || id.starts_with("waxed_") || !itemRegistry().blockItem(b)) continue;
            const std::string waxed = "waxed_" + std::string(id);
            if (itemRegistry().find(waxed)) r.push_back(shapeless({item(id), item("honeycomb")}, waxed));
        }
    }
    // Thin and small blocks, dyes (M23.2; wiki: each item's page).
    {
        auto has = [](std::string_view n) { return itemRegistry().find(n).has_value(); };
        const Ingredient nugget = item("iron_nugget");
        r.push_back(shaped({"###", "###"}, {{'#', item("glass")}}, "glass_pane", 16));
        r.push_back(shaped({"S.S", "SSS", "S.S"}, {{'S', stick}}, "ladder", 3));
        r.push_back(shaped({"NNN", "NTN", "NNN"}, {{'N', nugget}, {'T', item("torch")}}, "lantern"));
        r.push_back(shaped({"NNN", "NTN", "NNN"}, {{'N', nugget}, {'T', item("soul_torch")}}, "soul_lantern"));
        r.push_back(shaped({"N", "I", "N"}, {{'N', nugget}, {'I', item("iron_ingot")}}, "chain"));
        for (const char* soil : {"soul_sand", "soul_soil"}) {
            r.push_back(shaped({"C", "S", "X"}, {{'C', kCoal}, {'S', stick}, {'X', item(soil)}}, "soul_torch", 4));
            r.push_back(shaped({".S.", "SXS", "LLL"}, {{'S', stick}, {'X', item(soil)}, {'L', kLogs}}, "soul_campfire"));
        }
        r.push_back(shaped({"###", "###", "###"}, {{'#', item("wheat")}}, "hay_block")); // (wiki: Hay Bale)
        r.push_back(shapeless({item("hay_block")}, "wheat", 9));
        r.push_back(shaped({"###", "###", "###"}, {{'#', item("dried_kelp")}}, "dried_kelp_block")); // (M25.1)
        r.push_back(shapeless({item("dried_kelp_block")}, "dried_kelp", 9));
        // Campfire (wiki): sticks around coal over three logs.
        r.push_back(shaped({".S.", "SCS", "LLL"}, {{'S', stick}, {'C', kCoal}, {'L', kLogs}}, "campfire"));
        r.push_back(shapeless({item("iron_ingot")}, "iron_nugget", 9));
        r.push_back(shaped({"###", "###", "###"}, {{'#', nugget}}, "iron_ingot"));
        r.push_back(shapeless({item("gold_ingot")}, "gold_nugget", 9));
        r.push_back(shaped({"###", "###", "###"}, {{'#', item("gold_nugget")}}, "gold_ingot"));
        for (const char* c : kDyeColours) {
            const std::string colour(c);
            const Ingredient dye = item(colour + "_dye");
            r.push_back(shaped({"GGG", "GDG", "GGG"}, {{'G', item("glass")}, {'D', dye}}, colour + "_stained_glass", 8));
            r.push_back(shaped({"###", "###"}, {{'#', item(colour + "_stained_glass")}}, colour + "_stained_glass_pane", 16));
            r.push_back(shaped({"PPP", "PDP", "PPP"}, {{'P', item("glass_pane")}, {'D', dye}}, colour + "_stained_glass_pane", 8));
            r.push_back(shaped({"##"}, {{'#', item(colour + "_wool")}}, colour + "_carpet", 3));
            if (colour != "white") r.push_back(shapeless({item("white_wool"), dye}, colour + "_wool"));
            // M23.4a (wiki: Terracotta, Concrete Powder): dyed terracotta, powder from
            // sand, gravel and a dye.
            r.push_back(shaped({"TTT", "TDT", "TTT"}, {{'T', item("terracotta")}, {'D', dye}}, colour + "_terracotta", 8));
            r.push_back(shapeless({dye, item("sand"), item("sand"), item("sand"), item("sand"), item("gravel"),
                                   item("gravel"), item("gravel"), item("gravel")},
                                  colour + "_concrete_powder", 8));
        }
        // Dyes from flowers and minerals, and mixed (wiki: Dye). Black (ink sacs,
        // wither roses) and brown (cocoa beans) wait for their sources.
        static constexpr std::pair<const char*, const char*> kFrom[] = {
            {"dandelion", "yellow_dye"},    {"poppy", "red_dye"},          {"cornflower", "blue_dye"},
            {"ink_sac", "black_dye"}, // (M25 review: squid ink, wiki: Black Dye)
            {"azure_bluet", "light_gray_dye"}, {"oxeye_daisy", "light_gray_dye"}, {"white_tulip", "light_gray_dye"},
            {"allium", "magenta_dye"},      {"blue_orchid", "light_blue_dye"}, {"red_tulip", "red_dye"},
            {"orange_tulip", "orange_dye"}, {"pink_tulip", "pink_dye"},    {"lily_of_the_valley", "white_dye"},
            {"bone_meal", "white_dye"},     {"lapis_lazuli", "blue_dye"}};
        for (const auto& [from, to] : kFrom)
            if (has(from)) r.push_back(shapeless({item(from)}, to));
        static constexpr std::array<const char*, 3> kMix[] = {
            {"red_dye", "yellow_dye", "orange_dye"}, {"red_dye", "white_dye", "pink_dye"},
            {"blue_dye", "white_dye", "light_blue_dye"}, {"blue_dye", "green_dye", "cyan_dye"},
            {"blue_dye", "red_dye", "purple_dye"},   {"purple_dye", "pink_dye", "magenta_dye"},
            {"green_dye", "white_dye", "lime_dye"},  {"black_dye", "white_dye", "gray_dye"},
            {"gray_dye", "white_dye", "light_gray_dye"}};
        for (const auto& m : kMix)
            r.push_back(shapeless({item(m[0]), item(m[1])}, m[2], 2));
    }
    return r;
}

// Does `r` match the grid with its top-left at (ox, oy), optionally mirrored?
bool matchShaped(const Recipe& r, std::span<const ItemStack> grid, int size, int ox, int oy, bool mirror) {
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const ItemStack& s = grid[size_t(y * size + x)];
            const int rx = x - ox, ry = y - oy;
            const bool inside = rx >= 0 && ry >= 0 && rx < r.width && ry < r.height;
            if (!inside) {
                if (!s.empty()) return false;
                continue;
            }
            const Ingredient& in = r.pattern[size_t(ry * r.width + (mirror ? r.width - 1 - rx : rx))];
            if (in.kind == Ingredient::Kind::Empty ? !s.empty() : !in.matches(s)) return false;
        }
    return true;
}

} // namespace

namespace {

// Per-item tables built once from the name rules below (lookups on hot paths are
// array reads: furnaces tick every game tick, crafting re-matches on every click).
enum Tag : uint8_t { kTagPlanks = 1, kTagLogs = 2, kTagCoal = 4, kTagStoneTool = 8, kTagWoodenSlab = 16 };
std::optional<ItemStack> smeltByName(std::string_view n);
int fuelByName(std::string_view n, const ItemDef& def);
float smeltExperienceByName(std::string_view n);

// A smelting recipe's id, as vanilla names them (shown by /recipe; stored in a
// furnace's RecipesUsed): recipes with one kind of input, or a tag of inputs
// (glass from #smelts_to_glass, charcoal from #logs_that_burn), are named after the
// output; ore and raw-metal recipes, of which each output has several, are
// "<output>_from_smelting_<input>" (one per input; quartz has only one). The wiki
// has no full id list, so these are our rules after vanilla's pattern (agreed with
// the user, 2026-10-07); new recipes follow the same rules.
std::string smeltRecipeName(std::string_view input, std::string_view output) {
    std::string id = "minecraft:" + std::string(output);
    if ((input.starts_with("raw_") || input.ends_with("_ore")) && output != "quartz")
        id += "_from_smelting_" + std::string(input);
    return id;
}

struct ItemTables {
    std::vector<uint8_t> tags;
    std::vector<ItemStack> smelt; // empty stack: not smeltable
    std::vector<int> fuel;
    std::vector<world::RecipeId> smeltRecipe; // per input item
    std::vector<float> recipeXp;              // per recipe id (our smelting recipes)
    ItemTables() {
        const auto& items = itemRegistry();
        tags.resize(items.count());
        smelt.resize(items.count());
        fuel.resize(items.count());
        smeltRecipe.resize(items.count());
        for (size_t i = 1; i < items.count(); ++i) {
            const std::string_view n = nameOf(static_cast<ItemId>(i));
            // Vanilla #logs: logs, wood, stems, hyphae and their stripped forms.
            const bool log = n.ends_with("_log") || n.ends_with("_wood") || n.ends_with("_stem") ||
                             n.ends_with("_hyphae");
            // Vanilla #wooden_slabs: the slab of every wood (its planks exist).
            const bool woodSlab = n.ends_with("_slab") &&
                                  items.find(std::string(n.substr(0, n.size() - 5)) + "_planks").has_value();
            tags[i] = static_cast<uint8_t>((n.ends_with("_planks") ? kTagPlanks : 0) | (log ? kTagLogs : 0) |
                                           (woodSlab ? kTagWoodenSlab : 0) |
                                           (n == "coal" || n == "charcoal" ? kTagCoal : 0) |
                                           (n == "cobblestone" ? kTagStoneTool : 0));
            smelt[i] = smeltByName(n).value_or(ItemStack{});
            if (!smelt[i].empty()) {
                std::string_view out = nameOf(smelt[i].item);
                const world::RecipeId r = world::internRecipeId(smeltRecipeName(n, out));
                smeltRecipe[i] = r;
                if (recipeXp.size() <= r) recipeXp.resize(size_t(r) + 1, 0.0f);
                recipeXp[r] = smeltExperienceByName(n);
            }
            fuel[i] = fuelByName(n, items.item(static_cast<ItemId>(i)));
        }
    }
};

const ItemTables& tables() {
    static const ItemTables t;
    return t;
}

} // namespace

bool Ingredient::matches(const ItemStack& s) const {
    if (s.empty()) return false;
    const uint8_t tags = tables().tags[s.item];
    switch (kind) {
    case Kind::Empty: return false;
    case Kind::Item: return s.item == item;
    case Kind::Planks: return (tags & kTagPlanks) != 0;
    case Kind::Logs: return (tags & kTagLogs) != 0;
    case Kind::WoodenSlab: return (tags & kTagWoodenSlab) != 0;
    case Kind::Coal: return (tags & kTagCoal) != 0;
    case Kind::StoneTool: return (tags & kTagStoneTool) != 0; // + blackstone, cobbled deepslate in vanilla
    }
    return false;
}

const std::vector<Recipe>& craftingRecipes() {
    static const std::vector<Recipe> recipes = build();
    return recipes;
}

namespace {
std::optional<ItemStack> craftPlain(std::span<const ItemStack> grid, int size);
}

// A dyed shulker box keeps what it holds (wiki: Shulker Box › Dyeing).
std::optional<ItemStack> craft(std::span<const ItemStack> grid, int size) {
    std::optional<ItemStack> out = craftPlain(grid, size);
    if (out)
        for (const ItemStack& s : grid)
            if (s.contents) out->contents = s.contents;
    return out;
}

namespace {
std::optional<ItemStack> craftPlain(std::span<const ItemStack> grid, int size) {
    for (const Recipe& r : craftingRecipes()) {
        if (r.width == 0) { // shapeless: the same multiset of items, anywhere
            std::array<bool, 9> used{};
            bool ok = true;
            int filled = 0;
            for (const ItemStack& s : grid) {
                if (s.empty()) continue;
                ++filled;
                bool found = false;
                for (size_t i = 0; i < r.pattern.size(); ++i)
                    if (!used[i] && r.pattern[i].matches(s)) {
                        used[i] = found = true;
                        break;
                    }
                ok = ok && found;
            }
            if (ok && filled == static_cast<int>(r.pattern.size())) return r.result;
            continue;
        }
        if (r.width > size || r.height > size) continue;
        for (int oy = 0; oy + r.height <= size; ++oy)
            for (int ox = 0; ox + r.width <= size; ++ox)
                for (bool mirror : {false, true})
                    if (matchShaped(r, grid, size, ox, oy, mirror)) return r.result;
    }
    return std::nullopt;
}
} // namespace

std::optional<ItemStack> smelt(const ItemStack& input) {
    if (input.empty()) return std::nullopt;
    const ItemStack& r = tables().smelt[input.item];
    return r.empty() ? std::nullopt : std::optional<ItemStack>(r);
}

int fuelTicks(const ItemStack& fuel) { return fuel.empty() ? 0 : tables().fuel[fuel.item]; }

namespace {

std::optional<ItemStack> smeltByName(std::string_view n) {
    auto out = [](std::string_view name) { return std::optional<ItemStack>(ItemStack{id(name), 1}); };
    if (n == "raw_iron" || n == "iron_ore" || n == "deepslate_iron_ore") return out("iron_ingot");
    if (n == "raw_gold" || n == "gold_ore" || n == "deepslate_gold_ore" || n == "nether_gold_ore") return out("gold_ingot");
    if (n == "nether_quartz_ore") return out("quartz");
    if (n == "ancient_debris") return out("netherite_scrap"); // (wiki: Netherite Scrap, M23.6)
    if (n == "raw_copper" || n == "copper_ore" || n == "deepslate_copper_ore") return out("copper_ingot");
    if (n == "sand" || n == "red_sand") return out("glass");
    if (n == "cobblestone") return out("stone");
    if (n.ends_with("_log")) return out("charcoal");
    if (n == "coal_ore" || n == "deepslate_coal_ore") return out("coal");
    if (n == "diamond_ore" || n == "deepslate_diamond_ore") return out("diamond");
    if (n == "emerald_ore" || n == "deepslate_emerald_ore") return out("emerald");
    if (n == "clay") return out("terracotta");
    // Building blocks (M23.1; wiki: Smelting): bricks, smooth and cracked variants.
    if (n == "clay_ball") return out("brick");
    if (n.ends_with("_terracotta") && !n.ends_with("glazed_terracotta") && n != "terracotta") // (wiki: Glazed Terracotta)
        return out(std::string(n.substr(0, n.size() - 11)) + "_glazed_terracotta");
    if (n == "cactus") return out("green_dye"); // (wiki: Green Dye)
    if (n == "netherrack") return out("nether_brick");
    if (n == "stone") return out("smooth_stone");
    if (n == "sandstone") return out("smooth_sandstone");
    if (n == "red_sandstone") return out("smooth_red_sandstone");
    if (n == "quartz_block") return out("smooth_quartz");
    if (n == "stone_bricks") return out("cracked_stone_bricks");
    if (n == "cobbled_deepslate") return out("deepslate");
    if (n == "deepslate_bricks") return out("cracked_deepslate_bricks");
    if (n == "deepslate_tiles") return out("cracked_deepslate_tiles");
    if (n == "polished_blackstone_bricks") return out("cracked_polished_blackstone_bricks");
    if (n == "beef") return out("cooked_beef");
    if (n == "porkchop") return out("cooked_porkchop");
    if (n == "mutton") return out("cooked_mutton");
    if (n == "chicken") return out("cooked_chicken");
    if (n == "potato") return out("baked_potato");
    if (n == "chorus_fruit") return out("popped_chorus_fruit");
    if (n == "kelp") return out("dried_kelp"); // (M25.1)
    if (n == "cod") return out("cooked_cod"); // (M25.2)
    if (n == "salmon") return out("cooked_salmon");
    if (n == "rabbit") return out("cooked_rabbit"); // (M26.3)
    if (n == "wet_sponge") return out("sponge"); // (M25.5)
    if (n == "redstone_ore" || n == "deepslate_redstone_ore") return out("redstone");
    if (n == "lapis_ore" || n == "deepslate_lapis_ore") return out("lapis_lazuli");
    return std::nullopt;
}

float smeltExperienceByName(std::string_view n) {
    // wiki: Smelting - experience per item.
    if (n == "raw_gold" || n.ends_with("gold_ore") || n.ends_with("diamond_ore") || n.ends_with("emerald_ore")) return 1.0f;
    if (n == "raw_iron" || n == "raw_copper" || n.ends_with("iron_ore") || n.ends_with("copper_ore") ||
        n.ends_with("redstone_ore"))
        return 0.7f;
    if (n == "nether_quartz_ore" || n.ends_with("lapis_ore")) return 0.2f;
    if (n == "ancient_debris") return 2.0f;
    if (n.ends_with("coal_ore")) return 0.1f;
    if (n == "beef" || n == "porkchop" || n == "mutton" || n == "chicken" || n == "potato" || n == "cod" || n == "salmon" ||
        n == "rabbit")
        return 0.35f;
    if (n.ends_with("_log")) return 0.15f;
    if (n == "clay") return 0.35f;
    if (n == "kelp") return 0.1f; // (wiki: Dried Kelp)
    if (n == "wet_sponge") return 0.15f;
    if (n == "clay_ball") return 0.3f; // (wiki: Brick)
    if (n.ends_with("_terracotta") && !n.ends_with("glazed_terracotta")) return 0.1f;
    if (n == "cactus") return 1.0f;
    if (n == "netherrack" || n == "stone" || n == "sandstone" || n == "red_sandstone" || n == "quartz_block" ||
        n == "stone_bricks" || n == "cobbled_deepslate" || n == "deepslate_bricks" || n == "deepslate_tiles" ||
        n == "polished_blackstone_bricks")
        return 0.1f;
    if (n == "chorus_fruit") return 0.1f;
    if (n == "sand" || n == "red_sand" || n == "cobblestone") return 0.1f;
    return 0.0f;
}

int fuelByName(std::string_view n, const ItemDef& def) {
    // wiki: Fuel - burn durations in game ticks.
    if (n == "coal" || n == "charcoal") return 1600;
    if (n.ends_with("_log") || n.ends_with("_planks") || n == "crafting_table" || n == "chest") return 300;
    if (n == "stick" || n == "dead_bush" || n.ends_with("_sapling") || n.ends_with("_wool")) return 100;
    if (n == "bookshelf") return 300;
    if (n == "lava_bucket") return 20000; // the empty bucket stays in the fuel slot
    if (n == "coal_block") return 16000;
    if (n == "dried_kelp_block") return 4001; // (wiki: Fuel)
    if (n == "barrel" || n == "composter" || n == "smithing_table" || n == "loom" || n == "cartography_table")
        return 300; // (wooden workstations, M23.5-6)
    if (def.tool != ToolType::None && def.tier == ToolTier::Wood) return 200;
    return 0;
}

} // namespace

float smeltExperience(const ItemStack& input) { return recipeExperience(smeltRecipe(input)); }

world::RecipeId smeltRecipe(const ItemStack& input) {
    return input.empty() ? world::kNoRecipe : tables().smeltRecipe[input.item];
}

float recipeExperience(world::RecipeId recipe) {
    const auto& xp = tables().recipeXp;
    return recipe < xp.size() ? xp[recipe] : 0.0f;
}

} // namespace mc
