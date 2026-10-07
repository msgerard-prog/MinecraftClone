// Mining speed, harvest levels and drops (wiki: Breaking, Tiers, each block's page).
#include "gameplay/Mining.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }
ItemStack I(const char* id, int n = 1) { return {*itemRegistry().find(id), static_cast<uint8_t>(n)}; }

} // namespace

TEST_CASE("break times match the wiki's table (seconds x 20)") {
    const ItemStack hand{};
    CHECK(breakTicks(S(blocks::Stone), hand, true, false) == 150);                 // 7.5 s
    CHECK(breakTicks(S(blocks::Stone), I("wooden_pickaxe"), true, false) == 23);   // 1.15 s
    CHECK(breakTicks(S(blocks::Stone), I("stone_pickaxe"), true, false) == 12);    // 0.6 s
    CHECK(breakTicks(S(blocks::Stone), I("diamond_pickaxe"), true, false) == 6);   // 0.3 s
    CHECK(breakTicks(S(blocks::Dirt), hand, true, false) == 15);                   // 0.75 s
    CHECK(breakTicks(S(blocks::Dirt), I("wooden_shovel"), true, false) == 8);      // 0.4 s
    CHECK(breakTicks(S(blocks::OakLog), hand, true, false) == 60);                 // 3 s
    CHECK(breakTicks(S(blocks::DiamondOre), I("iron_pickaxe"), true, false) == 15); // 0.75 s
    // Wrong tier: like bare hands (no drops, /100).
    CHECK(breakTicks(S(blocks::DiamondOre), I("stone_pickaxe"), true, false) == 300);
    // Off the ground: 5x slower; bedrock never breaks; plants are instant.
    CHECK(breakTicks(S(blocks::Dirt), hand, false, false) == 75);
    CHECK(breakTicks(S(blocks::Bedrock), I("diamond_pickaxe"), true, false) == -1);
    CHECK(breakTicks(S(blocks::ShortGrass), hand, true, false) == 0);
}

TEST_CASE("harvest levels: iron needs stone, diamond needs iron; dirt by hand") {
    CHECK_FALSE(canHarvest(S(blocks::Stone), {}));
    CHECK(canHarvest(S(blocks::Stone), I("wooden_pickaxe")));
    CHECK_FALSE(canHarvest(S(blocks::IronOre), I("wooden_pickaxe")));
    CHECK(canHarvest(S(blocks::IronOre), I("stone_pickaxe")));
    CHECK_FALSE(canHarvest(S(blocks::DiamondOre), I("stone_pickaxe")));
    CHECK(canHarvest(S(blocks::DiamondOre), I("iron_pickaxe")));
    CHECK_FALSE(canHarvest(S(blocks::DiamondOre), I("golden_pickaxe"))); // gold is level 0
    CHECK(canHarvest(S(blocks::Dirt), {}));
}

TEST_CASE("drops: stone -> cobblestone, ores -> materials, nothing without the tool") {
    Xoroshiro rng(1);
    auto first = [&](BlockId b, const ItemStack& held) {
        std::vector<ItemStack> d;
        blockDrops(S(b), held, rng, d);
        return d.empty() ? std::string() : itemRegistry().item(d[0].item).id;
    };
    CHECK(first(blocks::Stone, I("wooden_pickaxe")) == "minecraft:cobblestone");
    CHECK(first(blocks::Stone, {}).empty());
    CHECK(first(blocks::GrassBlock, {}) == "minecraft:dirt");
    CHECK(first(blocks::CoalOre, I("wooden_pickaxe")) == "minecraft:coal");
    CHECK(first(blocks::IronOre, I("stone_pickaxe")) == "minecraft:raw_iron");
    CHECK(first(blocks::DiamondOre, I("iron_pickaxe")) == "minecraft:diamond");
    CHECK(first(blocks::OakLog, {}) == "minecraft:oak_log");
    CHECK(first(blocks::Glass, {}).empty());
    int redstone = 0;
    for (int i = 0; i < 50; ++i) {
        std::vector<ItemStack> d;
        blockDrops(S(blocks::RedstoneOre), I("iron_pickaxe"), rng, d);
        REQUIRE(d.size() == 1);
        CHECK(d[0].count >= 4);
        CHECK(d[0].count <= 5);
        redstone += d[0].count;
    }
    CHECK(redstone > 200);
}

TEST_CASE("item registry: block items share block ids; tools stack to 1") {
    const auto& items = itemRegistry();
    CHECK(items.item(items.blockItem(blocks::Stone)).id == "minecraft:stone");
    CHECK(items.blockItem(blocks::Water) == kNoItem);
    const auto pick = *items.find("diamond_pickaxe");
    CHECK(items.item(pick).maxStack == 1);
    CHECK(items.item(pick).durability == 1561);
    CHECK(items.item(*items.find("stick")).maxStack == 64);
}

TEST_CASE("furnaces need a pickaxe; crafting tables are axe blocks") {
    CHECK_FALSE(canHarvest(S(blocks::Furnace), {}));
    CHECK(canHarvest(S(blocks::Furnace), I("wooden_pickaxe")));
    CHECK(breakTicks(S(blocks::Furnace), {}, true, false) == 350); // 3.5 hardness / 100
    CHECK(breakTicks(S(blocks::CraftingTable), I("stone_axe"), true, false) <
          breakTicks(S(blocks::CraftingTable), {}, true, false));
}

TEST_CASE("copper tools (1.21.9): stone's harvest level, speed 5, 190 uses") {
    CHECK(canHarvest(S(blocks::IronOre), I("copper_pickaxe")));
    CHECK_FALSE(canHarvest(S(blocks::DiamondOre), I("copper_pickaxe")));
    CHECK(breakTicks(S(blocks::Stone), I("copper_pickaxe"), true, false) == 9); // 5 / 1.5 / 30 per tick
    const ItemDef& sword = itemRegistry().item(*itemRegistry().find("copper_sword"));
    CHECK(sword.durability == 190);
    CHECK(sword.attackDamage == 5.0f);
}

TEST_CASE("short grass sometimes drops wheat seeds (1 in 8); glass still nothing") {
    mc::world::Xoroshiro rng(9);
    int seeds = 0;
    for (int i = 0; i < 800; ++i) {
        std::vector<mc::world::ItemStack> out;
        mc::blockDrops(mc::world::blockRegistry().defaultState(mc::world::blocks::ShortGrass), {}, rng, out);
        seeds += int(out.size());
    }
    CHECK(seeds > 60);
    CHECK(seeds < 150);
}
