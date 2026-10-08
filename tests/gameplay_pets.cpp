// Pets (M26.1): wolves, cats, ocelots, parrots.
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

struct Field {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{31};
    ItemEntities items;
    Mobs mobs;
    Field() {
        for (int cz = -3; cz <= 3; ++cz)
            for (int cx = -3; cx <= 3; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 0.5});
        player.setCreative(false);
    }
    void tick(int n, uint64_t target = 0, bool survival = true) {
        for (int i = 0; i < n; ++i) {
            player.tick(world, {});
            Mobs::Context ctx{world, player, vitals, survival, false, 6000, 0.0f, rng, items};
            ctx.naturalSpawning = false;
            ctx.playerTargetUuid = target;
            mobs.tick(ctx);
        }
    }
    MobData* find(MobType t) {
        MobData* out = nullptr;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                if (m.type == t && m.health > 0.0f && !out) out = &m;
        });
        return out;
    }
    static ItemId item(const char* n) { return *itemRegistry().find(n); }
};

} // namespace

TEST_CASE("wolves tame with bones (1 in 3), sit and stand on a click, follow and teleport to the player (M26.1)") {
    Field f;
    REQUIRE(Mobs::add(f.world, Mobs::make(MobType::Wolf, {4.5, 64.0, 0.5}, f.rng)));
    MobData* w = f.find(MobType::Wolf);
    REQUIRE(w);
    int bones = 0;
    while (!w->tamed && bones < 50) {
        CHECK(Mobs::interact(*w, Field::item("bone"), f.rng, f.items) == Mobs::Use::Fed);
        ++bones;
    }
    REQUIRE(w->tamed);
    CHECK(w->sitting);
    CHECK(w->health == doctest::Approx(40.0f)); // (tamed: 40)
    CHECK(Mobs::interact(*w, 0, f.rng, f.items) == Mobs::Use::Sat);
    CHECK_FALSE(w->sitting);
    // Left far behind: it appears next to the player.
    f.player.setPosition({30.5, 64.0, 30.5});
    f.tick(40);
    w = f.find(MobType::Wolf);
    REQUIRE(w);
    CHECK(glm::length(w->pos - f.player.position()) < 8.0);
    // Sitting, it stays.
    w->sitting = true;
    const glm::dvec3 at = w->pos;
    f.player.setPosition({-30.5, 64.0, -30.5});
    f.tick(60);
    w = f.find(MobType::Wolf);
    CHECK(glm::length(w->pos - at) < 1.0);
}

TEST_CASE("a tamed wolf attacks what the player hits; a wild wolf hit by the player turns on them (M26.1)") {
    Field f;
    MobData pet = Mobs::make(MobType::Wolf, {2.5, 64.0, 0.5}, f.rng);
    pet.tamed = true;
    pet.health = 40.0f;
    REQUIRE(Mobs::add(f.world, pet));
    REQUIRE(Mobs::add(f.world, Mobs::make(MobType::Pig, {7.5, 64.0, 0.5}, f.rng)));
    const uint64_t pig = f.find(MobType::Pig)->uuidHi;
    f.tick(200, pig);
    CHECK((f.find(MobType::Pig) == nullptr || f.find(MobType::Pig)->health < 10.0f));
    Field g;
    REQUIRE(Mobs::add(g.world, Mobs::make(MobType::Wolf, {2.5, 64.0, 0.5}, g.rng)));
    MobData* wild = g.find(MobType::Wolf);
    Mobs::attack(*wild, 1.0f, g.player.position());
    CHECK(wild->angry);
    g.tick(100);
    CHECK(g.vitals.health() < Vitals::kMaxHealth);
}

TEST_CASE("creepers keep away from cats; parrots dance by a playing jukebox; pets save their owner, collar and variant (M26.1)") {
    Field f;
    MobData creeper = Mobs::make(MobType::Creeper, {6.5, 64.0, 0.5}, f.rng);
    REQUIRE(Mobs::add(f.world, creeper));
    REQUIRE(Mobs::add(f.world, Mobs::make(MobType::Cat, {4.5, 64.0, 0.5}, f.rng)));
    Mobs::Context ctx{f.world, f.player, f.vitals, true, false, 18000, 11.0f, f.rng, f.items};
    ctx.naturalSpawning = false;
    for (int t = 0; t < 40; ++t) f.mobs.tick(ctx);
    CHECK(f.find(MobType::Creeper)->fuse == 0); // (it never came for the player)
    // A cat: tamed with fish, its collar dyed blue, saved and loaded.
    MobData* cat = f.find(MobType::Cat);
    while (!cat->tamed) Mobs::interact(*cat, Field::item("cod"), f.rng, f.items);
    CHECK(Mobs::interact(*cat, Field::item("blue_dye"), f.rng, f.items) == Mobs::Use::Fed);
    CHECK(cat->color2 == 11);
    Chunk c({0, 0});
    c.mobs().push_back(*cat);
    MobData parrot = Mobs::make(MobType::Parrot, {3.5, 64.0, 3.5}, f.rng);
    parrot.woolColour = 3;
    c.mobs().push_back(parrot);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 2);
    CHECK(back.mobs()[0].tamed);
    CHECK(back.mobs()[0].sitting == cat->sitting);
    CHECK(back.mobs()[0].color2 == 11);
    CHECK(back.mobs()[0].woolColour == cat->woolColour);
    CHECK(back.mobs()[1].woolColour == 3);
    CHECK_FALSE(back.mobs()[1].tamed);
}

TEST_CASE("parrots dance within 3 blocks of a playing jukebox (M26.1)") {
    Field f;
    f.world.setBlock({2, 64, 2}, blockRegistry().defaultState(blocks::Jukebox));
    f.world.chunk({0, 0})->addJukebox(2, 64, 2).playing = true;
    REQUIRE(Mobs::add(f.world, Mobs::make(MobType::Parrot, {3.5, 64.0, 2.5}, f.rng)));
    const float yaw0 = f.find(MobType::Parrot)->yaw;
    f.tick(60);
    CHECK(f.find(MobType::Parrot)->peek == 1); // (dancing)
    CHECK(std::abs(f.find(MobType::Parrot)->yaw - yaw0) > 90.0f);
}
