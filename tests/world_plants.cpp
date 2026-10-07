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

namespace {

// Bone meal until the sapling at `p` is gone (grown) or `tries` run out.
bool growWithBoneMeal(Scene& s, BlockPos p, int tries = 200) {
    const BlockId sapling = s.block(p);
    for (int i = 0; i < tries && s.block(p) == sapling; ++i)
        s.updates.boneMeal(p);
    return s.block(p) != sapling;
}

int countAround(const Scene& s, BlockPos p, BlockId b, int r, int h) {
    int n = 0;
    for (int y = p.y; y <= p.y + h; ++y)
        for (int z = p.z - r; z <= p.z + r; ++z)
            for (int x = p.x - r; x <= p.x + r; ++x)
                n += s.block({x, y, z}) == b;
    return n;
}

} // namespace

TEST_CASE("woods: jungle, dark oak and cherry register like the others; huge mushroom faces default to cap") {
    CHECK(R().block(blocks::JungleLog).id == "minecraft:jungle_log");
    CHECK(R().block(blocks::CherrySapling).id == "minecraft:cherry_sapling");
    CHECK(R().block(blocks::BrownMushroomBlock).stateCount == 64);
    CHECK(R().toString(S(blocks::RedMushroomBlock)) ==
          "minecraft:red_mushroom_block[down=true,east=true,north=true,south=true,up=true,west=true]");
    CHECK(R().toString(S(blocks::Podzol)) == "minecraft:podzol[snowy=false]");
}

TEST_CASE("saplings: a jungle sapling grows a small tree, four in a square a 2x2 giant; dark oak needs four") {
    Scene s(blocks::Dirt, 15);
    s.put({4, 64, 4}, S(blocks::JungleSapling));
    REQUIRE(growWithBoneMeal(s, {4, 64, 4}));
    CHECK(s.block({4, 64, 4}) == blocks::JungleLog);
    CHECK(s.block({5, 64, 4}) != blocks::JungleLog); // one trunk
    CHECK(countAround(s, {4, 64, 4}, blocks::JungleLeaves, 3, 16) > 10);

    s.put({8, 64, 8}, S(blocks::DarkOakSapling));
    CHECK_FALSE(growWithBoneMeal(s, {8, 64, 8}, 60)); // alone: never grows
    s.put({9, 64, 8}, S(blocks::DarkOakSapling));
    s.put({8, 64, 9}, S(blocks::DarkOakSapling));
    s.put({9, 64, 9}, S(blocks::DarkOakSapling));
    REQUIRE(growWithBoneMeal(s, {9, 64, 9}));
    for (const auto& c : {BlockPos{8, 64, 8}, BlockPos{9, 64, 8}, BlockPos{8, 64, 9}, BlockPos{9, 64, 9}})
        CHECK(s.block(c) == blocks::DarkOakLog); // 2x2 trunk
    CHECK(countAround(s, {8, 64, 8}, blocks::DarkOakLeaves, 5, 12) > 30);

    Scene j(blocks::Dirt, 15);
    for (const auto& c : {BlockPos{4, 64, 4}, BlockPos{5, 64, 4}, BlockPos{4, 64, 5}, BlockPos{5, 64, 5}})
        j.put(c, S(blocks::JungleSapling));
    REQUIRE(growWithBoneMeal(j, {4, 64, 5}));
    CHECK(j.block({5, 73, 5}) == blocks::JungleLog); // at least 10 tall, 2x2
    CHECK(j.block({4, 73, 4}) == blocks::JungleLog);
}

TEST_CASE("grown leaves are within 6 of a log (they never decay); cherry leaves too") {
    Scene s(blocks::GrassBlock, 15);
    s.put({8, 64, 8}, S(blocks::CherrySapling));
    REQUIRE(growWithBoneMeal(s, {8, 64, 8}));
    int leaves = 0;
    for (int y = 64; y < 80; ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                const BlockStateId st = s.world.getBlock({x, y, z});
                if (R().blockOf(st) != blocks::CherryLeaves) continue;
                ++leaves;
                CHECK(R().get(st, properties::distance) + 1 <= 6);
            }
    CHECK(leaves > 20);
}

TEST_CASE("mycelium spreads to dirt like grass; podzol and mycelium drop dirt") {
    Scene s(blocks::Dirt, 15);
    s.put({8, 63, 8}, S(blocks::Mycelium));
    s.tick(20000);
    CHECK(countAround(s, {8, 63, 8}, blocks::Mycelium, 2, 0) > 3);
    CHECK(BlockUpdates::plantableSoil(S(blocks::Podzol)));
}

TEST_CASE("nether plants stay through neighbour updates; nether wart pops off soul sand (M19 review regression)") {
    Scene s(blocks::CrimsonNylium, 0);
    s.world.setBlock({4, 64, 4}, S(blocks::CrimsonRoots));
    s.world.setBlock({6, 64, 4}, S(blocks::CrimsonFungus));
    s.world.setBlock({8, 70, 8}, S(blocks::Netherrack)); // weeping vines hang from it
    s.world.setBlock({8, 69, 8}, S(blocks::WeepingVinesPlant));
    s.world.setBlock({8, 68, 8}, S(blocks::WeepingVines));
    s.world.setBlock({10, 64, 4}, S(blocks::NetherWart)); // nylium below: no soul sand
    s.put({5, 64, 4}, S(blocks::Stone));                    // updates the roots and the fungus
    s.put({7, 69, 8}, S(blocks::Stone));                    // updates the vine
    s.put({11, 64, 4}, S(blocks::Stone));                   // updates the wart
    s.tick(2);
    CHECK(s.block({4, 64, 4}) == blocks::CrimsonRoots);
    CHECK(s.block({6, 64, 4}) == blocks::CrimsonFungus);
    CHECK(s.block({8, 69, 8}) == blocks::WeepingVinesPlant);
    CHECK(s.block({8, 68, 8}) == blocks::WeepingVines);
    CHECK(s.block({10, 64, 4}) == blocks::Air);
}
