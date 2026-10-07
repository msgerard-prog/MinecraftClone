// Fire (wiki: Fire, Lava, Flint and Steel).
#include "gameplay/Portals.h"
#include "gameplay/Vitals.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

// 3x3 chunks with a stone floor at y 63.
struct Scene {
    World world;
    BlockUpdates updates{world};
    int64_t time = 0;
    Scene() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, S(blocks::Stone));
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
    int count(BlockId b, int r = 8) const {
        int n = 0;
        for (int x = -r; x <= r; ++x)
            for (int z = -r; z <= r; ++z)
                for (int y = 60; y <= 72; ++y)
                    n += block({x, y, z}) == b;
        return n;
    }
};

} // namespace

TEST_CASE("fire on bare stone burns out; on netherrack it burns forever") {
    Scene s;
    s.put({0, 64, 0}, BlockUpdates::fireState(0));
    CHECK(s.block({0, 64, 0}) == blocks::Fire);
    s.tick(1200); // ages 1 in 3 ticks of 30-40: past age 3 within a minute
    CHECK(s.block({0, 64, 0}) == 0);
    s.put({3, 63, 3}, S(blocks::Netherrack));
    s.put({3, 64, 3}, BlockUpdates::fireState(0));
    s.tick(6000);
    CHECK(s.block({3, 64, 3}) == blocks::Fire);
}

TEST_CASE("fire can't hang in the air unless something flammable is next to it") {
    Scene s;
    s.put({0, 66, 0}, BlockUpdates::fireState(0));
    CHECK(s.block({0, 66, 0}) == 0);
    s.put({1, 66, 0}, S(blocks::OakPlanks));
    s.put({0, 66, 0}, BlockUpdates::fireState(0));
    CHECK(s.block({0, 66, 0}) == blocks::Fire);
    s.put({1, 66, 0}, 0); // its support gone: it goes out
    CHECK(s.block({0, 66, 0}) == 0);
}

TEST_CASE("fire spreads over a floor of leaves and burns it away; stone doesn't burn") {
    Scene s;
    const BlockStateId leaves = R().set(S(blocks::OakLeaves), properties::persistent, 0);
    for (int x = -8; x <= 8; ++x)
        for (int z = -8; z <= 8; ++z)
            s.world.setBlock({x, 64, z}, leaves);
    // A single fire often burns its own support away first; a few starts spread.
    for (int x = -6; x <= 6; x += 6)
        for (int z = -6; z <= 6; z += 6)
            s.put({x, 65, z}, BlockUpdates::fireState(0));
    s.tick(4000);
    CHECK(s.count(blocks::OakLeaves) < 289 - 60); // much of it burnt away
    CHECK(s.count(blocks::Stone, 8) == 17 * 17); // the stone floor is intact
}

TEST_CASE("logs burn slower than leaves (burn odds 5 vs 60)") {
    Scene s;
    for (int z = -2; z <= 2; ++z) {
        s.world.setBlock({-2, 64, z}, S(blocks::OakLog));
        s.world.setBlock({2, 64, z}, R().set(S(blocks::OakLeaves), properties::persistent, 0));
        s.put({-1, 64, z}, BlockUpdates::fireState(0));
        s.put({1, 64, z}, BlockUpdates::fireState(0));
    }
    s.tick(1500);
    int logs = 0, leaves = 0;
    for (int z = -2; z <= 2; ++z) {
        logs += s.block({-2, 64, z}) == blocks::OakLog;
        leaves += s.block({2, 64, z}) == blocks::OakLeaves;
    }
    CHECK(logs > leaves);
}

TEST_CASE("lava sets nearby wood on fire through its random ticks") {
    Scene s;
    s.world.setBlock({0, 64, 0}, S(blocks::Lava));
    for (int x = -2; x <= 2; ++x)
        for (int z = -2; z <= 2; ++z)
            if (x != 0 || z != 0) s.world.setBlock({x, 64, z}, S(blocks::OakPlanks));
    s.tick(20000);
    CHECK(s.count(blocks::OakPlanks, 3) < 24); // something caught fire and burnt
}

TEST_CASE("flowing water puts fire out") {
    Scene s;
    s.put({1, 63, 0}, S(blocks::Netherrack));
    s.put({1, 64, 0}, BlockUpdates::fireState(0));
    s.put({0, 64, 0}, S(blocks::Water));
    s.tick(10);
    CHECK(s.block({1, 64, 0}) == blocks::Water);
    CHECK(s.updates.drops().empty()); // fire drops nothing
}

TEST_CASE("flint and steel lights fire on a solid face; not in the air") {
    Scene s;
    std::vector<BlockPos> changed;
    const ItemId flint = *itemRegistry().find("flint_and_steel");
    CHECK(mc::portals::useItem(s.world, Dimension::Overworld, flint, {0, 63, 0}, Direction::Up, changed));
    CHECK(s.block({0, 64, 0}) == blocks::Fire);
    s.put({5, 70, 5}, S(blocks::Glass)); // clicking glass's side: fire would hang in the air
    CHECK_FALSE(mc::portals::useItem(s.world, Dimension::Overworld, flint, {5, 70, 5}, Direction::East, changed));
}

TEST_CASE("standing in fire: 1 damage per hurt cooldown, alight after a second") {
    mc::Vitals v;
    v.tick(64.0, true, false, false);
    float total = 0.0f;
    for (int i = 0; i < 19; ++i) {
        total += v.touchFire(true);
        v.tick(64.0, true, false, false);
    }
    CHECK_FALSE(v.burning());
    CHECK(total >= 1.0f);
    v.touchFire(true);
    CHECK(v.burning());
    CHECK(v.fireTicks() == 160);
    v.touchFire(false);
}

TEST_CASE("fire more than 128 blocks from the player stays as it is (1.21.11)") {
    Scene s;
    s.updates.setPlayer({500.0, 64.0, 0.0});
    s.put({3, 63, 3}, S(blocks::OakLog));
    s.put({3, 64, 3}, BlockUpdates::fireState(0));
    s.tick(3000);
    CHECK(s.block({3, 64, 3}) == blocks::Fire); // not aged, spread or burnt out
    CHECK(s.block({3, 63, 3}) == blocks::OakLog);
    CHECK(R().get(s.world.getBlock({3, 64, 3}), properties::age) == 0);
}

TEST_CASE("lava doesn't light flowers") {
    Scene s;
    s.world.setBlock({0, 64, 0}, S(blocks::Lava));
    for (int x = -1; x <= 1; ++x)
        for (int z = -1; z <= 1; ++z)
            if (x != 0 || z != 0) s.world.setBlock({x, 64, z}, S(blocks::Stone));
    for (int x = -2; x <= 2; ++x)
        for (int z = -2; z <= 2; ++z)
            s.world.setBlock({x, 65, z}, S(blocks::Stone)); // a lid with poppies on it
    s.world.setBlock({0, 65, 0}, 0);
    s.world.setBlock({0, 66, 0}, 0);
    for (int x = -1; x <= 1; ++x)
        for (int z = -1; z <= 1; ++z)
            if (x != 0 || z != 0) s.world.setBlock({x, 66, z}, S(blocks::Poppy));
    s.tick(30000);
    CHECK(s.count(blocks::Fire, 3) == 0);
    CHECK(s.count(blocks::Poppy, 3) == 8);
}

TEST_CASE("fire on bedrock burns forever in the End only") {
    Scene s;
    s.put({0, 63, 0}, S(blocks::Bedrock));
    s.put({0, 64, 0}, BlockUpdates::fireState(0));
    s.tick(1500);
    CHECK(s.block({0, 64, 0}) == 0);
    Scene end;
    end.world.setHasSkyLight(false);
    end.put({0, 63, 0}, S(blocks::Bedrock));
    end.put({0, 64, 0}, BlockUpdates::fireState(0));
    end.tick(6000);
    CHECK(end.block({0, 64, 0}) == blocks::Fire);
}
