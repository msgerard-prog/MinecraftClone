// Leads, leash knots and llama caravans (M28.3c; wiki: Lead, Llama › Caravans).
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Recipes.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/World.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

struct Scene {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{9};
    ItemEntities items;
    Mobs mobs;
    Scene() {
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
            }
        player.setPosition({0.5, 64.0, 0.5});
        player.setCreative(false);
    }
    void tick(int n = 1) {
        for (int i = 0; i < n; ++i) {
            Mobs::Context ctx{world, player, vitals, true, false, 6000, 0.0f, rng, items};
            ctx.naturalSpawning = false;
            mobs.tick(ctx);
        }
    }
    MobData* find(MobType t) {
        MobData* out = nullptr;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                if (m.type == t && m.health > 0.0f) out = &m;
        });
        return out;
    }
};

} // namespace

TEST_CASE("leads: cows go on the lead, are pulled in past 6 blocks and snap past 12 (1.21.6)") {
    Scene s;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {3.5, 64.0, 0.5}, s.rng)));
    MobData* cow = s.find(MobType::Cow);
    CHECK(Mobs::leashToPlayer(*cow));
    CHECK_FALSE(Mobs::leashToPlayer(*cow)); // (already)
    MobData zombie = Mobs::make(MobType::Zombie, {0, 64, 0}, s.rng);
    CHECK_FALSE(Mobs::leashToPlayer(zombie));
    // The player walks off 8 blocks: the cow follows.
    s.player.setPosition({11.5, 64.0, 0.5});
    for (int i = 0; i < 60; ++i)
        s.tick();
    cow = s.find(MobType::Cow);
    REQUIRE(cow);
    CHECK(cow->leash == 1);
    CHECK(cow->pos.x > 4.5);
    // 11 blocks away the lead holds (it snapped at 10 before 1.21.6).
    s.player.setPosition({cow->pos.x + 11.0, 64.0, 0.5});
    s.tick(1);
    cow = s.find(MobType::Cow);
    CHECK(cow->leash == 1);
    // Teleporting away: the lead snaps and drops.
    s.player.setPosition({40.5, 64.0, 0.5});
    s.tick(2);
    cow = s.find(MobType::Cow);
    CHECK(cow->leash == 0);
    CHECK(s.items.items().size() == 1);
}

TEST_CASE(
    "leads: tied to a fence with a knot; breaking the knot frees them; saved as vanilla's leash") {
    Scene s;
    s.world.setBlock({2, 64, 0}, blockRegistry().defaultState(blocks::OakFence));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Sheep, {3.5, 64.0, 2.5}, s.rng)));
    MobData* sheep = s.find(MobType::Sheep);
    Mobs::leashToPlayer(*sheep);
    CHECK(Mobs::tieToFence(s.world, {2, 64, 0}, s.player.position(), s.rng) == 1);
    sheep = s.find(MobType::Sheep);
    CHECK(sheep->leash == 2);
    MobData* knot = s.find(MobType::LeashKnot);
    REQUIRE(knot);

    Chunk& c = *s.world.chunk({0, 0});
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(c, 0));
    bool savedLeash = false;
    for (const nbt::Tag& t : n.list("Entities")->items)
        if (const nbt::Tag* l = t.get<nbt::Compound>()->find("leash"))
            savedLeash =
                l->get<std::vector<int32_t>>() && (*l->get<std::vector<int32_t>>())[0] == 2;
    CHECK(savedLeash);

    Mobs::breakKnot(s.world, *knot, s.items, s.rng);
    CHECK(s.find(MobType::Sheep)->leash == 0);
    CHECK(s.items.items().size() == 1);

    // A knot killed by damage (a firework, an explosion) frees what was tied to it too.
    s.tick(25);
    Mobs::leashToPlayer(*s.find(MobType::Sheep));
    REQUIRE(Mobs::tieToFence(s.world, {2, 64, 0}, s.player.position(), s.rng) == 1);
    knot = s.find(MobType::LeashKnot);
    REQUIRE(knot);
    Mobs::attack(*knot, 5.0f, knot->pos);
    s.tick(2);
    CHECK(s.find(MobType::Sheep)->leash == 0);

    std::array<ItemStack, 9> g{};
    g[0] = g[1] = g[3] = g[8] = ItemStack{*itemRegistry().find("string"), 1};
    g[4] = ItemStack{*itemRegistry().find("slime_ball"), 1};
    const auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(itemRegistry().item(r->item).id == "minecraft:lead");
    CHECK(r->count == 2);
}

TEST_CASE("llama caravans: free llamas fall in behind a led one") {
    Scene s;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Llama, {2.5, 64.0, 0.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Llama, {5.5, 64.0, 0.5}, s.rng)));
    std::vector<MobData*> llamas;
    s.world.forEachChunk([&](Chunk& c) {
        for (auto& m : c.mobs())
            if (m.type == MobType::Llama) llamas.push_back(&m);
    });
    REQUIRE(llamas.size() == 2);
    Mobs::leashToPlayer(*llamas[0]);
    const uint64_t leader = llamas[0]->uuidHi;
    bool joined = false;
    for (int i = 0; i < 400 && !joined; ++i) {
        s.tick();
        s.world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                joined = joined || (m.type == MobType::Llama && m.caravanHead == leader);
        });
    }
    CHECK(joined);
}
