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

TEST_CASE("a water bucket emptied onto a torch washes it away with its drop; lava burns it") {
    Scene s;
    s.world.setBlock({2, 64, 2}, S(blocks::Torch));
    auto r = useBucket(s.world, I("water_bucket"), kEye, kDown, 4.5, s.changed);
    REQUIRE(r);
    CHECK(blockRegistry().blockOf(s.world.getBlock({2, 64, 2})) == blocks::Water);
    CHECK(r->washed.item == I("torch"));
    Scene t;
    t.world.setBlock({2, 64, 2}, S(blocks::Torch));
    r = useBucket(t.world, I("lava_bucket"), kEye, kDown, 4.5, t.changed);
    REQUIRE(r);
    CHECK(r->washed.empty());
}

TEST_CASE("bucket results: survival swaps or splits the stack, creative adds one filled bucket") {
    Inventory inv;
    for (int i = 0; i < Inventory::kSlots; ++i)
        inv.setSlot(i, {});
    inv.select(0);
    inv.setSlot(0, {I("bucket"), 1});
    CHECK(applyBucket(inv, I("water_bucket"), true).empty());
    CHECK(inv.slot(0).item == I("water_bucket"));

    inv.setSlot(0, {I("bucket"), 3}); // a stack: one is filled, it goes elsewhere
    CHECK(applyBucket(inv, I("water_bucket"), true).empty());
    CHECK(inv.slot(0).count == 2);
    int filled = 0;
    for (int i = 1; i < Inventory::kSlots; ++i)
        filled += inv.slot(i).item == I("water_bucket");
    CHECK(filled == 1);

    for (int i = 1; i < Inventory::kSlots; ++i) // full inventory: the filled bucket drops
        inv.setSlot(i, {I("stone"), 64});
    const ItemStack extra = applyBucket(inv, I("lava_bucket"), true);
    CHECK(extra.item == I("lava_bucket"));
    CHECK(inv.slot(0).count == 1);

    Inventory creative;
    for (int i = 0; i < Inventory::kSlots; ++i)
        creative.setSlot(i, {});
    creative.select(0);
    creative.setSlot(0, {I("bucket"), 1});
    applyBucket(creative, I("milk_bucket"), false);
    applyBucket(creative, I("milk_bucket"), false); // already carried: not added again
    int milk = 0;
    for (int i = 0; i < Inventory::kSlots; ++i)
        milk += creative.slot(i).item == I("milk_bucket");
    CHECK(milk == 1);
    CHECK(creative.slot(0).item == I("bucket"));
}
