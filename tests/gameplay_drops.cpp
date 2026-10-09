// M30.4: dropped items and experience orbs - merging, saving with their chunk, death drops.
#include "gameplay/ExperienceOrbs.h"
#include "gameplay/ItemEntities.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

World floor() {
    World w;
    for (int cx = -1; cx <= 1; ++cx) {
        Chunk& c = w.createChunk({cx, 0});
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
    }
    return w;
}

ItemStack I(const char* id, int n) { return {*itemRegistry().find(id), uint8_t(n)}; }

} // namespace

TEST_CASE("dropped stacks of the same item merge; different items and full stacks don't") {
    World w = floor();
    ItemEntities items;
    Xoroshiro rng(1);
    Inventory inv;
    const Aabb far = Aabb::fromFeet({100.0, 64.0, 100.0}, 0.6, 1.8);
    items.spawn({4.5, 64.0, 4.5}, I("cobblestone", 10), rng);
    items.spawn({4.6, 64.0, 4.5}, I("cobblestone", 20), rng);
    items.spawn({4.5, 64.0, 4.7}, I("dirt", 5), rng);
    items.spawn({8.5, 64.0, 8.5}, I("cobblestone", 64), rng);
    items.spawn({8.6, 64.0, 8.5}, I("cobblestone", 64), rng);
    for (int t = 0; t < 60; ++t) items.tick(w, far, false, inv);
    int stacks = 0, cobble = 0;
    for (const ItemEntity& e : items.items()) {
        ++stacks;
        if (e.stack.item == *itemRegistry().find("cobblestone")) cobble += e.stack.count;
    }
    CHECK(cobble == 30 + 128);
    CHECK(stacks == 4); // 30 cobblestone, 5 dirt, two full stacks
}

TEST_CASE("orbs of the same value merge into one with a count, collected one by one") {
    World w = floor();
    ExperienceOrbs orbs;
    Xoroshiro rng(2);
    for (int k = 0; k < 4; ++k) orbs.drop({4.5, 64.5, 4.5}, 3, rng);
    const Aabb far = Aabb::fromFeet({100.0, 64.0, 100.0}, 0.6, 1.8);
    for (int t = 0; t < 60; ++t) orbs.tick(w, far, false);
    REQUIRE(orbs.orbs().size() < 4);
    int total = 0;
    for (const ExperienceOrb& o : orbs.orbs()) total += o.value * o.count;
    CHECK(total == 12);
    const Aabb near = Aabb::fromFeet(orbs.orbs()[0].pos, 0.6, 1.8);
    int got = 0;
    for (int t = 0; t < 40; ++t) got += orbs.tick(w, near, true);
    CHECK(got == 12);
}

TEST_CASE("dropped items and orbs park in their chunk, save with it as vanilla entities and come back") {
    World w = floor();
    ItemEntities items;
    ExperienceOrbs orbs;
    Xoroshiro rng(3);
    items.spawn({4.5, 64.0, 4.5}, I("diamond", 3), rng);
    items.spawn({20.5, 64.0, 4.5}, I("emerald", 1), rng); // (chunk 1, 0)
    orbs.drop({5.5, 64.5, 5.5}, 7, rng);
    Chunk& c = *w.chunk({0, 0});
    CHECK(items.park(c) == 1);
    CHECK(orbs.park(c) == 1);
    CHECK(items.items().size() == 1); // the emerald stays: another chunk
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(c, 0));
    int itemTags = 0;
    for (const nbt::Tag& t : n.list("Entities")->items)
        itemTags += *t.get<nbt::Compound>()->string("id") == "minecraft:item";
    CHECK(itemTags == 1);
    Chunk back({0, 0});
    entitiesFromNbt(n, back);
    REQUIRE(back.droppedItems().size() == 1);
    CHECK(back.droppedItems()[0].stack.count == 3);
    REQUIRE(back.droppedOrbs().size() == 1);
    CHECK(back.droppedOrbs()[0].value == 7);
    CHECK(back.savedDrops == 2);
    CHECK(items.unpark(back, rng) == 1);
    CHECK(orbs.unpark(back) == 1);
    CHECK(items.items().size() == 2);
    CHECK(back.droppedItems().empty());
}

TEST_CASE("death drops scatter all around (vanilla dropAll)") {
    ItemEntities items;
    Xoroshiro rng(4);
    for (int k = 0; k < 40; ++k) items.scatter({0.0, 65.0, 0.0}, I("stick", 1), rng);
    bool east = false, west = false;
    for (const ItemEntity& e : items.items()) {
        CHECK(std::hypot(e.vel.x, e.vel.z) <= 0.5 + 1e-9);
        CHECK(e.vel.y == doctest::Approx(0.2));
        east = east || e.vel.x > 0.1;
        west = west || e.vel.x < -0.1;
    }
    CHECK(east);
    CHECK(west);
}

#include "gameplay/DropKeeper.h"

TEST_CASE("M30 review: the drop keeper - unloading parks, saving keeps the pools, emptied chunks save again") {
    World w = floor();
    ItemEntities items;
    ExperienceOrbs orbs;
    DropKeeper keeper(items, orbs, 9);
    Xoroshiro rng(5);
    items.spawn({4.5, 64.0, 4.5}, I("diamond", 1), rng);
    items.spawn({20.5, 64.0, 4.5}, I("emerald", 1), rng); // chunk (1, 0)
    Chunk& c0 = *w.chunk({0, 0});
    c0.clearDirty();
    // Saving: everything parks, the chunks with drops are dirty, then the pools are whole again.
    keeper.beforeSave(w);
    CHECK(items.items().empty());
    CHECK(c0.dirty());
    CHECK(c0.droppedItems().size() == 1);
    c0.clearDirty();
    w.chunk({1, 0})->clearDirty();
    keeper.afterSave(w);
    CHECK(items.items().size() == 2);
    CHECK(c0.savedDrops == 1);
    CHECK(c0.droppedItems().empty());
    // The diamond is picked up; the next save rewrites the chunk without it.
    items.mutableItems().erase(std::remove_if(items.mutableItems().begin(), items.mutableItems().end(),
                                              [](const ItemEntity& e) { return e.stack.item == *itemRegistry().find("diamond"); }),
                               items.mutableItems().end());
    keeper.beforeSave(w);
    CHECK(c0.dirty());
    CHECK(c0.droppedItems().empty());
    keeper.afterSave(w);
    CHECK(c0.savedDrops == 0);
    // Unloading parks the chunk's drops and marks it dirty; loading takes them back.
    Chunk& c1 = *w.chunk({1, 0});
    c1.clearDirty();
    keeper.chunkUnloading(c1);
    CHECK(c1.dirty());
    CHECK(c1.droppedItems().size() == 1);
    CHECK(items.items().empty());
    keeper.chunkLoaded(c1);
    CHECK(items.items().size() == 1);
}

TEST_CASE("M30 review: a full pool leaves the rest parked instead of evicting another chunk's drops") {
    ItemEntities items;
    Xoroshiro rng(6);
    for (int k = 0; k < ItemEntities::kMax; ++k) items.spawn({100.5, 64.0, 100.5}, I("stick", 1), rng);
    Chunk c({0, 0});
    c.droppedItems().push_back({{4.5, 64.0, 4.5}, {0.0, 0.0, 0.0}, I("diamond", 1), 0, 0});
    CHECK(items.unpark(c, rng) == 0);
    CHECK(c.droppedItems().size() == 1);
    int sticks = 0;
    for (const ItemEntity& e : items.items()) sticks += e.stack.item == *itemRegistry().find("stick");
    CHECK(sticks == ItemEntities::kMax);
}
