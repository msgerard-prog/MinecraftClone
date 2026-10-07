// Item entities (wiki: Item (entity)).
#include "gameplay/ItemEntities.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

World floorWorld() {
    World w;
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) {
            auto& c = w.createChunk({cx, cz});
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
        }
    return w;
}

ItemStack dirt(int n) { return Inventory::blockStack(blockRegistry().defaultState(blocks::Dirt), n); }

} // namespace

TEST_CASE("dropped items fall onto the ground and come to rest") {
    World w = floorWorld();
    ItemEntities items;
    Xoroshiro rng(1);
    Inventory inv;
    items.spawn({4.5, 70.0, 4.5}, dirt(3), rng);
    const Aabb far = Aabb::fromFeet({100, 64, 100}, 0.6, 1.8);
    for (int i = 0; i < 100; ++i)
        items.tick(w, far, true, inv);
    REQUIRE(items.items().size() == 1);
    CHECK(items.items()[0].pos.y == doctest::Approx(64.0));
    CHECK(items.items()[0].onGround);
}

TEST_CASE("pickup: only after the delay, only near the player, stacks into the inventory") {
    World w = floorWorld();
    ItemEntities items;
    Xoroshiro rng(2);
    Inventory inv;
    for (int i = 0; i < Inventory::kSlots; ++i)
        inv.setSlot(i, {});
    items.spawn({4.5, 64.0, 4.5}, dirt(5), rng);
    const Aabb near = Aabb::fromFeet({4.5, 64, 5.2}, 0.6, 1.8);
    for (int i = 0; i < 5; ++i)
        CHECK(items.tick(w, near, true, inv) == 0); // pickup delay (10 ticks)
    int picked = 0;
    for (int i = 0; i < 10; ++i)
        picked += items.tick(w, near, true, inv);
    CHECK(picked == 1);
    CHECK(items.items().empty());
    CHECK(inv.slot(0).count == 5);
}

TEST_CASE("items despawn after 6000 ticks; a full inventory leaves them on the ground") {
    World w = floorWorld();
    ItemEntities items;
    Xoroshiro rng(3);
    Inventory full;
    for (int i = 0; i < Inventory::kSlots; ++i)
        full.setSlot(i, {*itemRegistry().find("diamond_pickaxe"), 1});
    items.spawn({4.5, 64.0, 4.5}, dirt(1), rng);
    const Aabb near = Aabb::fromFeet({4.5, 64, 4.5}, 0.6, 1.8);
    for (int i = 0; i < ItemEntities::kDespawnTicks - 1; ++i)
        items.tick(w, near, true, full);
    CHECK(items.items().size() == 1);
    items.tick(w, near, true, full);
    CHECK(items.items().empty());
}

TEST_CASE("items burn up in fire, as in lava") {
    World w = floorWorld();
    w.setBlock({4, 64, 4}, BlockUpdates::fireState(0));
    ItemEntities items;
    Xoroshiro rng(1);
    Inventory inv;
    items.spawn({4.5, 64.2, 4.5}, dirt(1), rng);
    const Aabb far = Aabb::fromFeet({100, 64, 100}, 0.6, 1.8);
    items.tick(w, far, true, inv);
    CHECK(items.items().empty());
}
