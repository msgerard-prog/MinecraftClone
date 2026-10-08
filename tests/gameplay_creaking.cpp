// The creaking, creaking hearts, eyeblossoms and resin (M27.1c).
#include "gameplay/ItemEntities.h"
#include "gameplay/Mining.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Recipes.h"
#include "gameplay/Vitals.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/OverworldGenerator.h"

#include <doctest/doctest.h>

#include <functional>

using namespace mc;
using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }

struct Garden {
    World world;
    BlockUpdates updates{world};
    Player player;
    Vitals vitals;
    Xoroshiro rng{131};
    ItemEntities items;
    Mobs mobs;
    int64_t dayTime = 18000; // midnight
    const BlockPos heart{0, 67, 0};
    Garden() {
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 63, z, R().defaultState(blocks::GrassBlock));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        // A pale oak trunk with a natural heart in it.
        for (int y = 64; y <= 70; ++y) world.setBlock({0, y, 0}, R().defaultState(blocks::PaleOakLog));
        world.setBlock(heart, R().set(R().defaultState(blocks::CreakingHeart), properties::natural, 0));
        player.setPosition({0.5, 64.0, 12.5});
        player.setCreative(false);
        updates.setDayTime(dayTime);
    }
    void tick(int n, const std::function<void()>& each = {}) {
        for (int i = 0; i < n; ++i) {
            if (each) each();
            Mobs::Context ctx{world, player, vitals, true, false, dayTime, 0.0f, rng, items};
            ctx.naturalSpawning = false;
            mobs.tick(ctx);
        }
    }
    MobData* creaking() {
        MobData* out = nullptr;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                if (m.type == MobType::Creaking && m.health > 0.0f && !out) out = &m;
        });
        return out;
    }
    int count(BlockId b) {
        int n = 0;
        for (int y = 60; y < 75; ++y)
            for (int z = -4; z <= 4; ++z)
                for (int x = -4; x <= 4; ++x) n += R().blockOf(world.getBlock({x, y, z})) == b;
        return n;
    }
};

} // namespace

TEST_CASE("a creaking heart between pale oak logs wakes at night, sleeps by day, and is uprooted alone (M27.1c)") {
    Garden g;
    const BlockStateId s = g.world.getBlock(g.heart);
    CHECK(g.updates.heartState(g.heart, s) == 2); // awake at midnight
    g.updates.setDayTime(6000);
    CHECK(g.updates.heartState(g.heart, s) == 1); // dormant at noon
    g.world.setBlock({0, 68, 0}, 0);
    CHECK(g.updates.heartState(g.heart, s) == 0); // uprooted without a log above
    // Awake and natural, a random tick asks for its creaking.
    g.world.setBlock({0, 68, 0}, R().defaultState(blocks::PaleOakLog));
    g.updates.setDayTime(18000);
    g.updates.setRandomTicks({0, 0}, 1, 1000);
    for (int t = 0; t < 2000 && g.updates.hatched().empty(); ++t) {
        g.updates.setTime(t);
        g.updates.tick();
    }
    REQUIRE_FALSE(g.updates.hatched().empty());
    CHECK(g.updates.hatched()[0].type == MobType::Creaking);
    CHECK(R().get(g.world.getBlock(g.heart), properties::creakingState) == 2);
}

TEST_CASE("an awake heart calls one creaking near it when a player is close (M27.1c)") {
    Garden g;
    REQUIRE(Mobs::spawnCreaking(g.world, g.heart, g.player.position(), g.rng));
    MobData* c = g.creaking();
    REQUIRE(c);
    CHECK(c->home == glm::ivec3(g.heart.x, g.heart.y, g.heart.z));
    CHECK(std::abs(c->pos.x) <= 16.5);
    CHECK_FALSE(Mobs::spawnCreaking(g.world, g.heart, g.player.position(), g.rng)); // (its own is out)
    CHECK_FALSE(Mobs::spawnCreaking(g.world, g.heart, {100.0, 64.0, 100.0}, g.rng));
}

TEST_CASE("a creaking freezes while watched and hunts when not; only its heart can end it (M27.1c)") {
    Garden g;
    MobData m = Mobs::make(MobType::Creaking, {0.5, 64.0, 4.5}, g.rng);
    m.home = {g.heart.x, g.heart.y, g.heart.z};
    REQUIRE(Mobs::add(g.world, m));
    // The player at z 12.5 looks north (yaw 180) straight at it: it doesn't move.
    g.player.setRotation(180.0f, 10.0f);
    const glm::dvec3 start = g.creaking()->pos;
    g.tick(60);
    CHECK(glm::length(g.creaking()->pos - start) < 0.2);
    // Looking away (south), it closes in.
    g.player.setRotation(0.0f, 0.0f);
    g.tick(60);
    CHECK(g.creaking()->pos.z > start.z + 2.0);
    // Hits don't hurt it, but grow resin on the heart's tree.
    MobData* c = g.creaking();
    for (int i = 0; i < 5; ++i) {
        Mobs::attack(*c, 20.0f, g.player.position());
        g.tick(11);
        c = g.creaking();
        REQUIRE(c);
    }
    CHECK(c->health > 0.0f);
    CHECK(g.count(blocks::ResinClump) > 0);
    // The heart broken: it crumbles.
    g.world.setBlock(g.heart, 0);
    g.tick(2);
    CHECK(g.creaking() == nullptr);
}

TEST_CASE("creakings crumble at daybreak; a heartless creaking is an ordinary mob (M27.1c)") {
    Garden g;
    MobData m = Mobs::make(MobType::Creaking, {4.5, 64.0, 0.5}, g.rng);
    m.home = {g.heart.x, g.heart.y, g.heart.z};
    REQUIRE(Mobs::add(g.world, m));
    g.dayTime = 1000;
    g.tick(2);
    CHECK(g.creaking() == nullptr);
    MobData lone = Mobs::make(MobType::Creaking, {4.5, 64.0, 0.5}, g.rng);
    REQUIRE(Mobs::add(g.world, lone));
    g.tick(1);
    REQUIRE(g.creaking());
    Mobs::attack(*g.creaking(), 2.0f, g.player.position());
    g.tick(25);
    CHECK(g.creaking() == nullptr);
}

TEST_CASE("eyeblossoms open at night and close by day; hearts drop resin; resin crafts and smelts (M27.1c)") {
    Garden g;
    g.world.setBlock({3, 64, 3}, R().defaultState(blocks::ClosedEyeblossom));
    g.updates.setRandomTicks({0, 0}, 1, 1000);
    auto runUntil = [&](BlockId want) {
        for (int t = 0; t < 3000 && R().blockOf(g.world.getBlock({3, 64, 3})) != want; ++t) {
            g.updates.setTime(t);
            g.updates.tick();
        }
        return R().blockOf(g.world.getBlock({3, 64, 3})) == want;
    };
    CHECK(runUntil(blocks::OpenEyeblossom));
    g.updates.setDayTime(6000);
    CHECK(runUntil(blocks::ClosedEyeblossom));
    std::vector<ItemStack> out;
    blockDrops(R().defaultState(blocks::CreakingHeart), {}, g.rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].item == itemRegistry().blockItem(blocks::ResinClump));
    const auto brick = smelt({itemRegistry().blockItem(blocks::ResinClump), 1});
    REQUIRE(brick);
    CHECK(brick->item == *itemRegistry().find("resin_brick"));
    bool bricks = false;
    for (const Recipe& r : craftingRecipes())
        bricks = bricks || r.result.item == itemRegistry().blockItem(*R().findBlock("resin_bricks"));
    CHECK(bricks);
}

TEST_CASE("a creaking keeps its heart through a save; pale gardens hide hearts and eyeblossoms (M27.1c)") {
    Chunk c({0, 0});
    Xoroshiro rng(5);
    MobData m = Mobs::make(MobType::Creaking, {1.5, 65.0, 1.5}, rng);
    m.home = {3, 70, -2};
    c.mobs().push_back(m);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 1);
    CHECK(back.mobs()[0].home == glm::ivec3(3, 70, -2));
    // overworld6: hearts in the trunks of pale garden oaks, closed eyeblossoms on the ground.
    const OverworldGenerator gen(42);
    int hearts = 0, blossoms = 0;
    for (int cz = 6; cz <= 16 && (hearts == 0 || blossoms == 0); ++cz)
        for (int cx = -4; cx <= 8; ++cx) {
            if (gen.biomeAt(gen.column(cx * 16 + 8, cz * 16 + 8)) != Biome::PaleGarden) continue;
            Chunk ch({cx, cz});
            gen.generate(ch);
            for (int y = 60; y < 110; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockId b = R().blockOf(ch.get(x, y, z));
                        hearts += b == blocks::CreakingHeart;
                        blossoms += b == blocks::ClosedEyeblossom;
                    }
        }
    CHECK(hearts > 0);
    CHECK(blossoms > 0);
}

TEST_CASE("a heart-bound creaking survives damage from any source - an arrow, a blast (M27 review regression)") {
    Garden g;
    MobData m = Mobs::make(MobType::Creaking, {4.5, 64.0, 4.5}, g.rng);
    m.home = {g.heart.x, g.heart.y, g.heart.z};
    REQUIRE(Mobs::add(g.world, m));
    g.player.setRotation(0.0f, 0.0f);
    g.tick(2);
    g.creaking()->health -= 50.0f; // (as Projectiles/Explosion write it)
    g.tick(2);
    REQUIRE(g.creaking());
    CHECK(g.creaking()->health > 0.0f);
    // A heartless one dies of it.
    g.world.setBlock(g.heart, 0);
    g.tick(25);
    CHECK(g.creaking() == nullptr);
}

TEST_CASE("a trial spawner's mobs keep it through a save; wardens keep their phase and anger (M27 review regression)") {
    Chunk c({0, 0});
    Xoroshiro rng(9);
    MobData z = Mobs::make(MobType::Zombie, {1.5, 65.0, 1.5}, rng);
    z.home = {3, 60, 4};
    c.mobs().push_back(z);
    MobData w = Mobs::make(MobType::Warden, {5.5, 65.0, 5.5}, rng);
    w.phase = 2;
    w.phaseTicks = 40;
    w.angerTicks = 90;
    c.mobs().push_back(w);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 2);
    CHECK(back.mobs()[0].home == glm::ivec3(3, 60, 4));
    CHECK(back.mobs()[1].phase == 2);
    CHECK(back.mobs()[1].phaseTicks == 40);
    CHECK(back.mobs()[1].angerTicks == 90);
}
