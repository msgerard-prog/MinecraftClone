// M27.1: the blocks of the remaining biomes (two-block plants, mud, moss, pale moss).
#include "gameplay/Mining.h"
#include "gameplay/Recipes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Items.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

struct Garden {
    World world;
    BlockUpdates updates{world};
    Garden() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 63, z, S(blocks::GrassBlock));
            }
    }
    BlockId at(int x, int y, int z) { return R().blockOf(world.getBlock({x, y, z})); }
};

} // namespace

TEST_CASE("a two-block plant stands on soil with room above; its halves go together (M27.1)") {
    Garden g;
    const auto placed = BlockUpdates::placement(g.world, S(blocks::Sunflower), {2, 64, 2}, Direction::Up, 0, 0, 0.0);
    REQUIRE(placed);
    g.world.updateBlock({2, 64, 2}, *placed);
    CHECK(g.at(2, 64, 2) == blocks::Sunflower);
    CHECK(g.at(2, 65, 2) == blocks::Sunflower);
    CHECK(R().get(g.world.getBlock({2, 65, 2}), properties::doorHalf) == 0); // (upper)
    // Breaking the upper half breaks the lower, which drops the flower.
    g.updates.drops().clear();
    g.world.updateBlock({2, 65, 2}, 0);
    CHECK(g.at(2, 64, 2) == 0);
    REQUIRE(g.updates.drops().size() == 1);
    // No room above, or no soil: it can't go there.
    g.world.updateBlock({5, 65, 5}, S(blocks::Stone));
    CHECK_FALSE(BlockUpdates::placement(g.world, S(blocks::Peony), {5, 64, 5}, Direction::Up, 0, 0, 0.0));
    g.world.updateBlock({7, 63, 7}, S(blocks::Stone));
    CHECK_FALSE(BlockUpdates::placement(g.world, S(blocks::Lilac), {7, 64, 7}, Direction::Up, 0, 0, 0.0));
    // Mud and moss are soil too (vanilla #dirt).
    g.world.updateBlock({9, 63, 9}, S(blocks::Mud));
    CHECK(BlockUpdates::placement(g.world, S(blocks::RoseBush), {9, 64, 9}, Direction::Up, 0, 0, 0.0));
}

TEST_CASE("bone meal grows short grass into tall grass and copies tall flowers (M27.1)") {
    Garden g;
    g.world.updateBlock({1, 64, 1}, S(blocks::ShortGrass));
    REQUIRE(g.updates.boneMeal({1, 64, 1}));
    CHECK(g.at(1, 64, 1) == blocks::TallGrass);
    CHECK(g.at(1, 65, 1) == blocks::TallGrass);
    g.world.updateBlock({3, 64, 3}, R().set(S(blocks::Peony), properties::doorHalf, 1));
    g.updates.drops().clear();
    REQUIRE(g.updates.boneMeal({3, 64, 3}));
    REQUIRE(g.updates.drops().size() == 1);
    CHECK(g.updates.drops()[0].stack.item == itemRegistry().blockItem(blocks::Peony));
}

TEST_CASE("tall grass drops two short grass with shears, only the lower half drops (M27.1)") {
    Xoroshiro rng(4);
    std::vector<ItemStack> out;
    const BlockStateId lower = R().set(S(blocks::TallGrass), properties::doorHalf, 1);
    blockDrops(lower, {*itemRegistry().find("shears"), 1}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].item == itemRegistry().blockItem(blocks::ShortGrass));
    CHECK(out[0].count == 2);
    out.clear();
    blockDrops(R().set(S(blocks::Sunflower), properties::doorHalf, 0), {}, rng, out);
    CHECK(out.empty());
    blockDrops(S(blocks::PaleHangingMoss), {}, rng, out);
    CHECK(out.empty()); // (shears or Silk Touch only)
}

TEST_CASE("pale hanging moss hangs from a block; the lowest piece is the tip (M27.1)") {
    Garden g;
    g.world.updateBlock({4, 70, 4}, S(blocks::PaleMossBlock));
    REQUIRE(BlockUpdates::placement(g.world, S(blocks::PaleHangingMoss), {4, 69, 4}, Direction::Down, 0, 0, 0.0));
    g.world.updateBlock({4, 69, 4}, S(blocks::PaleHangingMoss));
    g.world.updateBlock({4, 68, 4}, S(blocks::PaleHangingMoss));
    CHECK(R().get(g.world.getBlock({4, 69, 4}), properties::mossTip) == 1); // (not the tip)
    CHECK(R().get(g.world.getBlock({4, 68, 4}), properties::mossTip) == 0);
    g.world.updateBlock({4, 70, 4}, 0); // the block above goes: the strand falls
    CHECK(g.at(4, 69, 4) == 0);
    CHECK(g.at(4, 68, 4) == 0);
}

TEST_CASE("mud, packed mud, mud bricks, moss carpets and tall-flower dyes are craftable (M27.1)") {
    auto made = [](const char* name, int count) {
        for (const Recipe& r : craftingRecipes())
            if (r.result.item == *itemRegistry().find(name) && r.result.count == count) return true;
        return false;
    };
    CHECK(made("packed_mud", 1));
    CHECK(made("mud_bricks", 4));
    CHECK(made("moss_carpet", 3));
    CHECK(made("pale_moss_carpet", 3));
    CHECK(made("yellow_dye", 2));
    CHECK(made("pink_dye", 2));
}
