// Frogs, tadpoles, frogspawn, froglights, axolotls (M26.3c).
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"

#include <doctest/doctest.h>

#include <functional>

using namespace mc;
using namespace mc::world;

namespace {

// Grass at y 63 with a pond (water y 61-63) at x 4..9, z 4..9.
struct Pond {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{71};
    ItemEntities items;
    Mobs mobs;
    Pond() {
        const auto& r = blockRegistry();
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 60, z, r.defaultState(blocks::Stone));
                        for (int y = 61; y <= 63; ++y) c.set(x, y, z, r.defaultState(blocks::Dirt));
                        c.set(x, 63, z, r.defaultState(blocks::GrassBlock));
                    }
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        for (int z = 4; z <= 9; ++z)
            for (int x = 4; x <= 9; ++x)
                for (int y = 61; y <= 63; ++y) world.setBlock({x, y, z}, r.defaultState(blocks::Water));
        player.setPosition({0.5, 64.0, 30.5});
        player.setCreative(false);
    }
    void tick(int n, const std::function<void()>& each = {}) {
        for (int i = 0; i < n; ++i) {
            if (each) each();
            Mobs::Context ctx{world, player, vitals, true, false, 6000, 0.0f, rng, items};
            ctx.naturalSpawning = false;
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
    bool dropped(const char* name) const {
        const ItemId id = *itemRegistry().find(name);
        for (const auto& it : items.items())
            if (it.stack.item == id) return true;
        return false;
    }
};

} // namespace

TEST_CASE("frogspawn sits only on water and hatches tadpoles; a tadpole grows into a frog of its biome (M26.3c)") {
    Pond p;
    const auto& r = blockRegistry();
    BlockUpdates updates(p.world);
    CHECK_FALSE(BlockUpdates::placement(p.world, r.defaultState(blocks::Frogspawn), {2, 64, 2}, Direction::Up, 0.0f, 0.0f).has_value());
    CHECK(BlockUpdates::placement(p.world, r.defaultState(blocks::Frogspawn), {5, 64, 5}, Direction::Up, 0.0f, 0.0f).has_value());
    p.world.setBlock({5, 64, 5}, r.defaultState(blocks::Frogspawn));
    updates.setRandomTicks({0, 0}, 1, 1000);
    for (int t = 0; t < 600 && updates.hatched().empty(); ++t) {
        updates.setTime(t);
        updates.tick();
    }
    REQUIRE(!updates.hatched().empty());
    CHECK(updates.hatched()[0].type == MobType::Tadpole);
    CHECK(updates.hatched()[0].count >= 2);
    CHECK(updates.hatched()[0].count <= 6);
    CHECK(p.world.getBlock({5, 64, 5}) == 0);
    // A tadpole about to grow up: a temperate frog here (our test chunks are plains).
    MobData tad = Mobs::make(MobType::Tadpole, {6.5, 62.0, 6.5}, p.rng);
    CHECK(tad.age == -24000);
    tad.age = -2;
    REQUIRE(Mobs::add(p.world, tad));
    p.tick(3);
    REQUIRE(p.find(MobType::Frog));
    CHECK(p.find(MobType::Frog)->woolColour == 0);
    CHECK(p.find(MobType::Tadpole) == nullptr);
}

TEST_CASE("frogs eat small magma cubes (a froglight of their kind) and slimes (a slime ball); a frog with spawn lays it on water (M26.3c)") {
    Pond p;
    MobData frog = Mobs::make(MobType::Frog, {1.5, 64.0, 1.5}, p.rng);
    frog.woolColour = 1; // warm: pearlescent
    REQUIRE(Mobs::add(p.world, frog));
    MobData cube = Mobs::make(MobType::MagmaCube, {1.5, 64.0, 3.5}, p.rng);
    cube.size = 1;
    cube.health = 1.0f;
    REQUIRE(Mobs::add(p.world, cube));
    p.tick(200);
    CHECK(p.find(MobType::MagmaCube) == nullptr);
    CHECK(p.dropped("pearlescent_froglight"));
    MobData slime = Mobs::make(MobType::Slime, {1.5, 64.0, 3.5}, p.rng);
    slime.size = 1;
    slime.health = 1.0f;
    REQUIRE(Mobs::add(p.world, slime));
    // (back beside it: frogs more than 32 blocks from a player stop wandering)
    MobData* f = p.find(MobType::Frog);
    f->pos = f->prevPos = f->goal = {1.5, 64.0, 1.5};
    p.tick(200);
    CHECK(p.dropped("slime_ball"));
    // Carrying spawn: it goes to the pond and lays it on the water.
    f = p.find(MobType::Frog);
    f->pos = f->prevPos = f->goal = {2.5, 64.0, 2.5};
    f->hasEgg = true;
    int spawn = 0;
    p.tick(600, [&] {
        spawn = 0;
        for (int z = 3; z <= 10; ++z)
            for (int x = 3; x <= 10; ++x) spawn += blockRegistry().blockOf(p.world.getBlock({x, 64, z})) == blocks::Frogspawn;
    });
    CHECK(spawn == 1);
    CHECK_FALSE(p.find(MobType::Frog)->hasEgg);
}

TEST_CASE("axolotls hunt fish in the water, play dead when hurt, and last 5 minutes on land (M26.3c)") {
    Pond p;
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Axolotl, {5.5, 62.0, 5.5}, p.rng)));
    MobData cod = Mobs::make(MobType::Cod, {8.5, 62.0, 8.5}, p.rng);
    REQUIRE(Mobs::add(p.world, cod));
    p.tick(400);
    CHECK((p.find(MobType::Cod) == nullptr || p.find(MobType::Cod)->health < 3.0f));
    // Hurt: now and then it lies still and heals.
    bool playedDead = false;
    for (int k = 0; k < 20 && !playedDead; ++k) {
        MobData* a = p.find(MobType::Axolotl);
        REQUIRE(a);
        a->hurtTime = 0;
        Mobs::attack(*a, 1.0f, p.player.position());
        p.tick(1);
        playedDead = p.find(MobType::Axolotl)->spellTicks > 0;
        p.tick(12);
    }
    CHECK(playedDead);
    // On land it dries out only after 5 minutes.
    Pond q;
    MobData dry = Mobs::make(MobType::Axolotl, {1.5, 64.0, 1.5}, q.rng);
    REQUIRE(Mobs::add(q.world, dry));
    q.tick(1000);
    REQUIRE(q.find(MobType::Axolotl));
    CHECK(q.find(MobType::Axolotl)->health == doctest::Approx(14.0f));
}

TEST_CASE("frogs and axolotls save their kinds; tadpoles and axolotls go into buckets (M26.3c)") {
    Pond p;
    Chunk c({0, 0});
    MobData frog = Mobs::make(MobType::Frog, {1.5, 64, 1.5}, p.rng);
    frog.woolColour = 2;
    MobData axo = Mobs::make(MobType::Axolotl, {2.5, 64, 1.5}, p.rng);
    axo.woolColour = 4;
    axo.fromBucket = true;
    c.mobs().push_back(frog);
    c.mobs().push_back(axo);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 2);
    CHECK(back.mobs()[0].woolColour == 2);
    CHECK(back.mobs()[1].woolColour == 4);
    CHECK(back.mobs()[1].fromBucket);
    CHECK(Mobs::isFood(MobType::Axolotl, *itemRegistry().find("tropical_fish_bucket")));
    CHECK(Mobs::isFood(MobType::Frog, *itemRegistry().find("slime_ball")));
}
