// Fireworks (M28.4c; wiki: Firework Rocket, Firework Star, Elytra › Boosting).
#include "gameplay/Fireworks.h"
#include "gameplay/Inventory.h"
#include "gameplay/Particles.h"
#include "gameplay/Player.h"
#include "gameplay/Projectiles.h"
#include "gameplay/Recipes.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/World.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {
ItemStack stack(const char* id, int n = 1) { return {*itemRegistry().find(id), uint8_t(n)}; }
}

TEST_CASE("fireworks: stars from gunpowder and dyes (+ shape, trail, twinkle), fades, rockets of 1-3 gunpowder") {
    std::array<ItemStack, 9> g{};
    g[0] = stack("gunpowder");
    g[1] = stack("red_dye");
    g[2] = stack("blue_dye");
    g[3] = stack("gold_nugget");
    g[4] = stack("diamond");
    auto star = craft(g, 3);
    REQUIRE(star);
    CHECK(itemRegistry().item(star->item).id == "minecraft:firework_star");
    auto f = fireworks(star->extra);
    REQUIRE(f);
    CHECK(f->explosions[0].shape == 2); // (gold nugget: star)
    CHECK(f->explosions[0].colours == ((1 << 14) | (1 << 11)));
    CHECK(f->explosions[0].trail);
    CHECK_FALSE(f->explosions[0].twinkle);

    std::array<ItemStack, 9> fade{};
    fade[0] = *star;
    fade[1] = stack("white_dye");
    const auto faded = craft(fade, 3);
    REQUIRE(faded);
    CHECK(fireworks(faded->extra)->explosions[0].fades == 1);

    std::array<ItemStack, 9> r{};
    r[0] = stack("paper");
    r[1] = r[2] = stack("gunpowder");
    r[3] = *faded;
    const auto rocket = craft(r, 3);
    REQUIRE(rocket);
    CHECK(rocket->count == 3);
    f = fireworks(rocket->extra);
    REQUIRE(f);
    CHECK(f->flight == 2);
    CHECK(f->count == 1);
    CHECK(rocketLifetime(2, 0, 0) == 30);
    CHECK(fireworkDamage(0, 1.0) == 0.0f);
    CHECK(fireworkDamage(1, 0.0) == doctest::Approx(7.0f));
    CHECK(fireworkDamage(1, 5.0) == 0.0f);

    // Saved as vanilla's components and read back to the same entry.
    const nbt::Compound n = itemToNbt(*rocket, 0);
    REQUIRE(n.compound("components")->compound("minecraft:fireworks"));
    CHECK(itemFromNbtPublic(n).extra == rocket->extra);
    const nbt::Compound sn = itemToNbt(*faded, 0);
    REQUIRE(sn.compound("components")->compound("minecraft:firework_explosion"));
    CHECK(itemFromNbtPublic(sn).extra == faded->extra);
}

TEST_CASE("rockets rise, burst into sparks after their flight time; an elytra boost speeds toward 1.5 a tick") {
    World world;
    world.createChunk({0, 0});
    Player player;
    player.setPosition({30.5, 64.0, 30.5});
    Vitals vitals;
    Inventory inv;
    Projectiles shots;
    Xoroshiro rng{8};
    Fireworks f;
    f.flight = 1;
    f.count = 1;
    f.explosions[0].colours = 1 << 5;
    ItemStack rocket = stack("firework_rocket");
    rocket.extra = addFireworks(f);
    REQUIRE(shots.launchFirework({8.5, 64.0, 8.5}, rocket, false, {0, 1, 0}, rng));
    bool burst = false;
    double top = 0.0;
    for (int t = 0; t < 60 && !burst; ++t) {
        shots.tick(world, player, &vitals, inv, true, rng);
        if (!shots.items().empty()) top = shots.items()[0].pos.y;
        burst = !shots.fireworkBursts().empty();
        if (burst) {
            Particles particles;
            particles.firework(shots.fireworkBursts()[0].pos, shots.fireworkBursts()[0].fireworks, rng);
            CHECK(particles.all().size() >= 70);
        }
    }
    CHECK(burst);
    CHECK(top > 70.0);

    glm::dvec3 v(0.0, -0.5, 0.0);
    for (int i = 0; i < 10; ++i) v = boostedVelocity(v, {0.0, 0.0, 1.0});
    CHECK(v.z > 1.4);
    CHECK(std::abs(v.y) < 0.05);
}

TEST_CASE("crossbows load a rocket from the offhand first and fire it straight") {
    Inventory inv;
    inv.setSlot(0, stack("crossbow"));
    inv.setSlot(3, stack("arrow", 5));
    ItemStack rocket = stack("firework_rocket", 2);
    inv.setOffhand(rocket);
    REQUIRE(loadCrossbow(inv, true));
    CHECK(inv.slot(0).state == kCrossbowFirework);
    CHECK(inv.offhand().count == 1);
    CHECK(inv.slot(3).count == 5);
    Projectiles shots;
    Xoroshiro rng{1};
    REQUIRE(fireCrossbow(inv, true, {0, 64, 0}, {0, 0, 1}, shots, rng));
    REQUIRE(shots.items().size() == 1);
    CHECK(shots.items()[0].kind == ProjectileKind::Firework);
    CHECK(shots.items()[0].straight);
}
