// Mobs (wiki: Zombie, Cow, Spawn, Entity format).
#include "gameplay/Mobs.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/OverworldGenerator.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

struct MobScene {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{11};
    ItemEntities items;
    Mobs mobs;
    bool survival = true;
    int64_t dayTime = 18000; // night: no burning
    float skyDarken = 11.0f;
    MobScene() {
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
                std::array<std::shared_ptr<const SectionLight>, kSectionsPerChunk> light;
                light.fill(std::make_shared<const SectionLight>()); // dark everywhere
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 0.5});
        player.setCreative(false);
    }
    void tick(int n = 1) {
        for (int i = 0; i < n; ++i) {
            player.tick(world, {});
            Mobs::Context ctx{world, player, vitals, survival, false, dayTime, skyDarken, rng, items};
            mobs.tick(ctx);
        }
    }
    std::vector<MobData*> all() {
        std::vector<MobData*> out;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                out.push_back(&m);
        });
        return out;
    }
};

} // namespace

TEST_CASE("zombies chase a survival player and hit for 3; creative players are ignored") {
    MobScene s;
    s.mobs = Mobs();
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Zombie, {8.5, 64.0, 0.5}, s.rng)));
    s.survival = false;
    s.tick(60);
    CHECK(s.vitals.health() == 20.0f);
    s.survival = true;
    float lowest = 20.0f;
    for (int i = 0; i < 200; ++i) {
        s.tick();
        lowest = std::min(lowest, s.vitals.health());
    }
    CHECK(lowest <= 17.0f); // reached and hit at least once (3 damage, normal)
}

TEST_CASE("hitting a cow hurts it, makes it panic and kills it after enough hits; loot drops") {
    MobScene s;
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {3.5, 64.0, 3.5}, s.rng)));
    MobData* cow = s.all().at(0);
    Mobs::attack(*cow, 4.0f, s.player.position());
    CHECK(cow->health == 6.0f);
    CHECK(cow->panicTicks > 0);
    CHECK(cow->hurtTime == 10);
    Mobs::attack(*cow, 4.0f, s.player.position()); // still invulnerable
    CHECK(cow->health == 6.0f);
    s.tick(10);
    cow = s.all().at(0);
    Mobs::attack(*cow, 7.0f, s.player.position());
    s.tick(25); // death animation, then gone with loot
    CHECK(s.all().empty());
    CHECK_FALSE(s.items.items().empty()); // 1-3 beef at least
}

TEST_CASE("mobs move to the chunk they walk into; far hostiles despawn") {
    MobScene s;
    s.survival = false;
    MobData m = Mobs::make(MobType::Cow, {15.5, 64.0, 8.5}, s.rng);
    m.vel = {0.5, 0, 0};
    REQUIRE(Mobs::add(s.world, m));
    s.tick(5);
    CHECK(s.world.chunk({0, 0})->mobs().empty());
    CHECK(s.world.chunk({1, 0})->mobs().size() == 1);
    MobData z = Mobs::make(MobType::Zombie, {0.5, 64.0, 0.5}, s.rng);
    REQUIRE(Mobs::add(s.world, z));
    s.player.setPosition({300.5, 64.0, 0.5}); // > 128 blocks away
    s.tick(2);
    int zombies = 0;
    for (MobData* p : s.all())
        zombies += p->type == MobType::Zombie;
    CHECK(zombies == 0);
}

TEST_CASE("zombies spawn in the dark around the player, never in light") {
    MobScene dark;
    dark.tick(2000);
    CHECK(dark.mobs.hostileCount() > 0);
    MobScene day;
    day.skyDarken = 0.0f;
    day.world.forEachChunk([](Chunk& c) {
        std::array<std::shared_ptr<const SectionLight>, kSectionsPerChunk> light;
        auto lit = std::make_shared<SectionLight>();
        lit->sky.fill(15);
        light.fill(lit);
        c.setLight(light);
    });
    day.tick(2000);
    CHECK(day.mobs.hostileCount() == 0);
}

TEST_CASE("mobs save in the chunk's entities file and load back") {
    Chunk c({1, -1});
    Xoroshiro rng(5);
    MobData cow = Mobs::make(MobType::Cow, {20.5, 70.0, -10.5}, rng);
    cow.health = 7.0f;
    cow.persistent = true;
    c.mobs().push_back(cow);
    const auto nbt = entitiesToNbt(ChunkSnapshot::of(c));
    CHECK(*nbt.list("Entities")->items[0].get<mc::nbt::Compound>()->string("id") == "minecraft:cow");
    Chunk d({1, -1});
    entitiesFromNbt(*mc::nbt::read(mc::nbt::write(nbt)), d);
    REQUIRE(d.mobs().size() == 1);
    CHECK(d.mobs()[0].health == 7.0f);
    CHECK(d.mobs()[0].pos.x == doctest::Approx(20.5));
    CHECK(d.mobs()[0].uuidHi == cow.uuidHi);
    CHECK(d.mobs()[0].persistent);
}

TEST_CASE("new chunks in grassy biomes sometimes come with a herd of cows") {
    const OverworldGenerator gen(42);
    int cows = 0, grassy = 0;
    for (int cz = -12; cz <= 12 && grassy < 80; ++cz)
        for (int cx = -12; cx <= 12 && grassy < 80; ++cx) {
            const Biome b = gen.biomeAt(gen.column(cx * 16 + 8, cz * 16 + 8));
            if (b != Biome::Plains && b != Biome::Forest && b != Biome::BirchForest) continue;
            ++grassy;
            Chunk c({cx, cz});
            gen.generate(c);
            for (const MobData& m : c.mobs()) {
                CHECK(m.type == MobType::Cow);
                CHECK(m.persistent);
                ++cows;
            }
        }
    REQUIRE(grassy >= 20);
    CHECK(cows > 0);
}

