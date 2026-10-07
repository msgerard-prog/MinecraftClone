// Beds (wiki: Bed).
#include "gameplay/Beds.h"
#include "gameplay/Mobs.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

struct Scene {
    World world;
    BlockUpdates updates{world};
    Scene() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, S(blocks::Stone));
            }
    }
    // A bed placed the way a player does, looking south (+Z).
    BlockPos bed(BlockPos foot) {
        const auto s = BlockUpdates::placement(world, S(blocks::RedBed), foot, Direction::Up, 0.0f, 0.0f);
        REQUIRE(s);
        world.updateBlock(foot, *s);
        return {foot.x, foot.y, foot.z + 1};
    }
};

} // namespace

TEST_CASE("placing a bed's foot brings its head; breaking either half removes both") {
    Scene s;
    const BlockPos head = s.bed({0, 64, 0});
    CHECK(R().blockOf(s.world.getBlock(head)) == blocks::RedBed);
    CHECK(R().value(s.world.getBlock(head), "part") == "head");
    CHECK(bedHead(s.world, {0, 64, 0}) == head);
    s.world.updateBlock(head, 0);
    CHECK(s.world.getBlock({0, 64, 0}) == 0);
    // No room for the head: can't be placed.
    s.world.setBlock({5, 64, 6}, S(blocks::Stone));
    CHECK_FALSE(BlockUpdates::placement(s.world, S(blocks::RedBed), {5, 64, 5}, Direction::Up, 0.0f, 0.0f));
}

TEST_CASE("sleeping: only at night, not with monsters near; beds explode outside the Overworld") {
    Scene s;
    const BlockPos head = s.bed({0, 64, 0});
    CHECK(useBed(s.world, head, 6000, Dimension::Overworld) == BedUse::NotNight);
    CHECK(useBed(s.world, {0, 64, 0}, 18000, Dimension::Overworld) == BedUse::Sleep);
    CHECK(useBed(s.world, head, 18000, Dimension::Nether) == BedUse::Explodes);
    Xoroshiro rng(1);
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Zombie, {6.5, 64.0, 3.5}, rng)));
    CHECK(useBed(s.world, head, 18000, Dimension::Overworld) == BedUse::Monsters);
    CHECK(canSleepAt(12542));
    CHECK_FALSE(canSleepAt(12541));
    CHECK(morningAfter(18000) == 24000);
    CHECK(morningAfter(24000 * 3 + 13000) == 24000 * 4);
}

TEST_CASE("respawning: a spot next to the bed; none when it is walled in") {
    Scene s;
    const BlockPos head = s.bed({0, 64, 0});
    const auto spot = bedStandSpot(s.world, head);
    REQUIRE(spot);
    CHECK(spot->y == doctest::Approx(64.0));
    for (int x = -2; x <= 2; ++x)
        for (int z = -2; z <= 3; ++z)
            for (int y = 64; y <= 66; ++y)
                if (R().blockOf(s.world.getBlock({x, y, z})) != blocks::RedBed) s.world.setBlock({x, y, z}, S(blocks::Stone));
    CHECK_FALSE(bedStandSpot(s.world, head));
}
