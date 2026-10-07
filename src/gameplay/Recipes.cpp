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
    for (std::string_view wood : {"oak", "birch", "spruce", "acacia"})
        r.push_back(shapeless({item(std::string(wood) + "_log")}, std::string(wood) + "_planks", 4));
    r.push_back(shaped({"#", "#"}, {{'#', kPlanks}}, "stick", 4));
    r.push_back(shaped({"##", "##"}, {{'#', kPlanks}}, "crafting_table"));
    // Light and stations (wiki: Torch, Furnace).
    r.push_back(shaped({"C", "S"}, {{'C', kCoal}, {'S', item("stick")}}, "torch", 4));
    r.push_back(shaped({"###", "#.#", "###"}, {{'#', kStoneTool}}, "furnace"));
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

struct ItemTables {
    std::vector<uint8_t> tags;
    std::vector<ItemStack> smelt; // empty stack: not smeltable
    std::vector<int> fuel;
    ItemTables() {
        const auto& items = itemRegistry();
        tags.resize(items.count());
        smelt.resize(items.count());
        fuel.resize(items.count());
        for (size_t i = 1; i < items.count(); ++i) {
            const std::string_view n = nameOf(static_cast<ItemId>(i));
            tags[i] = static_cast<uint8_t>((n.ends_with("_planks") ? kTagPlanks : 0) |
                                           (n.ends_with("_log") ? kTagLogs : 0) |
                                           (n == "coal" || n == "charcoal" ? kTagCoal : 0) |
                                           (n == "cobblestone" ? kTagStoneTool : 0));
            smelt[i] = smeltByName(n).value_or(ItemStack{});
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
    if (n == "redstone_ore" || n == "deepslate_redstone_ore") return out("redstone");
    if (n == "lapis_ore" || n == "deepslate_lapis_ore") return out("lapis_lazuli");
    return std::nullopt;
}

int fuelByName(std::string_view n, const ItemDef& def) {
    // wiki: Fuel - burn durations in game ticks.
    if (n == "coal" || n == "charcoal") return 1600;
    if (n.ends_with("_log") || n.ends_with("_planks") || n == "crafting_table") return 300;
    if (n == "stick" || n == "dead_bush" || n.ends_with("_sapling") || n.ends_with("_wool")) return 100;
    if (n == "lava_bucket") return 20000; // the empty bucket stays in the fuel slot
    if (def.tool != ToolType::None && def.tier == ToolTier::Wood) return 200;
    return 0;
}

} // namespace

} // namespace mc
