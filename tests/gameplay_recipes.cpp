// Crafting and smelting (wiki: Crafting, each item's recipe, Smelting, Fuel).
#include "gameplay/Recipes.h"

#include <doctest/doctest.h>

#include <array>

using namespace mc;
using namespace mc::world;

namespace {

ItemStack I(const char* name, int n = 1) { return {*itemRegistry().find(name), static_cast<uint8_t>(n)}; }
std::string out(std::span<const ItemStack> grid, int size) {
    const auto r = craft(grid, size);
    return r ? itemRegistry().item(r->item).id + "x" + std::to_string(r->count) : "";
}

} // namespace

TEST_CASE("crafting: logs -> 4 planks anywhere; planks -> sticks; 2x2 crafting table") {
    std::array<ItemStack, 4> g{};
    g[3] = I("birch_log");
    CHECK(out(g, 2) == "minecraft:birch_planksx4");
    g = {I("oak_planks"), {}, I("spruce_planks"), {}}; // mixed planks still make sticks
    CHECK(out(g, 2) == "minecraft:stickx4");
    g = {I("oak_planks"), I("oak_planks"), I("oak_planks"), I("oak_planks")};
    CHECK(out(g, 2) == "minecraft:crafting_tablex1");
    g = {I("oak_planks"), I("oak_planks"), I("oak_planks"), {}};
    CHECK(out(g, 2).empty());
}

TEST_CASE("crafting: tools need the 3x3 table; shapes may be mirrored but not rotated") {
    const ItemStack p = I("oak_planks"), s = I("stick"), c = I("cobblestone"), e{};
    std::array<ItemStack, 9> pick = {p, p, p, e, s, e, e, s, e};
    CHECK(out(pick, 3) == "minecraft:wooden_pickaxex1");
    std::array<ItemStack, 9> axe = {c, c, e, c, s, e, e, s, e};
    CHECK(out(axe, 3) == "minecraft:stone_axex1");
    std::array<ItemStack, 9> axeMirrored = {e, c, c, e, s, c, e, s, e};
    CHECK(out(axeMirrored, 3) == "minecraft:stone_axex1");
    std::array<ItemStack, 9> sword = {e, e, I("diamond"), e, e, I("diamond"), e, e, s};
    CHECK(out(sword, 3) == "minecraft:diamond_swordx1");
    std::array<ItemStack, 9> furnace = {c, c, c, c, e, c, c, c, c};
    CHECK(out(furnace, 3) == "minecraft:furnacex1");
    std::array<ItemStack, 9> torch = {e, I("charcoal"), e, e, s, e, e, e, e};
    CHECK(out(torch, 3) == "minecraft:torchx4");
    std::array<ItemStack, 9> broken = {p, p, p, e, s, e, e, e, s};
    CHECK(out(broken, 3).empty());
}

TEST_CASE("smelting and fuel: raw iron -> ingot, logs -> charcoal; coal burns 1600 ticks") {
    CHECK(itemRegistry().item(smelt(I("raw_iron"))->item).id == "minecraft:iron_ingot");
    CHECK(itemRegistry().item(smelt(I("spruce_log"))->item).id == "minecraft:charcoal");
    CHECK(itemRegistry().item(smelt(I("sand"))->item).id == "minecraft:glass");
    CHECK_FALSE(smelt(I("stick")).has_value());
    CHECK(fuelTicks(I("coal")) == 1600);
    CHECK(fuelTicks(I("oak_planks")) == 300);
    CHECK(fuelTicks(I("stick")) == 100);
    CHECK(fuelTicks(I("wooden_pickaxe")) == 200);
    CHECK(fuelTicks(I("diamond")) == 0);
}

#include "gameplay/Furnace.h"

TEST_CASE("furnace: one coal smelts 8 items, 200 ticks each; no fuel used without input") {
    Furnace f;
    f.fuel = I("coal", 2);
    for (int t = 0; t < 50; ++t)
        tickFurnace(f);
    CHECK(f.fuel.count == 2); // nothing to smelt: fuel not lit
    CHECK_FALSE(f.lit());
    f.input = I("raw_iron", 10);
    int litChanges = 0;
    for (int t = 0; t < 1600; ++t)
        litChanges += tickFurnace(f);
    CHECK(f.output.count == 8); // 1600 ticks of coal = 8 items
    CHECK(itemRegistry().item(f.output.item).id == "minecraft:iron_ingot");
    CHECK(f.input.count == 2);
    CHECK(litChanges >= 1);
    for (int t = 0; t < 400; ++t)
        tickFurnace(f);
    CHECK(f.output.count == 10); // second coal took over
}

TEST_CASE("crafting: redstone torch, repeater, lever, buttons, block of redstone, lamp, piston") {
    const ItemStack r = I("redstone"), s = I("stick"), st = I("stone"), c = I("cobblestone"), e{};
    std::array<ItemStack, 4> torch = {r, e, s, e};
    CHECK(out(torch, 2) == "minecraft:redstone_torchx1");
    const ItemStack t = I("redstone_torch");
    std::array<ItemStack, 9> repeater = {t, r, t, st, st, st, e, e, e};
    CHECK(out(repeater, 3) == "minecraft:repeaterx1");
    std::array<ItemStack, 4> lever = {s, e, c, e};
    CHECK(out(lever, 2) == "minecraft:leverx1");
    std::array<ItemStack, 4> button = {e, st, e, e};
    CHECK(out(button, 2) == "minecraft:stone_buttonx1");
    std::array<ItemStack, 9> block = {r, r, r, r, r, r, r, r, r};
    CHECK(out(block, 3) == "minecraft:redstone_blockx1");
    std::array<ItemStack, 4> back = {I("redstone_block"), e, e, e};
    CHECK(out(back, 2) == "minecraft:redstonex9");
    std::array<ItemStack, 9> lamp = {e, r, e, r, I("glowstone"), r, e, r, e};
    CHECK(out(lamp, 3) == "minecraft:redstone_lampx1");
    const ItemStack p = I("birch_planks"), i = I("iron_ingot");
    std::array<ItemStack, 9> piston = {p, p, p, c, i, c, c, r, c};
    CHECK(out(piston, 3) == "minecraft:pistonx1");
}

TEST_CASE("crafting: copper tools from copper ingots") {
    const ItemStack c = I("copper_ingot"), s = I("stick"), e{};
    std::array<ItemStack, 9> pick = {c, c, c, e, s, e, e, s, e};
    CHECK(out(pick, 3) == "minecraft:copper_pickaxex1");
}

TEST_CASE("smelting recipes carry vanilla ids: ores per input, tag recipes after the output") {
    CHECK(recipeIdName(smeltRecipe(I("raw_iron"))) == "minecraft:iron_ingot_from_smelting_raw_iron");
    CHECK(recipeIdName(smeltRecipe(I("deepslate_gold_ore"))) == "minecraft:gold_ingot_from_smelting_deepslate_gold_ore");
    CHECK(recipeIdName(smeltRecipe(I("sand"))) == "minecraft:glass");
    CHECK(smeltRecipe(I("sand")) == smeltRecipe(I("red_sand"))); // one recipe (#smelts_to_glass)
    CHECK(recipeIdName(smeltRecipe(I("oak_log"))) == "minecraft:charcoal");
    CHECK(recipeIdName(smeltRecipe(I("beef"))) == "minecraft:cooked_beef");
    CHECK(smeltRecipe(I("stick")) == kNoRecipe);
    CHECK(recipeExperience(smeltRecipe(I("raw_gold"))) == doctest::Approx(1.0f));
}

TEST_CASE("furnace: smelting counts recipe uses; taking pays uses x experience, fraction by chance") {
    Furnace f;
    f.input = I("raw_iron", 2);
    f.fuel = I("coal");
    for (int t = 0; t < 2 * kFurnaceCookTicks + 2; ++t)
        tickFurnace(f);
    REQUIRE(f.output.count == 2);
    CHECK(f.recipesUsed[0].recipe == smeltRecipe(I("raw_iron")));
    CHECK(f.recipesUsed[0].count == 2);
    // 2 x 0.7 = 1.4: always 1 point, a second 40% of the time.
    Xoroshiro rng(7);
    int twos = 0;
    for (int i = 0; i < 1000; ++i) {
        const int xp = recipesExperience(f.recipesUsed, rng);
        CHECK((xp == 1 || xp == 2));
        twos += xp == 2;
    }
    CHECK(twos > 330);
    CHECK(twos < 470);
    CHECK(takeFurnaceExperience(f, rng) >= 1);
    CHECK(f.recipesUsed[0].recipe == kNoRecipe); // paid out once
    // Recipes we don't have (from vanilla worlds) pay nothing.
    f.countRecipe(internRecipeId("minecraft:unknown_recipe"), 5);
    CHECK(takeFurnaceExperience(f, rng) == 0);
}
