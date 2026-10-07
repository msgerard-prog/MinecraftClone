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
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
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
    cow = nullptr;
    for (MobData* m : s.all())
        if (m->type == MobType::Cow) cow = m;
    REQUIRE(cow);
    Mobs::attack(*cow, 7.0f, s.player.position());
    s.tick(1); // loot drops at the moment of death (user decision; vanilla too)...
    CHECK_FALSE(s.items.items().empty());
    s.tick(24); // ...then the death animation, then gone
    int cows = 0; // (zombies may spawn in the dark scene meanwhile)
    for (MobData* m : s.all())
        cows += m->type == MobType::Cow;
    CHECK(cows == 0);
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
    s.mobs.setSimulationDistance(12); // (at the default 6, mobs > 128 blocks away don't tick at all)
    s.player.setPosition({200.5, 64.0, 0.5}); // > 128 blocks away, inside the simulation distance
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
        std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
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


TEST_CASE("cow herds are the same however chunks are generated") {
    const OverworldGenerator gen(42);
    std::vector<ChunkPos> grassy;
    for (int cz = -12; cz <= 12 && grassy.size() < 30; ++cz)
        for (int cx = -12; cx <= 12 && grassy.size() < 30; ++cx) {
            const Biome b = gen.biomeAt(gen.column(cx * 16 + 8, cz * 16 + 8));
            if (b == Biome::Plains || b == Biome::Forest || b == Biome::BirchForest) grassy.push_back({cx, cz});
        }
    auto herds = [&](bool reverse) {
        std::vector<std::vector<MobData>> out(grassy.size());
        for (size_t k = 0; k < grassy.size(); ++k) {
            const size_t i = reverse ? grassy.size() - 1 - k : k;
            Chunk c(grassy[i]);
            gen.generate(c);
            out[i] = c.mobs();
        }
        return out;
    };
    const auto a = herds(false), b = herds(true);
    for (size_t i = 0; i < a.size(); ++i) {
        REQUIRE(a[i].size() == b[i].size());
        for (size_t j = 0; j < a[i].size(); ++j) {
            CHECK(a[i][j].pos == b[i][j].pos);
            CHECK(a[i][j].uuidHi == b[i][j].uuidHi);
        }
    }
}

TEST_CASE("summoned mobs get distinct version-4 UUIDs") {
    Xoroshiro rng(1);
    const MobData a = Mobs::make(MobType::Cow, {0, 64, 0}, rng);
    Xoroshiro same(1);
    const MobData b = Mobs::make(MobType::Cow, {0, 64, 0}, same);
    CHECK((a.uuidHi != b.uuidHi || a.uuidLo != b.uuidLo)); // not from the game RNG
    CHECK(((a.uuidHi >> 12) & 0xF) == 4);
}

TEST_CASE("a chunk unloaded and loaded again is ticked once") {
    MobScene s;
    Mobs::add(s.world, Mobs::make(MobType::Cow, {8.5, 64.0, 8.5}, s.rng));
    s.world.insertChunk(s.world.removeChunk({0, 0}));
    int visits = 0;
    s.world.forEachTickingChunk([&](Chunk& c) { visits += c.pos() == ChunkPos{0, 0}; });
    s.world.markTicking({0, 0});
    s.world.forEachTickingChunk([&](Chunk& c) { visits += c.pos() == ChunkPos{0, 0}; });
    CHECK(visits == 2);
}

TEST_CASE("idle mobs don't mark their chunk for saving; mobs beyond the simulation distance don't tick") {
    MobScene s;
    Mobs::add(s.world, Mobs::make(MobType::Cow, {8.5, 64.0, 8.5}, s.rng));
    for (int i = 0; i < 40; ++i) s.tick(); // settle on the ground
    Chunk& c = *s.world.chunk({0, 0});
    c.mobs()[0].goal = c.mobs()[0].pos;
    c.mobs()[0].goalTicks = 0;
    c.clearDirty();
    s.tick();
    CHECK_FALSE(c.dirty());
    s.player.setPosition({(Mobs::kDefaultSimulationDistance + 3) * 16.0, 64.0, 0.0});
    c.mobs()[0].vel = {0.3, 0.0, 0.0};
    const glm::dvec3 before = c.mobs()[0].pos;
    s.tick();
    CHECK(c.mobs()[0].pos == before);
}

TEST_CASE("zombies in the sun burn for 1 damage a second until they die") {
    MobScene s;
    s.skyDarken = 0.0f; // noon
    std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
    auto bright = std::make_shared<SectionLight>();
    bright->sky.fill(15);
    light.fill(bright);
    s.world.forEachChunk([&](Chunk& c) { c.setLight(light); });
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Zombie, {8.5, 64.0, 8.5}, s.rng)));
    s.tick(101);
    MobData* z = s.all().at(0);
    CHECK(z->health == doctest::Approx(20.0f - 6.0f)); // ticks 0, 20, ..., 100
    s.tick(400);
    int zombies = 0;
    for (MobData* m : s.all())
        zombies += m->type == MobType::Zombie;
    CHECK(zombies == 0);
}

TEST_CASE("hit zombies don't flee; airborne mobs get no knockback lift") {
    MobScene s;
    MobData z = Mobs::make(MobType::Zombie, {3.5, 70.0, 3.5}, s.rng);
    z.onGround = false;
    Mobs::attack(z, 1.0f, {0.5, 70.0, 0.5});
    CHECK(z.panicTicks == 0);
    CHECK(z.vel.y == 0.0);
    MobData c = Mobs::make(MobType::Cow, {3.5, 64.0, 3.5}, s.rng);
    c.onGround = true;
    Mobs::attack(c, 1.0f, {0.5, 64.0, 0.5});
    CHECK(c.panicTicks > 0);
    CHECK(c.vel.y > 0.0);
}

TEST_CASE("mobs take fall damage beyond 3 blocks") {
    MobScene s;
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {8.5, 74.0, 8.5}, s.rng))); // 10 blocks up
    s.tick(60);
    MobData* cow = nullptr;
    for (MobData* m : s.all())
        if (m->type == MobType::Cow) cow = m;
    REQUIRE(cow);
    CHECK(cow->health == doctest::Approx(10.0f - 7.0f));
}

TEST_CASE("zombies don't spawn on bedrock or in lava") {
    MobScene s;
    s.survival = false;
    const auto& reg = blockRegistry();
    s.world.forEachChunk([&](Chunk& c) {
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                c.set(x, 63, z, reg.defaultState((x + z) % 2 ? blocks::Bedrock : blocks::Stone));
                if ((x + z) % 2 == 0) c.set(x, 64, z, reg.defaultState(blocks::Lava));
            }
    });
    s.tick(2000);
    CHECK(s.all().empty());
}
