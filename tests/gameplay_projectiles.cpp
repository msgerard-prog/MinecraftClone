// Projectiles and explosions (wiki: Arrow, Bow, Egg, Explosion).
#include "gameplay/Explosion.h"
#include "gameplay/Mobs.h"
#include "gameplay/Projectiles.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }

// 5x5 chunks with a 4-deep stone floor (y 60..63) on bedrock (y 59).
struct Scene {
    World world;
    Player player;
    Vitals vitals;
    Inventory inventory;
    ItemEntities items;
    Projectiles projectiles;
    Xoroshiro rng{4};
    Scene() {
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 59, z, S(blocks::Bedrock));
                        for (int y = 60; y <= 63; ++y)
                            c.set(x, y, z, S(blocks::Stone));
                    }
            }
        player.setPosition({0.5, 64.0, 0.5});
        player.setCreative(false);
        for (int i = 0; i < Inventory::kSlots; ++i)
            inventory.setSlot(i, {});
    }
    Projectiles::Hits tick(int n) {
        Projectiles::Hits total;
        for (int i = 0; i < n; ++i) {
            const auto h = projectiles.tick(world, player, &vitals, inventory, true, rng);
            total.playerDamage += h.playerDamage;
            total.mobsHit += h.mobsHit;
        }
        return total;
    }
};

} // namespace

TEST_CASE("bow power: (f^2 + 2f)/3 with f = ticks/20, full after a second") {
    CHECK(bowPower(0) == 0.0f);
    CHECK(bowPower(10) == doctest::Approx(0.41667f));
    CHECK(bowPower(20) == 1.0f);
    CHECK(bowPower(60) == 1.0f);
}

TEST_CASE("a fully drawn arrow hits a mob for ceil(speed x 2), at least 6") {
    Scene s;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {0.5, 64.0, 8.5}, s.rng)));
    s.projectiles.shoot(ProjectileKind::Arrow, {0.5, 65.0, 0.5}, {0, 0, 1}, 3.0, 0.0, true, false, s.rng);
    const auto hits = s.tick(10);
    CHECK(hits.mobsHit == 1);
    const MobData& cow = s.world.chunk({0, 0})->mobs().at(0);
    CHECK(cow.health <= 10.0f - 6.0f);
    CHECK(s.projectiles.items().empty());
}

TEST_CASE("arrows stick in walls; the shooter picks them back up") {
    Scene s;
    for (int y = 64; y <= 66; ++y)
        s.world.setBlock({0, y, 6}, S(blocks::Stone));
    s.projectiles.shoot(ProjectileKind::Arrow, {0.5, 65.0, 0.5}, {0, 0, 1}, 2.0, 0.0, true, false, s.rng);
    s.tick(10);
    REQUIRE(s.projectiles.items().size() == 1);
    CHECK(s.projectiles.items()[0].stuck);
    CHECK(s.projectiles.items()[0].pos.z == doctest::Approx(6.05).epsilon(0.01));
    s.player.setPosition({0.5, 64.0, 5.2}); // walk up to it
    s.tick(1);
    CHECK(s.projectiles.items().empty());
    CHECK(s.inventory.has(*itemRegistry().find("arrow")));
}

TEST_CASE("arrows fall: a horizontal shot drops over distance") {
    Scene s;
    s.projectiles.shoot(ProjectileKind::Arrow, {0.5, 70.0, 0.5}, {0, 0, 1}, 1.0, 0.0, false, false, s.rng);
    s.tick(10);
    REQUIRE(s.projectiles.items().size() == 1);
    CHECK(s.projectiles.items()[0].pos.y < 69.0);
}

TEST_CASE("a skeleton-style arrow hurts a survival player") {
    Scene s;
    s.projectiles.shoot(ProjectileKind::Arrow, {0.5, 65.0, 8.5}, {0, 0, -1}, 1.6, 0.0, false, false, s.rng);
    const auto hits = s.tick(10);
    CHECK(hits.playerDamage >= 3.0f);
    CHECK(s.vitals.health() < 20.0f);
}

TEST_CASE("thrown eggs break, now and then hatching chicks") {
    Scene s;
    for (int i = 0; i < 64; ++i)
        s.projectiles.shoot(ProjectileKind::Egg, {0.5, 66.0, 0.5}, {0.2, 0.3, 1}, 1.5, 1.0, true, false, s.rng);
    s.tick(60);
    CHECK(s.projectiles.items().empty());
    int chicks = 0;
    s.world.forEachChunk([&](const Chunk& c) {
        for (const MobData& m : c.mobs())
            chicks += m.type == MobType::Chicken && m.isBaby();
    });
    CHECK(chicks > 2);  // ~1 in 8
    CHECK(chicks < 25);
}

TEST_CASE("explosions: a crater in dirt (stone barely: resistance 6), never bedrock; damage by distance and cover") {
    Scene s;
    for (int x = -8; x <= 8; ++x)
        for (int z = -8; z <= 8; ++z)
            for (int y = 60; y <= 63; ++y)
                s.world.setBlock({x, y, z}, S(blocks::Dirt));
    Explosion e;
    std::vector<BlockPos> changed;
    s.player.setPosition({3.5, 64.0, 0.5});
    const int n = e.explode(s.world, {0.5, 64.0, 0.5}, 3.0f, s.rng, s.items, changed, {&s.player, &s.vitals});
    CHECK(n > 10);
    CHECK(s.world.getBlock({0, 63, 0}) == 0); // the block under it is gone
    for (int x = -6; x <= 6; ++x)
        for (int z = -6; z <= 6; ++z)
            CHECK(s.world.getBlock({x, 59, z}) == S(blocks::Bedrock));
    const float hurt = 20.0f - s.vitals.health();
    CHECK(hurt >= 3.0f); // 3 blocks from a power-3 blast (the floor shields part of the box)
    // Behind a wall there is no exposure.
    World w;
    w.createChunk({0, 0});
    for (int y = 0; y <= 80; ++y)
        for (int z = 0; z < 16; ++z)
            w.setBlock({8, y, z}, S(blocks::Stone));
    CHECK(Explosion::exposure(w, {4.5, 64.0, 8.5}, Aabb::fromFeet({12.5, 64.0, 8.5}, 0.6, 1.8)) == 0.0);
    CHECK(Explosion::exposure(w, {4.5, 64.0, 8.5}, Aabb::fromFeet({6.5, 64.0, 8.5}, 0.6, 1.8)) == 1.0);
}

TEST_CASE("water shields blocks from explosions (blast resistance 100)") {
    Scene s;
    for (int x = -3; x <= 3; ++x)
        for (int z = -3; z <= 3; ++z)
            for (int y = 64; y <= 65; ++y)
                s.world.setBlock({x, y, z}, S(blocks::Water));
    Explosion e;
    std::vector<BlockPos> changed;
    e.explode(s.world, {0.5, 64.5, 0.5}, 3.0f, s.rng, s.items, changed, {});
    CHECK(s.world.getBlock({0, 63, 0}) == S(blocks::Stone));
}

TEST_CASE("blown-up stone drops cobblestone (no tool needed), at about 1/power") {
    Scene s;
    Explosion e;
    std::vector<BlockPos> changed;
    int destroyed = 0;
    for (int i = 0; i < 6; ++i) // several blasts in the stone floor at different places
        destroyed += e.explode(s.world, {i * 8.0 - 20.0, 63.5, 0.5}, 4.0f, s.rng, s.items, changed, {});
    REQUIRE(destroyed > 8);
    int cobble = 0;
    for (const auto& it : s.items.items())
        cobble += it.stack.item == itemRegistry().blockItem(blocks::Cobblestone) ? it.stack.count : 0;
    CHECK(cobble > 0);
    CHECK(cobble < destroyed);
}

TEST_CASE("cover the blast destroys still shields what is behind it (entities first)") {
    Scene open, covered;
    for (Scene* s : {&open, &covered})
        for (int x = -8; x <= 8; ++x)
            for (int z = -8; z <= 8; ++z)
                for (int y = 60; y <= 63; ++y)
                    s->world.setBlock({x, y, z}, S(blocks::Dirt));
    for (int y = 64; y <= 67; ++y) // a dirt wall between blast and player, inside the crater's reach
        for (int z = -3; z <= 3; ++z)
            covered.world.setBlock({2, y, z}, S(blocks::Dirt));
    Explosion e;
    std::vector<BlockPos> changed;
    open.player.setPosition({3.5, 64.0, 0.5});
    covered.player.setPosition({3.5, 64.0, 0.5});
    e.explode(open.world, {0.5, 64.0, 0.5}, 3.0f, open.rng, open.items, changed, {&open.player, &open.vitals});
    e.explode(covered.world, {0.5, 64.0, 0.5}, 3.0f, covered.rng, covered.items, changed,
              {&covered.player, &covered.vitals});
    CHECK(covered.vitals.health() > open.vitals.health());
}

TEST_CASE("a mob's own arrow never hits it, even shot steeply down") {
    Scene s;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Skeleton, {0.5, 70.0, 0.5}, s.rng)));
    MobData& sk = s.world.chunk({0, 0})->mobs().at(0);
    s.projectiles.shoot(ProjectileKind::Arrow, {0.6, 71.0, 0.6}, {0.29, -0.96, 0.0}, 1.6, 0.0, false, false, s.rng,
                        sk.uuidHi);
    s.tick(5);
    CHECK(s.world.chunk({0, 0})->mobs().at(0).health == 20.0f);
}

TEST_CASE("bows: drawing needs an arrow in survival; release uses it and wears the bow") {
    Scene s;
    const ItemId bow = *itemRegistry().find("bow"), arrow = *itemRegistry().find("arrow");
    s.inventory.select(0);
    s.inventory.setSlot(0, {bow, 1});
    CHECK_FALSE(canDrawBow(s.inventory, true));
    CHECK(canDrawBow(s.inventory, false));
    s.inventory.setSlot(5, {arrow, 2});
    CHECK(canDrawBow(s.inventory, true));
    CHECK_FALSE(releaseBow(s.inventory, 1, true, {0.5, 65, 0.5}, {0, 0, 1}, s.projectiles, s.rng)); // too short
    CHECK(releaseBow(s.inventory, 20, true, {0.5, 65, 0.5}, {0, 0, 1}, s.projectiles, s.rng));
    CHECK(s.inventory.slot(5).count == 1);
    CHECK(s.inventory.slot(0).damage == 1);
    CHECK(s.projectiles.items().size() == 1);
    CHECK(s.projectiles.items()[0].critical);
    s.inventory.setSlot(0, {bow, 1, 383}); // one use left
    CHECK(releaseBow(s.inventory, 20, true, {0.5, 65, 0.5}, {0, 0, 1}, s.projectiles, s.rng));
    CHECK(s.inventory.slot(0).empty()); // broke
}

TEST_CASE("eyes of ender fly 12 blocks toward a far stronghold and up; near one, down; then drop or shatter") {
    int drops = 0;
    for (int trial = 0; trial < 50; ++trial) {
        Scene s;
        s.rng = Xoroshiro(uint64_t(trial) + 1);
        s.inventory.setSlot(0, {*itemRegistry().find("ender_eye"), 2});
        const glm::dvec3 eye(0.5, 65.6, 0.5);
        throwEye(s.inventory, true, eye, {1000, 0}, s.projectiles);
        CHECK(s.inventory.slot(0).count == 1); // survival uses it
        s.tick(40);
        REQUIRE(s.projectiles.items().size() == 1);
        const Projectile& p = s.projectiles.items()[0];
        CHECK(p.pos.x > 8.0); // toward +x
        CHECK(p.pos.x < 12.6);
        CHECK(p.pos.y > eye.y + 5.0); // climbing: the stronghold is far
        s.tick(40);
        CHECK(s.projectiles.items().empty());
        drops += static_cast<int>(s.projectiles.eyeDrops().size());
    }
    CHECK(drops > 30); // 80% come down as an item
    CHECK(drops < 50);
    Scene near;
    throwEye(near.inventory, false, {0.5, 65.6, 0.5}, {5, 0}, near.projectiles);
    near.tick(40);
    CHECK(near.projectiles.items()[0].pos.y < 65.6); // over the stronghold: it sinks
}

TEST_CASE("M29.5: an arrow in a target's centre gives 15 for 20 ticks; off-centre less") {
    Scene s;
    s.world.setBlock({0, 65, 6}, S(blocks::Target));
    s.projectiles.shoot(ProjectileKind::Arrow, {0.5, 65.5, 0.5}, {0, 0, 1}, 2.0, 0.0, true, false, s.rng);
    std::optional<Projectiles::TargetHit> hit;
    for (int i = 0; i < 10 && !hit; ++i) {
        s.projectiles.tick(s.world, s.player, &s.vitals, s.inventory, true, s.rng);
        if (!s.projectiles.targetHits().empty()) hit = s.projectiles.targetHits()[0];
    }
    REQUIRE(hit);
    CHECK(hit->block == BlockPos{0, 65, 6});
    CHECK(hit->strength >= 10); // (gravity pulls it about 0.1 below the centre)
    CHECK(hit->ticks == 20);
    CHECK(BlockUpdates::targetStrength({0.5, 65.5, 6.0}, Direction::North) == 15);
    CHECK(BlockUpdates::targetStrength({0.95, 65.5, 6.0}, Direction::North) == 2);
    // The block keeps the power until its tick.
    BlockUpdates u(s.world);
    s.world.setListener(&u);
    int64_t t = 0;
    u.setTime(t);
    u.hitTarget(hit->block, hit->strength, hit->ticks);
    CHECK(blockRegistry().get(s.world.getBlock({0, 65, 6}), properties::power) == hit->strength);
    for (int i = 0; i < 21; ++i) {
        u.setTime(++t);
        u.tick();
    }
    CHECK(blockRegistry().get(s.world.getBlock({0, 65, 6}), properties::power) == 0);
}

TEST_CASE("M29 review: a blast drops what chiseled bookshelves and shelves hold") {
    Scene s;
    s.world.setBlock({0, 64, 0}, S(blocks::ChiseledBookshelf));
    s.world.setBlock({1, 64, 0}, S(blocks::Shelf + 3));
    const uint16_t book = uint16_t(*itemRegistry().find("book"));
    const uint16_t apple = uint16_t(*itemRegistry().find("apple"));
    REQUIRE(s.world.chunk({0, 0})->chest(0, 64, 0));
    REQUIRE(s.world.chunk({0, 0})->chest(1, 64, 0));
    s.world.chunk({0, 0})->chest(0, 64, 0)->items[4] = {book, 1};
    s.world.chunk({0, 0})->chest(1, 64, 0)->items[1] = {apple, 5};
    Explosion e;
    std::vector<BlockPos> changed;
    ExplosionTargets t;
    t.dropAll = true;
    e.explode(s.world, {0.5, 64.5, 0.5}, 4.0f, s.rng, s.items, changed, t);
    REQUIRE(s.world.getBlock({0, 64, 0}) == 0);
    REQUIRE(s.world.getBlock({1, 64, 0}) == 0);
    int books = 0, apples = 0;
    for (const auto& it : s.items.items()) {
        books += it.stack.item == book ? it.stack.count : 0;
        apples += it.stack.item == apple ? it.stack.count : 0;
    }
    CHECK(books == 1);
    CHECK(apples == 5);
}
