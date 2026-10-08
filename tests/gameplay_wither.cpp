// Mob heads and the Wither (M26.4b).
#include "gameplay/Inventory.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Projectiles.h"
#include "gameplay/Vitals.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

#include <functional>

using namespace mc;
using namespace mc::world;

namespace {

struct Arena {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{97};
    ItemEntities items;
    Projectiles projectiles;
    Inventory inventory;
    Mobs mobs;
    bool survival = true;
    Arena() {
        const auto& r = blockRegistry();
        for (int cz = -3; cz <= 3; ++cz)
            for (int cx = -3; cx <= 3; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 63, z, r.defaultState(blocks::Stone));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 20.5});
        player.setCreative(false);
    }
    void tick(int n, const std::function<void()>& each = {}) {
        for (int i = 0; i < n; ++i) {
            if (each) each();
            Mobs::Context ctx{world, player, vitals, survival, false, 18000, 11.0f, rng, items};
            ctx.naturalSpawning = false;
            ctx.projectiles = &projectiles;
            mobs.tick(ctx);
            projectiles.tick(world, player, &vitals, inventory, survival, rng);
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

TEST_CASE("mob heads: on a side the wall kind, on top turned to the player; worn on the head; one item for both (M26.4b)") {
    const auto& r = blockRegistry();
    Arena a;
    const BlockStateId zombie = r.defaultState(blocks::ZombieHead);
    a.world.setBlock({0, 64, 0}, r.defaultState(blocks::Stone));
    const auto wall = BlockUpdates::placement(a.world, zombie, {0, 64, 1}, Direction::South, 0.0f, 0.0f);
    REQUIRE(wall);
    CHECK(r.blockOf(*wall) == blocks::ZombieWallHead);
    CHECK(r.get(*wall, properties::facing) == 1); // (south)
    const auto top = BlockUpdates::placement(a.world, zombie, {0, 65, 0}, Direction::Up, 90.0f, 0.0f);
    REQUIRE(top);
    CHECK(r.blockOf(*top) == blocks::ZombieHead);
    CHECK(r.get(*top, properties::rotation16) == 12); // (the player looking west: it faces east)
    const ItemId head = *itemRegistry().find("zombie_head");
    CHECK(itemRegistry().item(head).armorSlot == 1);
    CHECK(itemRegistry().blockItem(blocks::ZombieWallHead) == head);
    CHECK_FALSE(itemRegistry().find("zombie_wall_head").has_value());
}

TEST_CASE("a mob killed by a charged creeper's blast drops its head; wither skeletons now and then their skull (M26.4b)") {
    Arena a;
    MobData z = Mobs::make(MobType::Zombie, {2.5, 64.0, 2.5}, a.rng);
    z.chargedBlast = 3;
    z.health = 0.0f;
    REQUIRE(Mobs::add(a.world, z));
    a.tick(2);
    CHECK(a.dropped("zombie_head"));
    int skulls = 0;
    for (int i = 0; i < 400; ++i) {
        Arena b;
        MobData w = Mobs::make(MobType::WitherSkeleton, {2.5, 64.0, 2.5}, b.rng);
        w.health = 0.0f;
        w.lastHurtByPlayer = true;
        b.rng = Xoroshiro(uint64_t(i) * 7919u + 1u);
        REQUIRE(Mobs::add(b.world, w));
        b.tick(1);
        skulls += b.dropped("wither_skeleton_skull");
    }
    CHECK(skulls > 0);   // (2.5% of 400: about 10)
    CHECK(skulls < 40);
}

TEST_CASE("the Wither: built of soul sand and three skulls, it charges, bursts out, fires wither skulls and drops a nether star (M26.4b)") {
    Arena a;
    const auto& r = blockRegistry();
    const BlockStateId sand = r.defaultState(blocks::SoulSand), skull = r.defaultState(blocks::WitherSkeletonSkull);
    a.world.setBlock({0, 64, 0}, sand); // the stem
    for (int x = -1; x <= 1; ++x) a.world.setBlock({x, 65, 0}, sand);
    a.world.setBlock({-1, 66, 0}, skull);
    a.world.setBlock({0, 66, 0}, skull);
    CHECK_FALSE(Mobs::buildWither(a.world, {0, 66, 0}, a.rng)); // (two skulls: not yet)
    a.world.setBlock({1, 66, 0}, skull);
    REQUIRE(Mobs::buildWither(a.world, {1, 66, 0}, a.rng));
    CHECK(a.world.getBlock({0, 64, 0}) == 0);
    CHECK(a.world.getBlock({1, 66, 0}) == 0);
    MobData* w = a.find(MobType::Wither);
    REQUIRE(w);
    CHECK(w->spellTicks > 0);
    // Charging: it can't be hurt, and its health fills; then the blast.
    Mobs::attack(*w, 20.0f, a.player.position());
    a.tick(230);
    w = a.find(MobType::Wither);
    REQUIRE(w);
    CHECK(w->spellTicks == 0);
    CHECK(w->health > 290.0f);
    CHECK(a.world.getBlock({0, 63, 0}) == 0); // (the power-7 blast dug out the floor)
    // It goes for the player with wither skulls.
    a.player.setPosition({0.5, 64.0, 12.5});
    bool withered = false;
    a.tick(400, [&] {
        for (const auto& e : a.vitals.effects()) withered = withered || e.type == Effect::Wither;
    });
    CHECK(withered);
    // Killed: a nether star.
    a.find(MobType::Wither)->health = 0.0f;
    a.tick(2);
    CHECK(a.dropped("nether_star"));
}
