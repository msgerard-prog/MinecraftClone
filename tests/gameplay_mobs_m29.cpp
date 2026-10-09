// Mobs of M29 (completeness; wiki pages of each mob).
#include "gameplay/Inventory.h"
#include "gameplay/Projectiles.h"
#include "gameplay/Mobs.h"
#include "world/Weather.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Villagers.h"
#include "world/ChunkSerializer.h"
#include "world/OverworldGenerator.h"
#include "world/Potions.h"

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
    bool thundering = false;
    const Weather* weather = nullptr;
    bool mobDrops = true, mobGriefing = true; // (game rules, M28.1)
    int difficulty = 2;
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
            ctx.thundering = thundering;
            ctx.weather = weather;
            ctx.mobDrops = mobDrops;
            ctx.mobGriefing = mobGriefing;
            ctx.difficulty = difficulty;
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

// M29.1a: husks, strays, bogged and parched (wiki: Husk, Stray, Bogged, Parched).
namespace {
void brighten(MobScene& s) {
    s.skyDarken = 0.0f; // noon
    std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
    auto bright = std::make_shared<SectionLight>();
    bright->sky.fill(15);
    light.fill(bright);
    s.world.forEachChunk([&](Chunk& c) { c.setLight(light); });
}
MobData* findType(MobScene& s, MobType t) {
    for (MobData* m : s.all())
        if (m->type == t) return m;
    return nullptr;
}
} // namespace

TEST_CASE("husks and parched don't burn in the sun; strays and bogged do") {
    MobScene s;
    brighten(s);
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Husk, {8.5, 64.0, 8.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Parched, {-8.5, 64.0, 8.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Stray, {8.5, 64.0, -8.5}, s.rng)));
    s.tick(60);
    CHECK(findType(s, MobType::Husk)->health == doctest::Approx(20.0f));
    CHECK(findType(s, MobType::Parched)->health == doctest::Approx(16.0f));
    CHECK(findType(s, MobType::Stray)->health < 20.0f);
    CHECK(isUndead(MobType::Husk));
    CHECK(isSkeleton(MobType::Bogged));
    CHECK(mobInfo(MobType::Bogged).maxHealth == 16.0f);
}

TEST_CASE("a drowning husk turns into a zombie (45 s under water)") {
    MobScene s;
    s.survival = false;
    for (int y = 64; y <= 67; ++y)
        for (int z = -32; z < 48; ++z)
            for (int x = -32; x < 48; ++x)
                s.world.setBlock({x, y, z}, blockRegistry().defaultState(blocks::Water));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Husk, {8.5, 64.0, 8.5}, s.rng)));
    s.tick(950);
    CHECK(findType(s, MobType::Husk) == nullptr);
    CHECK(findType(s, MobType::Zombie) != nullptr);
}

TEST_CASE("a husk's hit starves; a stray's arrow slows") {
    MobScene s;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Husk, {2.0, 64.0, 0.5}, s.rng)));
    for (int i = 0; i < 100 && s.vitals.effectLevel(Effect::Hunger) == 0; ++i) s.tick();
    CHECK(s.vitals.effectLevel(Effect::Hunger) > 0);

    MobScene t;
    Projectiles shots;
    REQUIRE(Mobs::add(t.world, Mobs::make(MobType::Stray, {8.5, 64.0, 0.5}, t.rng)));
    for (int i = 0; i < 200 && t.vitals.effectLevel(Effect::Slowness) == 0; ++i) {
        t.player.tick(t.world, {});
        Mobs::Context ctx{t.world, t.player, t.vitals, true, false, t.dayTime, t.skyDarken, t.rng, t.items};
        ctx.projectiles = &shots;
        t.mobs.tick(ctx);
        Inventory inv;
        shots.tick(t.world, t.player, &t.vitals, inv, true, t.rng);
    }
    CHECK(t.vitals.effectLevel(Effect::Slowness) > 0);
}

// M29.1b: undead mounts and jockeys (wiki: Jockey, Skeleton Horse, Zombie Horse, Camel Husk,
// Zombie Nautilus).
TEST_CASE("jockeys: a rider sits on its mount and falls off when the mount dies") {
    MobScene s;
    s.survival = false;
    MobData spider = Mobs::make(MobType::Spider, {8.5, 64.0, 8.5}, s.rng);
    MobData rider = Mobs::make(MobType::Skeleton, {8.5, 64.0, 8.5}, s.rng);
    rider.vehicle = spider.uuidHi;
    REQUIRE(Mobs::add(s.world, spider));
    REQUIRE(Mobs::add(s.world, rider));
    s.tick(5);
    MobData* sp = findType(s, MobType::Spider);
    MobData* sk = findType(s, MobType::Skeleton);
    REQUIRE((sp && sk));
    CHECK(sk->pos.y == doctest::Approx(sp->pos.y + Mobs::seatHeight(*sp)));
    CHECK(sk->pos.x == doctest::Approx(sp->pos.x));
    CHECK(sp->mobRidden);
    sp->health = 0.0f;
    sp->deathTime = 19;
    s.tick(2);
    CHECK(findType(s, MobType::Skeleton)->vehicle == 0);
}

TEST_CASE("a skeleton trap springs into four skeleton horsemen near the player") {
    MobScene s;
    MobData trap = Mobs::make(MobType::SkeletonHorse, {5.5, 64.0, 0.5}, s.rng);
    trap.skeletonTrap = true;
    REQUIRE(Mobs::add(s.world, trap));
    s.tick(2);
    int horses = 0, skeletons = 0, tamed = 0, riders = 0;
    for (MobData* m : s.all()) {
        horses += m->type == MobType::SkeletonHorse;
        tamed += m->type == MobType::SkeletonHorse && m->tamed;
        skeletons += m->type == MobType::Skeleton;
        riders += m->type == MobType::Skeleton && m->vehicle != 0;
    }
    CHECK(horses == 4);
    CHECK(tamed == 4);
    CHECK(skeletons == 4);
    CHECK(riders == 4);
    // Bolts: 1% Easy .. 4.5% Hard of storms' strikes, none on Peaceful.
    World w;
    w.createChunk({0, 0});
    Xoroshiro rng{3};
    int traps = 0;
    for (int i = 0; i < 2000; ++i) traps += Mobs::spawnSkeletonTrap(w, {8.5, 64.0, 8.5}, 2, rng) ? 1 : 0;
    CHECK(traps > 20);
    CHECK(traps < 90);
    CHECK_FALSE(Mobs::spawnSkeletonTrap(w, {8.5, 64.0, 8.5}, 0, rng));
}

TEST_CASE("undead mounts: no breeding; zombie horses eat red mushrooms; stats") {
    MobScene s;
    MobData a = Mobs::make(MobType::ZombieHorse, {0, 64, 0}, s.rng);
    MobData b = Mobs::make(MobType::ZombieHorse, {0, 64, 0}, s.rng);
    CHECK(isUndeadMount(b.type)); // (never mate: Mounts.cpp canMate)
    CHECK(a.maxHealth == 25.0f);
    CHECK(Mobs::make(MobType::SkeletonHorse, {0, 64, 0}, s.rng).maxHealth == 15.0f);
    a.health = 10.0f;
    ItemEntities items;
    CHECK(Mobs::interact(a, *itemRegistry().find("red_mushroom"), s.rng, items) == Mobs::Use::Fed);
    CHECK(a.health == doctest::Approx(13.0f));
    MobData sk = Mobs::make(MobType::SkeletonHorse, {0, 64, 0}, s.rng);
    sk.health = 5.0f;
    CHECK(Mobs::interact(sk, *itemRegistry().find("wheat"), s.rng, items) != Mobs::Use::Fed);
    CHECK(isUndead(MobType::CamelHusk));
    CHECK(burnsInDaylight(MobType::ZombieHorse));
    CHECK_FALSE(burnsInDaylight(MobType::SkeletonHorse));
    MobData n = Mobs::make(MobType::ZombieNautilus, {0, 64, 0}, s.rng);
    int fed = 0;
    for (int i = 0; i < 30 && !n.tamed; ++i, ++fed)
        Mobs::interact(n, *itemRegistry().find("pufferfish"), s.rng, items);
    CHECK(n.tamed);
}
