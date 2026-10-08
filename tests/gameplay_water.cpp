// Water mobs, fish buckets and fishing (M25.2).
#include "gameplay/Buckets.h"
#include "gameplay/Fishing.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/Enchantments.h"
#include "world/Potions.h"

#include <doctest/doctest.h>

#include <map>

using namespace mc;
using namespace mc::world;

namespace {

// A pool: stone floor at y 63, water y 64..70, over 5x5 chunks; the player far off.
struct Pool {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{21};
    ItemEntities items;
    Mobs mobs;
    Pool() {
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
                        for (int y = 64; y <= 70; ++y) c.set(x, y, z, blockRegistry().defaultState(blocks::Water));
                    }
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({30.5, 71.0, 30.5});
        player.setCreative(true);
    }
    void tick(int n, bool survival = false) {
        for (int i = 0; i < n; ++i) {
            player.tick(world, {});
            Mobs::Context ctx{world, player, vitals, survival, false, 6000, 0.0f, rng, items};
            ctx.naturalSpawning = false;
            mobs.tick(ctx);
        }
    }
    std::vector<MobData*> all() {
        std::vector<MobData*> out;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs()) out.push_back(&m);
        });
        return out;
    }
};

} // namespace

TEST_CASE("fish swim about in the water without leaving it; out of water they flop and suffocate (M25.2)") {
    Pool p;
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Cod, {0.5, 66.0, 0.5}, p.rng)));
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Squid, {4.5, 66.0, 4.5}, p.rng)));
    p.tick(400);
    double moved = 0.0;
    for (MobData* m : p.all()) {
        CHECK(m->pos.y > 63.9);
        CHECK(m->pos.y < 71.0);
        CHECK(m->health == mobInfo(m->type).maxHealth);
        moved += glm::length(m->pos - glm::dvec3(m->type == MobType::Cod ? 0.5 : 4.5, 66.0, m->type == MobType::Cod ? 0.5 : 4.5));
    }
    CHECK(moved > 1.0);
    // On dry land: after 15 s of air, 2 damage a second.
    for (int y = 64; y <= 70; ++y)
        for (int z = 2; z < 22; ++z)
            for (int x = 2; x < 22; ++x) p.world.setBlock({x, y, z}, 0);
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Salmon, {11.5, 64.0, 11.5}, p.rng)));
    p.tick(340);
    bool hurt = false;
    for (MobData* m : p.all()) hurt = hurt || (m->type == MobType::Salmon && m->health < 3.0f);
    CHECK(hurt);
}

TEST_CASE("a pufferfish puffs up near a survival player and stings with poison (M25.2)") {
    Pool p;
    p.player.setCreative(false);
    p.player.setPosition({0.5, 64.0, 0.5});
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Pufferfish, {0.5, 64.5, 1.0}, p.rng)));
    p.tick(60, true);
    int puff = 0;
    for (MobData* m : p.all()) puff = m->size;
    CHECK(puff == 2);
    CHECK(p.vitals.effectLevel(Effect::Poison) > 0);
    CHECK(p.vitals.health() < Vitals::kMaxHealth);
}

TEST_CASE("fish buckets: which fish each holds; fish save their variant, puff state and bucket origin (M25.2)") {
    CHECK(bucketFish(*itemRegistry().find("salmon_bucket")) == MobType::Salmon);
    CHECK(bucketFish(*itemRegistry().find("water_bucket")) == MobType::Count);
    CHECK(fishBucketFor(MobType::Pufferfish) == *itemRegistry().find("pufferfish_bucket"));
    CHECK(fishBucketFor(MobType::Squid) == 0); // (squid can't be bucketed)
    Chunk c({0, 0});
    Xoroshiro rng(3);
    MobData t = Mobs::make(MobType::TropicalFish, {3.5, 64.0, 3.5}, rng);
    t.fromBucket = true;
    MobData f = Mobs::make(MobType::Pufferfish, {5.5, 64.0, 3.5}, rng);
    f.size = 2;
    c.mobs().push_back(t);
    c.mobs().push_back(f);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 2);
    CHECK(back.mobs()[0].size == t.size);
    CHECK(back.mobs()[0].woolColour == t.woolColour);
    CHECK(back.mobs()[0].color2 == t.color2);
    CHECK(back.mobs()[0].fromBucket);
    CHECK(back.mobs()[1].size == 2);
}

TEST_CASE("fishing: the bobber floats, a fish bites after 5-30 s, reeling in then catches mostly fish (M25.2)") {
    Pool p;
    Fishing f;
    const glm::dvec3 eye{0.5, 73.0, 0.5};
    f.cast(eye, glm::normalize(glm::dvec3(0.0, -0.4, 1.0)), 0, 0, p.rng);
    bool bit = false;
    int t = 0;
    for (; t < 800 && !bit; ++t) {
        f.tick(p.world, eye, p.rng);
        bit = f.biting();
    }
    REQUIRE(bit);
    CHECK(t >= 100); // (5 s at least without Lure)
    CHECK(f.bobber().y > 69.0); // floating near the surface
    ExperienceOrbs orbs;
    CHECK(f.reel(p.world, eye, p.items, &orbs, p.rng) == 1); // the rod wears 1
    REQUIRE(p.items.items().size() == 1);
    CHECK(glm::length(p.items.items()[0].vel) > 0.1); // flying to the player
    // The catch table: fish 85%, the rest junk and treasure.
    std::map<std::string, int> counts;
    int fish = 0;
    for (int i = 0; i < 2000; ++i) {
        const ItemStack s = Fishing::rollCatch(0, p.rng);
        const std::string id(itemRegistry().item(s.item).id);
        fish += id == "minecraft:cod" || id == "minecraft:salmon" || id == "minecraft:pufferfish" ||
                id == "minecraft:tropical_fish";
        ++counts[id];
    }
    CHECK(fish > 1650);
    CHECK(fish < 1900);
    CHECK(counts["minecraft:cod"] > counts["minecraft:salmon"]);
    // Lure: bites 5 s sooner per level; a rod takes Lure and Luck of the Sea.
    CHECK(canEnchant(*itemRegistry().find("fishing_rod"), Enchantment::Lure));
    CHECK_FALSE(canEnchant(*itemRegistry().find("bow"), Enchantment::LuckOfTheSea));
}

TEST_CASE("boats float, paddle up to 0.4 blocks a tick, turn 1 degree a tick more, keep their wood when saved (M25.2b)") {
    Pool p;
    REQUIRE(Mobs::placeBoat(p.world, {0.5, 70.8, 0.5}, 0.0f, 7, p.rng)); // cherry
    MobData* boat = nullptr;
    auto find = [&] {
        boat = nullptr;
        for (MobData* m : p.all())
            if (m->type == MobType::Boat) boat = m;
        return boat != nullptr;
    };
    REQUIRE(find());
    p.tick(60);
    REQUIRE(find());
    CHECK(boat->pos.y > 70.0); // afloat (the water's top is y 71)
    CHECK(boat->pos.y < 71.2);
    for (int t = 0; t < 120; ++t) {
        REQUIRE(find());
        boat->paddleForward = 1;
        p.tick(1);
    }
    REQUIRE(find());
    const double speed = glm::length(glm::dvec2(boat->vel.x, boat->vel.z));
    CHECK(speed > 0.3);
    CHECK(speed < 0.41);
    CHECK(boat->vel.z > 0.0); // yaw 0: toward +Z (south)
    const float yaw0 = boat->yaw;
    for (int t = 0; t < 10; ++t) {
        REQUIRE(find());
        boat->paddleTurn = 1;
        p.tick(1);
    }
    REQUIRE(find());
    CHECK(boat->yaw - yaw0 > 20.0f); // (turning builds up: 1, 1.9, 2.7...)
    Chunk c({0, 0});
    c.mobs().push_back(*boat);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 1);
    CHECK(back.mobs()[0].type == MobType::Boat);
    CHECK(back.mobs()[0].woolColour == 7);
    CHECK(boatId(9) == "minecraft:bamboo_raft");
    CHECK(itemRegistry().find("cherry_boat").has_value());
}
