// The warden (M27.3c): summoned by shriekers, emerging unhurt, angered by a player's
// vibrations and closeness, the sonic boom, darkness, digging back down.
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

#include <functional>

using namespace mc;
using namespace mc::world;

namespace {

struct Deep {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{151};
    ItemEntities items;
    Mobs mobs;
    Deep() {
        const auto& r = blockRegistry();
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 59, z, r.defaultState(blocks::Deepslate));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({0.5, 60.0, 12.5});
        player.setCreative(false);
    }
    void tick(int n, const std::function<void()>& each = {}) {
        for (int i = 0; i < n; ++i) {
            if (each) each();
            Mobs::Context ctx{world, player, vitals, true, false, 18000, 0.0f, rng, items};
            ctx.naturalSpawning = false;
            mobs.tick(ctx);
        }
    }
    MobData* warden() {
        MobData* out = nullptr;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                if (m.type == MobType::Warden && m.health > 0.0f && !out) out = &m;
        });
        return out;
    }
};

} // namespace

TEST_CASE("a shrieker calls a warden out of the ground; only one within 48 blocks (M27.3c)") {
    Deep d;
    REQUIRE(Mobs::summonWarden(d.world, {0, 60, 0}, d.rng));
    MobData* w = d.warden();
    REQUIRE(w);
    CHECK(w->phase == 0); // emerging
    CHECK(glm::length(w->pos - glm::dvec3(0.5, 60.0, 0.5)) < 8.0);
    CHECK_FALSE(Mobs::summonWarden(d.world, {10, 60, 0}, d.rng));
    // Unhurt while emerging.
    Mobs::attack(*w, 50.0f, d.player.position());
    CHECK(w->health == doctest::Approx(500.0f));
    d.tick(140);
    CHECK(d.warden()->phase == 1);
}

TEST_CASE("a player's vibrations anger the warden; angry, it booms the player from afar through armor (M27.3c)") {
    Deep d;
    MobData w = Mobs::make(MobType::Warden, {0.5, 60.0, 0.5}, d.rng);
    w.phase = 1;
    REQUIRE(Mobs::add(d.world, w));
    d.tick(1);
    // Three steps near it (sneaking would make none).
    for (int i = 0; i < 3; ++i) {
        d.world.vibration({0.5, 60.0, 8.5}, true);
        d.tick(1);
    }
    CHECK(d.warden()->angerTicks >= 80);
    const float before = d.vitals.health();
    d.tick(60);
    CHECK(d.vitals.health() <= before - 9.0f); // (the sonic boom: 10)
    d.tick(70);                                 // (its pulse comes every 6 s)
    CHECK(d.vitals.effectLevel(Effect::Darkness) > 0);
}

TEST_CASE("left alone a minute, the warden digs back down; killed, it drops a sculk catalyst (M27.3c)") {
    Deep d;
    MobData w = Mobs::make(MobType::Warden, {0.5, 60.0, 0.5}, d.rng);
    w.phase = 1;
    REQUIRE(Mobs::add(d.world, w));
    d.player.setPosition({0.5, 60.0, 60.5}); // (far: nothing to smell)
    d.tick(1450);
    CHECK(d.warden() == nullptr);
    MobData k = Mobs::make(MobType::Warden, {0.5, 60.0, 0.5}, d.rng);
    k.phase = 1;
    k.health = 0.0f;
    REQUIRE(Mobs::add(d.world, k));
    d.tick(25);
    int catalysts = 0;
    for (const auto& it : d.items.items()) catalysts += it.stack.item == itemRegistry().blockItem(blocks::SculkCatalyst);
    CHECK(catalysts == 1);
}

TEST_CASE("a trial spawner sends out its mobs when a player comes near; beaten, it gives a trial key and rests (M27.4d)") {
    Deep d;
    const auto& r = blockRegistry();
    d.world.updateBlock({0, 60, 0}, r.defaultState(blocks::TrialSpawner));
    REQUIRE(d.world.chunk({0, 0})->spawner(0, 60, 0));
    d.world.chunk({0, 0})->spawner(0, 60, 0)->mob = MobType::Zombie;
    d.world.markTicking({0, 0});
    d.player.setPosition({0.5, 60.0, 8.5});
    int most = 0;
    d.tick(400, [&] {
        int alive = 0;
        d.world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs()) alive += m.type == MobType::Zombie && m.health > 0.0f;
        });
        most = std::max(most, alive);
        // (the player beats each one as it comes)
        d.world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                if (m.type == MobType::Zombie && m.health > 0.0f) m.health = 0.0f;
        });
    });
    CHECK(most >= 1);
    CHECK(most <= 3);
    int keys = 0;
    for (const auto& it : d.items.items()) keys += it.stack.item == *itemRegistry().find("trial_key");
    CHECK(keys == 1);
    CHECK(r.get(d.world.getBlock({0, 60, 0}), properties::trialState) == 5); // cooldown
    CHECK(d.world.chunk({0, 0})->spawner(0, 60, 0)->cooldown > 30000);
}

TEST_CASE("sniffers dig up torchflower seeds or pitcher pods; fed seeds, two lay an egg that hatches (M27.5c)") {
    Deep d;
    const auto& r = blockRegistry();
    for (int z = -8; z <= 8; ++z)
        for (int x = -8; x <= 8; ++x) d.world.setBlock({x, 59, z}, r.defaultState(blocks::GrassBlock));
    MobData s = Mobs::make(MobType::Sniffer, {0.5, 60.0, 0.5}, d.rng);
    s.eggTicks = 5;
    REQUIRE(Mobs::add(d.world, s));
    d.player.setPosition({0.5, 60.0, 6.5});
    d.tick(120);
    int finds = 0;
    for (const auto& it : d.items.items())
        finds += it.stack.item == *itemRegistry().find("torchflower_seeds") || it.stack.item == *itemRegistry().find("pitcher_pod");
    CHECK(finds == 1);
    // Two fed sniffers: an egg where one stands.
    MobData b = Mobs::make(MobType::Sniffer, {2.5, 60.0, 0.5}, d.rng);
    REQUIRE(Mobs::add(d.world, b));
    d.world.forEachChunk([&](Chunk& c) {
        for (auto& m : c.mobs())
            if (m.type == MobType::Sniffer)
                CHECK(Mobs::interact(m, *itemRegistry().find("torchflower_seeds"), d.rng, d.items) == Mobs::Use::Fed);
    });
    bool egg = false;
    d.tick(400, [&] {
        for (int z = -6; z <= 6 && !egg; ++z)
            for (int x = -6; x <= 6 && !egg; ++x) egg = r.blockOf(d.world.getBlock({x, 60, z})) == blocks::SnifferEgg;
    });
    CHECK(egg);
}

TEST_CASE("a brush gets a scute from an armadillo (M27.5c)") {
    Deep d;
    MobData a = Mobs::make(MobType::Armadillo, {0.5, 60.0, 0.5}, d.rng);
    CHECK(Mobs::interact(a, *itemRegistry().find("brush"), d.rng, d.items) == Mobs::Use::Sheared);
    int scutes = 0;
    for (const auto& it : d.items.items()) scutes += it.stack.item == *itemRegistry().find("armadillo_scute");
    CHECK(scutes == 1);
}
