// Falling blocks (wiki: Falling Block, Sand, Gravel).
#include "gameplay/FallingBlocks.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

// 3x3 chunks, stone floor at y 63; block updates, falling blocks and items ticked
// like main does.
struct Scene {
    World world;
    BlockUpdates updates{world};
    FallingBlocks falling;
    ItemEntities items;
    Xoroshiro rng{3};
    std::vector<BlockPos> changed;
    int64_t time = 0;
    Scene() {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, S(blocks::Stone));
            }
    }
    void put(BlockPos p, BlockStateId s) {
        updates.setTime(time);
        world.updateBlock(p, s);
    }
    void tick(int n) {
        Inventory inv;
        const Aabb far = Aabb::fromFeet({1000, 64, 1000}, 0.6, 1.8);
        for (int i = 0; i < n; ++i) {
            ++time;
            updates.setTime(time);
            updates.tick();
            for (const auto& f : updates.fallingStarts())
                falling.spawn(f.pos, f.state);
            updates.fallingStarts().clear();
            falling.tick(world, items, rng, changed);
            items.tick(world, far, false, inv);
        }
    }
    BlockId block(BlockPos p) const { return R().blockOf(world.getBlock(p)); }
};

} // namespace

TEST_CASE("sand over air starts falling 2 ticks later and lands on the ground") {
    Scene s;
    s.put({0, 70, 0}, S(blocks::Sand));
    s.tick(1);
    CHECK(s.block({0, 70, 0}) == blocks::Sand);
    s.tick(1);
    CHECK(s.block({0, 70, 0}) == 0);
    REQUIRE(s.falling.blocks().size() == 1);
    s.tick(40);
    CHECK(s.falling.blocks().empty());
    CHECK(s.block({0, 64, 0}) == blocks::Sand);
}

TEST_CASE("a supported block stays; removing the support makes the whole column fall") {
    Scene s;
    for (int y = 64; y <= 67; ++y)
        s.put({2, y, 2}, S(blocks::Gravel));
    s.put({2, 63, 2}, S(blocks::Stone));
    s.tick(10);
    CHECK(s.falling.blocks().empty());
    s.put({2, 63, 2}, 0); // dig out the floor under the column
    s.put({2, 62, 2}, S(blocks::Stone));
    s.tick(80);
    for (int y = 63; y <= 66; ++y)
        CHECK(s.block({2, y, 2}) == blocks::Gravel);
    CHECK(s.block({2, 67, 2}) == 0);
}

TEST_CASE("landing on a torch drops the block as an item; in water it sinks and settles") {
    Scene s;
    s.put({0, 64, 0}, S(blocks::Torch));
    s.put({0, 70, 0}, S(blocks::Sand));
    s.tick(60);
    CHECK(s.block({0, 64, 0}) == blocks::Torch);
    REQUIRE(s.items.items().size() == 1);
    CHECK(s.items.items()[0].stack.item == itemRegistry().blockItem(blocks::Sand));
    s.world.setBlock({5, 64, 5}, S(blocks::Water));
    s.world.setBlock({5, 65, 5}, S(blocks::Water));
    s.put({5, 70, 5}, S(blocks::Gravel));
    s.tick(60);
    CHECK(s.block({5, 64, 5}) == blocks::Gravel);
}

TEST_CASE("replaceable blocks: grass and a single snow layer, not flowers") {
    CHECK(BlockUpdates::replaceable(0));
    CHECK(BlockUpdates::replaceable(S(blocks::ShortGrass)));
    CHECK(BlockUpdates::replaceable(S(blocks::Snow)));
    CHECK_FALSE(BlockUpdates::replaceable(R().set(S(blocks::Snow), properties::layers, 3)));
    CHECK_FALSE(BlockUpdates::replaceable(S(blocks::Poppy)));
    CHECK_FALSE(BlockUpdates::replaceable(S(blocks::Torch)));
}
