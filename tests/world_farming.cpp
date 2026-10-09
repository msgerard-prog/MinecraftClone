// Farming (wiki: Farmland, Hoe, crops, Bone Meal, Tutorial:Crop farming).
#include "gameplay/Mining.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

// 3x3 chunks of dirt (top y 63), lit by full sky light; the centre chunk random-ticks.
struct Scene {
    World world;
    BlockUpdates updates{world};
    int64_t time = 0;
    explicit Scene(int sky = 15) {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, S(blocks::Dirt));
                auto l = std::make_shared<SectionLight>();
                l->sky.fill(static_cast<uint8_t>(sky));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(l);
                c.setLight(light);
            }
        updates.setRandomTicks({0, 0}, 1, BlockUpdates::kDefaultRandomTickSpeed);
    }
    void put(BlockPos p, BlockStateId s) {
        updates.setTime(time);
        world.updateBlock(p, s);
    }
    void tick(int n) {
        for (int i = 0; i < n; ++i) {
            ++time;
            updates.setTime(time);
            updates.tick();
        }
    }
    BlockId block(BlockPos p) const { return R().blockOf(world.getBlock(p)); }
};

BlockStateId wetFarmland() { return R().set(S(blocks::Farmland), properties::moisture, 7); }

} // namespace

TEST_CASE("a hoe tills dirt and grass into farmland, coarse dirt into dirt; not under a block") {
    Scene s;
    CHECK(BlockUpdates::till(s.world, {2, 63, 2}, Direction::Up));
    CHECK(s.block({2, 63, 2}) == blocks::Farmland);
    s.world.setBlock({3, 63, 2}, S(blocks::CoarseDirt));
    CHECK(BlockUpdates::till(s.world, {3, 63, 2}, Direction::Up));
    CHECK(s.block({3, 63, 2}) == blocks::Dirt);
    s.world.setBlock({4, 64, 2}, S(blocks::Stone));
    CHECK_FALSE(BlockUpdates::till(s.world, {4, 63, 2}, Direction::Up));
    CHECK_FALSE(BlockUpdates::till(s.world, {5, 63, 2}, Direction::Down));
}

TEST_CASE("farmland: wet near water (4 blocks), dries out and turns to dirt when bare") {
    Scene s;
    s.world.setBlock({8, 63, 8}, S(blocks::Water));
    s.put({11, 63, 8}, S(blocks::Farmland)); // 3 away
    s.put({8, 63, 2}, wetFarmland());        // 6 away, bare
    s.put({2, 63, 2}, S(blocks::Farmland)); // dry, but a stem keeps it (M29.4b regression)
    s.put({2, 64, 1}, S(blocks::Pumpkin));
    s.put({2, 64, 2}, R().set(S(blocks::AttachedPumpkinStem), properties::facing, 0));
    s.tick(20000);
    CHECK(R().get(s.world.getBlock({11, 63, 8}), properties::moisture) == 7);
    CHECK(s.block({8, 63, 2}) == blocks::Dirt);
    CHECK(s.block({2, 63, 2}) == blocks::Farmland);
    CHECK(s.block({2, 64, 2}) == blocks::AttachedPumpkinStem);
}

TEST_CASE("growth points: wet farmland 4 + 8 wet neighbours x 0.75 = 10; halved by a diagonal twin") {
    Scene s;
    for (int x = 4; x <= 6; ++x)
        for (int z = 4; z <= 6; ++z)
            s.world.setBlock({x, 63, z}, wetFarmland());
    s.world.setBlock({5, 64, 5}, S(blocks::Wheat));
    CHECK(s.updates.growthPoints({5, 64, 5}, blocks::Wheat) == doctest::Approx(10.0f));
    s.world.setBlock({4, 64, 4}, S(blocks::Wheat));
    CHECK(s.updates.growthPoints({5, 64, 5}, blocks::Wheat) == doctest::Approx(5.0f));
    s.world.setBlock({4, 64, 4}, S(blocks::Carrots)); // another crop doesn't count
    CHECK(s.updates.growthPoints({5, 64, 5}, blocks::Wheat) == doctest::Approx(10.0f));
}

TEST_CASE("wheat grows to age 7 on lit wet farmland; not in the dark") {
    Scene s; // a wet 3x3 field (10 points: a 1 in 3 chance per random tick)
    for (int x = 4; x <= 6; ++x)
        for (int z = 4; z <= 6; ++z)
            s.world.setBlock({x, 63, z}, wetFarmland());
    s.world.setBlock({7, 63, 5}, S(blocks::Water)); // keeps it wet (same level)
    s.put({5, 64, 5}, S(blocks::Wheat));
    s.tick(80000);
    CHECK(BlockUpdates::cropAge(s.world.getBlock({5, 64, 5})) == 7);
    Scene d(0);
    d.world.setBlock({5, 63, 5}, wetFarmland());
    d.world.setBlock({5, 62, 4}, S(blocks::Water));
    d.put({5, 64, 5}, S(blocks::Wheat));
    d.tick(20000);
    CHECK(BlockUpdates::cropAge(d.world.getBlock({5, 64, 5})) == 0);
}

TEST_CASE("crops plant only on farmland and pop off when it goes; trampling") {
    Scene s;
    CHECK_FALSE(BlockUpdates::placement(s.world, S(blocks::Carrots), {2, 64, 2}, Direction::Up, 0, 0));
    s.world.setBlock({2, 63, 2}, wetFarmland());
    CHECK(BlockUpdates::placement(s.world, S(blocks::Carrots), {2, 64, 2}, Direction::Up, 0, 0));
    s.put({2, 64, 2}, S(blocks::Carrots));
    s.updates.trample({2, 63, 2});
    CHECK(s.block({2, 63, 2}) == blocks::Dirt);
    CHECK(s.block({2, 64, 2}) == 0);
    REQUIRE(s.updates.drops().size() == 1);
    CHECK(R().blockOf(s.updates.drops()[0].loot) == blocks::Carrots);
}

TEST_CASE("bone meal: crops +2..5 stages (beetroots +1), not past ripe; grass sprouts plants") {
    Scene s;
    s.world.setBlock({2, 63, 2}, wetFarmland());
    s.world.setBlock({2, 64, 2}, S(blocks::Wheat));
    CHECK(s.updates.boneMeal({2, 64, 2}));
    const int a = BlockUpdates::cropAge(s.world.getBlock({2, 64, 2}));
    CHECK(a >= 2);
    CHECK(a <= 5);
    while (BlockUpdates::cropAge(s.world.getBlock({2, 64, 2})) < 7)
        s.updates.boneMeal({2, 64, 2});
    CHECK_FALSE(s.updates.boneMeal({2, 64, 2})); // ripe: not used
    s.world.setBlock({4, 63, 4}, wetFarmland());
    s.world.setBlock({4, 64, 4}, S(blocks::Beetroots));
    s.updates.boneMeal({4, 64, 4});
    CHECK(BlockUpdates::cropAge(s.world.getBlock({4, 64, 4})) == 1);
    for (int x = 5; x <= 11; ++x)
        for (int z = 5; z <= 11; ++z)
            s.world.setBlock({x, 63, z}, S(blocks::GrassBlock));
    CHECK(s.updates.boneMeal({8, 63, 8}));
    int plants = 0;
    for (int x = 5; x <= 11; ++x)
        for (int z = 5; z <= 11; ++z)
            plants += s.world.getBlock({x, 64, z}) != 0;
    CHECK(plants > 5);
}

TEST_CASE("crop drops: ripe wheat gives wheat and 1-4 seeds, unripe a seed; carrots 2-5") {
    Xoroshiro rng(3);
    std::vector<ItemStack> out;
    mc::blockDrops(R().set(S(blocks::Wheat), properties::age7, 7), {}, rng, out);
    REQUIRE(out.size() == 2);
    CHECK(out[0].item == *itemRegistry().find("wheat"));
    CHECK(out[1].item == *itemRegistry().find("wheat_seeds"));
    CHECK(out[1].count >= 1);
    CHECK(out[1].count <= 4);
    out.clear();
    mc::blockDrops(S(blocks::Wheat), {}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].count == 1);
    out.clear();
    mc::blockDrops(R().set(S(blocks::Carrots), properties::age7, 7), {}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].count >= 2);
    CHECK(out[0].count <= 5);
}

TEST_CASE("M29.4b: a melon stem grows, sets a melon beside it and bends; picking the melon unbends it") {
    Scene s;
    for (int x = 4; x <= 6; ++x)
        for (int z = 4; z <= 6; ++z)
            s.world.setBlock({x, 63, z}, wetFarmland());
    s.world.setBlock({7, 63, 7}, S(blocks::Water));
    s.put({5, 64, 5}, S(blocks::MelonStem));
    BlockPos fruit{};
    for (int i = 0; i < 400 && fruit.y == 0; ++i) {
        s.tick(500);
        for (const BlockPos q : {BlockPos{4, 64, 5}, BlockPos{6, 64, 5}, BlockPos{5, 64, 4}, BlockPos{5, 64, 6}})
            if (s.block(q) == blocks::Melon) fruit = q;
    }
    REQUIRE(fruit.y == 64);
    CHECK(s.block({5, 64, 5}) == blocks::AttachedMelonStem);
    s.put(fruit, 0); // harvested
    CHECK(s.block({5, 64, 5}) == blocks::MelonStem);
    CHECK(R().get(s.world.getBlock({5, 64, 5}), properties::age7) == 7);
    // Seeds plant only on farmland; drops: a melon gives 3-7 slices.
    Xoroshiro rng(5);
    std::vector<ItemStack> out;
    mc::blockDrops(S(blocks::Melon), {}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].item == *itemRegistry().find("melon_slice"));
    CHECK(out[0].count >= 3);
    CHECK(out[0].count <= 7);
}

TEST_CASE("M29.4b: cocoa hangs from a jungle log, ripens and drops 3 beans; it falls with the log") {
    Scene s;
    s.put({5, 64, 5}, S(blocks::JungleLog));
    // facing north (index 0): the log is north of the pod
    s.put({5, 64, 6}, R().set(S(blocks::Cocoa), properties::facing, 0));
    s.tick(30000);
    CHECK(R().get(s.world.getBlock({5, 64, 6}), properties::age2) == 2);
    Xoroshiro rng(5);
    std::vector<ItemStack> out;
    mc::blockDrops(s.world.getBlock({5, 64, 6}), {}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].count == 3);
    s.put({5, 64, 5}, 0);
    CHECK(s.block({5, 64, 6}) == 0);
}
