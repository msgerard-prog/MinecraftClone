// Random ticks (wiki: Tick › Random tick, Grass Block, Leaves, Sapling, Snow, Ice).
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

// 3x3 chunks, dirt up to y 63 with a lit sky (15) everywhere; `blockLight` everywhere.
struct Scene {
    World world;
    BlockUpdates updates{world};
    int64_t time = 0;
    explicit Scene(int sky = 15, int blockLight = 0) {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        for (int y = 60; y <= 63; ++y)
                            c.set(x, y, z, S(blocks::Dirt));
                auto l = std::make_shared<SectionLight>();
                l->sky.fill(static_cast<uint8_t>(sky));
                l->block.fill(static_cast<uint8_t>(blockLight));
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

} // namespace

TEST_CASE("sections count their random-ticking blocks") {
    Section s;
    CHECK(s.randomTickingCount() == 0);
    s.set(1, 2, 3, S(blocks::GrassBlock));
    s.set(4, 5, 6, S(blocks::Stone));
    CHECK(s.randomTickingCount() == 1);
    s.set(1, 2, 3, S(blocks::Dirt));
    CHECK(s.randomTickingCount() == 0);
    s.fill(S(blocks::Ice));
    CHECK(s.randomTickingCount() == Section::kVolume);
    // Leaves tick only while they could decay (distance 7, not persistent).
    CHECK(R().randomTicks(S(blocks::OakLeaves))); // default: distance 7, not persistent
    CHECK_FALSE(R().randomTicks(R().set(S(blocks::OakLeaves), properties::distance, 2)));
    CHECK_FALSE(R().randomTicks(R().set(S(blocks::OakLeaves), properties::persistent, 0)));
}

TEST_CASE("grass spreads to lit dirt nearby and dies under an opaque block") {
    Scene s; // (only the centre chunk random-ticks: its 8 neighbours are loaded)
    s.put({8, 63, 8}, S(blocks::GrassBlock));
    s.tick(6000);
    int grass = 0;
    for (int x = 5; x <= 11; ++x)
        for (int z = 5; z <= 11; ++z)
            grass += s.block({x, 63, z}) == blocks::GrassBlock;
    CHECK(grass > 4);
    s.put({8, 64, 8}, S(blocks::Stone));
    s.tick(6000);
    CHECK(s.block({8, 63, 8}) == blocks::Dirt);
}

TEST_CASE("grass doesn't spread at night without block light") {
    Scene s;
    s.updates.setSkyDarken(11); // sky 15 -> 4
    s.put({0, 63, 0}, S(blocks::GrassBlock));
    s.tick(6000);
    int grass = 0;
    for (int x = -3; x <= 3; ++x)
        for (int z = -3; z <= 3; ++z)
            grass += s.block({x, 63, z}) == blocks::GrassBlock;
    CHECK(grass == 1);
}

TEST_CASE("leaves cut off from their log decay with their loot; persistent leaves stay") {
    Scene s;
    s.put({0, 64, 0}, S(blocks::OakLog));
    s.put({0, 65, 0}, S(blocks::OakLog));
    // Natural leaves (not persistent) around the top log, as a tree grows them.
    const BlockStateId leaf = R().set(S(blocks::OakLeaves), properties::persistent, 1);
    s.world.setBlock({1, 65, 0}, R().set(leaf, properties::distance, 0));
    s.world.setBlock({2, 65, 0}, R().set(leaf, properties::distance, 1));
    // Placed by a player (through the placement rule): persistent.
    s.put({0, 66, 0}, *BlockUpdates::placement(s.world, S(blocks::OakLeaves), {0, 66, 0}, Direction::Up, 0, 0));
    s.tick(10);
    CHECK(R().get(s.world.getBlock({0, 66, 0}), properties::persistent) == 0);
    s.tick(4000);
    CHECK(s.block({1, 65, 0}) == blocks::OakLeaves); // a log within reach: stays
    // Remove the logs: the two leaves raise each other's distance one step per tick
    // up to 7, then they can decay.
    s.put({0, 65, 0}, 0);
    s.put({0, 64, 0}, 0);
    s.tick(20);
    CHECK(R().get(s.world.getBlock({1, 65, 0}), properties::distance) == 6); // "7"
    s.tick(20000);
    CHECK(s.block({1, 65, 0}) == 0);
    CHECK(s.block({2, 65, 0}) == 0);
    CHECK(s.block({0, 66, 0}) == blocks::OakLeaves); // persistent
    int loot = 0;
    for (const auto& d : s.updates.drops())
        loot += d.loot != 0;
    CHECK(loot == 2);
}

TEST_CASE("a lit sapling grows into a tree of its kind; without room it waits") {
    Scene s;
    s.put({0, 64, 0}, S(blocks::BirchSapling));
    CHECK(s.block({0, 64, 0}) == blocks::BirchSapling);
    s.tick(60000);
    CHECK(s.block({0, 64, 0}) == blocks::BirchLog);
    CHECK(s.block({0, 68, 0}) == blocks::BirchLog); // birch: 5-7 logs
    CHECK(s.block({0, 63, 0}) == blocks::Dirt);
    int leaves = 0;
    for (int x = -2; x <= 2; ++x)
        for (int z = -2; z <= 2; ++z)
            for (int y = 66; y <= 72; ++y)
                leaves += s.block({x, y, z}) == blocks::BirchLeaves;
    CHECK(leaves > 20);
    // A stone roof 2 blocks up: no room for the trunk.
    s.put({5, 64, 5}, S(blocks::OakSapling));
    s.put({5, 66, 5}, S(blocks::Stone));
    s.tick(60000);
    CHECK(s.block({5, 64, 5}) == blocks::OakSapling);
}

TEST_CASE("saplings in the dark don't grow; a sapling without soil pops off") {
    Scene s(0, 0);
    s.put({0, 64, 0}, S(blocks::OakSapling));
    s.tick(30000);
    CHECK(s.block({0, 64, 0}) == blocks::OakSapling);
    s.put({0, 63, 0}, S(blocks::Stone));
    CHECK(s.block({0, 64, 0}) == 0);
    REQUIRE(s.updates.drops().size() == 1);
    CHECK(R().blockOf(s.updates.drops()[0].loot) == blocks::OakSapling);
    CHECK_FALSE(BlockUpdates::placement(s.world, S(blocks::OakSapling), {0, 64, 0}, Direction::Up, 0, 0));
}

TEST_CASE("ice and snow layers melt in block light above 11; ice becomes water") {
    Scene s(15, 12);
    s.put({0, 64, 0}, S(blocks::Ice));
    s.put({2, 64, 0}, S(blocks::Snow));
    s.tick(20000);
    CHECK(s.block({0, 64, 0}) == blocks::Water);
    CHECK(s.block({2, 64, 0}) == 0);
    Scene sun(15, 0); // sky light alone never melts them
    sun.put({0, 64, 0}, S(blocks::Ice));
    sun.tick(20000);
    CHECK(sun.block({0, 64, 0}) == blocks::Ice);
}

TEST_CASE("grass under 2+ snow layers dies; one layer is fine; spreads under snow as snowy") {
    Scene s;
    s.put({0, 63, 0}, S(blocks::GrassBlock));
    s.put({0, 64, 0}, S(blocks::Snow)); // 1 layer
    s.put({1, 63, 1}, S(blocks::GrassBlock));
    s.put({1, 64, 1}, R().set(S(blocks::Snow), properties::layers, 2)); // 3 layers
    s.put({2, 64, 2}, S(blocks::Snow)); // dirt under snow nearby
    s.tick(8000);
    CHECK(s.block({0, 63, 0}) == blocks::GrassBlock);
    CHECK(s.block({1, 63, 1}) == blocks::Dirt);
    if (s.block({2, 63, 2}) == blocks::GrassBlock)
        CHECK(R().get(s.world.getBlock({2, 63, 2}), properties::snowy) == 0); // snowy=true
}
