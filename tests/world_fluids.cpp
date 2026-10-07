// Water and lava flow (wiki: Fluid, Water, Lava).
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

// A stone floor (top at y 63) over 3x3 chunks around the origin.
struct Scene {
    World world;
    BlockUpdates updates{world};
    int64_t time = 0;
    explicit Scene(bool nether = false) {
        world.setUltrawarm(nether);
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
        for (int i = 0; i < n; ++i) {
            ++time;
            updates.setTime(time);
            updates.tick();
        }
    }
    BlockId block(BlockPos p) const { return R().blockOf(world.getBlock(p)); }
    int level(BlockPos p) const { return R().get(world.getBlock(p), properties::level); }
};

} // namespace

TEST_CASE("water spreads 7 blocks on flat ground, one level per block, 1 block per 5 ticks") {
    Scene s;
    s.put({0, 64, 0}, S(blocks::Water));
    s.tick(5);
    CHECK(s.block({1, 64, 0}) == blocks::Water);
    CHECK(s.level({1, 64, 0}) == 1);
    CHECK(s.block({2, 64, 0}) == 0); // not yet
    s.tick(100);
    for (int x = 1; x <= 7; ++x)
        CHECK(s.level({x, 64, 0}) == x);
    CHECK(s.block({8, 64, 0}) == 0);
    CHECK(s.block({-7, 64, 0}) == blocks::Water);
    CHECK(s.block({4, 64, 4}) == 0); // diamond shape: |x| + |z| <= 7 within reach
    CHECK(s.block({3, 64, 4}) == blocks::Water);
}

TEST_CASE("water flows toward a drop within 4 blocks, then falls") {
    Scene s;
    s.put({3, 63, 0}, 0); // a hole 3 blocks east
    s.put({0, 64, 0}, S(blocks::Water));
    s.tick(5);
    CHECK(s.block({1, 64, 0}) == blocks::Water);
    CHECK(s.block({-1, 64, 0}) == 0); // only toward the drop
    CHECK(s.block({0, 64, 1}) == 0);
    s.tick(20);
    CHECK(s.block({3, 63, 0}) == blocks::Water);
    CHECK(s.level({3, 63, 0}) >= 8); // falling
}

TEST_CASE("two sources make a third; flowing water dries up without its source") {
    Scene s;
    s.put({0, 64, 0}, S(blocks::Water));
    s.put({2, 64, 0}, S(blocks::Water));
    s.tick(10);
    CHECK(s.level({1, 64, 0}) == 0); // a new source (floor below)
    s.put({0, 64, 0}, 0);
    s.put({1, 64, 0}, 0);
    s.put({2, 64, 0}, 0);
    s.tick(100);
    for (int x = -8; x <= 8; ++x)
        CHECK(s.block({x, 64, 0}) == 0);
}

TEST_CASE("lava: reach 3 and 30 ticks per block in the Overworld; reach 7 and 10 ticks in the Nether") {
    Scene s;
    s.put({0, 64, 0}, S(blocks::Lava));
    s.tick(29);
    CHECK(s.block({1, 64, 0}) == 0);
    s.tick(1);
    CHECK(s.block({1, 64, 0}) == blocks::Lava);
    s.tick(300);
    CHECK(s.block({3, 64, 0}) == blocks::Lava);
    CHECK(s.block({4, 64, 0}) == 0);
    Scene n(true);
    n.put({0, 64, 0}, S(blocks::Lava));
    n.tick(10);
    CHECK(n.block({1, 64, 0}) == blocks::Lava);
    n.tick(200);
    CHECK(n.block({7, 64, 0}) == blocks::Lava);
    CHECK(n.block({8, 64, 0}) == 0);
}

TEST_CASE("lava and water: source -> obsidian, flowing -> cobblestone, lava onto water -> stone") {
    Scene s;
    s.put({0, 64, 0}, S(blocks::Lava));
    s.put({1, 64, 0}, S(blocks::Water));
    CHECK(s.block({0, 64, 0}) == blocks::Obsidian);
    s.put({5, 64, 5}, BlockUpdates::fluidState(blocks::Lava, 6, false)); // flowing lava
    s.put({6, 64, 5}, S(blocks::Water));
    CHECK(s.block({5, 64, 5}) == blocks::Cobblestone);
    Scene t;
    t.put({0, 64, 0}, S(blocks::Water));
    t.put({0, 66, 0}, S(blocks::Lava));
    t.put({0, 65, 0}, 0); // (air between: the lava falls)
    t.tick(70); // falls at tick 30 (into y 65), reaches the water at tick 60
    CHECK(t.block({0, 64, 0}) == blocks::Stone);
}

TEST_CASE("flowing water washes away torches and plants, dropping them") {
    Scene s;
    s.put({1, 64, 0}, S(blocks::Torch));
    s.put({0, 64, 0}, S(blocks::Water));
    s.tick(5);
    CHECK(s.block({1, 64, 0}) == blocks::Water);
    REQUIRE(s.updates.drops().size() == 1);
    CHECK(R().blockOf(s.updates.drops()[0].loot) == blocks::Torch); // its loot: the torch
}

#include "world/ChunkSerializer.h"

TEST_CASE("pending fluid ticks save in fluid_ticks with vanilla's fluid ids and load back") {
    Scene s;
    s.put({0, 64, 0}, S(blocks::Water)); // a source with its tick pending
    Chunk& c = *s.world.chunk({0, 0});
    const auto nbt = chunkToNbt(ChunkSnapshot::of(c, s.time));
    const auto* fluid = nbt.list("fluid_ticks");
    REQUIRE(fluid);
    REQUIRE(fluid->items.size() == 1);
    CHECK(*fluid->items[0].get<mc::nbt::Compound>()->string("i") == "minecraft:water");
    CHECK(nbt.list("block_ticks")->items.empty());
    Chunk d({0, 0});
    REQUIRE(chunkFromNbt(nbt, d));
    REQUIRE(d.blockTicks().size() == 1);
    CHECK(d.blockTicks()[0].block == blocks::Water);
}

TEST_CASE("flowing lava that reaches still water hardens at once (review fix)") {
    Scene s;
    s.put({2, 64, 0}, S(blocks::Water));
    s.put({1, 63, 0}, S(blocks::Stone)); // (floor is stone already; water just sits)
    s.put({-1, 64, 0}, S(blocks::Lava)); // flows east next to the water at x = 1
    s.tick(5);
    s.put({3, 64, 0}, S(blocks::Stone)); // keep the water from flowing east... it still flows west
    s.tick(400);
    // Whatever flowing lava ended up beside water is cobblestone, never liquid.
    for (int x = -4; x <= 4; ++x)
        for (int z = -4; z <= 4; ++z) {
            const BlockPos p{x, 64, z};
            if (s.block(p) != blocks::Lava || s.level(p) == 0) continue;
            for (const BlockPos n : {BlockPos{x + 1, 64, z}, {x - 1, 64, z}, {x, 64, z + 1}, {x, 64, z - 1}})
                CHECK(s.block(n) != blocks::Water);
        }
}

TEST_CASE("lava placed by flow next to a water source becomes cobblestone") {
    Scene s;
    s.world.setBlock({0, 64, 0}, S(blocks::Water)); // a quiet source (no pending tick)
    s.put({3, 64, 0}, S(blocks::Lava));             // reaches x = 1 at tick 60
    s.tick(61);
    CHECK(s.block({1, 64, 0}) == blocks::Cobblestone);
}

TEST_CASE("lava burns torches without a drop; water drops them") {
    Scene s;
    s.put({1, 64, 0}, S(blocks::Torch));
    s.put({0, 64, 0}, S(blocks::Lava));
    s.tick(30);
    CHECK(s.block({1, 64, 0}) == blocks::Lava);
    CHECK(s.updates.drops().empty());
}

TEST_CASE("no new water source over a non-solid block") {
    Scene s;
    s.world.setBlock({1, 63, 0}, S(blocks::EndPortal)); // not solid, not washed away
    s.put({0, 64, 0}, S(blocks::Water));
    s.put({2, 64, 0}, S(blocks::Water));
    s.tick(10);
    CHECK(s.block({1, 64, 0}) == blocks::Water);
    CHECK(s.level({1, 64, 0}) != 0);
}

TEST_CASE("fluids treat unloaded chunks as walls and wait for them") {
    Scene s;
    // x = 47 is beyond the loaded 3x3 chunks (-16..31): water at the edge (x = 31)
    // doesn't see a hole there and keeps its tick until the chunk exists.
    s.put({31, 64, 0}, S(blocks::Water));
    s.tick(20);
    CHECK(s.block({30, 64, 0}) == 0); // waiting: the chunk east of it isn't loaded
    for (int cz = -1; cz <= 1; ++cz) {
        Chunk& c = s.world.createChunk({2, cz});
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x)
                c.set(x, 63, z, S(blocks::Stone));
    }
    s.tick(10);
    CHECK(s.block({30, 64, 0}) == blocks::Water);
    CHECK(s.block({32, 64, 0}) == blocks::Water);
}

TEST_CASE("a source in midair flows down, then out to its four sides") {
    Scene s;
    s.put({0, 70, 0}, S(blocks::Water));
    s.tick(10);
    CHECK(s.block({0, 69, 0}) == blocks::Water);
    for (const BlockPos n : {BlockPos{1, 70, 0}, {-1, 70, 0}, {0, 70, 1}, {0, 70, -1}}) {
        CHECK(s.block(n) == blocks::Water);
        CHECK(s.level(n) == 1);
    }
}

TEST_CASE("flowing water washes a sapling away (it doesn't stop the flow)") {
    Scene s;
    s.put({1, 64, 0}, S(blocks::OakSapling));
    s.put({0, 64, 0}, S(blocks::Water));
    s.tick(5);
    CHECK(s.block({1, 64, 0}) == blocks::Water);
    REQUIRE(s.updates.drops().size() == 1);
    CHECK(R().blockOf(s.updates.drops()[0].loot) == blocks::OakSapling);
}
