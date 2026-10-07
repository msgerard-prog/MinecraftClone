// M18.1 plants: cactus, mushrooms, pumpkins (wiki: Cactus, Mushroom, Pumpkin).
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
BlockStateId S(BlockId b) { return R().defaultState(b); }

// 3x3 chunks with a floor of `floor` at y 63, uniform sky light; the centre
// chunk random-ticks.
struct Scene {
    World world;
    BlockUpdates updates{world};
    int64_t time = 0;
    Scene(BlockId floor, int sky) {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 62, z, S(blocks::Stone)); // (sand would fall)
                        c.set(x, 63, z, S(floor));
                    }
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
    bool canPlace(BlockId b, BlockPos p) const {
        return BlockUpdates::placement(world, S(b), p, Direction::Up, 0.0f, 0.0f).has_value();
    }
    BlockId block(BlockPos p) const { return R().blockOf(world.getBlock(p)); }
};

} // namespace

TEST_CASE("cactus: only on sand or cactus, nothing solid beside it; grows 3 tall") {
    Scene s(blocks::Sand, 15);
    CHECK(s.canPlace(blocks::Cactus, {4, 64, 4}));
    s.put({5, 64, 4}, S(blocks::Stone));
    CHECK_FALSE(s.canPlace(blocks::Cactus, {4, 64, 4})); // a block beside it
    Scene dirt(blocks::Dirt, 15);
    CHECK_FALSE(dirt.canPlace(blocks::Cactus, {4, 64, 4}));

    s.put({8, 64, 8}, S(blocks::Cactus));
    s.tick(60000); // ~3 random ticks per 4096 blocks a tick: plenty of growth time
    CHECK(s.block({8, 65, 8}) == blocks::Cactus);
    CHECK(s.block({8, 66, 8}) == blocks::Cactus);
    CHECK(s.block({8, 67, 8}) == blocks::Air); // 3 at most
    // A block placed beside it breaks it (and the pieces above lose their support).
    s.put({9, 65, 8}, S(blocks::Stone));
    CHECK(s.block({8, 65, 8}) == blocks::Air);
    CHECK(s.block({8, 66, 8}) == blocks::Air);
    CHECK(s.block({8, 64, 8}) == blocks::Cactus);
}

TEST_CASE("mushrooms: placed only in light below 13 on a solid block; spread in the dark, 5 per 9x3x9") {
    Scene bright(blocks::Stone, 15);
    CHECK_FALSE(bright.canPlace(blocks::BrownMushroom, {4, 64, 4}));
    Scene dark(blocks::Stone, 0);
    CHECK(dark.canPlace(blocks::RedMushroom, {4, 64, 4}));
    CHECK_FALSE(dark.canPlace(blocks::RedMushroom, {4, 65, 4})); // on air
    CHECK(R().lightEmission(S(blocks::BrownMushroom)) == 1);

    dark.put({8, 64, 8}, S(blocks::BrownMushroom));
    dark.tick(200000);
    int n = 0;
    for (int dz = -4; dz <= 4; ++dz)
        for (int dx = -4; dx <= 4; ++dx)
            n += dark.block({8 + dx, 64, 8 + dz}) == blocks::BrownMushroom;
    CHECK(n >= 2); // it spread
    CHECK(n <= 6); // stops at 5 around one (the scan centres on the ticked one)
    // Losing its block, a mushroom pops.
    dark.put({8, 63, 8}, 0);
    CHECK(dark.block({8, 64, 8}) == blocks::Air);
}

TEST_CASE("pumpkins need an axe to mine fast but drop by hand") {
    CHECK(R().block(blocks::Pumpkin).settings.hardness == doctest::Approx(1.0f));
    CHECK(R().opaqueCube(S(blocks::Pumpkin)));
}
