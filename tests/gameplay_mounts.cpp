// Mounts (M26.2): horses, donkeys, mules, llamas, camels, chest boats.
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Projectiles.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

struct Field {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{47};
    ItemEntities items;
    Projectiles projectiles;
    Mobs mobs;
    Field() {
        for (int cz = -4; cz <= 4; ++cz)
            for (int cx = -4; cx <= 4; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 60.5}); // (out of the way)
        player.setCreative(false);
    }
    void tick(int n, const std::function<void()>& each = {}) {
        for (int i = 0; i < n; ++i) {
            if (each) each();
            Mobs::Context ctx{world, player, vitals, true, false, 6000, 0.0f, rng, items};
            ctx.naturalSpawning = false;
            ctx.projectiles = &projectiles;
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

} // namespace

TEST_CASE("horses spawn with their own stats in vanilla's ranges; foals average their parents; a horse and a donkey have a mule (M26.2)") {
    Xoroshiro rng(5);
    for (int i = 0; i < 200; ++i) {
        const MobData h = Mobs::make(MobType::Horse, {0, 64, 0}, rng);
        CHECK(h.maxHealth >= 15.0f);
        CHECK(h.maxHealth <= 30.0f);
        CHECK(h.health == h.maxHealth);
        CHECK(h.moveSpeed >= 0.1125f);
        CHECK(h.moveSpeed <= 0.3375f);
        CHECK(h.jumpStrength >= 0.4f);
        CHECK(h.jumpStrength <= 1.0f);
        CHECK(h.woolColour < 7);
        CHECK(h.color2 < 5);
        const MobData l = Mobs::make(MobType::Llama, {0, 64, 0}, rng);
        CHECK(l.strength >= 1);
        CHECK(l.strength <= 5);
    }
    // Two fast parents have fast foals (on average close to theirs).
    MobData a = Mobs::make(MobType::Horse, {0, 64, 0}, rng), b = a;
    a.moveSpeed = b.moveSpeed = 0.33f;
    double sum = 0.0;
    for (int i = 0; i < 100; ++i) {
        MobData foal = Mobs::make(MobType::Horse, {0, 64, 0}, rng);
        Mobs::mountOffspring(a, b, foal, rng);
        CHECK(foal.moveSpeed <= 0.3375f);
        sum += foal.moveSpeed;
    }
    CHECK(sum / 100.0 > 0.3);
    MobData donkey = Mobs::make(MobType::Donkey, {0, 64, 0}, rng);
    MobData foal = Mobs::make(MobType::Horse, {0, 64, 0}, rng);
    Mobs::mountOffspring(a, donkey, foal, rng);
    CHECK(foal.type == MobType::Mule);
}

TEST_CASE("taming: a wild horse bucks its rider off and grows calmer; at full temper it is tamed (M26.2)") {
    Field f;
    REQUIRE(Mobs::add(f.world, Mobs::make(MobType::Horse, {0.5, 64.0, 0.5}, f.rng)));
    MobData* h = f.find(MobType::Horse);
    // A saddle does nothing on a wild horse: the click gets on it.
    CHECK(Mobs::interact(*h, Field::item("saddle"), f.rng, f.items) == Mobs::Use::Ride);
    CHECK_FALSE(h->saddled);
    h->temper = 0;
    f.tick(80);
    h = f.find(MobType::Horse);
    CHECK_FALSE(h->ridden); // (thrown off: a roll of 0-99 is never below 0)
    CHECK_FALSE(h->tamed);
    CHECK(h->temper == 5);
    // Fed until its temper is full, the next ride tames it.
    while (h->temper < 100) REQUIRE(Mobs::interact(*h, Field::item("golden_apple"), f.rng, f.items) == Mobs::Use::Fed);
    CHECK(Mobs::interact(*h, 0, f.rng, f.items) == Mobs::Use::Ride);
    f.tick(80);
    h = f.find(MobType::Horse);
    CHECK(h->tamed);
    CHECK(h->ridden);
}

TEST_CASE("a saddled horse runs at its own speed where its rider looks, and jumps with its jump strength (M26.2)") {
    Field f;
    MobData m = Mobs::make(MobType::Horse, {0.5, 64.0, -40.5}, f.rng);
    m.tamed = true;
    m.saddled = true;
    m.moveSpeed = 0.3f;
    m.jumpStrength = 1.0f;
    m.ridden = true;
    REQUIRE(Mobs::add(f.world, m));
    f.tick(40, [&] {
        MobData* h = f.find(MobType::Horse);
        h->paddleForward = 1;
        h->headYaw = 0.0f; // (south: +Z)
    });
    MobData* h = f.find(MobType::Horse);
    // 0.3 x 2.1585 blocks a tick (12.95 blocks/s) once up to speed.
    CHECK(h->pos.z - (-40.5) > 20.0);
    CHECK(h->pos.z - (-40.5) < 27.0);
    CHECK(std::abs(h->pos.x - 0.5) < 0.1);
    // A full jump bar with strength 1.0 clears about 5 blocks.
    f.tick(10, [&] { f.find(MobType::Horse)->paddleForward = 0; });
    const double ground = f.find(MobType::Horse)->pos.y;
    double top = ground;
    f.find(MobType::Horse)->riderJump = 100;
    f.tick(30, [&] { top = std::max(top, f.find(MobType::Horse)->pos.y); });
    CHECK(top - ground > 4.5);
    CHECK(top - ground < 6.5);
    // Unsaddled it only carries the rider.
    h = f.find(MobType::Horse);
    h->saddled = false;
    const glm::dvec3 at = h->pos;
    f.tick(20, [&] { f.find(MobType::Horse)->paddleForward = 1; });
    CHECK(glm::length(f.find(MobType::Horse)->pos - at) < 0.5);
}

TEST_CASE("gear: saddles, horse armor, carpets and chests go on tamed mounts; shears take armor, then the saddle (M26.2)") {
    Field f;
    MobData m = Mobs::make(MobType::Horse, {0.5, 64.0, 0.5}, f.rng);
    m.tamed = true;
    CHECK(Mobs::interact(m, Field::item("saddle"), f.rng, f.items) == Mobs::Use::Fed);
    CHECK(m.saddled);
    CHECK(Mobs::interact(m, Field::item("diamond_horse_armor"), f.rng, f.items) == Mobs::Use::Fed);
    CHECK(m.horseArmor == 4);
    CHECK(Mobs::interact(m, Field::item("chest"), f.rng, f.items) != Mobs::Use::Fed); // (horses carry no chest)
    CHECK(Mobs::interact(m, Field::item("shears"), f.rng, f.items) == Mobs::Use::Sheared);
    CHECK(m.horseArmor == 0);
    CHECK(m.saddled);
    CHECK(Mobs::interact(m, Field::item("shears"), f.rng, f.items) == Mobs::Use::Sheared);
    CHECK_FALSE(m.saddled);
    MobData d = Mobs::make(MobType::Donkey, {0.5, 64.0, 0.5}, f.rng);
    d.tamed = true;
    CHECK(Mobs::interact(d, Field::item("chest"), f.rng, f.items) == Mobs::Use::Fed);
    CHECK(d.hasChest);
    CHECK(chestSlots(d.type, d.strength) == 15);
    MobData l = Mobs::make(MobType::Llama, {0.5, 64.0, 0.5}, f.rng);
    l.tamed = true;
    CHECK(Mobs::interact(l, Field::item("saddle"), f.rng, f.items) != Mobs::Use::Fed); // (llamas take no saddle)
    CHECK(Mobs::interact(l, Field::item("lime_carpet"), f.rng, f.items) == Mobs::Use::Fed);
    CHECK(l.decor == 6);
    l.strength = 2;
    CHECK(chestSlots(l.type, l.strength) == 6);
    // A camel needs no taming: it takes a saddle at once.
    MobData c = Mobs::make(MobType::Camel, {0.5, 64.0, 0.5}, f.rng);
    CHECK(Mobs::interact(c, Field::item("saddle"), f.rng, f.items) == Mobs::Use::Fed);
    CHECK(c.saddled);
}

TEST_CASE("mounts and chest boats save their stats, gear and chests; a dead donkey drops its chest and contents (M26.2)") {
    Field f;
    Chunk c({0, 0});
    MobData h = Mobs::make(MobType::Horse, {3.5, 64.0, 3.5}, f.rng);
    h.tamed = true;
    h.saddled = true;
    h.horseArmor = 2;
    h.temper = 40;
    h.woolColour = 5;
    h.color2 = 3;
    c.mobs().push_back(h);
    MobData d = Mobs::make(MobType::Donkey, {4.5, 64.0, 3.5}, f.rng);
    d.tamed = true;
    d.hasChest = true;
    c.mobs().push_back(d);
    c.addMobStore(d.uuidHi)[7] = {Field::item("diamond"), 5};
    MobData l = Mobs::make(MobType::Llama, {5.5, 64.0, 3.5}, f.rng);
    l.strength = 4;
    l.decor = 12;
    l.woolColour = 2;
    c.mobs().push_back(l);
    REQUIRE(Mobs::placeBoat(f.world, {0.5, 64.0, 0.5}, 0.0f, 3, f.rng, true));
    MobData boat = *f.find(MobType::Boat);
    c.mobs().push_back(boat);
    c.addMobStore(boat.uuidHi)[26] = {Field::item("apple"), 3};
    // (regression, M26.1 review: a tamed wolf kept its 40 health only until reloaded)
    MobData w = Mobs::make(MobType::Wolf, {6.5, 64.0, 3.5}, f.rng);
    w.tamed = true;
    w.health = 36.0f;
    c.mobs().push_back(w);

    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 5);
    const MobData& h2 = back.mobs()[0];
    CHECK(h2.tamed);
    CHECK(h2.saddled);
    CHECK(h2.horseArmor == 2);
    CHECK(h2.temper == 40);
    CHECK(h2.woolColour == 5);
    CHECK(h2.color2 == 3);
    CHECK(h2.maxHealth == doctest::Approx(h.maxHealth));
    CHECK(h2.moveSpeed == doctest::Approx(h.moveSpeed));
    CHECK(h2.jumpStrength == doctest::Approx(h.jumpStrength));
    const MobData& d2 = back.mobs()[1];
    CHECK(d2.hasChest);
    REQUIRE(back.mobStore(d2.uuidHi));
    CHECK((*back.mobStore(d2.uuidHi))[7].count == 5);
    CHECK(back.mobs()[2].strength == 4);
    CHECK(back.mobs()[2].decor == 12);
    CHECK(back.mobs()[2].woolColour == 2);
    CHECK(back.mobs()[3].type == MobType::Boat);
    CHECK(back.mobs()[3].hasChest);
    CHECK(back.mobs()[3].woolColour == 3);
    REQUIRE(back.mobStore(back.mobs()[3].uuidHi));
    CHECK((*back.mobStore(back.mobs()[3].uuidHi))[26].count == 3);
    CHECK(back.mobs()[4].health == doctest::Approx(36.0f));

    // A donkey dies: its chest and the diamonds drop.
    Field g;
    REQUIRE(Mobs::add(g.world, d));
    g.world.chunk({0, 0})->addMobStore(d.uuidHi)[0] = {Field::item("diamond"), 5};
    g.find(MobType::Donkey)->health = 0.0f;
    g.tick(2);
    int diamonds = 0, chests = 0;
    for (const auto& it : g.items.items()) {
        if (it.stack.item == Field::item("diamond")) diamonds += it.stack.count;
        if (it.stack.item == Field::item("chest")) chests += it.stack.count;
    }
    CHECK(diamonds == 5);
    CHECK(chests == 1);
    CHECK(g.world.chunk({0, 0})->mobStore(d.uuidHi) == nullptr);
}

TEST_CASE("a mount's chest goes with it into the next chunk (M26.2)") {
    Field f;
    MobData d = Mobs::make(MobType::Donkey, {15.2, 64.0, 0.5}, f.rng);
    d.tamed = true;
    d.hasChest = true;
    d.saddled = true;
    d.ridden = true;
    REQUIRE(Mobs::add(f.world, d));
    f.world.chunk({0, 0})->addMobStore(d.uuidHi)[3] = {Field::item("apple"), 9};
    f.tick(20, [&] {
        MobData* m = f.find(MobType::Donkey);
        m->paddleForward = 1;
        m->headYaw = -90.0f; // (east: +X)
    });
    REQUIRE(f.find(MobType::Donkey)->pos.x > 16.0);
    CHECK(f.world.chunk({0, 0})->mobStore(d.uuidHi) == nullptr);
    REQUIRE(f.world.chunk({1, 0})->mobStore(d.uuidHi));
    CHECK((*f.world.chunk({1, 0})->mobStore(d.uuidHi))[3].count == 9);
}

TEST_CASE("camels dash forward on a jump; llamas spit at wolves (M26.2)") {
    Field f;
    MobData c = Mobs::make(MobType::Camel, {0.5, 64.0, -40.5}, f.rng);
    c.saddled = true;
    c.ridden = true;
    REQUIRE(Mobs::add(f.world, c));
    f.tick(5, [&] { f.find(MobType::Camel)->headYaw = 0.0f; });
    const double z0 = f.find(MobType::Camel)->pos.z;
    f.find(MobType::Camel)->riderJump = 100;
    f.tick(25, [&] { f.find(MobType::Camel)->headYaw = 0.0f; });
    CHECK(f.find(MobType::Camel)->pos.z - z0 > 9.0);
    CHECK(f.find(MobType::Camel)->pos.z - z0 < 14.0);
    CHECK(f.find(MobType::Camel)->dashCooldown > 0);

    Field g;
    REQUIRE(Mobs::add(g.world, Mobs::make(MobType::Llama, {0.5, 64.0, 0.5}, g.rng)));
    REQUIRE(Mobs::add(g.world, Mobs::make(MobType::Wolf, {5.5, 64.0, 0.5}, g.rng)));
    bool spat = false;
    g.tick(100, [&] { spat = spat || g.find(MobType::Llama)->attackCooldown > 0; });
    CHECK(spat);
}
