// Blocks of M29 (completeness; wiki pages of each block).
#include "gameplay/Mobs.h"
#include "gameplay/Recipes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Potions.h"
#include "world/World.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

TEST_CASE("M29.4a: flowers behave like poppies; lily pads need water; cave air is air; raw blocks craft") {
    const auto& r = blockRegistry();
    CHECK(r.likeOf(blocks::RedTulip) == blocks::Poppy);
    CHECK(r.findBlock("minecraft:cave_air") == BlockId(0));
    World w;
    w.createChunk({0, 0});
    BlockUpdates u(w);
    w.setListener(&u);
    // A lily pad stays on water, breaks when the water goes; a wither rose stands on soul sand.
    w.setBlock({4, 63, 4}, r.defaultState(blocks::Water));
    w.setBlock({4, 64, 4}, r.defaultState(blocks::LilyPad));
    w.updateBlock({4, 63, 4}, r.defaultState(blocks::Water));
    CHECK(w.getBlock({4, 64, 4}) == r.defaultState(blocks::LilyPad));
    w.updateBlock({4, 63, 4}, r.defaultState(blocks::Stone));
    CHECK(w.getBlock({4, 64, 4}) == 0);
    w.setBlock({6, 63, 4}, r.defaultState(blocks::SoulSand));
    w.setBlock({6, 64, 4}, r.defaultState(blocks::WitherRose));
    w.updateBlock({6, 63, 4}, r.defaultState(blocks::SoulSoil));
    CHECK(w.getBlock({6, 64, 4}) == r.defaultState(blocks::WitherRose));
    std::array<ItemStack, 9> g{};
    for (auto& s : g) s = {*itemRegistry().find("raw_gold"), 1};
    CHECK(itemRegistry().item(craft(g, 3)->item).id == "minecraft:raw_gold_block");
    g = {};
    g[0] = {*itemRegistry().find("wither_rose"), 1};
    CHECK(itemRegistry().item(craft(g, 3)->item).id == "minecraft:black_dye");
    // A jack o'lantern on two snow blocks makes a snow golem.
    w.setBlock({8, 64, 8}, r.defaultState(blocks::SnowBlock));
    w.setBlock({8, 65, 8}, r.defaultState(blocks::SnowBlock));
    w.setBlock({8, 66, 8}, r.defaultState(blocks::JackOLantern));
    Xoroshiro rng{1};
    CHECK(Mobs::buildSnowGolem(w, {8, 66, 8}, rng));
    CHECK(r.lightEmission(r.defaultState(blocks::JackOLantern)) == 15);
}
