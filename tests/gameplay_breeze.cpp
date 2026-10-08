// The breeze and wind charges (M26.4c).
#include "gameplay/Inventory.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Projectiles.h"
#include "gameplay/Recipes.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

#include <functional>

using namespace mc;
using namespace mc::world;

namespace {

struct Chamber {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{101};
    ItemEntities items;
    Projectiles projectiles;
    Inventory inventory;
    Mobs mobs;
    Chamber() {
        const auto& r = blockRegistry();
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 63, z, r.defaultState(blocks::Stone));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 8.5});
        player.setCreative(false);
    }
    void tick(int n, const std::function<void()>& each = {}) {
        for (int i = 0; i < n; ++i) {
            if (each) each();
            player.tick(world, {});
            Mobs::Context ctx{world, player, vitals, true, false, 18000, 11.0f, rng, items};
            ctx.naturalSpawning = false;
            ctx.projectiles = &projectiles;
            mobs.tick(ctx);
            projectiles.tick(world, player, &vitals, inventory, true, rng);
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
};

} // namespace

TEST_CASE("a breeze shoots wind charges at the player, leaps about and takes no fall damage (M26.4c)") {
    Chamber c;
    REQUIRE(Mobs::add(c.world, Mobs::make(MobType::Breeze, {0.5, 64.0, 0.5}, c.rng)));
    int bursts = 0;
    double highest = 64.0;
    c.tick(400, [&] {
        bursts += int(c.projectiles.windBursts().size());
        if (MobData* b = c.find(MobType::Breeze)) highest = std::max(highest, b->pos.y);
    });
    CHECK(bursts > 0);
    CHECK(highest > 65.0); // (a leap)
    CHECK(c.find(MobType::Breeze)->health == doctest::Approx(30.0f)); // (no fall damage from its leaps)
    CHECK(c.vitals.health() < Vitals::kMaxHealth); // (its charges hit for 1)
}

TEST_CASE("a thrown wind charge bursts where it hits, without harming its thrower; breeze rods make 4 (M26.4c)") {
    Chamber c;
    c.inventory.setSlot(0, {*itemRegistry().find("wind_charge"), 3});
    const glm::dvec3 eye = c.player.eyePosition(1.0);
    throwWindCharge(c.inventory, true, eye, {0.0, -1.0, 0.0}, c.projectiles, c.rng); // (straight down)
    CHECK(c.inventory.slot(0).count == 2);
    bool burst = false;
    const float hp = c.vitals.health();
    c.tick(20, [&] {
        for (const auto& b : c.projectiles.windBursts()) burst = burst || (b.fromPlayer && b.pos.y < 64.5);
    });
    CHECK(burst);
    CHECK(c.vitals.health() == doctest::Approx(hp));
    // Breeze rods: 4 wind charges each.
    bool recipe = false;
    for (const Recipe& r : craftingRecipes())
        recipe = recipe || (r.result.item == *itemRegistry().find("wind_charge") && r.result.count == 4);
    CHECK(recipe);
    // A breeze killed by the player drops 1-2 breeze rods.
    MobData b = Mobs::make(MobType::Breeze, {3.5, 64.0, 3.5}, c.rng);
    b.health = 0.0f;
    b.lastHurtByPlayer = true;
    REQUIRE(Mobs::add(c.world, b));
    c.tick(2);
    int rods = 0;
    for (const auto& it : c.items.items())
        if (it.stack.item == *itemRegistry().find("breeze_rod")) rods += it.stack.count;
    CHECK(rods >= 1);
}
