// Monsters 3 (M26.4a): cave spiders, silverfish and infested blocks, wither skeletons and
// the Wither effect, phantoms and insomnia, cobwebs.
#include "gameplay/ItemEntities.h"
#include "gameplay/Mining.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/LevelData.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <functional>

using namespace mc;
using namespace mc::world;

namespace {

struct Cave {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{83};
    ItemEntities items;
    Mobs mobs;
    BlockUpdates updates{world};
    int64_t dayTime = 18000;
    float skyDarken = 11.0f;
    int awake = 0;
    bool naturalSpawning = false;
    bool survival = true;
    explicit Cave(bool open = false) {
        world.setListener(&updates);
        const auto& r = blockRegistry();
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        for (int y = 50; y <= 63; ++y) c.set(x, y, z, r.defaultState(blocks::Stone));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                auto l = std::make_shared<SectionLight>();
                if (open) l->sky.fill(15);
                light.fill(l);
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 0.5});
        player.setCreative(false);
    }
    void tick(int n, const std::function<void()>& each = {}) {
        for (int i = 0; i < n; ++i) {
            if (each) each();
            Mobs::Context ctx{world, player, vitals, survival, false, dayTime, skyDarken, rng, items};
            ctx.naturalSpawning = naturalSpawning;
            ctx.timeSinceRest = awake;
            mobs.tick(ctx);
        }
    }
    int count(MobType t) {
        int n = 0;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs()) n += m.type == t && m.health > 0.0f;
        });
        return n;
    }
    MobData* find(MobType t) {
        MobData* out = nullptr;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                if (m.type == t && m.health > 0.0f && !out) out = &m;
        });
        return out;
    }
};

} // namespace

TEST_CASE("infested blocks let a silverfish out when broken; a hurt silverfish calls out the others; idle ones burrow (M26.4a)") {
    Cave c;
    const auto& r = blockRegistry();
    c.world.updateBlock({3, 63, 3}, r.defaultState(blocks::InfestedStone));
    c.world.updateBlock({3, 63, 3}, 0);
    REQUIRE(c.updates.silverfishOut().size() == 1);
    CHECK(c.updates.silverfishOut()[0] == BlockPos{3, 63, 3});
    c.updates.silverfishOut().clear();
    // Three hidden nearby: hitting a silverfish wakes them.
    for (int i = 0; i < 3; ++i) c.world.updateBlock({5 + i, 62, 2}, r.defaultState(blocks::InfestedStone));
    REQUIRE(Mobs::add(c.world, Mobs::make(MobType::Silverfish, {1.5, 64.0, 1.5}, c.rng)));
    Mobs::attack(*c.find(MobType::Silverfish), 1.0f, c.player.position());
    c.tick(2);
    CHECK(c.updates.silverfishOut().size() == 3);
    c.updates.silverfishOut().clear();
    // An idle one slips into the stone (no target: the player in creative nearby).
    c.survival = false;
    c.tick(4000);
    bool burrowed = false; // (a burrowed silverfish leaves its block infested for good)
    for (int z = -32; z < 48 && !burrowed; ++z)
        for (int x = -32; x < 48 && !burrowed; ++x)
            for (int y = 60; y <= 66 && !burrowed; ++y)
                burrowed = BlockUpdates::isInfested(r.blockOf(c.world.getBlock({x, y, z})));
    CHECK(burrowed);
}

TEST_CASE("cave spiders poison, wither skeletons wither; Wither hurts every 2 s and can kill (M26.4a)") {
    Cave c;
    REQUIRE(Mobs::add(c.world, Mobs::make(MobType::CaveSpider, {2.5, 64.0, 0.5}, c.rng)));
    c.tick(100);
    bool poisoned = false;
    for (const auto& e : c.vitals.effects()) poisoned = poisoned || e.type == Effect::Poison;
    CHECK(poisoned);
    Cave w;
    REQUIRE(Mobs::add(w.world, Mobs::make(MobType::WitherSkeleton, {2.5, 64.0, 0.5}, w.rng)));
    w.tick(100);
    bool withered = false;
    for (const auto& e : w.vitals.effects()) withered = withered || e.type == Effect::Wither;
    CHECK(withered);
    Vitals v;
    v.addEffect(Effect::Wither, 0, 2000);
    v.setState(20.0f, 10, 0.0f, 0.0f); // (no quick regeneration)
    for (int t = 0; t < 2000 && !v.dead(); ++t) {
        v.tickEffects();
        v.tick(64.0, true, false, false); // (the hurt cooldown runs down)
    }
    CHECK(v.dead()); // (unlike Poison, Wither kills)
}

TEST_CASE("phantoms come for players awake 3 days at night under the open sky, and swoop to bite (M26.4a)") {
    Cave rested(true);
    rested.naturalSpawning = true;
    rested.awake = 1000;
    rested.tick(6000);
    CHECK(rested.count(MobType::Phantom) == 0);
    Cave tired(true);
    tired.naturalSpawning = true;
    tired.awake = 500000; // (long awake: very likely each try)
    const float before = tired.vitals.health();
    tired.tick(6000, [&] { tired.vitals.addEffect(Effect::Regeneration, 0, 0); });
    CHECK(tired.count(MobType::Phantom) + int(tired.vitals.health() < before) > 0);
    // A phantom over the player swoops down and bites.
    Cave one(true);
    MobData ph = Mobs::make(MobType::Phantom, {0.5, 84.0, 0.5}, one.rng);
    ph.chargeTicks = 5;
    REQUIRE(Mobs::add(one.world, ph));
    const float hp = one.vitals.health();
    one.tick(400);
    CHECK(one.vitals.health() < hp);
}

TEST_CASE("cobwebs: a sword cuts them for string, shears keep them; infested blocks drop nothing; level.dat keeps the time awake (M26.4a)") {
    const auto& r = blockRegistry();
    Xoroshiro rng(3);
    std::vector<ItemStack> out;
    blockDrops(r.defaultState(blocks::Cobweb), {*itemRegistry().find("iron_sword"), 1}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].item == *itemRegistry().find("string"));
    out.clear();
    blockDrops(r.defaultState(blocks::Cobweb), {*itemRegistry().find("shears"), 1}, rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].item == itemRegistry().blockItem(blocks::Cobweb));
    out.clear();
    blockDrops(r.defaultState(blocks::InfestedStone), {*itemRegistry().find("iron_pickaxe"), 1}, rng, out);
    CHECK(out.empty());
    CHECK(breakTicks(r.defaultState(blocks::Cobweb), {*itemRegistry().find("iron_sword"), 1}, false, true) <
          breakTicks(r.defaultState(blocks::Cobweb), {}, false, true));
    const auto dir = std::filesystem::temp_directory_path() / "mc_tests" / "insomnia";
    std::filesystem::create_directories(dir);
    LevelData l;
    l.timeSinceRest = 123456;
    REQUIRE(l.save(dir));
    const auto back = LevelData::load(dir);
    REQUIRE(back);
    CHECK(back->timeSinceRest == 123456);
}
