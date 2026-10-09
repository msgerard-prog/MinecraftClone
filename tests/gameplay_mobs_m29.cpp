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
