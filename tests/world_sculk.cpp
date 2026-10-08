// Sculk (M27.3a): vibrations wake sensors (redstone power by distance), a player's
// vibrations set off shriekers; shriekers warn toward a warden; catalysts bloom sculk.
#include "gameplay/Mining.h"
#include "gameplay/Vitals.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Items.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

struct Dark {
    World world;
    BlockUpdates updates{world};
    int64_t now = 0;
    Dark() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 59, z, S(blocks::Deepslate));
            }
    }
    void run(int ticks) {
        for (int t = 0; t < ticks; ++t) {
            updates.setTime(++now);
            updates.tick();
        }
    }
    int phase(const BlockPos& p) { return R().get(world.getBlock(p), properties::sculkPhase); }
};

} // namespace

TEST_CASE("a vibration wakes a sculk sensor within 8 blocks: power by distance, then a rest (M27.3)") {
    Dark d;
    const BlockPos sensor{0, 60, 0};
    d.world.updateBlock(sensor, S(blocks::SculkSensor));
    d.updates.vibrate({0.5, 60.5, 12.5}, true); // 12 blocks: too far
    CHECK(d.phase(sensor) == 0);
    d.updates.vibrate({0.5, 60.5, 2.5}, true);
    CHECK(d.phase(sensor) == 1);
    const int near = R().get(d.world.getBlock(sensor), properties::power);
    CHECK(near >= 11);
    d.run(31);
    CHECK(d.phase(sensor) == 2); // resting: deaf
    d.updates.vibrate({0.5, 60.5, 1.5}, true);
    CHECK(d.phase(sensor) == 2);
    d.run(11);
    CHECK(d.phase(sensor) == 0);
    // Farther away: weaker.
    d.updates.vibrate({0.5, 60.5, 7.5}, true);
    CHECK(R().get(d.world.getBlock(sensor), properties::power) < near);
}

TEST_CASE("a player's vibration heard by a sensor sets off a shrieker near it; others don't (M27.3)") {
    Dark d;
    d.world.updateBlock({0, 60, 0}, S(blocks::SculkSensor));
    d.world.updateBlock({3, 60, 0}, R().set(S(blocks::SculkShrieker), properties::canSummon, 0));
    d.updates.vibrate({0.5, 60.5, 2.5}, false); // (an explosion, say)
    CHECK(d.updates.shrieks().empty());
    d.run(45);
    d.updates.vibrate({0.5, 60.5, 2.5}, true);
    REQUIRE(d.updates.shrieks().size() == 1);
    CHECK(d.updates.shrieks()[0].canSummon);
    CHECK(R().get(d.world.getBlock({3, 60, 0}), properties::shrieking) == 0);
    d.run(91);
    CHECK(R().get(d.world.getBlock({3, 60, 0}), properties::shrieking) == 1);
}

TEST_CASE("four warnings call the warden; one at most every 10 s; they wear off after 10 minutes (M27.3)") {
    Vitals v;
    for (int i = 0; i < 3; ++i) {
        CHECK_FALSE(v.wardenWarn());
        CHECK_FALSE(v.wardenWarn()); // (within 10 s: no change)
        for (int t = 0; t < 200; ++t) v.tickWardenTracker();
    }
    CHECK(v.wardenLevel() == 3);
    CHECK(v.wardenWarn()); // the 4th
    CHECK(v.wardenLevel() == 4);
    for (int t = 0; t < 12000; ++t) v.tickWardenTracker();
    CHECK(v.wardenLevel() == 3);
}

TEST_CASE("a catalyst takes a nearby death's experience and blooms sculk; sculk needs Silk Touch (M27.3)") {
    Dark d;
    d.world.updateBlock({0, 60, 0}, S(blocks::SculkCatalyst));
    Xoroshiro rng(7);
    CHECK_FALSE(BlockUpdates::sculkBloom(d.world, {20.5, 60.0, 20.5}, 5, rng)); // (too far)
    REQUIRE(BlockUpdates::sculkBloom(d.world, {3.5, 60.0, 3.5}, 5, rng));
    int sculk = 0;
    for (int z = -1; z <= 8; ++z)
        for (int x = -1; x <= 8; ++x) sculk += R().blockOf(d.world.getBlock({x, 59, z})) == blocks::Sculk;
    CHECK(sculk > 0);
    CHECK(sculk <= 5);
    std::vector<ItemStack> out;
    blockDrops(S(blocks::Sculk), {}, rng, out);
    CHECK(out.empty());
    CHECK(blockExperience(S(blocks::Sculk), rng) == 1);
    blockDrops(S(blocks::SculkSensor), {}, rng, out);
    CHECK(out.size() == 1);
}
