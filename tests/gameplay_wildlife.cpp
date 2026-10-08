// Wildlife (M26.3): rabbits, foxes, polar bears, pandas, goats, armadillos; wolf armor;
// sweet berry bushes.
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

struct Field {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{53};
    ItemEntities items;
    Mobs mobs;
    int64_t dayTime = 6000;
    bool survival = true;
    Field() {
        for (int cz = -3; cz <= 3; ++cz)
            for (int cx = -3; cx <= 3; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 62, z, blockRegistry().defaultState(blocks::Stone));
                        c.set(x, 63, z, blockRegistry().defaultState(blocks::GrassBlock));
                    }
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 45.5}); // (out of the way unless a test moves it)
        player.setCreative(false);
    }
    void tick(int n, const std::function<void()>& each = {}) {
        for (int i = 0; i < n; ++i) {
            if (each) each();
            player.tick(world, {});
            Mobs::Context ctx{world, player, vitals, survival, false, dayTime, 0.0f, rng, items};
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
    MobData& spawn(MobType t, const glm::dvec3& at) {
        REQUIRE(Mobs::add(world, Mobs::make(t, at, rng)));
        MobData* m = nullptr;
        world.forEachChunk([&](Chunk& c) {
            for (auto& o : c.mobs())
                if (o.type == t && glm::length(o.pos - at) < 0.01) m = &o;
        });
        REQUIRE(m);
        return *m;
    }
    static ItemId item(const char* n) { return *itemRegistry().find(n); }
};

} // namespace

TEST_CASE("rabbits run from players and nibble carrot crops; foxes sleep by day and hunt at night (M26.3)") {
    Field f;
    f.player.setPosition({0.5, 64.0, 0.5});
    f.spawn(MobType::Rabbit, {3.5, 64.0, 0.5});
    f.tick(40);
    CHECK(glm::length(f.find(MobType::Rabbit)->pos - f.player.position()) > 5.0);

    Field g;
    const auto& r = blockRegistry();
    for (int x = 2; x <= 4; ++x) {
        g.world.updateBlock({x, 63, 2}, r.defaultState(blocks::Farmland));
        g.world.updateBlock({x, 64, 2}, r.set(r.defaultState(blocks::Carrots), properties::age7, 7));
    }
    g.spawn(MobType::Rabbit, {3.5, 64.0, 0.5});
    int eaten = 0;
    g.tick(3000, [&] {
        eaten = 0;
        for (int x = 2; x <= 4; ++x) {
            const BlockStateId s = g.world.getBlock({x, 64, 2});
            eaten += r.blockOf(s) != blocks::Carrots ? 8 : 7 - r.get(s, properties::age7);
        }
    });
    CHECK(eaten > 0);

    Field h;
    h.spawn(MobType::Fox, {0.5, 64.0, 0.5});
    h.tick(20);
    CHECK(h.find(MobType::Fox)->sitting); // (asleep at noon)
    h.dayTime = 18000;
    h.spawn(MobType::Chicken, {6.5, 64.0, 0.5});
    h.tick(1200);
    CHECK_FALSE(h.find(MobType::Fox)->sitting);
    CHECK((h.find(MobType::Chicken) == nullptr || h.find(MobType::Chicken)->health < 4.0f));
}

TEST_CASE("foxes pick up an item in their mouth and drop it when they die (M26.3)") {
    Field f;
    f.dayTime = 18000;
    f.spawn(MobType::Fox, {0.5, 64.0, 0.5});
    f.items.spawn({0.5, 64.2, 0.5}, {Field::item("emerald"), 1}, f.rng, 0);
    f.tick(40);
    REQUIRE(f.find(MobType::Fox));
    CHECK(f.find(MobType::Fox)->mouthItem == Field::item("emerald"));
    f.find(MobType::Fox)->health = 0.0f;
    f.tick(3);
    bool dropped = false;
    for (const auto& it : f.items.items()) dropped = dropped || it.stack.item == Field::item("emerald");
    CHECK(dropped);
}

TEST_CASE("goats ram the player and lose a horn ramming stone (M26.3)") {
    Field f;
    f.player.setPosition({0.5, 64.0, 7.5});
    MobData& goat = f.spawn(MobType::Goat, {0.5, 64.0, 0.5});
    goat.chargeTicks = 1;
    goat.yaw = 0.0f;
    const float before = f.vitals.health();
    f.tick(80);
    CHECK(f.vitals.health() < before);

    // A stone wall between a goat and a pig: the charge breaks a horn off.
    Field g;
    for (int y = 64; y <= 66; ++y)
        for (int x = -3; x <= 3; ++x) g.world.updateBlock({x, y, 4}, blockRegistry().defaultState(blocks::Stone));
    g.spawn(MobType::Pig, {0.5, 64.0, 8.5});
    MobData& g2 = g.spawn(MobType::Goat, {0.5, 64.0, 0.5});
    g2.chargeTicks = 1;
    g.tick(80, [&] {
        if (MobData* p = g.find(MobType::Pig)) p->pos = p->prevPos = {0.5, 64.0, 8.5}; // (the pig stays put)
    });
    CHECK(g.find(MobType::Goat)->horns != 3);
    bool horn = false;
    for (const auto& it : g.items.items()) horn = horn || it.stack.item == Field::item("goat_horn");
    CHECK(horn);
}

TEST_CASE("panda genes: recessive brown and weak show only in pairs; weak pandas have 10 health (M26.3)") {
    CHECK(pandaPersonality(4, 4) == 4);
    CHECK(pandaPersonality(4, 0) == 0);
    CHECK(pandaPersonality(6, 0) == 6);
    CHECK(pandaPersonality(5, 1) == 0);
    Xoroshiro rng(9);
    for (int i = 0; i < 200; ++i) {
        const MobData p = Mobs::make(MobType::Panda, {0, 64, 0}, rng);
        CHECK(maxHealthOf(p) == doctest::Approx(pandaPersonality(p.woolColour, p.color2) == 5 ? 10.0f : 20.0f));
    }
    MobData a = Mobs::make(MobType::Panda, {0, 64, 0}, rng), b = a;
    a.woolColour = a.color2 = 4; // (both brown)
    b.woolColour = b.color2 = 4;
    int brown = 0;
    for (int i = 0; i < 100; ++i) {
        MobData cub = Mobs::make(MobType::Panda, {0, 64, 0}, rng);
        Mobs::wildlifeOffspring(a, b, cub, rng);
        brown += pandaPersonality(cub.woolColour, cub.color2) == 4;
    }
    CHECK(brown > 80); // (brown parents have brown cubs, but for a rare new gene)
}

TEST_CASE("a polar bear with a cub turns on a player who comes close (M26.3)") {
    Field f;
    f.player.setPosition({0.5, 64.0, 5.5});
    f.spawn(MobType::PolarBear, {0.5, 64.0, 0.5});
    MobData& cub = f.spawn(MobType::PolarBear, {3.5, 64.0, 0.5});
    cub.age = -24000;
    const float before = f.vitals.health();
    f.tick(200);
    CHECK(f.vitals.health() < before);
}

TEST_CASE("armadillos roll up when hurt (less damage) and shed scutes (M26.3)") {
    Field f;
    MobData& a = f.spawn(MobType::Armadillo, {0.5, 64.0, 0.5});
    a.eggTicks = 5;
    Mobs::attack(a, 2.0f, f.player.position());
    f.tick(10);
    MobData* m = f.find(MobType::Armadillo);
    CHECK(m->sitting);
    const float hp = m->health;
    m->hurtTime = 0;
    Mobs::attack(*m, 5.0f, f.player.position());
    CHECK(hp - m->health == doctest::Approx(2.0f)); // ((5 - 1) / 2 rolled up)
    bool scute = false;
    for (const auto& it : f.items.items()) scute = scute || it.stack.item == Field::item("armadillo_scute");
    CHECK(scute);
}

TEST_CASE("wolf armor takes a tamed wolf's damage until it breaks; shears take it off (M26.3)") {
    Field f;
    MobData& w = f.spawn(MobType::Wolf, {0.5, 64.0, 0.5});
    w.tamed = true;
    w.sitting = true;
    w.health = 40.0f;
    CHECK(Mobs::interact(w, Field::item("wolf_armor"), f.rng, f.items) == Mobs::Use::Fed);
    CHECK(w.horseArmor == 1);
    f.tick(1);
    f.find(MobType::Wolf)->health -= 10.0f;
    f.tick(1);
    CHECK(f.find(MobType::Wolf)->health == doctest::Approx(40.0f));
    CHECK(f.find(MobType::Wolf)->armorWear == 10);
    f.find(MobType::Wolf)->health -= 60.0f; // (more than the armor has left: it breaks)
    f.tick(1);
    CHECK(f.find(MobType::Wolf)->horseArmor == 0);
    CHECK(f.find(MobType::Wolf)->health == doctest::Approx(40.0f));
    MobData* wolf = f.find(MobType::Wolf);
    wolf->horseArmor = 1;
    CHECK(Mobs::interact(*wolf, Field::item("shears"), f.rng, f.items) == Mobs::Use::Sheared);
    CHECK(wolf->horseArmor == 0);
}

TEST_CASE("sweet berry bushes: picking at age 2-3 gives berries and goes back to age 1; bone meal grows them (M26.3)") {
    Field f;
    const auto& r = blockRegistry();
    f.world.updateBlock({0, 64, 0}, r.set(r.defaultState(blocks::SweetBerryBush), properties::age3, 3));
    const int n = BlockUpdates::pickBerries(f.world, {0, 64, 0}, f.rng);
    CHECK(n >= 2);
    CHECK(n <= 3);
    CHECK(r.get(f.world.getBlock({0, 64, 0}), properties::age3) == 1);
    CHECK(BlockUpdates::pickBerries(f.world, {0, 64, 0}, f.rng) == 0);
    BlockUpdates updates(f.world);
    CHECK(updates.boneMeal({0, 64, 0}));
    CHECK(r.get(f.world.getBlock({0, 64, 0}), properties::age3) == 2);
}

TEST_CASE("wildlife saves its kinds, genes, horns, trust and gear; goat horns keep their instrument (M26.3)") {
    Field f;
    Chunk c({0, 0});
    MobData rabbit = Mobs::make(MobType::Rabbit, {1.5, 64, 1.5}, f.rng);
    rabbit.woolColour = 4;
    MobData fox = Mobs::make(MobType::Fox, {2.5, 64, 1.5}, f.rng);
    fox.woolColour = 1;
    fox.tamed = true;
    fox.mouthItem = Field::item("sweet_berries");
    MobData panda = Mobs::make(MobType::Panda, {3.5, 64, 1.5}, f.rng);
    panda.woolColour = 6;
    panda.color2 = 4;
    MobData goat = Mobs::make(MobType::Goat, {4.5, 64, 1.5}, f.rng);
    goat.horns = 2;
    goat.powered = true;
    MobData wolf = Mobs::make(MobType::Wolf, {5.5, 64, 1.5}, f.rng);
    wolf.tamed = true;
    wolf.horseArmor = 1;
    wolf.armorWear = 12;
    for (const MobData& m : {rabbit, fox, panda, goat, wolf}) c.mobs().push_back(m);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 5);
    CHECK(back.mobs()[0].woolColour == 4);
    CHECK(back.mobs()[1].woolColour == 1);
    CHECK(back.mobs()[1].tamed);
    CHECK(back.mobs()[1].mouthItem == Field::item("sweet_berries"));
    CHECK(back.mobs()[2].woolColour == 6);
    CHECK(back.mobs()[2].color2 == 4);
    CHECK(back.mobs()[3].horns == 2);
    CHECK(back.mobs()[3].powered);
    CHECK(back.mobs()[4].horseArmor == 1);
    CHECK(back.mobs()[4].armorWear == 12);
    ItemStack horn{Field::item("goat_horn"), 1};
    horn.damage = 6; // ("yearn")
    CHECK(itemFromNbtPublic(itemToNbt(horn, -1)).damage == 6);
}
