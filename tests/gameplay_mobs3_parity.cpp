// M26 review fixes: wiki values for taming odds, drops, genes, growth and recipes; stale
// player-fight memories that tamed wolves and trader llamas looked up every tick.
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Recipes.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

struct Meadow {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{211};
    ItemEntities items;
    Mobs mobs;
    Meadow() {
        const auto& r = blockRegistry();
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, 63, z, r.defaultState(blocks::GrassBlock));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 8.5});
        player.setCreative(false);
    }
    void tick(int n) {
        for (int i = 0; i < n; ++i) {
            Mobs::Context ctx{world, player, vitals, true, false, 6000, 0.0f, rng, items};
            ctx.naturalSpawning = false;
            mobs.tick(ctx);
        }
    }
    int dropped(const char* name) {
        int n = 0;
        for (const auto& it : items.items())
            if (it.stack.item == *itemRegistry().find(name)) n += it.stack.count;
        return n;
    }
    static ItemId item(const char* n) { return *itemRegistry().find(n); }
};

} // namespace

TEST_CASE("parrots tame 1 in 10 per seed, wolves 1 in 3 per bone (wiki: Parrot, Wolf)") {
    Meadow f;
    int parrots = 0, wolves = 0;
    for (int i = 0; i < 2000; ++i) {
        MobData p = Mobs::make(MobType::Parrot, {0.5, 64.0, 0.5}, f.rng);
        Mobs::interact(p, Meadow::item("wheat_seeds"), f.rng, f.items);
        parrots += p.tamed;
        MobData w = Mobs::make(MobType::Wolf, {0.5, 64.0, 0.5}, f.rng);
        Mobs::interact(w, Meadow::item("bone"), f.rng, f.items);
        wolves += w.tamed;
    }
    CHECK(parrots > 140);
    CHECK(parrots < 260); // (~200)
    CHECK(wolves > 580);  // (~667)
}

TEST_CASE("panda genes follow the wiki's odds: normal 163/256, brown 1/64 (wiki: Panda › Genes)") {
    Xoroshiro rng(5);
    int normal = 0, brown = 0, weak = 0;
    constexpr int n = 8192;
    for (int i = 0; i < n; ++i) {
        const MobData p = Mobs::make(MobType::Panda, {0, 64, 0}, rng);
        normal += p.woolColour == 0;
        brown += p.woolColour == 4;
        weak += p.woolColour == 5;
    }
    CHECK(normal / double(n) == doctest::Approx(163.0 / 256.0).epsilon(0.05));
    CHECK(brown / double(n) == doctest::Approx(1.0 / 64.0).epsilon(0.3));
    CHECK(weak / double(n) == doctest::Approx(25.0 / 256.0).epsilon(0.15));
}

TEST_CASE("cats drop string, parrots feathers, pandas exactly 1 bamboo (wiki drops, Java)") {
    Meadow f;
    for (const MobType t : {MobType::Cat, MobType::Parrot, MobType::Panda}) {
        for (int i = 0; i < 6; ++i) {
            MobData m = Mobs::make(t, {0.5 + i, 64.0, 0.5}, f.rng);
            m.health = 0.0f;
            REQUIRE(Mobs::add(f.world, m));
        }
    }
    f.tick(25);
    CHECK(f.dropped("string") >= 1);
    CHECK(f.dropped("feather") >= 6);
    CHECK(f.dropped("bamboo") == 6);
}

TEST_CASE("a hay bale grows a llama cria by 90 s; the flow template copies with a breeze rod") {
    Meadow f;
    MobData cria = Mobs::make(MobType::Llama, {0.5, 64.0, 0.5}, f.rng);
    cria.age = -24000;
    Mobs::interact(cria, Meadow::item("hay_block"), f.rng, f.items);
    CHECK(cria.age == -24000 + 1800);
    bool flow = false;
    for (const Recipe& r : craftingRecipes())
        flow = flow || r.result.item == Meadow::item("flow_armor_trim_smithing_template");
    CHECK(flow);
}

TEST_CASE("a trader llama whose trader is gone stops looking for it (M26 perf review)") {
    Meadow f;
    MobData l = Mobs::make(MobType::TraderLlama, {0.5, 64.0, 0.5}, f.rng);
    l.targetUuid = 0x1234; // (no such trader)
    l.despawnDelay = 0;
    REQUIRE(Mobs::add(f.world, l));
    f.tick(2);
    const MobData* found = nullptr;
    f.world.forEachChunk([&](Chunk& c) {
        for (auto& m : c.mobs())
            if (m.type == MobType::TraderLlama) found = &m;
    });
    REQUIRE(found);
    CHECK(found->targetUuid == 0);
}

TEST_CASE("the undead: Smite and the Wither know wither skeletons, phantoms and the Wither (M26 review)") {
    for (const MobType t : {MobType::Zombie, MobType::Skeleton, MobType::WitherSkeleton, MobType::Phantom,
                            MobType::Wither, MobType::ZombifiedPiglin, MobType::Drowned})
        CHECK(isUndead(t));
    for (const MobType t : {MobType::Spider, MobType::Creeper, MobType::Cow, MobType::Breeze}) CHECK_FALSE(isUndead(t));
}

TEST_CASE("a copper golem keeps what it carries and a charging Wither its charge through a save (M26 review)") {
    Chunk c({0, 0});
    Xoroshiro rng(3);
    MobData g = Mobs::make(MobType::CopperGolem, {1.5, 65.0, 1.5}, rng);
    g.mouthItem = Meadow::item("apple");
    g.allayCount = 16;
    c.mobs().push_back(g);
    MobData w = Mobs::make(MobType::Wither, {4.5, 65.0, 4.5}, rng);
    w.spellTicks = 120;
    c.mobs().push_back(w);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 2);
    CHECK(back.mobs()[0].mouthItem == g.mouthItem);
    CHECK(back.mobs()[0].allayCount == 16);
    CHECK(back.mobs()[1].spellTicks == 120);
}
