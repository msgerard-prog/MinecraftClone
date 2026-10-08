// Allays and nautiluses (M26.5a).
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"

#include <doctest/doctest.h>

#include <functional>

using namespace mc;
using namespace mc::world;

namespace {

struct Meadow {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{103};
    ItemEntities items;
    Mobs mobs;
    explicit Meadow(bool sea = false) {
        const auto& r = blockRegistry();
        for (int cz = -3; cz <= 3; ++cz)
            for (int cx = -3; cx <= 3; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 50, z, r.defaultState(blocks::Stone));
                        for (int y = 51; y <= 63; ++y) c.set(x, y, z, sea ? r.defaultState(blocks::Water) : r.defaultState(blocks::Stone));
                    }
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 0.5});
        player.setCreative(false);
    }
    void tick(int n, const std::function<void()>& each = {}) {
        for (int i = 0; i < n; ++i) {
            if (each) each();
            Mobs::Context ctx{world, player, vitals, true, false, 6000, 0.0f, rng, items};
            ctx.naturalSpawning = false;
            mobs.tick(ctx);
            items.tick(world, Aabb{glm::dvec3(1e9), glm::dvec3(1e9)}, false, inventory); // (nobody picks up)
            world.levelEvents().clear();
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
    int count(MobType t) {
        int n = 0;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs()) n += m.type == t && m.health > 0.0f;
        });
        return n;
    }
    Inventory inventory;
};

} // namespace

TEST_CASE("an allay fetches dropped stacks of its item and drops them by the player; an empty hand takes it back (M26.5a)") {
    Meadow m;
    const ItemId apple = *itemRegistry().find("apple");
    REQUIRE(Mobs::add(m.world, Mobs::make(MobType::Allay, {1.5, 65.0, 1.5}, m.rng)));
    CHECK(Mobs::interact(*m.find(MobType::Allay), apple, m.rng, m.items) == Mobs::Use::Fed);
    m.items.spawn({14.5, 64.2, 9.5}, {apple, 5}, m.rng, 0);
    m.items.spawn({14.5, 64.2, 9.5}, {*itemRegistry().find("bone"), 1}, m.rng, 0); // (not its item)
    bool carried = false, delivered = false;
    m.tick(600, [&] {
        MobData* a = m.find(MobType::Allay);
        carried = carried || a->allayCount == 5;
        for (const auto& it : m.items.items())
            if (it.stack.item == apple && glm::length(it.pos - m.player.position()) < 4.0) delivered = true;
    });
    CHECK(carried);
    CHECK(delivered);
    CHECK(Mobs::interact(*m.find(MobType::Allay), kNoItem, m.rng, m.items) == Mobs::Use::Sat);
    CHECK(m.find(MobType::Allay)->mouthItem == kNoItem);
}

TEST_CASE("an allay dancing by a playing jukebox duplicates with an amethyst shard; allays save what they hold (M26.5a)") {
    Meadow m;
    m.world.setBlock({2, 64, 2}, blockRegistry().defaultState(blocks::Jukebox));
    m.world.chunk({0, 0})->addJukebox(2, 64, 2).playing = true;
    REQUIRE(Mobs::add(m.world, Mobs::make(MobType::Allay, {3.5, 65.0, 3.5}, m.rng)));
    m.tick(40);
    REQUIRE(m.find(MobType::Allay)->peek == 1); // (dancing)
    CHECK(Mobs::interact(*m.find(MobType::Allay), *itemRegistry().find("amethyst_shard"), m.rng, m.items) == Mobs::Use::Fed);
    m.tick(2);
    CHECK(m.count(MobType::Allay) == 2);
    Chunk c({0, 0});
    MobData a = Mobs::make(MobType::Allay, {1.5, 65.0, 1.5}, m.rng);
    a.mouthItem = *itemRegistry().find("apple");
    a.allayCount = 12;
    c.mobs().push_back(a);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 1);
    CHECK(back.mobs()[0].mouthItem == a.mouthItem);
    CHECK(back.mobs()[0].allayCount == 12);
}

TEST_CASE("a nautilus is tamed with pufferfish, takes a saddle and swims where its rider looks; its rider keeps their breath (M26.5a)") {
    Meadow m(true);
    REQUIRE(Mobs::add(m.world, Mobs::make(MobType::Nautilus, {0.5, 58.0, 0.5}, m.rng)));
    MobData* n = m.find(MobType::Nautilus);
    CHECK(Mobs::interact(*n, *itemRegistry().find("saddle"), m.rng, m.items) == Mobs::Use::None); // (wild)
    int fed = 0;
    while (!n->tamed && fed < 60) {
        REQUIRE(Mobs::interact(*n, *itemRegistry().find("pufferfish"), m.rng, m.items) == Mobs::Use::Fed);
        ++fed;
    }
    REQUIRE(n->tamed);
    CHECK(Mobs::interact(*n, *itemRegistry().find("saddle"), m.rng, m.items) == Mobs::Use::Fed);
    CHECK(Mobs::interact(*n, kNoItem, m.rng, m.items) == Mobs::Use::Ride);
    // Forward, looking down a little to the south: it swims that way.
    m.tick(60, [&] {
        MobData* r = m.find(MobType::Nautilus);
        r->paddleForward = 1;
        r->headYaw = 0.0f;
        r->pitch = 20.0f;
    });
    n = m.find(MobType::Nautilus);
    CHECK(n->pos.z > 10.0);
    CHECK(n->pos.y < 58.0);
    Vitals v;
    v.addEffect(Effect::BreathOfTheNautilus, 0, 100);
    const int air = v.air();
    for (int t = 0; t < 50; ++t) v.breathe(true);
    CHECK(v.air() == air);
}
