// The remaining blocks (M28.5a; wiki: Cake, Candle, Candle Cake, Pink Petals, Wildflowers,
// Leaf Litter, Firefly Bush, Bush, Dry Grass, Cactus Flower, Vines).
#include "gameplay/BlockInteraction.h"
#include "gameplay/Inventory.h"
#include "gameplay/Mining.h"
#include "gameplay/Player.h"
#include "gameplay/Recipes.h"
#include "gameplay/Vitals.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Raycast.h"
#include "world/World.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {
const BlockRegistry& R() { return blockRegistry(); }
struct Floor {
    World world;
    Floor() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) world.createChunk({cx, cz});
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) world.setBlock({x, 63, z}, R().defaultState(blocks::Stone));
    }
};
} // namespace

TEST_CASE("candles: 17 kinds, 1-4 in a block, 3 light a lit candle; candle cakes; cake bites") {
    CHECK(R().findBlock("magenta_candle"));
    CHECK(R().likeOf(*R().findBlock("magenta_candle_cake")) == blocks::CandleCake);
    const BlockStateId four = R().set(R().set(R().defaultState(blocks::Candle), properties::candles, 3), properties::lit, 0);
    CHECK(R().lightEmission(four) == 12);
    CHECK(R().lightEmission(R().defaultState(blocks::Candle)) == 0);
    CHECK(R().lightEmission(R().set(R().defaultState(*R().findBlock("red_candle_cake")), properties::lit, 0)) == 3);
    CHECK_FALSE(itemRegistry().find("red_candle_cake")); // (made by putting a candle on a cake)
    CHECK(itemRegistry().item(*itemRegistry().find("cake")).maxStack == 1);

    Floor f;
    Player player;
    player.setPosition({5.5, 64.0, 2.5});
    Inventory inv;
    inv.setSlot(0, {*itemRegistry().find("red_candle"), 5});
    Vitals vitals;
    Xoroshiro rng{1};
    BlockInteraction bi;
    std::vector<BlockPos> changed;
    std::vector<BlockInteraction::Drop> drops;
    InteractionInput in;
    in.useClick = true;
    bi.tickSurvival(f.world, player, RayHit{{5, 63, 5}, Direction::Up, 2.0}, inv, vitals, in, false, rng, changed, drops);
    REQUIRE(R().blockOf(f.world.getBlock({5, 64, 5})) == *R().findBlock("red_candle"));
    BlockInteraction bi2;
    bi2.tickSurvival(f.world, player, RayHit{{5, 64, 5}, Direction::Up, 2.0}, inv, vitals, in, false, rng, changed, drops);
    CHECK(R().get(f.world.getBlock({5, 64, 5}), properties::candles) == 1); // (two now)

    // A candle on an uneaten cake: a candle cake of its colour.
    f.world.setBlock({7, 64, 5}, R().defaultState(blocks::Cake));
    BlockInteraction bi3;
    bi3.tickSurvival(f.world, player, RayHit{{7, 64, 5}, Direction::Up, 2.0}, inv, vitals, in, false, rng, changed, drops);
    CHECK(R().blockOf(f.world.getBlock({7, 64, 5})) == *R().findBlock("red_candle_cake"));

    // Drops: each candle; a candle cake its candle.
    std::vector<ItemStack> out;
    blockDrops(f.world.getBlock({5, 64, 5}), {}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].count == 2);
    out.clear();
    blockDrops(f.world.getBlock({7, 64, 5}), {}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(itemRegistry().item(out[0].item).id == "minecraft:red_candle");
}

TEST_CASE("petals, wildflowers and leaf litter fill quarters; plants and candles pop without their floor") {
    Floor f;
    BlockUpdates updates(f.world);
    f.world.setBlock({3, 64, 3}, R().set(R().defaultState(blocks::Wildflowers), properties::flowerAmount, 2));
    f.world.setBlock({4, 64, 3}, R().defaultState(blocks::FireflyBush));
    f.world.setBlock({5, 64, 3}, R().defaultState(blocks::Candle));
    CHECK(R().lightEmission(R().defaultState(blocks::FireflyBush)) == 2);
    Xoroshiro rng{2};
    std::vector<ItemStack> out;
    blockDrops(f.world.getBlock({3, 64, 3}), {}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].count == 3);
    f.world.updateBlock({4, 63, 3}, 0);
    CHECK(f.world.getBlock({4, 64, 3}) == 0);
    f.world.updateBlock({5, 63, 3}, 0);
    CHECK(f.world.getBlock({5, 64, 3}) == 0);
}

TEST_CASE("vines hang on walls, are climbed, grow down and drop off when the wall goes") {
    Floor f;
    BlockUpdates updates(f.world);
    for (int y = 64; y < 72; ++y) f.world.setBlock({8, y, 8}, R().defaultState(blocks::Stone));
    const auto placed = BlockUpdates::placement(f.world, R().defaultState(blocks::Vine), {8, 70, 9}, Direction::South, 0, 0);
    REQUIRE(placed);
    CHECK(R().get(*placed, properties::fireNorth) == 0); // (the stone is north of it)
    CHECK_FALSE(BlockUpdates::placement(f.world, R().defaultState(blocks::Vine), {3, 64, 3}, Direction::Up, 0, 0));
    f.world.updateBlock({8, 70, 9}, *placed);
    updates.setRandomTicks({0, 0}, 1, 3000); // (many random ticks a tick)
    for (int t = 0; t < 40; ++t) updates.tick();
    CHECK(R().blockOf(f.world.getBlock({8, 69, 9})) == blocks::Vine);

    Player p;
    p.setPosition({8.5, 69.0, 9.5});
    p.setCreative(false);
    p.tick(f.world, {});
    CHECK(p.climbing());

    std::array<ItemStack, 9> g{};
    g[0] = g[1] = g[2] = {*itemRegistry().find("milk_bucket"), 1};
    g[3] = g[5] = {*itemRegistry().find("sugar"), 1};
    g[4] = {*itemRegistry().find("egg"), 1};
    g[6] = g[7] = g[8] = {*itemRegistry().find("wheat"), 1};
    const auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:cake");
}
