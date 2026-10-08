// Pointed dripstone (M27.2b): thickness, support, falling stalactites, dripping into
// cauldrons, mud drying, growth; landing on a stalagmite.
#include "gameplay/FallingBlocks.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Vitals.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }
BlockStateId hanging() { return R().set(S(blocks::PointedDripstone), properties::verticalDirection, 1); }

struct Cave {
    World world;
    BlockUpdates updates{world};
    int64_t now = 0;
    Cave() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 59, z, S(blocks::Stone));
                        c.set(x, 70, z, S(blocks::DripstoneBlock));
                    }
            }
    }
    int thick(int x, int y, int z) { return R().get(world.getBlock({x, y, z}), properties::thickness); }
    BlockId at(int x, int y, int z) { return R().blockOf(world.getBlock({x, y, z})); }
    void run(int ticks) {
        updates.setRandomTicks({0, 0}, 1, 1000);
        for (int t = 0; t < ticks; ++t) {
            updates.setTime(++now);
            updates.tick();
        }
    }
};

} // namespace

TEST_CASE("a stalactite's pieces take their thickness from the column: base, middle, frustum, tip (M27.2b)") {
    Cave c;
    REQUIRE(BlockUpdates::placement(c.world, S(blocks::PointedDripstone), {2, 69, 2}, Direction::Down, 0, 0, 0.0) ==
            hanging());
    for (int y = 69; y >= 66; --y) c.world.updateBlock({2, y, 2}, hanging());
    CHECK(c.thick(2, 69, 2) == 4); // base
    CHECK(c.thick(2, 68, 2) == 3); // middle
    CHECK(c.thick(2, 67, 2) == 2); // frustum
    CHECK(c.thick(2, 66, 2) == 1); // tip
    // A stalagmite rising to meet it: both tips merge.
    for (int y = 60; y <= 65; ++y) c.world.updateBlock({2, y, 2}, S(blocks::PointedDripstone));
    CHECK(c.thick(2, 65, 2) == 0);
    CHECK(c.thick(2, 66, 2) == 0);
}

TEST_CASE("cut loose, a stalactite falls and skewers what's below; a stalagmite breaks (M27.2b)") {
    Cave c;
    for (int y = 69; y >= 67; --y) c.world.updateBlock({4, y, 4}, hanging());
    c.world.updateBlock({4, 60, 4}, S(blocks::PointedDripstone));
    c.updates.fallingStarts().clear();
    c.world.updateBlock({4, 70, 4}, 0); // its ceiling mined
    CHECK(c.updates.fallingStarts().size() == 3);
    CHECK(c.at(4, 67, 4) == 0);
    FallingBlocks falling;
    for (const auto& f : c.updates.fallingStarts()) falling.spawn(f.pos, f.state);
    ItemEntities items;
    Xoroshiro rng(3);
    std::vector<BlockPos> changed;
    for (int t = 0; t < 60 && falling.impacts().empty(); ++t) falling.tick(c.world, items, rng, changed);
    REQUIRE_FALSE(falling.impacts().empty());
    CHECK(falling.impacts()[0].damage >= 6.0f);
    // The stalagmite loses its floor: it breaks.
    c.world.updateBlock({4, 59, 4}, 0);
    CHECK(c.at(4, 60, 4) == 0);
}

TEST_CASE("under water a stalactite drips into a cauldron and dries mud; it grows slowly (M27.2b)") {
    Cave c;
    c.world.setBlock({6, 71, 6}, S(blocks::Water)); // water over the dripstone block
    c.world.updateBlock({6, 69, 6}, hanging());
    c.world.updateBlock({6, 60, 6}, S(blocks::Cauldron));
    c.run(4000);
    CHECK(c.at(6, 60, 6) == blocks::WaterCauldron);
    // Over mud: the mud turns to clay.
    c.world.setBlock({8, 70, 8}, S(blocks::Mud));
    c.world.setBlock({8, 71, 8}, S(blocks::Water));
    c.world.updateBlock({8, 69, 8}, hanging());
    c.run(4000);
    CHECK(c.at(8, 70, 8) == blocks::Clay);
    // Growth over a long time: longer, or a stalagmite under it.
    c.run(30000);
    CHECK((c.at(6, 68, 6) == blocks::PointedDripstone || c.at(6, 61, 6) == blocks::PointedDripstone ||
           c.at(6, 60, 6) != blocks::WaterCauldron));
}

TEST_CASE("landing on a stalagmite's point hurts double (M27.2b)") {
    Vitals v;
    v.setStalagmite(true);
    for (double y = 70.0; y > 64.0; y -= 0.5) v.tick(y, false, false, false);
    v.tick(64.0, true, false, false);
    CHECK(v.health() == doctest::Approx(Vitals::kMaxHealth - 10.0f)); // ceil(6 x 2 - 2)
}
