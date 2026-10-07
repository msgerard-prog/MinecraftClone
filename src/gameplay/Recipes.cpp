#include "gameplay/Recipes.h"

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
    for (std::string_view wood : {"oak", "birch", "spruce", "acacia", "jungle", "dark_oak", "cherry"})
        r.push_back(shapeless({item(std::string(wood) + "_log")}, std::string(wood) + "_planks", 4));
    r.push_back(shaped({"#", "#"}, {{'#', kPlanks}}, "stick", 4));
    r.push_back(shaped({"##", "##"}, {{'#', kPlanks}}, "crafting_table"));
    // Light and stations (wiki: Torch, Furnace).
    r.push_back(shaped({"C", "S"}, {{'C', kCoal}, {'S', item("stick")}}, "torch", 4));
    r.push_back(shaped({"###", "#.#", "###"}, {{'#', kStoneTool}}, "furnace"));
    r.push_back(shaped({"###", "#.#", "###"}, {{'#', kPlanks}}, "chest")); // wiki: Chest
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
        r.push_back(shaped({"#.#", "###", "###"}, {{'#', x}}, m + "_chestplate"));
        r.push_back(shaped({"###", "#.#", "#.#"}, {{'#', x}}, m + "_leggings"));
        r.push_back(shaped({"#.#", "#.#"}, {{'#', x}}, m + "_boots"));
    }
    r.push_back(shaped({"WIW", "WWW", ".W."}, {{'W', kPlanks}, {'I', item("iron_ingot")}}, "shield"));
    // Books and enchanting (wiki: Paper, Book, Bookshelf, Enchanting Table, Anvil,
    // Block of Iron).
    r.push_back(shaped({"###"}, {{'#', item("sugar_cane")}}, "paper", 3));
    r.push_back(shapeless({item("paper"), item("paper"), item("paper"), item("leather")}, "book"));
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
    r.push_back(shapeless({item("iron_block")}, "iron_ingot", 9));
    r.push_back(shaped({"BBB", ".I.", "III"}, {{'B', item("iron_block")}, {'I', item("iron_ingot")}}, "anvil"));
    // (wiki: Arrow - flint, stick, feather -> 4)
    r.push_back(shaped({"F", "S", "E"}, {{'F', item("flint")}, {'S', stick}, {'E', item("feather")}}, "arrow", 4));
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
enum Tag : uint8_t { kTagPlanks = 1, kTagLogs = 2, kTagCoal = 4, kTagStoneTool = 8 };
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
            tags[i] = static_cast<uint8_t>((n.ends_with("_planks") ? kTagPlanks : 0) |
                                           (n.ends_with("_log") ? kTagLogs : 0) |
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
    case Kind::Coal: return (tags & kTagCoal) != 0;
    case Kind::StoneTool: return (tags & kTagStoneTool) != 0; // + blackstone, cobbled deepslate in vanilla
    }
    return false;
}

const std::vector<Recipe>& craftingRecipes() {
    static const std::vector<Recipe> recipes = build();
    return recipes;
}

std::optional<ItemStack> craft(std::span<const ItemStack> grid, int size) {
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
    if (n == "raw_copper" || n == "copper_ore" || n == "deepslate_copper_ore") return out("copper_ingot");
    if (n == "sand" || n == "red_sand") return out("glass");
    if (n == "cobblestone") return out("stone");
    if (n.ends_with("_log")) return out("charcoal");
    if (n == "coal_ore" || n == "deepslate_coal_ore") return out("coal");
    if (n == "diamond_ore" || n == "deepslate_diamond_ore") return out("diamond");
    if (n == "emerald_ore" || n == "deepslate_emerald_ore") return out("emerald");
    if (n == "clay") return out("terracotta");
    if (n == "beef") return out("cooked_beef");
    if (n == "porkchop") return out("cooked_porkchop");
    if (n == "mutton") return out("cooked_mutton");
    if (n == "chicken") return out("cooked_chicken");
    if (n == "potato") return out("baked_potato");
    if (n == "chorus_fruit") return out("popped_chorus_fruit");
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
    if (n.ends_with("coal_ore")) return 0.1f;
    if (n == "beef" || n == "porkchop" || n == "mutton" || n == "chicken" || n == "potato") return 0.35f;
    if (n.ends_with("_log")) return 0.15f;
    if (n == "clay") return 0.35f;
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
