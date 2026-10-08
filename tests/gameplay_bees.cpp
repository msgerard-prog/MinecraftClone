// Bees (M26.3b): hives, pollen, honey, stinging; nests from saplings.
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Beehives.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"

#include <doctest/doctest.h>

#include <functional>

using namespace mc;
using namespace mc::world;

namespace {

struct Garden {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{61};
    ItemEntities items;
    Mobs mobs;
    int64_t dayTime = 3000;
    Garden() {
        const auto& r = blockRegistry();
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 63, z, r.defaultState(blocks::GrassBlock));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 30.5});
        player.setCreative(false);
    }
    void tick(int n, const std::function<void()>& each = {}) {
        for (int i = 0; i < n; ++i) {
            if (each) each();
            Mobs::Context ctx{world, player, vitals, true, false, dayTime, 0.0f, rng, items};
            ctx.naturalSpawning = false;
            mobs.tick(ctx);
        }
    }
    MobData* bee() {
        MobData* out = nullptr;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                if (m.type == MobType::Bee && m.health > 0.0f && !out) out = &m;
        });
        return out;
    }
    BeehiveData* hive(const BlockPos& p) {
        Chunk* c = world.chunk(p.chunk());
        return c ? c->beehive(blockToLocal(p.x), p.y, blockToLocal(p.z)) : nullptr;
    }
    void placeHive(const BlockPos& p) {
        const auto& r = blockRegistry();
        world.updateBlock(p, r.set(r.defaultState(blocks::Beehive), properties::facing, 1)); // (south)
        world.markTicking(p.chunk());
    }
};

} // namespace

TEST_CASE("a bee gathers pollen from a flower, goes into its hive, and leaves a level of honey coming out (M26.3b)") {
    Garden g;
    const BlockPos hp{0, 66, 0};
    g.placeHive(hp);
    REQUIRE(g.hive(hp));
    for (int i = 0; i < 4; ++i) g.world.updateBlock({3 + i, 64, 3}, blockRegistry().defaultState(blocks::Poppy));
    MobData bee = Mobs::make(MobType::Bee, {0.5, 66.0, 2.5}, g.rng);
    bee.home = {hp.x, hp.y, hp.z};
    REQUIRE(Mobs::add(g.world, bee));
    bool hadNectar = false, wentIn = false;
    g.tick(4000, [&] {
        if (MobData* b = g.bee()) hadNectar = hadNectar || b->nectar;
        wentIn = wentIn || g.hive(hp)->count > 0;
    });
    CHECK(hadNectar);
    CHECK(wentIn);
    // It stays inside 2400 ticks with pollen, then comes out: honey level 1.
    g.tick(2600);
    CHECK(blockRegistry().get(g.world.getBlock(hp), properties::honeyLevel) >= 1);
    // At night every bee stays in.
    g.dayTime = 18000;
    g.tick(3000);
    CHECK(g.bee() == nullptr);
    CHECK(g.hive(hp)->count == 1);
}

TEST_CASE("a hit bee and its hive-mates sting: Poison, and a bee that stung dies soon (M26.3b)") {
    Garden g;
    g.player.setPosition({0.5, 64.0, 4.5});
    for (int i = 0; i < 3; ++i) REQUIRE(Mobs::add(g.world, Mobs::make(MobType::Bee, {0.5 + i, 66.0, 0.5}, g.rng)));
    Mobs::attack(*g.bee(), 1.0f, g.player.position());
    g.tick(200);
    CHECK(g.vitals.health() < Vitals::kMaxHealth);
    bool poisoned = false;
    for (const auto& e : g.vitals.effects()) poisoned = poisoned || e.type == Effect::Poison;
    CHECK(poisoned);
    int stung = 0;
    g.world.forEachChunk([&](Chunk& c) {
        for (auto& m : c.mobs()) stung += m.type == MobType::Bee && m.stung;
    });
    CHECK(stung >= 2); // (the others joined in)
    g.tick(1400);
    int alive = 0;
    g.world.forEachChunk([&](Chunk& c) {
        for (auto& m : c.mobs()) alive += m.type == MobType::Bee && m.health > 0.0f;
    });
    CHECK(alive < 3);
}

TEST_CASE("breaking a hive lets its bees out angry; a campfire below smokes it; hives save their bees (M26.3b)") {
    Garden g;
    const BlockPos hp{0, 66, 0};
    g.placeHive(hp);
    BeehiveData& h = *g.hive(hp);
    h.count = 2;
    h.bees[0].uuidHi = 11;
    h.bees[0].nectar = true;
    h.bees[1].uuidHi = 12;
    // Saved and loaded with its bees.
    Chunk back({0, 0});
    REQUIRE(chunkFromNbt(chunkToNbt(ChunkSnapshot::of(*g.world.chunk({0, 0}), 0)), back));
    REQUIRE(back.beehive(0, 66, 0));
    CHECK(back.beehive(0, 66, 0)->count == 2);
    CHECK(back.beehive(0, 66, 0)->bees[0].nectar);
    // Smoke: a lit campfire up to 5 below.
    CHECK_FALSE(hiveSmoked(g.world, hp));
    g.world.updateBlock({0, 63, 0}, blockRegistry().defaultState(blocks::Campfire));
    CHECK(hiveSmoked(g.world, hp));
    // Broken: both bees come out angry (queued, then added by the mob pass).
    g.world.updateBlock(hp, 0);
    CHECK(g.world.queuedMobs().size() == 2);
    g.tick(1);
    int angry = 0;
    g.world.forEachChunk([&](Chunk& c) {
        for (auto& m : c.mobs()) angry += m.type == MobType::Bee && m.angry;
    });
    CHECK(angry == 2);
}

TEST_CASE("an oak grown beside flowers sometimes carries a bee nest with bees (M26.3b)") {
    Garden g;
    BlockUpdates updates(g.world);
    const auto& r = blockRegistry();
    int nests = 0;
    for (int i = 0; i < 160 && nests == 0; ++i) {
        const int x = -30 + (i % 12) * 5, z = -30 + (i / 12) * 5;
        if (z > 30) break;
        g.world.updateBlock({x + 1, 64, z}, r.defaultState(blocks::Dandelion));
        g.world.updateBlock({x, 64, z}, r.defaultState(blocks::OakSapling));
        for (int k = 0; k < 40 && r.blockOf(g.world.getBlock({x, 64, z})) == blocks::OakSapling; ++k)
            updates.boneMeal({x, 64, z});
        for (int y = 64; y < 74; ++y)
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx)
                    if (r.blockOf(g.world.getBlock({x + dx, y, z + dz})) == blocks::BeeNest) {
                        ++nests;
                        REQUIRE(g.hive({x + dx, y, z + dz}));
                        CHECK(g.hive({x + dx, y, z + dz})->count >= 1);
                    }
        // (clear the tree away for the next try)
        for (int y = 64; y < 74; ++y)
            for (int dz = -2; dz <= 2; ++dz)
                for (int dx = -2; dx <= 2; ++dx) g.world.setBlock({x + dx, y, z + dz}, 0);
    }
    CHECK(nests > 0);
}
