// Buckets (wiki: Bucket, Fuel).
#include "gameplay/Buckets.h"
#include "gameplay/Furnace.h"
#include "gameplay/Recipes.h"
#include "world/Blocks.h"
#include "world/Raycast.h"

#include <doctest/doctest.h>

#include <array>

using namespace mc;
using namespace mc::world;

namespace {

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }
ItemId I(const char* id) { return *itemRegistry().find(id); }

struct Scene {
    World world;
    std::vector<BlockPos> changed;
    Scene() {
        Chunk& c = world.createChunk({0, 0});
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x)
                c.set(x, 63, z, S(blocks::Stone));
    }
};

const glm::dvec3 kEye{2.5, 66.0, 2.5};
const glm::dvec3 kDown{0.0, -1.0, 0.0};

} // namespace

TEST_CASE("an empty bucket picks up a water source; a water bucket places one") {
    Scene s;
    s.world.setBlock({2, 64, 2}, S(blocks::Water));
    auto r = useBucket(s.world, I("bucket"), kEye, kDown, 4.5, s.changed);
    REQUIRE(r);
    CHECK(r->filled == I("water_bucket"));
    CHECK(s.world.getBlock({2, 64, 2}) == 0);
    r = useBucket(s.world, I("water_bucket"), kEye, kDown, 4.5, s.changed);
    REQUIRE(r);
    CHECK(r->filled == I("bucket"));
    CHECK(s.world.getBlock({2, 64, 2}) == S(blocks::Water)); // on top of the stone it hit
}

TEST_CASE("flowing water can't be picked up; water evaporates in the Nether") {
    Scene s;
    s.world.setBlock({2, 64, 2}, blockRegistry().set(S(blocks::Water), properties::level, 3));
    CHECK_FALSE(useBucket(s.world, I("bucket"), kEye, kDown, 4.5, s.changed));
    s.world.setBlock({2, 64, 2}, 0);
    s.world.setUltrawarm(true);
    const auto r = useBucket(s.world, I("water_bucket"), kEye, kDown, 4.5, s.changed);
    REQUIRE(r);
    CHECK(r->filled == I("bucket")); // emptied...
    CHECK(s.world.getBlock({2, 64, 2}) == 0); // ...but nothing placed
}

TEST_CASE("lava: targeted only by buckets; a lava bucket burns 20000 ticks and leaves the bucket") {
    Scene s;
    s.world.setBlock({2, 64, 2}, S(blocks::Lava));
    const auto normal = raycastBlocks(s.world, kEye, kDown, 4.5);
    REQUIRE(normal);
    CHECK(normal->block.y == 63); // through the lava to the stone
    const auto bucket = raycastBlocks(s.world, kEye, kDown, 4.5, RayFluids::Sources);
    REQUIRE(bucket);
    CHECK(bucket->block.y == 64);
    Furnace f;
    f.input = {I("raw_iron"), 1};
    f.fuel = {I("lava_bucket"), 1};
    tickFurnace(f);
    CHECK(f.burnLeft == 20000); // (lit this tick)
    CHECK(f.fuel.item == I("bucket"));
}

TEST_CASE("crafting: a bucket from three iron ingots") {
    const ItemStack i{I("iron_ingot"), 1}, e{};
    std::array<ItemStack, 9> g = {i, e, i, e, i, e, e, e, e};
    const auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(r->item == I("bucket"));
}
