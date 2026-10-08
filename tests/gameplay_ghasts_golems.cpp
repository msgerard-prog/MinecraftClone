// Happy ghasts, dried ghasts, harnesses, snowballs; copper golems and copper chests (M26.5b).
#include "gameplay/ItemEntities.h"
#include "gameplay/Mining.h"
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

struct Yard {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{109};
    ItemEntities items;
    Mobs mobs;
    Yard() {
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
        player.setPosition({0.5, 64.0, 12.5});
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
    static ItemId item(const char* n) { return *itemRegistry().find(n); }
};

int countOf(const ChestData& c, ItemId item) {
    int n = 0;
    for (const ItemStack& s : c.items)
        if (s.item == item) n += s.count;
    return n;
}

} // namespace

TEST_CASE("a dried ghast soaked in water hydrates and becomes a ghastling (M26.5b)") {
    Yard y;
    const auto& r = blockRegistry();
    BlockUpdates updates(y.world);
    y.world.setBlock({2, 64, 2}, r.set(r.defaultState(blocks::DriedGhast), properties::waterlogged, 0));
    updates.setRandomTicks({0, 0}, 1, 1000);
    for (int t = 0; t < 2000 && updates.hatched().empty(); ++t) {
        updates.setTime(t);
        updates.tick();
    }
    REQUIRE(!updates.hatched().empty());
    CHECK(updates.hatched()[0].type == MobType::HappyGhast);
    CHECK(r.blockOf(y.world.getBlock({2, 64, 2})) == blocks::Water);
    // A dry one never hydrates.
    Yard dry;
    BlockUpdates u2(dry.world);
    dry.world.setBlock({2, 64, 2}, r.defaultState(blocks::DriedGhast));
    u2.setRandomTicks({0, 0}, 1, 1000);
    for (int t = 0; t < 500; ++t) {
        u2.setTime(t);
        u2.tick();
    }
    CHECK(r.get(dry.world.getBlock({2, 64, 2}), properties::hydration) == 0);
}

TEST_CASE("happy ghasts: snowballs grow ghastlings, a harness lets the rider fly it where they look (M26.5b)") {
    Yard y;
    MobData baby = Mobs::make(MobType::HappyGhast, {0.5, 70.0, 0.5}, y.rng);
    baby.age = -24000;
    CHECK(Mobs::interact(baby, Yard::item("snowball"), y.rng, y.items) == Mobs::Use::Fed);
    CHECK(baby.age == -21600);
    CHECK(Mobs::interact(baby, Yard::item("red_harness"), y.rng, y.items) == Mobs::Use::None); // (adults only)
    CHECK(Mobs::box(baby).max.x - Mobs::box(baby).min.x == doctest::Approx(0.95));
    MobData g = Mobs::make(MobType::HappyGhast, {0.5, 70.0, 0.5}, y.rng);
    CHECK(Mobs::interact(g, kNoItem, y.rng, y.items) == Mobs::Use::None); // (no harness: no seat)
    CHECK(Mobs::interact(g, Yard::item("red_harness"), y.rng, y.items) == Mobs::Use::Fed);
    CHECK(g.decor == 15);
    CHECK(Mobs::interact(g, kNoItem, y.rng, y.items) == Mobs::Use::Ride);
    REQUIRE(Mobs::add(y.world, g));
    y.tick(60, [&] {
        MobData* h = y.find(MobType::HappyGhast);
        h->paddleForward = 1;
        h->headYaw = -90.0f; // (east)
        h->pitch = -20.0f;   // (a little up)
    });
    MobData* h = y.find(MobType::HappyGhast);
    CHECK(h->pos.x > 6.0);
    CHECK(h->pos.y > 70.5);
    CHECK(Mobs::interact(*h, Yard::item("shears"), y.rng, y.items) == Mobs::Use::Sheared);
    CHECK(h->decor == 0);
}

TEST_CASE("a carved pumpkin on copper makes a copper golem that sorts a copper chest's items into chests (M26.5b)") {
    Yard y;
    const auto& r = blockRegistry();
    y.world.updateBlock({0, 64, 0}, r.defaultState(*r.findBlock("exposed_copper")));
    y.world.updateBlock({0, 65, 0}, r.defaultState(blocks::CarvedPumpkin));
    REQUIRE(Mobs::buildCopperGolem(y.world, {0, 65, 0}, y.rng));
    CHECK(r.block(r.blockOf(y.world.getBlock({0, 64, 0}))).id == "minecraft:exposed_copper_chest");
    MobData* g = y.find(MobType::CopperGolem);
    REQUIRE(g);
    CHECK(g->woolColour == 1);
    // 20 apples in the copper chest; a wooden chest with an apple in it nearby.
    ChestData* source = y.world.chunk({0, 0})->chest(0, 64, 0);
    REQUIRE(source);
    source->items[0] = {Yard::item("apple"), 20};
    y.world.updateBlock({5, 64, 3}, r.defaultState(blocks::Chest));
    ChestData* dest = y.world.chunk({0, 0})->chest(5, 64, 3);
    REQUIRE(dest);
    dest->items[4] = {Yard::item("apple"), 1};
    y.tick(1200);
    CHECK(countOf(*y.world.chunk({0, 0})->chest(5, 64, 3), Yard::item("apple")) >= 17);
    // Waxed with honeycomb it ages no more; an axe scrapes the wax off, then a stage.
    g = y.find(MobType::CopperGolem);
    CHECK(Mobs::interact(*g, Yard::item("honeycomb"), y.rng, y.items) == Mobs::Use::Fed);
    CHECK(g->sheared);
    CHECK(Mobs::interact(*g, Yard::item("iron_axe"), y.rng, y.items) == Mobs::Use::Sheared);
    CHECK_FALSE(g->sheared);
    CHECK(Mobs::interact(*g, Yard::item("iron_axe"), y.rng, y.items) == Mobs::Use::Sheared);
    CHECK(g->woolColour == 0);
}

TEST_CASE("copper chests keep their contents as they age; snow gives snowballs (M26.5b)") {
    Yard y;
    const auto& r = blockRegistry();
    y.world.updateBlock({1, 64, 1}, r.defaultState(*r.findBlock("copper_chest")));
    ChestData* c = y.world.chunk({0, 0})->chest(1, 64, 1);
    REQUIRE(c);
    c->items[3] = {Yard::item("diamond"), 7};
    y.world.updateBlock({1, 64, 1}, r.defaultState(*r.findBlock("exposed_copper_chest")));
    REQUIRE(y.world.chunk({0, 0})->chest(1, 64, 1));
    CHECK(y.world.chunk({0, 0})->chest(1, 64, 1)->items[3].count == 7);
    std::vector<ItemStack> out;
    blockDrops(r.defaultState(blocks::SnowBlock), {Yard::item("iron_shovel"), 1}, y.rng, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].item == Yard::item("snowball"));
    CHECK(out[0].count == 4);
    out.clear();
    blockDrops(r.defaultState(blocks::Glass), {Yard::item("iron_pickaxe"), 1}, y.rng, out);
    CHECK(out.empty()); // (glass still breaks to nothing)
}
