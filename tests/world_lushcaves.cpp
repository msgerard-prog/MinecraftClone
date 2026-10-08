// Lush cave blocks (M27.2): cave vines and glow berries, spore blossoms, azaleas and azalea
// trees, rooted dirt, dripleaves, moss patches.
#include "gameplay/Mining.h"
#include "world/BlockShapes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Items.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

struct Cave {
    World world;
    BlockUpdates updates{world};
    Cave() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 59, z, S(blocks::Stone));
                        c.set(x, 70, z, S(blocks::Stone)); // (a ceiling)
                    }
            }
    }
    BlockId at(int x, int y, int z) { return R().blockOf(world.getBlock({x, y, z})); }
    int64_t now = 0;
    void run(int ticks) {
        updates.setRandomTicks({0, 0}, 1, 1000);
        for (int t = 0; t < ticks; ++t) {
            updates.setTime(++now);
            updates.tick();
        }
    }
};

} // namespace

TEST_CASE("cave vines hang from the ceiling, grow down, glow with berries, and give glow berries (M27.2)") {
    Cave c;
    REQUIRE(BlockUpdates::placement(c.world, S(blocks::CaveVines), {2, 69, 2}, Direction::Down, 0, 0, 0.0));
    CHECK_FALSE(BlockUpdates::placement(c.world, S(blocks::CaveVines), {2, 65, 2}, Direction::Down, 0, 0, 0.0));
    c.world.updateBlock({2, 69, 2}, S(blocks::CaveVines));
    c.run(4000);
    CHECK(c.at(2, 69, 2) == blocks::CaveVinesPlant); // (it grew: the top is now vine plant)
    // Berries: bone meal, then picking them.
    BlockPos tip{2, 68, 2};
    while (c.at(tip.x, tip.y - 1, tip.z) == blocks::CaveVines || c.at(tip.x, tip.y - 1, tip.z) == blocks::CaveVinesPlant)
        --tip.y;
    if (R().get(c.world.getBlock(tip), properties::berries) != 0) REQUIRE(c.updates.boneMeal(tip));
    CHECK(R().lightEmission(c.world.getBlock(tip)) == 14);
    Xoroshiro rng(1);
    CHECK(BlockUpdates::pickBerries(c.world, tip, rng) == 1);
    CHECK(R().get(c.world.getBlock(tip), properties::berries) == 1);
    // The ceiling gone: the whole strand falls.
    c.world.updateBlock({2, 70, 2}, 0);
    CHECK(c.at(2, 69, 2) == 0);
}

TEST_CASE("bone meal grows an azalea into an azalea tree on rooted dirt (M27.2)") {
    Cave c;
    c.world.updateBlock({5, 60, 5}, S(blocks::MossBlock)); // (moss is soil)
    for (int y = 70; y < 72; ++y) c.world.updateBlock({5, y, 5}, 0); // (room above)
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) c.world.setBlock({x, 70, z}, 0);
    REQUIRE(BlockUpdates::placement(c.world, S(blocks::Azalea), {5, 61, 5}, Direction::Up, 0, 0, 0.0));
    c.world.updateBlock({5, 61, 5}, S(blocks::Azalea));
    for (int i = 0; i < 20 && c.at(5, 61, 5) == blocks::Azalea; ++i) c.updates.boneMeal({5, 61, 5});
    CHECK(c.at(5, 61, 5) == blocks::OakLog);
    CHECK(c.at(5, 60, 5) == blocks::RootedDirt);
    int leaves = 0;
    for (int y = 62; y < 70; ++y)
        for (int z = 2; z <= 8; ++z)
            for (int x = 2; x <= 8; ++x)
                leaves += c.at(x, y, z) == blocks::AzaleaLeaves || c.at(x, y, z) == blocks::FloweringAzaleaLeaves;
    CHECK(leaves > 10);
}

TEST_CASE("dripleaves: small ones grow into big ones; a big dripleaf tips under you and springs back (M27.2)") {
    Cave c;
    c.world.updateBlock({3, 59, 3}, S(blocks::Clay));
    const auto small = BlockUpdates::placement(c.world, S(blocks::SmallDripleaf), {3, 60, 3}, Direction::Up, 0, 0, 0.0);
    REQUIRE(small);
    c.world.updateBlock({3, 60, 3}, *small);
    CHECK(c.at(3, 61, 3) == blocks::SmallDripleaf); // (its upper half)
    REQUIRE(c.updates.boneMeal({3, 60, 3}));
    CHECK(c.at(3, 60, 3) == blocks::BigDripleafStem);
    BlockPos leaf{3, 61, 3};
    while (c.at(leaf.x, leaf.y, leaf.z) == blocks::BigDripleafStem) ++leaf.y;
    REQUIRE(c.at(leaf.x, leaf.y, leaf.z) == blocks::BigDripleaf);
    c.updates.tiltDripleaf(leaf);
    CHECK(R().get(c.world.getBlock(leaf), properties::tilt) == 1);
    c.run(25);
    CHECK(R().get(c.world.getBlock(leaf), properties::tilt) == 3); // fully tipped
    CHECK(collisionShape(c.world.getBlock(leaf)).count == 0);
    c.run(110);
    CHECK(R().get(c.world.getBlock(leaf), properties::tilt) == 0); // back
}

TEST_CASE("rooted dirt grows hanging roots; moss spreads over stone; lush drops (M27.2)") {
    Cave c;
    c.world.updateBlock({7, 66, 7}, S(blocks::RootedDirt));
    REQUIRE(c.updates.boneMeal({7, 66, 7}));
    CHECK(c.at(7, 65, 7) == blocks::HangingRoots);
    c.world.updateBlock({7, 66, 7}, 0);
    CHECK(c.at(7, 65, 7) == 0); // (nothing holds the roots)
    c.world.updateBlock({9, 59, 9}, S(blocks::MossBlock));
    REQUIRE(c.updates.boneMeal({9, 59, 9}));
    int moss = 0;
    for (int z = 6; z <= 12; ++z)
        for (int x = 6; x <= 12; ++x) moss += c.at(x, 59, z) == blocks::MossBlock;
    CHECK(moss > 3);
    Xoroshiro rng(2);
    std::vector<ItemStack> out;
    blockDrops(R().set(S(blocks::CaveVines), properties::berries, 0), {}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].item == *itemRegistry().find("glow_berries"));
    out.clear();
    blockDrops(S(blocks::BigDripleafStem), {}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].item == itemRegistry().blockItem(blocks::BigDripleaf));
}
