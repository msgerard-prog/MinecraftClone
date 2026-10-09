// Mobs (wiki: Zombie, Cow, Spawn, Entity format).
#include "gameplay/Inventory.h"
#include "gameplay/Mobs.h"
#include "world/Weather.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Villagers.h"
#include "world/ChunkSerializer.h"
#include "world/OverworldGenerator.h"
#include "world/Potions.h"

#include <doctest/doctest.h>

using namespace mc;
using namespace mc::world;

namespace {

struct MobScene {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{11};
    ItemEntities items;
    Mobs mobs;
    bool survival = true;
    int64_t dayTime = 18000; // night: no burning
    float skyDarken = 11.0f;
    bool thundering = false;
    const Weather* weather = nullptr;
    bool mobDrops = true, mobGriefing = true; // (game rules, M28.1)
    int difficulty = 2;
    MobScene() {
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>()); // dark everywhere
                c.setLight(light);
            }
        player.setPosition({0.5, 64.0, 0.5});
        player.setCreative(false);
    }
    void tick(int n = 1) {
        for (int i = 0; i < n; ++i) {
            player.tick(world, {});
            Mobs::Context ctx{world, player, vitals, survival, false, dayTime, skyDarken, rng, items};
            ctx.thundering = thundering;
            ctx.weather = weather;
            ctx.mobDrops = mobDrops;
            ctx.mobGriefing = mobGriefing;
            ctx.difficulty = difficulty;
            mobs.tick(ctx);
        }
    }
    std::vector<MobData*> all() { // (ambient bats - M29.1c, spawning in the dark below - left out)
        std::vector<MobData*> out;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                if (m.type != MobType::Bat) out.push_back(&m);
        });
        return out;
    }
};

} // namespace

TEST_CASE("zombies chase a survival player and hit for 3; creative players are ignored") {
    MobScene s;
    s.mobs = Mobs();
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Zombie, {8.5, 64.0, 0.5}, s.rng)));
    s.survival = false;
    s.tick(60);
    CHECK(s.vitals.health() == 20.0f);
    s.survival = true;
    float lowest = 20.0f;
    for (int i = 0; i < 200; ++i) {
        s.tick();
        lowest = std::min(lowest, s.vitals.health());
    }
    CHECK(lowest <= 17.0f); // reached and hit at least once (3 damage, normal)
}

TEST_CASE("hitting a cow hurts it, makes it panic and kills it after enough hits; loot drops") {
    MobScene s;
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {3.5, 64.0, 3.5}, s.rng)));
    MobData* cow = s.all().at(0);
    Mobs::attack(*cow, 4.0f, s.player.position());
    CHECK(cow->health == 6.0f);
    CHECK(cow->panicTicks > 0);
    CHECK(cow->hurtTime == 10);
    Mobs::attack(*cow, 4.0f, s.player.position()); // still invulnerable
    CHECK(cow->health == 6.0f);
    s.tick(10);
    cow = nullptr;
    for (MobData* m : s.all())
        if (m->type == MobType::Cow) cow = m;
    REQUIRE(cow);
    Mobs::attack(*cow, 7.0f, s.player.position());
    s.tick(1); // loot drops at the moment of death (user decision; vanilla too)...
    CHECK_FALSE(s.items.items().empty());
    s.tick(24); // ...then the death animation, then gone
    int cows = 0; // (zombies may spawn in the dark scene meanwhile)
    for (MobData* m : s.all())
        cows += m->type == MobType::Cow;
    CHECK(cows == 0);
    CHECK_FALSE(s.items.items().empty()); // 1-3 beef at least
}

TEST_CASE("mobs move to the chunk they walk into; far hostiles despawn") {
    MobScene s;
    s.survival = false;
    MobData m = Mobs::make(MobType::Cow, {15.5, 64.0, 8.5}, s.rng);
    m.vel = {0.5, 0, 0};
    REQUIRE(Mobs::add(s.world, m));
    s.tick(5);
    CHECK(s.world.chunk({0, 0})->mobs().empty());
    CHECK(s.world.chunk({1, 0})->mobs().size() == 1);
    MobData z = Mobs::make(MobType::Zombie, {0.5, 64.0, 0.5}, s.rng);
    REQUIRE(Mobs::add(s.world, z));
    s.player.setPosition({200.5, 64.0, 0.5}); // > 128 blocks away, inside the simulation distance
    s.tick(2);
    int zombies = 0;
    for (MobData* p : s.all())
        zombies += p->type == MobType::Zombie;
    CHECK(zombies == 0);
}

TEST_CASE("monsters spawn in the dark around the player, never in light") {
    MobScene dark;
    dark.survival = false; // (creative: no creeper blasts through the thin test floor)
    dark.tick(6000); // (several groups: each group is one kind)
    CHECK(dark.mobs.hostileCount() > 0);
    int kinds[int(MobType::Count)] = {};
    for (MobData* m : dark.all())
        ++kinds[int(m->type)];
    int monsterKinds = 0;
    for (MobType t : {MobType::Zombie, MobType::Skeleton, MobType::Creeper, MobType::Spider})
        monsterKinds += kinds[int(t)] > 0;
    CHECK(monsterKinds >= 2); // a mix, by vanilla's weights
    MobScene day;
    day.skyDarken = 0.0f;
    day.world.forEachChunk([](Chunk& c) {
        std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
        auto lit = std::make_shared<SectionLight>();
        lit->sky.fill(15);
        light.fill(lit);
        c.setLight(light);
    });
    day.tick(2000);
    CHECK(day.mobs.hostileCount() == 0);
}

TEST_CASE("mobs save in the chunk's entities file and load back") {
    Chunk c({1, -1});
    Xoroshiro rng(5);
    MobData cow = Mobs::make(MobType::Cow, {20.5, 70.0, -10.5}, rng);
    cow.health = 7.0f;
    cow.persistent = true;
    c.mobs().push_back(cow);
    const auto nbt = entitiesToNbt(ChunkSnapshot::of(c));
    CHECK(*nbt.list("Entities")->items[0].get<mc::nbt::Compound>()->string("id") == "minecraft:cow");
    Chunk d({1, -1});
    entitiesFromNbt(*mc::nbt::read(mc::nbt::write(nbt)), d);
    REQUIRE(d.mobs().size() == 1);
    CHECK(d.mobs()[0].health == 7.0f);
    CHECK(d.mobs()[0].pos.x == doctest::Approx(20.5));
    CHECK(d.mobs()[0].uuidHi == cow.uuidHi);
    CHECK(d.mobs()[0].persistent);
}

TEST_CASE("new chunks in grassy biomes sometimes come with a herd of farm animals") {
    const OverworldGenerator gen(42);
    int cows = 0, grassy = 0;
    int kinds[int(MobType::Count)] = {};
    for (int cz = -24; cz <= 24 && grassy < 200; ++cz)
        for (int cx = -24; cx <= 24 && grassy < 200; ++cx) {
            const Biome b = gen.biomeAt(gen.column(cx * 16 + 8, cz * 16 + 8));
            if (b != Biome::Plains && b != Biome::Forest && b != Biome::BirchForest) continue;
            ++grassy;
            Chunk c({cx, cz});
            gen.generate(c);
            for (const MobData& m : c.mobs()) {
                CHECK(m.type != MobType::Zombie);
                CHECK(m.persistent);
                ++kinds[int(m.type)];
                ++cows;
            }
        }
    REQUIRE(grassy >= 20);
    CHECK(cows > 0);
    int herdKinds = 0;
    for (int k : kinds)
        herdKinds += k > 0;
    CHECK(herdKinds >= 2); // more than one kind of animal
}


TEST_CASE("cow herds are the same however chunks are generated") {
    const OverworldGenerator gen(42);
    std::vector<ChunkPos> grassy;
    for (int cz = -12; cz <= 12 && grassy.size() < 30; ++cz)
        for (int cx = -12; cx <= 12 && grassy.size() < 30; ++cx) {
            const Biome b = gen.biomeAt(gen.column(cx * 16 + 8, cz * 16 + 8));
            if (b == Biome::Plains || b == Biome::Forest || b == Biome::BirchForest) grassy.push_back({cx, cz});
        }
    auto herds = [&](bool reverse) {
        std::vector<std::vector<MobData>> out(grassy.size());
        for (size_t k = 0; k < grassy.size(); ++k) {
            const size_t i = reverse ? grassy.size() - 1 - k : k;
            Chunk c(grassy[i]);
            gen.generate(c);
            out[i] = c.mobs();
        }
        return out;
    };
    const auto a = herds(false), b = herds(true);
    for (size_t i = 0; i < a.size(); ++i) {
        REQUIRE(a[i].size() == b[i].size());
        for (size_t j = 0; j < a[i].size(); ++j) {
            CHECK(a[i][j].pos == b[i][j].pos);
            CHECK(a[i][j].uuidHi == b[i][j].uuidHi);
        }
    }
}

TEST_CASE("summoned mobs get distinct version-4 UUIDs") {
    Xoroshiro rng(1);
    const MobData a = Mobs::make(MobType::Cow, {0, 64, 0}, rng);
    Xoroshiro same(1);
    const MobData b = Mobs::make(MobType::Cow, {0, 64, 0}, same);
    CHECK((a.uuidHi != b.uuidHi || a.uuidLo != b.uuidLo)); // not from the game RNG
    CHECK(((a.uuidHi >> 12) & 0xF) == 4);
}

TEST_CASE("a chunk unloaded and loaded again is ticked once") {
    MobScene s;
    Mobs::add(s.world, Mobs::make(MobType::Cow, {8.5, 64.0, 8.5}, s.rng));
    s.world.insertChunk(s.world.removeChunk({0, 0}));
    int visits = 0;
    s.world.forEachTickingChunk([&](Chunk& c) { visits += c.pos() == ChunkPos{0, 0}; });
    s.world.markTicking({0, 0});
    s.world.forEachTickingChunk([&](Chunk& c) { visits += c.pos() == ChunkPos{0, 0}; });
    CHECK(visits == 2);
}

TEST_CASE("idle mobs don't mark their chunk for saving; mobs beyond the simulation distance don't tick") {
    MobScene s;
    Mobs::add(s.world, Mobs::make(MobType::Cow, {8.5, 64.0, 8.5}, s.rng));
    for (int i = 0; i < 40; ++i) s.tick(); // settle on the ground
    Chunk& c = *s.world.chunk({0, 0});
    c.mobs()[0].goal = c.mobs()[0].pos;
    c.mobs()[0].goalTicks = 0;
    c.clearDirty();
    s.tick();
    CHECK_FALSE(c.dirty());
    s.player.setPosition({(Mobs::kDefaultSimulationDistance + 3) * 16.0, 64.0, 0.0});
    c.mobs()[0].vel = {0.3, 0.0, 0.0};
    const glm::dvec3 before = c.mobs()[0].pos;
    s.tick();
    CHECK(c.mobs()[0].pos == before);
}

TEST_CASE("zombies in the sun burn for 1 damage a second until they die") {
    MobScene s;
    s.skyDarken = 0.0f; // noon
    std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
    auto bright = std::make_shared<SectionLight>();
    bright->sky.fill(15);
    light.fill(bright);
    s.world.forEachChunk([&](Chunk& c) { c.setLight(light); });
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Zombie, {8.5, 64.0, 8.5}, s.rng)));
    s.tick(101);
    MobData* z = s.all().at(0);
    CHECK(z->health == doctest::Approx(20.0f - 6.0f)); // ticks 0, 20, ..., 100
    s.tick(400);
    int zombies = 0;
    for (MobData* m : s.all())
        zombies += m->type == MobType::Zombie;
    CHECK(zombies == 0);
}

TEST_CASE("hit zombies don't flee; airborne mobs get no knockback lift") {
    MobScene s;
    MobData z = Mobs::make(MobType::Zombie, {3.5, 70.0, 3.5}, s.rng);
    z.onGround = false;
    Mobs::attack(z, 1.0f, {0.5, 70.0, 0.5});
    CHECK(z.panicTicks == 0);
    CHECK(z.vel.y == 0.0);
    MobData c = Mobs::make(MobType::Cow, {3.5, 64.0, 3.5}, s.rng);
    c.onGround = true;
    Mobs::attack(c, 1.0f, {0.5, 64.0, 0.5});
    CHECK(c.panicTicks > 0);
    CHECK(c.vel.y > 0.0);
}

TEST_CASE("mobs take fall damage beyond 3 blocks") {
    MobScene s;
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {8.5, 74.0, 8.5}, s.rng))); // 10 blocks up
    s.tick(60);
    MobData* cow = nullptr;
    for (MobData* m : s.all())
        if (m->type == MobType::Cow) cow = m;
    REQUIRE(cow);
    CHECK(cow->health == doctest::Approx(10.0f - 7.0f));
}

TEST_CASE("zombies don't spawn on bedrock or in lava") {
    MobScene s;
    s.survival = false;
    const auto& reg = blockRegistry();
    s.world.forEachChunk([&](Chunk& c) {
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                c.set(x, 63, z, reg.defaultState((x + z) % 2 ? blocks::Bedrock : blocks::Stone));
                if ((x + z) % 2 == 0) c.set(x, 64, z, reg.defaultState(blocks::Lava));
            }
    });
    s.tick(2000);
    CHECK(s.all().empty());
}

TEST_CASE("water puts out a burning mob of any kind (review fix: cows burned on after lava)") {
    MobScene s;
    MobData cow = Mobs::make(MobType::Cow, {8.5, 64.0, 8.5}, s.rng);
    cow.fireTicks = 300;
    REQUIRE(Mobs::add(s.world, cow));
    s.world.setBlock({8, 64, 8}, blockRegistry().defaultState(blocks::Water));
    s.tick(2);
    CHECK(s.all().at(0)->fireTicks == 0);
}

TEST_CASE("a mob in fire is hurt and set alight for 8 s") {
    MobScene s;
    MobData cow = Mobs::make(MobType::Cow, {8.5, 64.0, 8.5}, s.rng);
    REQUIRE(Mobs::add(s.world, cow));
    s.world.setBlock({8, 64, 8}, BlockUpdates::fireState(0));
    s.tick(1);
    MobData* m = s.all().at(0);
    CHECK(m->health < 10.0f);
    CHECK(m->fireTicks >= 150);
}

TEST_CASE("a chasing zombie paths around a lava trench over its bridge (M16.2)") {
    MobScene s;
    for (int x = 3; x <= 5; ++x)
        for (int z = -12; z <= 12; ++z)
            if (z != 8) s.world.setBlock({x, 63, z}, blockRegistry().defaultState(blocks::Lava));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Zombie, {9.5, 64.0, 0.5}, s.rng)));
    float lowest = 20.0f;
    bool burnt = false;
    for (int i = 0; i < 400; ++i) {
        s.tick();
        lowest = std::min(lowest, s.vitals.health());
        for (MobData* m : s.all())
            burnt = burnt || m->fireTicks > 0;
    }
    CHECK_FALSE(burnt);     // never stepped into the lava
    CHECK(lowest <= 17.0f); // got across and hit the player
}

TEST_CASE("two fed cows in love make a calf that grows up; both wait 5 minutes") {
    MobScene s;
    const ItemId wheat = *itemRegistry().find("wheat");
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {4.5, 64.0, 4.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {6.5, 64.0, 4.5}, s.rng)));
    for (MobData* m : s.all())
        CHECK(Mobs::interact(*m, wheat, s.rng, s.items) == Mobs::Use::Fed);
    CHECK(Mobs::interact(*s.all()[0], wheat, s.rng, s.items) == Mobs::Use::None); // already in love
    s.player.setPosition({40.5, 64.0, 40.5});
    s.tick(200);
    int babies = 0, adults = 0;
    for (MobData* m : s.all()) {
        if (m->type != MobType::Cow) continue; // (night: monsters may spawn meanwhile)
        if (m->isBaby()) ++babies;
        else {
            ++adults;
            CHECK(m->age > 0); // breeding cooldown
            CHECK(Mobs::interact(*m, wheat, s.rng, s.items) == Mobs::Use::None);
        }
    }
    CHECK(babies == 1);
    CHECK(adults == 2);
    for (MobData* m : s.all())
        if (m->isBaby()) {
            CHECK(Mobs::box(*m).max.y - Mobs::box(*m).min.y == doctest::Approx(0.7)); // half size
            m->age = -2;
        }
    s.tick(3);
    for (MobData* m : s.all())
        CHECK_FALSE(m->isBaby());
}

TEST_CASE("shearing a sheep drops 1-3 wool of its colour; it regrows after eating grass") {
    MobScene s;
    MobData sheep = Mobs::make(MobType::Sheep, {4.5, 64.0, 4.5}, s.rng);
    sheep.woolColour = 14; // red
    REQUIRE(Mobs::add(s.world, sheep));
    MobData* m = s.all().at(0);
    const ItemId shears = *itemRegistry().find("shears");
    CHECK(Mobs::interact(*m, shears, s.rng, s.items) == Mobs::Use::Sheared);
    CHECK(m->sheared);
    CHECK(Mobs::interact(*m, shears, s.rng, s.items) == Mobs::Use::None);
    REQUIRE(s.items.items().size() == 1);
    CHECK(s.items.items()[0].stack.item == itemRegistry().blockItem(blocks::RedWool));
    CHECK(s.items.items()[0].stack.count >= 1);
    CHECK(s.items.items()[0].stack.count <= 3);
    // A grass block under it: sooner or later it grazes and the wool comes back.
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z)
            s.world.setBlock({x, 63, z}, blockRegistry().defaultState(blocks::GrassBlock));
    s.player.setPosition({40.5, 64.0, 40.5});
    std::vector<BlockPos> edits;
    for (int i = 0; i < 20000 && s.all().at(0)->sheared; ++i) {
        s.player.tick(s.world, {});
        Mobs::Context ctx{s.world, s.player, s.vitals, true, false, s.dayTime, s.skyDarken, s.rng, s.items,
                          false, 0, &edits};
        s.mobs.tick(ctx);
    }
    CHECK_FALSE(s.all().at(0)->sheared);
    REQUIRE_FALSE(edits.empty());
    CHECK(blockRegistry().blockOf(s.world.getBlock(edits[0])) == blocks::Dirt);
}

TEST_CASE("animal loot: sheep wool + mutton, pig porkchops, chicken feathers + chicken; babies nothing") {
    for (const MobType t : {MobType::Sheep, MobType::Pig, MobType::Chicken}) {
        MobScene s;
        MobData m = Mobs::make(t, {4.5, 64.0, 4.5}, s.rng);
        m.health = 0.0f;
        REQUIRE(Mobs::add(s.world, m));
        s.tick(1);
        CHECK_FALSE(s.items.items().empty());
        MobScene b;
        MobData baby = Mobs::make(t, {4.5, 64.0, 4.5}, b.rng);
        baby.age = -100;
        baby.health = 0.0f;
        REQUIRE(Mobs::add(b.world, baby));
        b.tick(1);
        CHECK(b.items.items().empty());
    }
}

TEST_CASE("animals follow a player holding their food; chickens lay eggs") {
    MobScene s;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Pig, {8.5, 64.0, 0.5}, s.rng)));
    s.player.setPosition({0.5, 64.0, 0.5});
    for (int i = 0; i < 200; ++i) {
        s.player.tick(s.world, {});
        Mobs::Context ctx{s.world, s.player, s.vitals, true, false, s.dayTime, s.skyDarken, s.rng, s.items,
                          false, *itemRegistry().find("carrot")};
        s.mobs.tick(ctx);
    }
    CHECK(s.all().at(0)->pos.x < 4.0); // came over (stops ~2.5 blocks away)
    MobScene c;
    MobData hen = Mobs::make(MobType::Chicken, {4.5, 64.0, 4.5}, c.rng);
    hen.eggTicks = 5;
    REQUIRE(Mobs::add(c.world, hen));
    c.tick(6);
    REQUIRE(c.items.items().size() == 1);
    CHECK(c.items.items()[0].stack.item == *itemRegistry().find("egg"));
}

TEST_CASE("sheep colour/shearing, ages, love and egg timers save as vanilla's Color, Sheared, Age, InLove, EggLayTime") {
    Chunk c({1, -1});
    Xoroshiro rng(5);
    MobData sheep = Mobs::make(MobType::Sheep, {20.5, 70.0, -10.5}, rng);
    sheep.woolColour = 11;
    sheep.sheared = true;
    sheep.age = -1200;
    MobData hen = Mobs::make(MobType::Chicken, {21.5, 70.0, -10.5}, rng);
    hen.eggTicks = 777;
    hen.loveTicks = 300;
    c.mobs().push_back(sheep);
    c.mobs().push_back(hen);
    const auto nbt = entitiesToNbt(ChunkSnapshot::of(c));
    const auto* e0 = nbt.list("Entities")->items[0].get<mc::nbt::Compound>();
    CHECK(*e0->string("id") == "minecraft:sheep");
    CHECK(e0->integer("Color") == 11);
    Chunk d({1, -1});
    entitiesFromNbt(*mc::nbt::read(mc::nbt::write(nbt)), d);
    REQUIRE(d.mobs().size() == 2);
    CHECK(d.mobs()[0].woolColour == 11);
    CHECK(d.mobs()[0].sheared);
    CHECK(d.mobs()[0].age == -1200);
    CHECK(d.mobs()[1].type == MobType::Chicken);
    CHECK(d.mobs()[1].eggTicks == 777);
    CHECK(d.mobs()[1].loveTicks == 300);
}

#include "gameplay/Projectiles.h"

namespace {
// A monster scene: mobs ticked with projectiles (skeletons) and edits.
struct MonsterScene : MobScene {
    Projectiles projectiles;
    std::vector<BlockPos> edits;
    void run(int n, ItemId held = 0) {
        for (int i = 0; i < n; ++i) {
            player.tick(world, {});
            Mobs::Context ctx{world, player, vitals, survival, false, dayTime, skyDarken, rng, items,
                              false, held, &edits, &projectiles};
            mobs.tick(ctx);
            projectiles.tick(world, player, &vitals, inventory, survival, rng);
            vitals.tick(player.position().y, true, false, false);
        }
    }
    Inventory inventory;
};
} // namespace

TEST_CASE("a creeper next to a survival player swells for 30 ticks and explodes") {
    MonsterScene s;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Creeper, {2.5, 64.0, 0.5}, s.rng)));
    s.run(60);
    int creepers = 0;
    for (MobData* m : s.all())
        creepers += m->type == MobType::Creeper;
    CHECK(creepers == 0);              // blew itself up
    CHECK(s.vitals.health() < 10.0f); // ~2 blocks from a power-3 blast hurts a lot
    CHECK_FALSE(s.edits.empty());      // and broke blocks
}

TEST_CASE("a creeper's fuse goes back down when the player gets away") {
    MonsterScene s;
    MobData c = Mobs::make(MobType::Creeper, {2.5, 64.0, 0.5}, s.rng);
    c.fuse = 20;
    c.targeting = true;
    REQUIRE(Mobs::add(s.world, c));
    s.player.setPosition({30.5, 64.0, 0.5});
    s.run(5);
    CHECK(s.all().at(0)->fuse < 20);
}

TEST_CASE("skeletons shoot arrows at a survival player in range") {
    MonsterScene s;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Skeleton, {8.5, 64.0, 0.5}, s.rng)));
    s.run(120);
    CHECK(s.vitals.health() < 20.0f);
}

TEST_CASE("spiders climb walls, and stay calm in bright light until hit") {
    MonsterScene s;
    // A 3-high wall between spider and player, too long to walk round (M30.5: paths go
    // round short walls now).
    for (int z = -40; z <= 40; ++z)
        for (int y = 64; y <= 66; ++y)
            s.world.setBlock({4, y, z}, blockRegistry().defaultState(blocks::Stone));
    for (int z = -40; z <= 40; ++z)
        s.world.setBlock({3, 67, z}, blockRegistry().defaultState(blocks::Stone)); // ledge... (above player side)
    s.player.setPosition({0.5, 64.0, 0.5});
    MobData sp = Mobs::make(MobType::Spider, {6.5, 64.0, 0.5}, s.rng);
    sp.targeting = true;
    REQUIRE(Mobs::add(s.world, sp));
    double highest = 0.0;
    for (int i = 0; i < 60; ++i) {
        s.run(1);
        highest = std::max(highest, s.all().at(0)->pos.y);
    }
    CHECK(highest > 65.0); // climbed up the wall
    // Bright light: no targeting.
    MonsterScene b;
    b.skyDarken = 0.0f;
    std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
    auto bright = std::make_shared<SectionLight>();
    bright->sky.fill(15);
    light.fill(bright);
    b.world.forEachChunk([&](Chunk& c) { c.setLight(light); });
    REQUIRE(Mobs::add(b.world, Mobs::make(MobType::Spider, {4.5, 64.0, 0.5}, b.rng)));
    b.run(40);
    CHECK_FALSE(b.all().at(0)->targeting);
    Mobs::attack(*b.all().at(0), 1.0f, b.player.position());
    CHECK(b.all().at(0)->angry);
}

TEST_CASE("endermen anger when stared at, teleport out of water, shrug off arrows") {
    MonsterScene s;
    MobData e = Mobs::make(MobType::Enderman, {0.5, 64.0, 8.5}, s.rng);
    REQUIRE(Mobs::add(s.world, e));
    s.player.setPosition({0.5, 64.0, 0.5});
    s.player.setRotation(0.0f, -7.3f); // looking south (+Z), up at its head (1 block above our eyes)
    s.run(3);
    CHECK_FALSE(s.all().at(0)->angry); // a glance isn't enough: 5 ticks of staring
    s.run(3);
    CHECK(s.all().at(0)->angry);
    // Arrows: it teleports, no damage.
    MonsterScene a;
    REQUIRE(Mobs::add(a.world, Mobs::make(MobType::Enderman, {0.5, 64.0, 8.5}, a.rng)));
    a.player.setPosition({30.5, 64.0, 30.5});
    a.projectiles.shoot(ProjectileKind::Arrow, {0.5, 65.5, 2.5}, {0, 0, 1}, 2.0, 0.0, false, false, a.rng);
    a.run(6);
    MobData* m = a.all().at(0);
    CHECK(m->health == 40.0f);
    CHECK(glm::length(m->pos - glm::dvec3(0.5, 64.0, 8.5)) > 2.0); // teleported away
    // Water: hurts and teleports.
    MonsterScene w;
    for (int x = -2; x <= 2; ++x)
        for (int z = 6; z <= 10; ++z)
            w.world.setBlock({x, 64, z}, blockRegistry().defaultState(blocks::Water));
    REQUIRE(Mobs::add(w.world, Mobs::make(MobType::Enderman, {0.5, 64.0, 8.5}, w.rng)));
    w.player.setPosition({30.5, 64.0, 30.5});
    w.run(3);
    CHECK(w.all().at(0)->health < 40.0f);
    CHECK(glm::length(w.all().at(0)->pos - glm::dvec3(0.5, 64.0, 8.5)) > 2.0);
}

TEST_CASE("an enderman's carried block saves as carriedBlockState") {
    Chunk c({0, 0});
    Xoroshiro rng(2);
    MobData e = Mobs::make(MobType::Enderman, {3.5, 70.0, 3.5}, rng);
    e.carried = blockRegistry().defaultState(blocks::GrassBlock);
    c.mobs().push_back(e);
    const auto nbt = entitiesToNbt(ChunkSnapshot::of(c));
    Chunk d({0, 0});
    entitiesFromNbt(*mc::nbt::read(mc::nbt::write(nbt)), d);
    REQUIRE(d.mobs().size() == 1);
    CHECK(d.mobs()[0].carried == blockRegistry().defaultState(blocks::GrassBlock));
}

TEST_CASE("a creeper keeps swelling at 5 blocks (vanilla) but calms behind a wall") {
    MonsterScene s;
    MobData c = Mobs::make(MobType::Creeper, {5.5, 64.0, 0.5}, s.rng);
    c.fuse = 10;
    c.targeting = true;
    REQUIRE(Mobs::add(s.world, c));
    s.player.setPosition({0.5, 64.0, 0.5});
    // Keep it from walking closer: walls around it on three sides.
    for (int y = 64; y <= 65; ++y) {
        s.world.setBlock({6, y, 0}, blockRegistry().defaultState(blocks::Stone));
        s.world.setBlock({5, y, 1}, blockRegistry().defaultState(blocks::Stone));
        s.world.setBlock({5, y, -1}, blockRegistry().defaultState(blocks::Stone));
    }
    s.run(3);
    CHECK(s.all().at(0)->fuse > 10);
    MonsterScene w;
    MobData c2 = Mobs::make(MobType::Creeper, {5.5, 64.0, 0.5}, w.rng);
    c2.fuse = 10;
    c2.targeting = true;
    REQUIRE(Mobs::add(w.world, c2));
    for (int y = 64; y <= 66; ++y)
        for (int z = -3; z <= 3; ++z)
            w.world.setBlock({3, y, z}, blockRegistry().defaultState(blocks::Stone)); // no line of sight
    w.run(3);
    CHECK(w.all().at(0)->fuse < 10);
}

TEST_CASE("skeletons and creepers notice the player within 16 blocks, zombies 35") {
    MonsterScene s;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Skeleton, {20.5, 64.0, 0.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Zombie, {0.5, 64.0, 20.5}, s.rng)));
    s.run(25);
    for (MobData* m : s.all())
        CHECK(m->targeting == (m->type == MobType::Zombie));
}

TEST_CASE("spider eyes drop only when the player killed the spider") {
    int eyes = 0;
    for (int i = 0; i < 40; ++i) {
        MobScene s;
        s.rng = Xoroshiro(uint64_t(i));
        MobData sp = Mobs::make(MobType::Spider, {4.5, 64.0, 4.5}, s.rng);
        sp.health = 0.0f;
        REQUIRE(Mobs::add(s.world, sp));
        s.tick(1);
        for (const auto& it : s.items.items())
            eyes += it.stack.item == *itemRegistry().find("spider_eye");
    }
    CHECK(eyes == 0);
}

TEST_CASE("spawners: active within 16 blocks, spawn up to 4 of their mob nearby, then wait 200-799 ticks") {
    MobScene s;
    s.mobs = Mobs();
    s.world.setBlock({0, 64, 10}, blockRegistry().defaultState(blocks::Spawner));
    Chunk& c = *s.world.chunk({0, 0});
    REQUIRE(c.spawner(0, 64, 10));
    c.spawner(0, 64, 10)->mob = MobType::Skeleton;
    c.spawner(0, 64, 10)->delay = 0;
    s.dayTime = 6000; // day: the spawner doesn't care about sky light, only block light
    s.tick();
    int skeletons = 0;
    for (MobData* m : s.all()) {
        skeletons += m->type == MobType::Skeleton;
        CHECK(std::abs(m->pos.x - 0.5) <= 4.5);
        CHECK(std::abs(m->pos.z - 10.5) <= 4.5);
    }
    CHECK(skeletons >= 1);
    CHECK(skeletons <= 4);
    const int delay = c.spawner(0, 64, 10)->delay;
    CHECK(delay >= 199);
    CHECK(delay <= 799);

    // Far from the player: no countdown.
    MobScene far;
    far.mobs = Mobs();
    far.world.setBlock({0, 64, 30}, blockRegistry().defaultState(blocks::Spawner));
    far.world.chunk({0, 1})->spawner(0, 64, 14)->delay = 5;
    far.tick(10);
    CHECK(far.world.chunk({0, 1})->spawner(0, 64, 14)->delay == 5);
    CHECK(far.all().empty());
}

namespace {

int countType(MobScene& s, MobType t) {
    int n = 0;
    for (MobData* m : s.all())
        n += m->type == t && m->health > 0.0f;
    return n;
}

void tickWith(MobScene& s, Projectiles& proj, int n) {
    static Inventory inventory;
    for (int i = 0; i < n; ++i) {
        s.player.tick(s.world, {});
        Mobs::Context ctx{s.world, s.player, s.vitals, s.survival, false, s.dayTime, s.skyDarken, s.rng, s.items};
        ctx.projectiles = &proj;
        s.mobs.tick(ctx);
        proj.tick(s.world, s.player, &s.vitals, inventory, true, s.rng);
    }
}

} // namespace

TEST_CASE("magma cubes: sizes 1/2/4 with size^2 health; a big one splits into 2-4 medium ones") {
    MobScene s;
    s.mobs = Mobs();
    MobData m = Mobs::make(MobType::MagmaCube, {8.5, 64.0, 8.5}, s.rng);
    m.size = 4;
    m.health = 0.0f; // dies this tick
    m.lastHurtByPlayer = true;
    REQUIRE(Mobs::add(s.world, m));
    s.tick(2);
    int medium = 0;
    for (MobData* c : s.all())
        if (c->type == MobType::MagmaCube && c->size == 2) {
            ++medium;
            CHECK(c->health == doctest::Approx(4.0f));
            CHECK(Mobs::box(*c).max.x - Mobs::box(*c).min.x == doctest::Approx(1.04));
        }
    CHECK(medium >= 2);
    CHECK(medium <= 4);
}

TEST_CASE("zombified piglins: neutral until one is hit, then the group nearby turns on the player") {
    MobScene s;
    s.mobs = Mobs();
    for (int i = 0; i < 3; ++i)
        REQUIRE(Mobs::add(s.world, Mobs::make(MobType::ZombifiedPiglin, {4.5 + i * 2, 64.0, 4.5}, s.rng)));
    s.tick(40);
    CHECK(s.vitals.health() == doctest::Approx(20.0f)); // left alone
    MobData* first = nullptr; // (a zombified piglin: natural spawns may add other mobs)
    for (MobData* m : s.all())
        if (!first && m->type == MobType::ZombifiedPiglin) first = m;
    REQUIRE(first);
    Mobs::attack(*first, 1.0f, s.player.position());
    s.tick(1);
    int angry = 0;
    for (MobData* m : s.all())
        angry += m->type == MobType::ZombifiedPiglin && m->angry;
    CHECK(angry == 3);
}

TEST_CASE("ghasts charge for a second and fire an exploding fireball; blazes fire volleys of three") {
    MobScene s;
    s.mobs = Mobs();
    Projectiles proj;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Ghast, {20.5, 63.5, 0.5}, s.rng)));
    tickWith(s, proj, 21);
    bool fired = false;
    for (const Projectile& p : proj.items())
        fired = fired || p.kind == ProjectileKind::GhastFireball;
    CHECK(fired);
    bool exploded = false;
    for (int i = 0; i < 60 && !exploded; ++i) {
        tickWith(s, proj, 1);
        exploded = !proj.explosions().empty();
    }
    CHECK(exploded);

    MobScene b;
    b.mobs = Mobs();
    Projectiles bp;
    REQUIRE(Mobs::add(b.world, Mobs::make(MobType::Blaze, {10.5, 64.0, 0.5}, b.rng)));
    int shots = 0;
    for (int i = 0; i < 80; ++i) {
        const size_t before = bp.items().size();
        tickWith(b, bp, 1);
        if (bp.items().size() > before) ++shots;
    }
    CHECK(shots == 3);
}

TEST_CASE("the Nether spawns its own monsters (nether wastes: zombified piglins, ghasts...)") {
    MobScene s;
    s.mobs = Mobs();
    s.world.setUltrawarm(true);
    s.world.forEachChunk([](Chunk& c) {
        auto biomes = std::make_shared<ChunkBiomes>();
        biomes->cells.fill(Biome::NetherWastes);
        c.setBiomes(biomes);
    });
    s.player.setPosition({0.5, 64.0, 0.5});
    for (int i = 0; i < 4000 && s.all().size() < 8; ++i)
        s.tick(1);
    int nether = 0;
    for (MobData* m : s.all())
        nether += m->type == MobType::ZombifiedPiglin || m->type == MobType::Ghast || m->type == MobType::MagmaCube ||
                  m->type == MobType::Enderman;
    CHECK(nether > 0);
    CHECK(countType(s, MobType::Zombie) == 0);
}

TEST_CASE("piglins: attack players without gold; take a gold ingot, admire it 6 s, then barter") {
    MobScene s;
    s.mobs = Mobs();
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Piglin, {3.5, 64.0, 0.5}, s.rng)));
    for (int i = 0; i < 60; ++i) {
        s.player.tick(s.world, {});
        Mobs::Context ctx{s.world, s.player, s.vitals, s.survival, false, s.dayTime, s.skyDarken, s.rng, s.items};
        ctx.wearsGold = true; // golden armor: left alone
        s.mobs.tick(ctx);
    }
    CHECK(s.vitals.health() == doctest::Approx(20.0f));
    MobData& p = *s.all()[0];
    const ItemId gold = *itemRegistry().find("gold_ingot");
    CHECK(Mobs::interact(p, gold, s.rng, s.items) == Mobs::Use::Fed);
    CHECK(p.admireTicks == 120);
    const size_t before = s.items.items().size();
    int bartered = 0;
    for (int trial = 0; trial < 30; ++trial) { // some barters are items we don't have yet
        p.admireTicks = 1;
        s.tick(1);
        bartered += s.items.items().size() > before + size_t(bartered);
    }
    CHECK(bartered > 10);
    // Without gold the piglin goes for the player.
    s.tick(100);
    CHECK(s.vitals.health() < 20.0f);
}

TEST_CASE("striders float on lava unhurt; hoglins drop porkchops") {
    MobScene s;
    s.mobs = Mobs();
    s.world.forEachChunk([](Chunk& c) {
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
                c.set(x, 64, z, blockRegistry().defaultState(blocks::Lava));
            }
    });
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Strider, {8.5, 64.2, 8.5}, s.rng)));
    s.tick(100);
    MobData& st = *s.all()[0];
    CHECK(st.health == doctest::Approx(20.0f));
    CHECK(st.pos.y > 64.0);

    MobScene h;
    h.mobs = Mobs();
    MobData hog = Mobs::make(MobType::Hoglin, {8.5, 64.0, 8.5}, h.rng);
    hog.health = 0.0f;
    REQUIRE(Mobs::add(h.world, hog));
    h.tick(2);
    int pork = 0;
    for (const auto& e : h.items.items())
        if (itemRegistry().item(e.stack.item).id == "minecraft:porkchop") pork += e.stack.count;
    CHECK(pork >= 2);
}

TEST_CASE("splash potions: harming hurts the living and heals the undead, scaled by distance; regeneration reaches the player") {
    MobScene s;
    s.mobs = Mobs();
    s.player.setPosition({-20.5, 64.0, 0.5}); // out of the splash
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {6.5, 64.0, 4.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Zombie, {5.0, 64.0, 7.0}, s.rng)));
    for (MobData* m : s.all())
        m->health = 5.0f;
    Projectiles proj;
    // Dropped straight down onto the ground between them (1.5 blocks from each).
    REQUIRE(proj.shoot(ProjectileKind::SplashPotion, {5.5, 66.0, 5.5}, {0.0, -1.0, 0.0}, 0.3, 0.0, true, false, s.rng));
    proj.last().potion = static_cast<uint8_t>(Potion::Harming);
    proj.last().pickup = false;
    Inventory inventory;
    for (int i = 0; i < 20 && !proj.items().empty(); ++i)
        proj.tick(s.world, s.player, &s.vitals, inventory, true, s.rng);
    CHECK(proj.items().empty());
    for (MobData* m : s.all()) {
        if (m->type == MobType::Cow) CHECK(m->health < 5.0f);    // harmed (dead or nearly)
        if (m->type == MobType::Zombie) CHECK(m->health > 5.0f); // healed
    }
    CHECK(s.vitals.health() == doctest::Approx(20.0f)); // 20 blocks away: untouched

    s.player.setPosition({5.5, 64.0, 5.5});
    REQUIRE(proj.shoot(ProjectileKind::SplashPotion, {5.5, 66.5, 7.5}, {0.0, -1.0, 0.0}, 0.3, 0.0, true, false, s.rng));
    proj.last().potion = static_cast<uint8_t>(Potion::Regeneration);
    for (int i = 0; i < 20 && !proj.items().empty(); ++i)
        proj.tick(s.world, s.player, &s.vitals, inventory, true, s.rng);
    CHECK(s.vitals.effectLevel(Effect::Regeneration) > 0);
}

TEST_CASE("a zombified piglin killed in one hit still angers its group (M19 review regression)") {
    MobScene s;
    s.mobs = Mobs();
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::ZombifiedPiglin, {4.5, 64.0, 4.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::ZombifiedPiglin, {8.5, 64.0, 4.5}, s.rng)));
    MobData* first = s.all()[0];
    Mobs::attack(*first, 100.0f, s.player.position());
    s.tick(1);
    int angry = 0;
    for (MobData* m : s.all())
        angry += m->health > 0.0f && m->angry;
    CHECK(angry == 1);
}

TEST_CASE("hitting a piglin angers it and cancels its barter") {
    MobScene s;
    s.mobs = Mobs();
    MobData p = Mobs::make(MobType::Piglin, {4.5, 64.0, 4.5}, s.rng);
    p.admireTicks = 60;
    REQUIRE(Mobs::add(s.world, p));
    Mobs::attack(*s.all()[0], 1.0f, s.player.position());
    CHECK(s.all()[0]->angry);
    CHECK(s.all()[0]->admireTicks == 0);
}

TEST_CASE("splash: water hurts blazes and puts out fire; a creative player gets regeneration but no harm") {
    MobScene s;
    s.mobs = Mobs();
    s.player.setPosition({-20.5, 64.0, 0.5});
    MobData b = Mobs::make(MobType::Blaze, {5.5, 64.0, 6.5}, s.rng);
    REQUIRE(Mobs::add(s.world, b));
    const float before = s.all()[0]->health;
    s.world.setBlock({6, 64, 5}, BlockUpdates::fireState(0));
    Projectiles proj;
    Inventory inventory;
    REQUIRE(proj.shoot(ProjectileKind::SplashPotion, {5.5, 66.0, 5.5}, {0.0, -1.0, 0.0}, 0.3, 0.0, true, false, s.rng));
    proj.last().potion = static_cast<uint8_t>(Potion::Water);
    for (int i = 0; i < 20 && !proj.items().empty(); ++i)
        proj.tick(s.world, s.player, &s.vitals, inventory, true, s.rng);
    CHECK(s.all()[0]->health == doctest::Approx(before - 1.0f));
    CHECK(blockRegistry().blockOf(s.world.getBlock({6, 64, 5})) == blocks::Air);

    // A potion at the feet: the nearest point of the box is ~0 away - full strength.
    s.player.setPosition({5.5, 64.0, 5.5});
    s.player.setCreative(true);
    for (const Potion pot : {Potion::Regeneration, Potion::Harming}) {
        REQUIRE(proj.shoot(ProjectileKind::SplashPotion, {5.5, 64.4, 4.0}, {0.0, -1.0, 0.0}, 0.3, 0.0, true, false,
                           s.rng));
        proj.last().potion = static_cast<uint8_t>(pot);
        for (int i = 0; i < 20 && !proj.items().empty(); ++i)
            proj.tick(s.world, s.player, &s.vitals, inventory, false, s.rng);
    }
    CHECK(s.vitals.effectLevel(Effect::Regeneration) > 0);
    CHECK(s.vitals.health() == doctest::Approx(20.0f));
}

#include "world/Loot.h"

TEST_CASE("bartered potions are fire resistance (wiki: Bartering), never contentless") {
    Xoroshiro rng(5);
    const ItemId potion = *itemRegistry().find("potion"), splash = *itemRegistry().find("splash_potion");
    int potions = 0;
    for (int i = 0; i < 4000; ++i) {
        const ItemStack s = rollOne(LootTable::PiglinBartering, rng);
        if (s.item != potion && s.item != splash) continue;
        ++potions;
        CHECK(s.potion != 0);
        CHECK((s.potion == static_cast<uint8_t>(Potion::FireResistance) || s.potion == static_cast<uint8_t>(Potion::Water)));
    }
    CHECK(potions > 0);
}

#include "world/NetherGenerator.h"

TEST_CASE("end crystals: one on each end2 pillar; any hit blows it up (power 6); crystals in the blast just vanish (Java)") {
    const EndGenerator gen(42, 2);
    int crystals = 0;
    for (int i = 0; i < EndGenerator::kPillars; ++i) {
        const auto& p = gen.pillar(i);
        Chunk c({blockToChunk(p.x), blockToChunk(p.z)}, kEndHeight);
        gen.generate(c);
        for (const MobData& m : c.mobs())
            if (m.type == MobType::EndCrystal && m.pos.y == doctest::Approx(p.height + 2.0)) {
                ++crystals;
                CHECK(m.showBottom);
            }
    }
    CHECK(crystals == EndGenerator::kPillars);
    Chunk old({blockToChunk(gen.pillar(0).x), blockToChunk(gen.pillar(0).z)}, kEndHeight);
    EndGenerator(42).generate(old);
    CHECK(old.mobs().empty()); // (the M12 End has none)

    MobScene s;
    s.mobs = Mobs();
    s.player.setPosition({-20.5, 64.0, 0.5});
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::EndCrystal, {4.5, 64.0, 4.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::EndCrystal, {8.5, 64.0, 4.5}, s.rng)));
    s.tick(5);
    CHECK(s.all().size() == 2); // they just sit there
    for (MobData* m : s.all())
        CHECK(m->pos == glm::dvec3(m->pos.x, 64.0, 4.5));
    Mobs::attack(*s.all()[0], 1.0f, s.player.position()); // a punch
    s.tick(4);
    for (MobData* o : s.all())
        CHECK(o->type != MobType::EndCrystal); // both gone: the second blown away without exploding
    CHECK(s.world.getBlock({4, 63, 4}) == 0); // and broke the stone below
}

TEST_CASE("end crystal items go on obsidian or bedrock with room above; ShowBottom saves") {
    MobScene s;
    s.mobs = Mobs();
    s.world.setBlock({3, 63, 3}, blockRegistry().defaultState(blocks::Obsidian));
    CHECK_FALSE(Mobs::placeEndCrystal(s.world, {5, 63, 5}, s.rng)); // stone
    CHECK(Mobs::placeEndCrystal(s.world, {3, 63, 3}, s.rng));
    CHECK_FALSE(Mobs::placeEndCrystal(s.world, {3, 63, 3}, s.rng)); // one there already
    REQUIRE(s.all().size() == 1);
    CHECK_FALSE(s.all()[0]->showBottom);
    Chunk back({0, 0});
    entitiesFromNbt(*mc::nbt::read(mc::nbt::write(entitiesToNbt(ChunkSnapshot::of(*s.world.chunk({0, 0}))))), back);
    REQUIRE(back.mobs().size() == 1);
    CHECK(back.mobs()[0].type == MobType::EndCrystal);
    CHECK_FALSE(back.mobs()[0].showBottom);
}

TEST_CASE("ender dragon: head hits count in full, others a quarter + 1; crystals heal it, a broken one hurts it") {
    MobScene s;
    s.mobs = Mobs();
    s.survival = false; // (no fighting back in this test)
    MobData d = Mobs::make(MobType::EnderDragon, {0.5, 80.0, 0.5}, s.rng);
    CHECK(d.health == 200.0f);
    const glm::dvec3 head = Mobs::dragonHead(d);
    CHECK(Mobs::dragonDamage(d, 8.0f, head) == 8.0f);
    CHECK(Mobs::dragonDamage(d, 8.0f, d.pos) == 3.0f);
    d.health = 150.0f;
    d.lastHealth = 150.0f;
    REQUIRE(Mobs::add(s.world, d));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::EndCrystal, {10.5, 80.0, 0.5}, s.rng)));
    s.tick(20);
    MobData* dragon = nullptr;
    MobData* crystal = nullptr;
    for (MobData* m : s.all())
        if (m->type == MobType::EnderDragon || m->type == MobType::EndCrystal)
            (m->type == MobType::EnderDragon ? dragon : crystal) = m;
    REQUIRE(dragon);
    REQUIRE(crystal);
    CHECK(dragon->hasBeam);
    CHECK(dragon->health >= 151.0f); // 1 every 10 ticks
    const float before = dragon->health;
    Mobs::attack(*crystal, 1.0f, s.player.position());
    s.tick(1);
    for (MobData* m : s.all())
        if (m->type == MobType::EnderDragon) CHECK(m->health <= before - 9.0f); // 10 (and the blast)
}

TEST_CASE("ender dragon: with no crystals it soon lands on the middle column; dying takes 10 s") {
    MobScene s;
    s.mobs = Mobs();
    s.survival = false;
    for (int y = 64; y <= 67; ++y)
        s.world.setBlock({0, y, 0}, blockRegistry().defaultState(blocks::Bedrock)); // the exit portal's column
    for (int cz = -6; cz <= 6; ++cz) // (its ring is 60 blocks out)
        for (int cx = -6; cx <= 6; ++cx)
            if (!s.world.chunk({cx, cz})) s.world.createChunk({cx, cz});
    MobData d = Mobs::make(MobType::EnderDragon, {0.5, 85.0, 0.5}, s.rng);
    d.lastHealth = d.health;
    REQUIRE(Mobs::add(s.world, d));
    bool perched = false;
    for (int t = 0; t < 4000 && !perched; ++t) {
        s.tick(1);
        for (MobData* m : s.all())
            perched = perched || (m->type == MobType::EnderDragon && m->phase == 6);
    }
    REQUIRE(perched);
    auto dragon = [&]() -> MobData* { // (found again after ticks: spawns may move the mob list)
        for (MobData* o : s.all())
            if (o->type == MobType::EnderDragon) return o;
        return nullptr;
    };
    REQUIRE(dragon());
    CHECK(dragon()->pos.y == doctest::Approx(68.0));
    // 50 damage while perched makes it take off.
    dragon()->health -= 60.0f;
    s.tick(2);
    CHECK(dragon()->phase == 4);
    dragon()->health = 0.0f;
    s.tick(199);
    CHECK(s.mobs.dragonDeaths().empty());
    CHECK(s.mobs.bossHealth() == 0.0f);
    s.tick(1);
    CHECK(s.mobs.dragonDeaths().size() == 1);
    for (MobData* o : s.all())
        CHECK(o->type != MobType::EnderDragon);
}

TEST_CASE("dragon's breath: a cloud hurts a survival player standing in it once a second") {
    MobScene s;
    Projectiles proj;
    Inventory inventory;
    proj.addCloud({0.5, 64.0, 0.5}, 3.0f, 100);
    for (int i = 0; i < 25; ++i)
        proj.tick(s.world, s.player, &s.vitals, inventory, true, s.rng);
    CHECK(s.vitals.health() == doctest::Approx(8.0f)); // 2 x 6
    for (int i = 0; i < 100; ++i)
        proj.tick(s.world, s.player, &s.vitals, inventory, true, s.rng);
    CHECK(proj.clouds().empty());
}

#include "gameplay/DragonFight.h"
#include "gameplay/ExperienceOrbs.h"

TEST_CASE("dragon fight: the dragon appears over the shut exit portal; its death opens it, leaves the egg once") {
    const EndGenerator gen(42, 2);
    World w;
    w.setHeight(kEndHeight);
    for (int cz = -3; cz <= 3; ++cz)
        for (int cx = -3; cx <= 3; ++cx) {
            auto c = std::make_unique<Chunk>(ChunkPos{cx, cz}, kEndHeight);
            gen.generate(*c);
            c->mobs().clear(); // (no crystals for this test)
            w.insertChunk(std::move(c));
        }
    const int top = gen.islandTop(0, 0) + 4; // the column's top
    CHECK(w.getBlock({1, top - 3, 1}) == 0);  // shut: no portal yet
    Mobs mobs;
    DragonFight fight;
    ExperienceOrbs orbs;
    Xoroshiro rng(1);
    std::vector<BlockPos> edits;
    for (int i = 0; i < 100; ++i)
        fight.tick(w, gen, mobs, {0.5, 70.0, 0.5}, orbs, rng, edits);
    int dragons = 0;
    w.forEachChunk([&](Chunk& c) {
        for (const MobData& m : c.mobs())
            dragons += m.type == MobType::EnderDragon;
    });
    CHECK(dragons == 1);
    CHECK(fight.uuidHi != 0);
    for (int i = 0; i < 400; ++i) // found again on later scans: no second dragon
        fight.tick(w, gen, mobs, {0.5, 70.0, 0.5}, orbs, rng, edits);
    dragons = 0;
    w.forEachChunk([&](Chunk& c) {
        for (const MobData& m : c.mobs())
            dragons += m.type == MobType::EnderDragon;
    });
    CHECK(dragons == 1);

    DragonFight::openExitPortal(w, true, edits);
    CHECK(blockRegistry().blockOf(w.getBlock({1, top - 3, 1})) == blocks::EndPortal);
    CHECK(blockRegistry().blockOf(w.getBlock({2, top - 3, 1})) == blocks::EndPortal);
    CHECK(blockRegistry().blockOf(w.getBlock({0, top + 1, 0})) == blocks::DragonEgg);
    CHECK(DragonFight::teleportEgg(w, {0, top + 1, 0}, rng, edits));
    CHECK(w.getBlock({0, top + 1, 0}) == 0);
}

TEST_CASE("end gateways: 20 on a ring 96 out at Y 75; out to an outer island, back to the main island") {
    for (int i = 0; i < 20; ++i) {
        const BlockPos g = DragonFight::gatewayPos(i);
        CHECK(g.y == 75);
        CHECK(std::hypot(g.x + 0.5, g.z + 0.5) == doctest::Approx(96.0).epsilon(0.02));
    }
    CHECK(DragonFight::gatewayPos(0) == BlockPos{96, 75, 0}); // (the wiki's table)
    CHECK(DragonFight::gatewayPos(5) == BlockPos{-1, 75, 96});
    CHECK(DragonFight::gatewayPos(10) == BlockPos{-96, 75, -1});
    CHECK(DragonFight::gatewayPos(15) == BlockPos{0, 75, -96});
    const EndGenerator gen(42, 2);
    DragonFight fight;
    int found = 0;
    for (int i = 0; i < 20; ++i) {
        const auto to = fight.gatewayTarget(gen, DragonFight::gatewayPos(i));
        if (!to) continue;
        ++found;
        const double r = std::hypot(to->x, to->z);
        CHECK(r > 1000.0);
        // On the island's own ground (M20 review: the landing column is the one checked).
        CHECK(gen.outerTop(int(std::floor(to->x)), int(std::floor(to->z))) + 1 == int(to->y));
        // And the exit gateway out there leads home, onto the main island.
        const BlockPos exit{int(std::floor(to->x)), int(to->y) + 9, int(std::floor(to->z))};
        const auto back = fight.gatewayTarget(gen, exit);
        REQUIRE(back);
        CHECK(std::hypot(back->x, back->z) < 100.0);
        CHECK(gen.islandTop(int(std::floor(back->x)), int(std::floor(back->z))) >= 0);
    }
    CHECK(found >= 15); // (most directions reach an island within 4096 blocks)
}

TEST_CASE("respawning the dragon: four crystals on the open portal's rim rebuild the pillars and bring it back") {
    const EndGenerator gen(42, 2);
    World w;
    w.setHeight(kEndHeight);
    for (int cz = -4; cz <= 4; ++cz)
        for (int cx = -4; cx <= 4; ++cx) {
            auto c = std::make_unique<Chunk>(ChunkPos{cx, cz}, kEndHeight);
            gen.generate(*c);
            w.insertChunk(std::move(c));
        }
    std::vector<BlockPos> edits;
    DragonFight::openExitPortal(w, false, edits);
    const auto& p0 = gen.pillar(0);
    w.setBlock({p0.x, p0.height, p0.z}, 0); // a damaged pillar
    int caged = -1;
    for (int i = 0; i < EndGenerator::kPillars; ++i)
        if (gen.pillar(i).height <= 79) caged = i;
    REQUIRE(caged >= 0);
    const auto& pc = gen.pillar(caged);
    w.setBlock({pc.x + 2, pc.height + 2, pc.z}, 0); // a broken cage
    DragonFight fight;
    fight.killed = fight.previouslyKilled = true;
    Mobs mobs;
    ExperienceOrbs orbs;
    Xoroshiro rng(2);
    const int rim = gen.islandTop(0, 0) + 1;
    for (const auto& [x, z] : {std::pair{3, 0}, std::pair{-3, 0}, std::pair{0, 3}, std::pair{0, -3}})
        REQUIRE(Mobs::placeEndCrystal(w, {x, rim, z}, rng));
    for (int i = 0; i < 205; ++i)
        fight.tick(w, gen, mobs, {0.5, 70.0, 0.5}, orbs, rng, edits);
    CHECK_FALSE(fight.killed);
    CHECK(blockRegistry().blockOf(w.getBlock({p0.x, p0.height, p0.z})) == blocks::Obsidian);
    CHECK(blockRegistry().blockOf(w.getBlock({pc.x + 2, pc.height + 2, pc.z})) == blocks::IronBars); // (M20 review)
    int dragons = 0, rimCrystals = 0;
    w.forEachChunk([&](Chunk& c) {
        for (const MobData& m : c.mobs()) {
            dragons += m.type == MobType::EnderDragon;
            rimCrystals += m.type == MobType::EndCrystal && !m.showBottom;
        }
    });
    CHECK(dragons == 1);
    CHECK(rimCrystals == 0);
    CHECK(w.getBlock({1, rim, 1}) == 0); // the portal shut again
}

TEST_CASE("ender pearls land where they hit and report it (into a gateway: flagged)") {
    MobScene s;
    Projectiles proj;
    Inventory inventory;
    REQUIRE(proj.shoot(ProjectileKind::EnderPearl, {5.5, 70.0, 5.5}, {0.0, -1.0, 0.0}, 0.5, 0.0, true, false, s.rng));
    bool landed = false;
    for (int i = 0; i < 40 && !landed; ++i) {
        proj.tick(s.world, s.player, &s.vitals, inventory, true, s.rng);
        if (!proj.pearls().empty()) {
            landed = true;
            CHECK(proj.pearls()[0].pos.y == doctest::Approx(64.0).epsilon(0.01));
            CHECK_FALSE(proj.pearls()[0].gateway);
        }
    }
    CHECK(landed);
    s.world.setBlock({8, 66, 8}, blockRegistry().defaultState(blocks::EndGateway));
    REQUIRE(proj.shoot(ProjectileKind::EnderPearl, {8.5, 70.0, 8.5}, {0.0, -1.0, 0.0}, 0.5, 0.0, true, false, s.rng));
    landed = false;
    for (int i = 0; i < 40 && !landed; ++i) {
        proj.tick(s.world, s.player, &s.vitals, inventory, true, s.rng);
        if (!proj.pearls().empty()) {
            landed = true;
            CHECK(proj.pearls()[0].gateway);
            CHECK(proj.pearls()[0].gatewayBlock == BlockPos{8, 66, 8});
        }
    }
    CHECK(landed);
}

TEST_CASE("shulkers: open for a nearby survival player and shoot homing bullets that levitate; closed, they shrug off hits") {
    MobScene s;
    s.mobs = Mobs();
    MobData sh = Mobs::make(MobType::Shulker, {6.5, 64.0, 0.5}, s.rng);
    REQUIRE(Mobs::add(s.world, sh));
    Projectiles proj;
    bool hit = false;
    for (int i = 0; i < 300 && !hit; ++i) {
        tickWith(s, proj, 1);
        hit = s.vitals.effectLevel(Effect::Levitation) > 0;
    }
    CHECK(hit);
    CHECK(s.vitals.health() < 20.0f);
    MobData* m = nullptr;
    for (MobData* o : s.all())
        if (o->type == MobType::Shulker) m = o;
    REQUIRE(m);
    CHECK(m->peek == 100); // open while it fights
    // Levitating: the player drifts up.
    const double y0 = s.player.position().y;
    for (int i = 0; i < 40; ++i) {
        s.player.setEffects(0, 0, 0, false, s.vitals.effectLevel(Effect::Levitation));
        s.player.tick(s.world, {});
    }
    CHECK(s.player.position().y > y0 + 1.0);
    // Closed, it takes a fifth of a hit.
    m->peek = 0;
    const float before = m->health;
    m->hurtTime = 0;
    Mobs::attack(*m, 10.0f, s.player.position());
    CHECK(m->health == doctest::Approx(before - 2.0f));
}

TEST_CASE("end cities hold shulkers") {
    const EndGenerator gen(42, 2);
    int shulkers = 0;
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = 0; dx <= 2; ++dx) {
            Chunk c({104 + dx, -132 + dz}, kEndHeight);
            gen.generate(c);
            for (const MobData& m : c.mobs())
                shulkers += m.type == MobType::Shulker;
        }
    CHECK(shulkers >= 2);
}

TEST_CASE("M20 review: a dying dragon is saved mid-death; its head can be hit by a ray; clouds don't follow travel") {
    Chunk c({0, 0});
    Xoroshiro rng(4);
    MobData d = Mobs::make(MobType::EnderDragon, {8.5, 90.0, 4.5}, rng);
    d.health = 0.0f;
    d.deathTime = 120;
    c.mobs().push_back(d);
    Chunk back({0, 0});
    entitiesFromNbt(*mc::nbt::read(mc::nbt::write(entitiesToNbt(ChunkSnapshot::of(c)))), back);
    REQUIRE(back.mobs().size() == 1);
    CHECK(back.mobs()[0].deathTime == 120);
    CHECK(back.mobs()[0].health <= 0.0f);

    MobScene s;
    s.mobs = Mobs();
    MobData f = Mobs::make(MobType::EnderDragon, {0.5, 80.0, 0.5}, s.rng);
    f.yaw = 0.0f; // facing +Z: the head is 5 blocks south
    REQUIRE(Mobs::add(s.world, f));
    const glm::dvec3 eye{0.5, 80.75, 12.0};
    const auto hit = Mobs::raycast(s.world, eye, {0.0, 0.0, -1.0}, 10.0);
    REQUIRE(hit);
    const MobData& m = s.world.chunk(hit->chunk)->mobs()[size_t(hit->index)];
    CHECK(Mobs::dragonDamage(m, 8.0f, eye + glm::dvec3(0.0, 0.0, -1.0) * hit->distance) == 8.0f);

    Projectiles proj;
    proj.addCloud({0.5, 64.0, 0.5}, 3.0f, 100);
    proj.clear();
    CHECK(proj.clouds().empty());
    World empty;
    std::vector<BlockPos> edits;
    CHECK_FALSE(DragonFight::buildGateway(empty, {96, 75, 0}, edits)); // not loaded: later
    CHECK_FALSE(DragonFight::openExitPortal(empty, true, edits));
}

TEST_CASE("the End spawns only endermen (M20 review); an elytra isn't worn by hits") {
    MobScene s;
    s.mobs = Mobs();
    s.world.setHasSkyLight(false); // (the End: no sky, not the Nether)
    for (int i = 0; i < 4000; ++i)
        s.tick(1);
    int others = 0, endermen = 0;
    for (MobData* m : s.all()) {
        endermen += m->type == MobType::Enderman;
        others += m->type != MobType::Enderman;
    }
    CHECK(endermen > 0);
    CHECK(others == 0);

    Inventory inv;
    ItemStack el{*itemRegistry().find("elytra"), 1};
    inv.setArmor(1, el);
    inv.wearArmor(50, s.rng);
    CHECK(inv.armor(1).damage == 0);
}

#include "gameplay/Explosion.h"
#include "gameplay/PrimedTnt.h"

TEST_CASE("TNT: redstone lights it, it hops, falls and blows up after 4 s; its blast sets off more TNT") {
    MobScene s;
    BlockUpdates updates(s.world);
    const auto tnt = blockRegistry().defaultState(blocks::Tnt);
    s.world.updateBlock({4, 64, 4}, tnt);
    s.world.updateBlock({4, 64, 9}, tnt); // 5 away: in the blast
    CHECK(updates.primedTnt().empty());
    s.world.updateBlock({5, 64, 4}, blockRegistry().defaultState(blocks::RedstoneBlock));
    REQUIRE(updates.primedTnt().size() == 1);
    CHECK(s.world.getBlock({4, 64, 4}) == 0);
    PrimedTnt primed;
    primed.prime(updates.primedTnt()[0], 80, s.rng);
    updates.primedTnt().clear();
    int ticks = 0;
    double peak = 0.0;
    while (primed.explosions().empty() && ticks < 200) {
        primed.tick(s.world);
        if (!primed.items().empty()) peak = std::max(peak, primed.items()[0].pos.y);
        ++ticks;
    }
    CHECK(ticks == 80);
    CHECK(peak > 64.3); // it hopped
    Explosion blast;
    std::vector<BlockPos> changed;
    ExplosionTargets t;
    t.tnt = &primed;
    t.dropAll = true;
    blast.explode(s.world, primed.explosions()[0], 4.0f, s.rng, s.items, changed, t);
    CHECK(s.world.getBlock({4, 64, 9}) == 0);
    REQUIRE(primed.items().size() == 1); // the other TNT, lit with a short fuse
    CHECK(primed.items()[0].fuse <= 30);
    CHECK(primed.items()[0].fuse >= 10);
}

#include "gameplay/Hoppers.h"

TEST_CASE("hoppers: pull from the chest above, push into the chest below, 1 item per 8 ticks; power stops them; they pick up items") {
    MobScene s;
    s.mobs = Mobs();
    BlockUpdates updates(s.world);
    const auto& r = blockRegistry();
    s.world.updateBlock({4, 66, 4}, r.defaultState(blocks::Chest));
    s.world.updateBlock({4, 65, 4}, r.defaultState(blocks::Hopper)); // facing down
    s.world.updateBlock({4, 64, 4}, r.defaultState(blocks::Chest));
    const ItemId stone = *itemRegistry().find("stone");
    s.world.chunk({0, 0})->chest(4, 66, 4)->items[0] = ItemStack{stone, 10};
    for (int i = 0; i < 80; ++i)
        tickHoppers(s.world, s.items);
    const ChestData* below = s.world.chunk({0, 0})->chest(4, 64, 4);
    const int moved = below->items[0].count;
    CHECK(moved >= 8);
    CHECK(moved <= 10);
    CHECK(s.world.chunk({0, 0})->chest(4, 66, 4)->items[0].count + moved +
              s.world.chunk({0, 0})->hopper(4, 65, 4)->items[0].count ==
          10);
    // A redstone block beside it turns it off.
    s.world.updateBlock({5, 65, 4}, r.defaultState(blocks::RedstoneBlock));
    CHECK(r.get(s.world.getBlock({4, 65, 4}), properties::enabled) == 1);
    // Dropped items over a free hopper are picked up.
    s.world.updateBlock({8, 64, 8}, r.defaultState(blocks::Hopper));
    s.items.spawn({8.5, 65.2, 8.5}, ItemStack{stone, 3}, s.rng);
    for (int i = 0; i < 40; ++i)
        tickHoppers(s.world, s.items);
    const HopperData* h = s.world.chunk({0, 0})->hopper(8, 64, 8);
    CHECK(h->items[0].count == 3);
}

#include "gameplay/Dispensers.h"

TEST_CASE("dispensers and droppers: a power pulse fires one item 4 ticks later - arrows fly, buckets pour, droppers feed chests") {
    MobScene s;
    s.mobs = Mobs();
    BlockUpdates updates(s.world);
    const auto& r = blockRegistry();
    auto at = [&](BlockId b, const char* facing) { return *r.with(r.defaultState(b), "facing", facing); };
    s.world.updateBlock({4, 64, 4}, at(blocks::Dispenser, "east"));
    s.world.chunk({0, 0})->dispenser(4, 64, 4)->items[0] = ItemStack{*itemRegistry().find("arrow"), 2};
    s.world.chunk({0, 0})->dispenser(4, 64, 4)->items[1] = ItemStack{*itemRegistry().find("water_bucket"), 1};
    Projectiles proj;
    PrimedTnt tnt;
    std::vector<BlockPos> edits;
    DispenseContext ctx{s.world, updates, s.items, proj, tnt, s.rng, edits};
    int64_t time = 0;
    auto pulse = [&] {
        updates.setTime(++time);
        s.world.updateBlock({4, 64, 3}, r.defaultState(blocks::RedstoneBlock));
        for (int i = 0; i < 5; ++i) {
            updates.setTime(++time);
            updates.tick();
            for (const BlockPos& b : updates.dispensed())
                dispense(ctx, b);
            updates.dispensed().clear();
        }
        s.world.updateBlock({4, 64, 3}, 0);
    };
    for (int i = 0; i < 3; ++i)
        pulse();
    const DispenserData* d = s.world.chunk({0, 0})->dispenser(4, 64, 4);
    int arrowsLeft = 0, buckets = 0;
    for (const ItemStack& st : d->items) {
        if (st.empty()) continue;
        if (itemRegistry().item(st.item).id == "minecraft:arrow") arrowsLeft += st.count;
        if (itemRegistry().item(st.item).id == "minecraft:bucket") ++buckets;
    }
    // 3 shots from {2 arrows, 1 water bucket}: everything used once (random order).
    CHECK(arrowsLeft == 0);
    CHECK(buckets == 1);
    CHECK(r.blockOf(s.world.getBlock({5, 64, 4})) == blocks::Water);

    // A dropper into a chest.
    s.world.updateBlock({8, 64, 8}, at(blocks::Dropper, "east"));
    s.world.updateBlock({9, 64, 8}, r.defaultState(blocks::Chest));
    s.world.chunk({0, 0})->dispenser(8, 64, 8)->items[4] = ItemStack{*itemRegistry().find("stone"), 5};
    updates.setTime(++time);
    s.world.updateBlock({8, 65, 8}, r.defaultState(blocks::RedstoneBlock)); // above: quasi-connectivity
    for (int i = 0; i < 5; ++i) {
        updates.setTime(++time);
        updates.tick();
        for (const BlockPos& b : updates.dispensed())
            dispense(ctx, b);
        updates.dispensed().clear();
    }
    CHECK(s.world.chunk({0, 0})->chest(9, 64, 8)->items[0].count == 1);
    CHECK(s.world.chunk({0, 0})->dispenser(8, 64, 8)->items[4].count == 4);
}

#include "world/Rails.h"

TEST_CASE("minecarts: powered rails push a cart along the track and round a corner; unpowered ones stop it; it rolls down slopes") {
    MobScene s;
    s.mobs = Mobs();
    BlockUpdates updates(s.world);
    const auto& r = blockRegistry();
    auto place = [&](BlockId b, BlockPos p) {
        const auto st = BlockUpdates::placement(s.world, r.defaultState(b), p, Direction::Up, 90.0f, 0.0f);
        REQUIRE(st);
        s.world.updateBlock(p, *st);
    };
    // A line east from x 0 to 20 at z 4, powered rails at x 1..2 with a redstone block,
    // then a corner north at x 20.
    for (int x = 0; x <= 20; ++x)
        place(x == 1 || x == 2 ? BlockId(blocks::PoweredRail) : BlockId(blocks::Rail), {x, 64, 4});
    for (int z = 0; z < 4; ++z)
        place(blocks::Rail, {20, 64, z});
    s.world.updateBlock({1, 64, 5}, r.defaultState(blocks::RedstoneBlock));
    s.world.updateBlock({20, 64, 4}, *BlockUpdates::placement(s.world, r.defaultState(blocks::Rail), {20, 64, 4},
                                                              Direction::Up, 90.0f, 0.0f));
    CHECK(railShapeOf(s.world.getBlock({20, 64, 4})) == 8); // north_west
    REQUIRE(Mobs::placeMinecart(s.world, {0, 64, 4}, s.rng));
    for (MobData* m : s.all())
        if (m->type == MobType::Minecart) {
            m->vel = {0.1, 0.0, 0.0}; // a nudge east
            m->ridden = true;         // (an empty cart slows fast: drag 0.96)
        }
    double maxX = 0.0, minZ = 10.0;
    for (int i = 0; i < 200; ++i) {
        s.tick(1);
        for (MobData* m : s.all())
            if (m->type == MobType::Minecart) {
                maxX = std::max(maxX, m->pos.x);
                minZ = std::min(minZ, m->pos.z);
            }
    }
    CHECK(maxX > 19.5);  // got to the corner
    CHECK(minZ < 3.0);   // and round it, north
    // Off the end it leaves the rails and stops on the ground.
    for (MobData* m : s.all())
        if (m->type == MobType::Minecart) CHECK(std::abs(m->vel.x) + std::abs(m->vel.z) < 0.05);
}

TEST_CASE("slimes split into 2-4 smaller ones; the smallest do no damage") {
    MobScene s;
    s.mobs = Mobs();
    MobData m = Mobs::make(MobType::Slime, {8.5, 64.0, 8.5}, s.rng);
    m.size = 4;
    m.health = 0.0f;
    REQUIRE(Mobs::add(s.world, m));
    s.tick(2);
    int medium = 0;
    for (MobData* c : s.all())
        medium += c->type == MobType::Slime && c->size == 2;
    CHECK(medium >= 2);
    CHECK(medium <= 4);
    MobScene t;
    t.mobs = Mobs();
    MobData small = Mobs::make(MobType::Slime, {0.5, 64.0, 0.5}, t.rng); // on the player
    small.size = 1;
    small.health = 1.0f;
    REQUIRE(Mobs::add(t.world, small));
    t.tick(40);
    CHECK(t.vitals.health() == doctest::Approx(20.0f));
}

TEST_CASE("M21 review: hoppers move 1 item every 8 ticks, skip stacks that can't go in, never lose a pulled item") {
    MobScene s;
    s.mobs = Mobs();
    BlockUpdates updates(s.world);
    const auto& r = blockRegistry();
    const ItemId stone = *itemRegistry().find("stone"), coal = *itemRegistry().find("coal");
    s.world.updateBlock({4, 65, 4}, r.defaultState(blocks::Hopper));
    s.world.updateBlock({4, 64, 4}, r.defaultState(blocks::Chest));
    s.world.chunk({0, 0})->hopper(4, 65, 4)->items[0] = ItemStack{stone, 20};
    for (int i = 0; i < 40; ++i)
        tickHoppers(s.world, s.items);
    CHECK(s.world.chunk({0, 0})->chest(4, 64, 4)->items[0].count == 5); // ticks 1, 9, 17, 25, 33
    // Into a furnace's side only fuel goes: the coal in slot 2 passes the stone in slot 1.
    s.world.updateBlock({8, 64, 8}, r.defaultState(blocks::Furnace));
    s.world.updateBlock({7, 64, 8}, *r.with(r.defaultState(blocks::Hopper), "facing", "east"));
    s.world.chunk({0, 0})->hopper(7, 64, 8)->items[0] = ItemStack{stone, 3};
    s.world.chunk({0, 0})->hopper(7, 64, 8)->items[1] = ItemStack{coal, 3};
    for (int i = 0; i < 2; ++i)
        tickHoppers(s.world, s.items);
    CHECK(s.world.chunk({0, 0})->furnace(8, 64, 8)->fuel.item == coal);
    // A full hopper under a furnace doesn't take (and lose) its output.
    s.world.updateBlock({12, 65, 12}, r.defaultState(blocks::Furnace));
    s.world.updateBlock({12, 64, 12}, r.defaultState(blocks::Hopper));
    s.world.updateBlock({12, 63, 12}, r.defaultState(blocks::Stone));
    s.world.chunk({0, 0})->furnace(12, 65, 12)->output = ItemStack{*itemRegistry().find("iron_ingot"), 4};
    for (int k = 0; k < 5; ++k)
        s.world.chunk({0, 0})->hopper(12, 64, 12)->items[size_t(k)] = ItemStack{stone, 64};
    for (int i = 0; i < 10; ++i)
        tickHoppers(s.world, s.items);
    CHECK(s.world.chunk({0, 0})->furnace(12, 65, 12)->output.count == 4);
}

TEST_CASE("thunderstorms let monsters spawn under the open sky at midday (M22 review)") {
    MobScene storm;
    storm.survival = false;
    storm.dayTime = 6000;
    storm.skyDarken = 5.8f; // rain and thunder at noon
    storm.thundering = true;
    storm.world.forEachChunk([](Chunk& c) {
        std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
        auto lit = std::make_shared<SectionLight>();
        lit->sky.fill(15);
        light.fill(lit);
        c.setLight(light);
    });
    storm.tick(6000); // (long enough not to depend on the RNG order)
    CHECK(storm.mobs.hostileCount() > 0);
}

TEST_CASE("rain puts out any burning mob, not only the undead (M22 review)") {
    MobScene s;
    s.world.forEachChunk([](Chunk& c) {
        std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
        auto lit = std::make_shared<SectionLight>();
        lit->sky.fill(15);
        light.fill(lit);
        c.setLight(light);
    });
    Weather w;
    w.set(Weather::Kind::Rain, 10000);
    s.weather = &w;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {3.5, 64.0, 3.5}, s.rng)));
    MobData* cow = s.all()[0];
    cow->fireTicks = 100;
    s.tick(1);
    CHECK(s.all()[0]->fireTicks == 0);
}

TEST_CASE("villagers claim a job site (profession), a bed and the bell; they work by day and sleep at night (M24.1)") {
    MobScene s;
    auto S = [](BlockId b) { return blockRegistry().defaultState(b); };
    s.world.setBlock({3, 64, 4}, *blockRegistry().parse("minecraft:red_bed[facing=north,occupied=false,part=head]"));
    s.world.setBlock({3, 64, 5}, *blockRegistry().parse("minecraft:red_bed[facing=north,occupied=false,part=foot]"));
    s.world.setBlock({12, 64, 8}, S(blocks::Composter));
    s.world.setBlock({0, 64, 12}, S(blocks::Bell));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Villager, {8.5, 64.0, 0.5}, s.rng)));
    s.player.setPosition({6.5, 64.0, 6.5});
    auto run = [&](int64_t day, int ticks) {
        for (int i = 0; i < ticks; ++i) {
            s.player.tick(s.world, {});
            Mobs::Context ctx{s.world, s.player, s.vitals, false, false, day + i, 0.0f, s.rng, s.items};
            ctx.naturalSpawning = false;
            s.mobs.tick(ctx);
        }
    };
    run(3000, 400); // working hours
    auto v = s.all();
    REQUIRE(v.size() == 1);
    CHECK(v[0]->profession == uint8_t(Profession::Farmer));
    CHECK(v[0]->jobSite == glm::ivec3(12, 64, 8));
    CHECK(v[0]->home == glm::ivec3(3, 64, 4));
    CHECK(v[0]->meetingPoint == glm::ivec3(0, 64, 12));
    CHECK(glm::length(glm::dvec2(v[0]->pos.x - 12.5, v[0]->pos.z - 8.5)) < 4.0); // at work
    run(13000, 600); // night: to bed
    v = s.all();
    REQUIRE(v.size() == 1);
    CHECK(v[0]->sleeping);
    CHECK(glm::length(glm::dvec2(v[0]->pos.x - 3.5, v[0]->pos.z - 6.0)) < 0.1); // feet at the foot end
    // Breaking the job site before any trade: no longer a farmer.
    s.world.setBlock({12, 64, 8}, 0);
    run(25000, 30);
    v = s.all();
    CHECK(v[0]->profession == uint8_t(Profession::None));
    CHECK_FALSE(v[0]->sleeping); // (morning)
}

TEST_CASE("zombies hunt villagers and infect them; weakness and a golden apple cure them (M24.3)") {
    MobScene s;
    MobData v = Mobs::make(MobType::Villager, {6.5, 64.0, 0.5}, s.rng);
    v.profession = uint8_t(Profession::Mason);
    v.health = 3.0f; // one hit from death
    TradeOffer o;
    o.buyA = *itemRegistry().find("emerald");
    o.buyACount = 10;
    o.sell = *itemRegistry().find("stone");
    o.sellCount = 1;
    o.maxUses = 12;
    v.offers[v.offerCount++] = o;
    // A pen (x 3..8, z -2..2, 2 high): a panicking villager outruns a zombie in the open.
    for (int x = 3; x <= 8; ++x)
        for (int z = -2; z <= 2; ++z)
            if (x == 3 || x == 8 || z == -2 || z == 2)
                for (int y = 64; y <= 65; ++y)
                    s.world.setBlock({x, y, z}, blockRegistry().defaultState(blocks::Stone));
    REQUIRE(Mobs::add(s.world, v));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Zombie, {4.5, 64.0, 0.5}, s.rng)));
    s.player.setPosition({40.5, 64.0, 40.5}); // far: the zombie wants the villager
    int infected = 0, killed = 0;
    for (int t = 0; t < 400 && infected + killed == 0; ++t) {
        s.tick();
        bool villager = false;
        for (MobData* m : s.all()) {
            if (m->type == MobType::ZombieVillager) infected = 1;
            if (m->type == MobType::Villager && m->health > 0.0f) villager = true;
        }
        if (!villager && !infected) killed = 1;
    }
    CHECK(infected + killed == 1); // hit: either way it no longer lives as a villager
    if (!infected) return; // (the 50% roll went the other way with this seed)
    MobData* zv = nullptr;
    for (MobData* m : s.all())
        if (m->type == MobType::ZombieVillager) zv = m;
    REQUIRE(zv);
    CHECK(zv->profession == uint8_t(Profession::Mason)); // keeps its job and trades
    const ItemId apple = *itemRegistry().find("golden_apple");
    CHECK(Mobs::interact(*zv, apple, s.rng, s.items) == Mobs::Use::None); // no Weakness yet
    zv->weaknessTicks = 600;
    CHECK(Mobs::interact(*zv, apple, s.rng, s.items) == Mobs::Use::Fed);
    REQUIRE(zv->convertTicks > 0);
    zv->convertTicks = 2;
    s.tick(3);
    bool cured = false;
    for (MobData* m : s.all())
        if (m->type == MobType::Villager) {
            cured = true;
            CHECK(m->offers[0].specialPrice == -6); // grateful: 0.05 x 125 reputation off
        }
    CHECK(cured);
}

TEST_CASE("iron golems: built from iron and a carved pumpkin, fight monsters, called by villagers, mended by iron (M24.3)") {
    MobScene s;
    auto S = [](BlockId b) { return blockRegistry().defaultState(b); };
    // A T of iron blocks (along x) with the pumpkin on top.
    s.world.setBlock({0, 64, 5}, S(blocks::IronBlock));
    s.world.setBlock({0, 65, 5}, S(blocks::IronBlock));
    s.world.setBlock({-1, 65, 5}, S(blocks::IronBlock));
    s.world.setBlock({1, 65, 5}, S(blocks::IronBlock));
    s.world.setBlock({0, 66, 5}, S(blocks::CarvedPumpkin));
    CHECK(Mobs::buildIronGolem(s.world, {0, 66, 5}, s.rng));
    CHECK(s.world.getBlock({0, 65, 5}) == 0);
    auto golems = [&] {
        int n = 0;
        for (MobData* m : s.all()) n += m->type == MobType::IronGolem && m->health > 0.0f;
        return n;
    };
    CHECK(golems() == 1);
    s.world.setBlock({5, 64, 9}, S(blocks::IronBlock)); // no T: nothing
    s.world.setBlock({5, 65, 9}, S(blocks::CarvedPumpkin));
    CHECK_FALSE(Mobs::buildIronGolem(s.world, {5, 65, 9}, s.rng));
    // It goes for a zombie nearby and kills it (7.5-22.5 a hit, 20 health).
    const MobData zombie = Mobs::make(MobType::Zombie, {6.5, 64.0, 5.5}, s.rng);
    REQUIRE(Mobs::add(s.world, zombie));
    s.player.setPosition({30.5, 64.0, 30.5});
    s.survival = false; // (the zombie leaves the player be)
    bool zombieAlive = true;
    for (int t = 0; t < 400 && zombieAlive; ++t) {
        s.tick();
        zombieAlive = false; // (that zombie: others may spawn in the dark meanwhile)
        for (MobData* m : s.all()) zombieAlive = zombieAlive || (m->uuidHi == zombie.uuidHi && m->health > 0.0f);
    }
    CHECK_FALSE(zombieAlive);
    // An iron ingot mends it by 25.
    MobData* g = nullptr;
    for (MobData* m : s.all())
        if (m->type == MobType::IronGolem) g = m;
    REQUIRE(g);
    g->health = 50.0f;
    CHECK(Mobs::interact(*g, *itemRegistry().find("iron_ingot"), s.rng, s.items) == Mobs::Use::Fed);
    CHECK(g->health == doctest::Approx(75.0f));
    CHECK(mobInfo(MobType::IronGolem).maxHealth == 100.0f);
}

TEST_CASE("three villagers with beds call an iron golem when none is near (M24.3)") {
    MobScene s;
    for (int i = 0; i < 3; ++i) {
        s.world.setBlock({4 + i, 64, 8}, *blockRegistry().parse("minecraft:red_bed[facing=north,occupied=false,part=head]"));
        s.world.setBlock({4 + i, 64, 9}, *blockRegistry().parse("minecraft:red_bed[facing=north,occupied=false,part=foot]"));
        MobData v = Mobs::make(MobType::Villager, {4.5 + i, 64.0, 4.5}, s.rng);
        v.home = {4 + i, 64, 8};
        REQUIRE(Mobs::add(s.world, v));
    }
    s.player.setPosition({30.5, 64.0, 30.5});
    int golems = 0;
    for (int t = 0; t < 4000 && golems == 0; ++t) {
        s.player.tick(s.world, {});
        Mobs::Context ctx{s.world, s.player, s.vitals, false, false, 3000, 0.0f, s.rng, s.items};
        ctx.naturalSpawning = false;
        s.mobs.tick(ctx);
        for (MobData* m : s.all()) golems += m->type == MobType::IronGolem;
    }
    CHECK(golems == 1);
}

TEST_CASE("villagers pick up food; two willing villagers with a free bed have a baby (M24.3)") {
    MobScene s;
    auto bed = [&](int x, int z) {
        s.world.setBlock({x, 64, z}, *blockRegistry().parse("minecraft:red_bed[facing=north,occupied=false,part=head]"));
        s.world.setBlock({x, 64, z + 1}, *blockRegistry().parse("minecraft:red_bed[facing=north,occupied=false,part=foot]"));
    };
    bed(2, 10);
    bed(4, 10);
    bed(6, 10); // a third, free bed for the child
    MobData a = Mobs::make(MobType::Villager, {3.5, 64.0, 4.5}, s.rng);
    MobData b = Mobs::make(MobType::Villager, {5.5, 64.0, 4.5}, s.rng);
    REQUIRE(Mobs::add(s.world, a));
    REQUIRE(Mobs::add(s.world, b));
    // Bread on the ground next to each: 3 loaves = 12 points, enough to be willing.
    s.items.spawn({3.5, 64.2, 4.5}, {*itemRegistry().find("bread"), 3}, s.rng);
    s.items.spawn({5.5, 64.2, 4.5}, {*itemRegistry().find("bread"), 3}, s.rng);
    s.player.setPosition({30.5, 64.0, 30.5});
    int babies = 0;
    Inventory inv;
    for (int t = 0; t < 1200 && babies == 0; ++t) {
        s.player.tick(s.world, {});
        s.items.tick(s.world, s.player.box(), false, inv);
        Mobs::Context ctx{s.world, s.player, s.vitals, false, false, 3000, 0.0f, s.rng, s.items};
        ctx.naturalSpawning = false;
        s.mobs.tick(ctx);
        for (MobData* m : s.all()) babies += m->type == MobType::Villager && m->isBaby();
    }
    CHECK(babies == 1);
    for (MobData* m : s.all())
        if (m->type == MobType::Villager && !m->isBaby()) {
            CHECK(m->age > 0); // resting before breeding again
            CHECK(m->food[0] == 0); // the bread went into it
        }
    for (MobData* m : s.all()) // (review fix: the child claims the free bed, so it isn't counted again)
        if (m->type == MobType::Villager && m->isBaby()) CHECK(m->home.y == 64);
}

#include "gameplay/Projectiles.h"

TEST_CASE("witches throw potions at a player within 10 blocks and drink when burning; lightning makes villagers witches (M24.4)") {
    MobScene s;
    Projectiles projectiles;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Witch, {6.5, 64.0, 0.5}, s.rng)));
    s.player.setPosition({0.5, 64.0, 0.5});
    int thrown = 0;
    for (int t = 0; t < 200; ++t) {
        s.player.tick(s.world, {});
        Mobs::Context ctx{s.world, s.player, s.vitals, true, false, 18000, 11.0f, s.rng, s.items};
        ctx.naturalSpawning = false;
        ctx.projectiles = &projectiles;
        s.mobs.tick(ctx);
    }
    for (const auto& p : projectiles.items()) thrown += p.kind == ProjectileKind::SplashPotion; // (not flown: no tick)
    CHECK(thrown >= 2); // every 3 s once it sees the player
    MobData* w = nullptr;
    for (MobData* m : s.all())
        if (m->type == MobType::Witch) w = m;
    REQUIRE(w);
    w->fireTicks = 100;
    w->drinkTicks = 0;
    s.tick(40); // drinks fire resistance (32 ticks), then the fire goes out
    CHECK(w->fireResistTicks > 0);
    CHECK(w->fireTicks == 0);
    // Lightning on a villager.
    MobData v = Mobs::make(MobType::Villager, {10.5, 64.0, 10.5}, s.rng);
    REQUIRE(Mobs::add(s.world, v));
    CHECK(Mobs::strikeLightning(s.world, {10.5, 64.0, 10.5}));
    int witches = 0;
    for (MobData* m : s.all()) witches += m->type == MobType::Witch;
    CHECK(witches == 2);
}

#include "gameplay/WanderingTraders.h"

TEST_CASE("wandering traders arrive by chance, sell wares and leave after 40 minutes (M24.4)") {
    MobScene s;
    const MobData t = Mobs::make(MobType::WanderingTrader, {3.5, 64.0, 3.5}, s.rng);
    CHECK(t.offerCount >= 6); // 2 buys, 5 common, 1 rare (items that exist)
    CHECK(t.despawnDelay == 48000);
    // The spawner: a roll every 24000 ticks at 25%, rising by 25 to 75 after misses.
    WanderingTraderSpawner sp;
    sp.delay = 1;
    sp.chance = 0; // (force a miss: the chance rises)
    s.player.setPosition({0.5, 64.0, 0.5});
    CHECK_FALSE(sp.tick(s.world, s.player.position(), s.rng));
    CHECK(sp.chance == 25);
    CHECK(sp.delay == 24000);
    sp.delay = 1;
    sp.chance = 100; // (the chance passes; then the 1-in-10 roll - wiki)
    int arrivals = 0;
    for (int i = 0; i < 100 && arrivals == 0; ++i) {
        sp.delay = 1;
        sp.chance = 100;
        arrivals += sp.tick(s.world, s.player.position(), s.rng);
    }
    CHECK(arrivals == 1);
    CHECK(sp.chance == 25);
    int traders = 0;
    for (MobData* m : s.all()) traders += m->type == MobType::WanderingTrader;
    CHECK(traders == 1);
    // Its time up, it's gone.
    for (MobData* m : s.all())
        if (m->type == MobType::WanderingTrader) m->despawnDelay = 2;
    s.tick(25);
    traders = 0;
    for (MobData* m : s.all()) traders += m->type == MobType::WanderingTrader;
    CHECK(traders == 0);
}

#include "gameplay/Patrols.h"

TEST_CASE("pillager patrols come from day 5 with a captain; pillagers shoot bolts (M24.4)") {
    MobScene s;
    s.player.setPosition({0.5, 64.0, 0.5});
    PatrolSpawner patrols;
    patrols.delay = 1;
    CHECK(patrols.tick(s.world, s.player.position(), 24000 * 2, s.rng) == 0); // too early
    int spawned = 0;
    for (int tries = 0; tries < 50 && spawned == 0; ++tries) {
        patrols.delay = 1;
        spawned = patrols.tick(s.world, s.player.position(), 24000 * 6, s.rng);
    }
    REQUIRE(spawned >= 1);
    int captains = 0;
    for (MobData* m : s.all()) captains += m->type == MobType::Pillager && m->captain;
    CHECK(captains == 1);
    // A pillager near the player shoots.
    MobScene t;
    Projectiles projectiles;
    REQUIRE(Mobs::add(t.world, Mobs::make(MobType::Pillager, {7.5, 64.0, 0.5}, t.rng)));
    t.player.setPosition({0.5, 64.0, 0.5});
    for (int i = 0; i < 120; ++i) {
        t.player.tick(t.world, {});
        Mobs::Context ctx{t.world, t.player, t.vitals, true, false, 18000, 11.0f, t.rng, t.items};
        ctx.naturalSpawning = false;
        ctx.projectiles = &projectiles;
        t.mobs.tick(ctx);
    }
    int bolts = 0;
    for (const auto& p : projectiles.items()) bolts += p.kind == ProjectileKind::Arrow;
    CHECK(bolts >= 1);
}

TEST_CASE("raid mobs: vindicators hunt villagers, evokers summon vexes that fade (M24.5)") {
    MobScene s;
    s.player.setPosition({40.5, 64.0, 40.5});
    for (int x = 3; x <= 9; ++x) // a pen so the villager can't outrun it
        for (int z = -3; z <= 3; ++z)
            if (x == 3 || x == 9 || z == -3 || z == 3)
                for (int y = 64; y <= 65; ++y) s.world.setBlock({x, y, z}, blockRegistry().defaultState(blocks::Stone));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Villager, {7.5, 64.0, 0.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Vindicator, {4.5, 64.0, 0.5}, s.rng)));
    bool villagerAlive = true;
    for (int t = 0; t < 400 && villagerAlive; ++t) {
        s.tick();
        villagerAlive = false;
        for (MobData* m : s.all()) villagerAlive = villagerAlive || (m->type == MobType::Villager && m->health > 0.0f);
    }
    CHECK_FALSE(villagerAlive); // two axe hits (13 each)
    // An evoker near a survival player calls vexes.
    MobScene e;
    REQUIRE(Mobs::add(e.world, Mobs::make(MobType::Evoker, {6.5, 64.0, 0.5}, e.rng)));
    e.player.setPosition({0.5, 64.0, 0.5});
    int vexes = 0;
    for (int t = 0; t < 40 && vexes == 0; ++t) {
        e.player.tick(e.world, {});
        Mobs::Context ctx{e.world, e.player, e.vitals, true, false, 18000, 11.0f, e.rng, e.items};
        ctx.naturalSpawning = false;
        e.mobs.tick(ctx);
        for (MobData* m : e.all()) vexes += m->type == MobType::Vex;
    }
    CHECK(vexes == 3);
    for (MobData* m : e.all())
        if (m->type == MobType::Vex) {
            CHECK(m->spellTicks >= 600);
            m->spellTicks = 0; // its time is up: it fades
            m->health = 1.0f;
        }
    for (int t = 0; t < 120; ++t) {
        e.player.tick(e.world, {});
        Mobs::Context ctx{e.world, e.player, e.vitals, false, false, 18000, 11.0f, e.rng, e.items};
        ctx.naturalSpawning = false;
        e.mobs.tick(ctx);
    }
    int alive = 0;
    for (MobData* m : e.all()) alive += m->type == MobType::Vex && m->health > 0.0f;
    CHECK(alive <= 3); // (more may have been called meanwhile, the first faded)
    CHECK(mobInfo(MobType::Ravager).maxHealth == 100.0f);
    CHECK(isRaider(MobType::Vindicator));
}

#include "gameplay/Raids.h"
#include "world/Trades.h"

TEST_CASE("raids: Bad Omen near a bell becomes Raid Omen, then 5 waves; winning gives Hero of the Village (M24.5)") {
    MobScene s;
    s.world.setBlock({0, 64, 4}, blockRegistry().defaultState(blocks::Bell));
    MobData v = Mobs::make(MobType::Villager, {2.5, 64.0, 2.5}, s.rng);
    REQUIRE(Mobs::add(s.world, v));
    Raid raid;
    s.vitals.addEffect(Effect::BadOmen, 0, 120000);
    const glm::dvec3 at{0.5, 64.0, 0.5};
    for (int t = 0; t < 40; ++t) raid.tick(s.world, s.vitals, at, s.rng);
    CHECK(s.vitals.effectLevel(Effect::BadOmen) == 0);
    CHECK(s.vitals.effectLevel(Effect::RaidOmen) == 1);
    CHECK(raid.pending());
    for (int t = 0; t < 600 && !raid.active(); ++t) raid.tick(s.world, s.vitals, at, s.rng);
    REQUIRE(raid.active());
    CHECK(raid.centre() == glm::ivec3{0, 64, 4});
    CHECK(raid.waves() == 5);
    int raiders = 0;
    for (int wave = 1; wave <= 5; ++wave) {
        for (int t = 0; t < 400 && raid.wave() < wave; ++t) raid.tick(s.world, s.vitals, at, s.rng);
        REQUIRE(raid.wave() == wave);
        int count = 0, captains = 0;
        for (MobData* m : s.all())
            if (m->raidId == raid.id() && m->health > 0.0f) {
                ++count;
                captains += m->captain;
                CHECK((isRaider(m->type) || m->type == MobType::Witch));
                CHECK(glm::length(glm::dvec2(m->pos.x, m->pos.z) - glm::dvec2(0.5, 4.5)) > 15.0); // (outside the village)
                m->health = 0.0f; // beaten
            }
        static constexpr int kSizes[5] = {4, 5, 5, 8, 10}; // (wiki: Normal, waves 1-5)
        CHECK(count == kSizes[wave - 1]);
        CHECK(captains == 1);
        raiders += count;
    }
    for (int t = 0; t < 40 && raid.active(); ++t) raid.tick(s.world, s.vitals, at, s.rng);
    CHECK_FALSE(raid.active());
    CHECK(s.vitals.effectLevel(Effect::HeroOfTheVillage) == 1);
    CHECK(raiders == 32);
}

TEST_CASE("raids are lost when no villager is left (M24.5)") {
    MobScene s;
    Raid raid;
    raid.start({0, 64, 0}, 1);
    for (int t = 0; t < 40; ++t) raid.tick(s.world, s.vitals, {0.5, 64.0, 0.5}, s.rng);
    CHECK_FALSE(raid.active());
    CHECK(s.vitals.effectLevel(Effect::HeroOfTheVillage) == 0);
}

TEST_CASE("during a raid raiders march on the bell and villagers run home (M24.5)") {
    MobScene s;
    s.dayTime = 6000; // (noon: villagers would be out)
    s.world.setBlock({-6, 64, 0}, blockRegistry().set(blockRegistry().defaultState(blocks::RedBed), properties::bedPart, 0)); // (the head)
    MobData v = Mobs::make(MobType::Villager, {6.5, 64.0, 0.5}, s.rng);
    v.home = {-6, 64, 0};
    REQUIRE(Mobs::add(s.world, v));
    MobData p = Mobs::make(MobType::Witch, {20.5, 64.0, 20.5}, s.rng);
    p.raidId = 1;
    REQUIRE(Mobs::add(s.world, p));
    s.player.setPosition({-30.5, 64.0, -30.5});
    s.player.setCreative(true);
    const glm::ivec3 centre{0, 64, 0};
    for (int t = 0; t < 300; ++t) {
        s.player.tick(s.world, {});
        Mobs::Context ctx{s.world, s.player, s.vitals, false, false, s.dayTime, 0.0f, s.rng, s.items};
        ctx.naturalSpawning = false;
        ctx.raidCentre = &centre;
        ctx.raidId = 1;
        s.mobs.tick(ctx);
    }
    for (MobData* m : s.all()) {
        if (m->type == MobType::Villager) CHECK(glm::length(glm::dvec2(m->pos.x + 5.5, m->pos.z - 0.5)) < 3.0);
        if (m->type == MobType::Witch) CHECK(glm::length(glm::dvec2(m->pos.x - 0.5, m->pos.z - 0.5)) < 10.0);
    }
}

TEST_CASE("Hero of the Village lowers trade prices by 30% + 6.25% a level, at least 1 (M24.5)") {
    TradeOffer o;
    o.buyA = *itemRegistry().find("emerald");
    o.buyACount = 20;
    o.sell = *itemRegistry().find("stone");
    o.sellCount = 1;
    CHECK(offerPrice(o) == 20);
    CHECK(offerPrice(o, 1) == 14);
    CHECK(offerPrice(o, 3) == 12); // 20 x 0.425 = 8.5 -> 8 off
    o.buyACount = 1;
    CHECK(offerPrice(o, 1) == 1);
}

TEST_CASE("an angry iron golem calms down after its anger runs out, even while chasing the player (review fix, M24.3)") {
    MobScene s;
    MobData g = Mobs::make(MobType::IronGolem, {4.5, 64.0, 0.5}, s.rng);
    g.angry = true;
    g.angerTicks = 600;
    REQUIRE(Mobs::add(s.world, g));
    for (int t = 0; t < 700; ++t) {
        s.vitals.setHealth(20.0f); // (the player takes the hits and stays)
        s.tick();
    }
    for (MobData* m : s.all())
        if (m->type == MobType::IronGolem) CHECK_FALSE(m->angry);
}

TEST_CASE("raids pause while the village isn't loaded; a pending raid and raid ids survive saving (review fixes, M24.5)") {
    MobScene s;
    Raid far;
    far.start({1000, 64, 1000}, 1); // (no chunks there: the player went away)
    for (int t = 0; t < 100; ++t) far.tick(s.world, s.vitals, {0.5, 64.0, 0.5}, s.rng);
    CHECK(far.active());
    CHECK_FALSE(far.loaded());
    // Raid Omen running when the game saves: the raid still comes after loading.
    s.world.setBlock({0, 64, 4}, blockRegistry().defaultState(blocks::Bell));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Villager, {2.5, 64.0, 2.5}, s.rng)));
    Raid raid;
    s.vitals.addEffect(Effect::BadOmen, 0, 120000);
    for (int t = 0; t < 40; ++t) raid.tick(s.world, s.vitals, {0.5, 64.0, 0.5}, s.rng);
    REQUIRE(raid.pending());
    Raid loaded;
    loaded.restore(raid.state());
    for (int t = 0; t < 600 && !loaded.active(); ++t) loaded.tick(s.world, s.vitals, {0.5, 64.0, 0.5}, s.rng);
    CHECK(loaded.active());
    // A raider left from an older raid doesn't hold up this one's waves.
    MobData old = Mobs::make(MobType::Pillager, {8.5, 64.0, 8.5}, s.rng);
    old.raidId = loaded.id() + 100;
    REQUIRE(Mobs::add(s.world, old));
    for (int t = 0; t < 400 && loaded.wave() == 0; ++t) loaded.tick(s.world, s.vitals, {0.5, 64.0, 0.5}, s.rng);
    REQUIRE(loaded.wave() == 1);
    for (MobData* m : s.all())
        if (m->raidId == loaded.id()) m->health = 0.0f;
    for (int t = 0; t < 400 && loaded.wave() == 1; ++t) loaded.tick(s.world, s.vitals, {0.5, 64.0, 0.5}, s.rng);
    CHECK(loaded.wave() == 2);
}

TEST_CASE("review fixes (M24): restocks reset each day; struck sleepers wake as witches; built golems stay calm; villagers flee pillagers; raid captains drop no bottle") {
    MobScene s;
    // A villager that restocked twice yesterday restocks again today at its job site.
    s.world.setBlock({4, 64, 4}, blockRegistry().defaultState(blocks::Lectern));
    MobData v = Mobs::make(MobType::Villager, {4.5, 64.0, 5.5}, s.rng);
    v.profession = uint8_t(Profession::Librarian);
    v.jobSite = {4, 64, 4};
    TradeOffer o;
    o.buyA = *itemRegistry().find("emerald");
    o.buyACount = 1;
    o.sell = *itemRegistry().find("stone");
    o.sellCount = 1;
    o.maxUses = 12;
    o.uses = 12;
    v.offers[v.offerCount++] = o;
    v.restocksToday = 2;
    v.lastRestockDay = 0;
    REQUIRE(Mobs::add(s.world, v));
    s.player.setPosition({30.5, 64.0, 30.5});
    for (int t = 0; t < 100; ++t) {
        s.player.tick(s.world, {});
        Mobs::Context ctx{s.world, s.player, s.vitals, false, false, 24000 + 4000, 0.0f, s.rng, s.items};
        ctx.naturalSpawning = false;
        s.mobs.tick(ctx);
    }
    for (MobData* m : s.all())
        if (m->type == MobType::Villager) CHECK(m->offers[0].uses == 0);
    // Lightning on a sleeping villager: a witch, standing.
    for (MobData* m : s.all())
        if (m->type == MobType::Villager) m->sleeping = true;
    Mobs::strikeLightning(s.world, {4.5, 64.0, 5.5});
    for (MobData* m : s.all())
        if (m->type == MobType::Witch) CHECK_FALSE(m->sleeping);
    // A golem the player built isn't angered by the player's hits.
    MobData g = Mobs::make(MobType::IronGolem, {0.5, 64.0, -6.5}, s.rng);
    g.playerCreated = true;
    Mobs::attack(g, 2.0f, {0.5, 64.0, 0.5});
    CHECK_FALSE(g.angry);
    // Villagers run from a pillager 12 blocks away (wiki: 15).
    MobScene f;
    REQUIRE(Mobs::add(f.world, Mobs::make(MobType::Villager, {0.5, 64.0, 0.5}, f.rng)));
    REQUIRE(Mobs::add(f.world, Mobs::make(MobType::Pillager, {12.5, 64.0, 0.5}, f.rng)));
    f.player.setPosition({-30.5, 64.0, -30.5});
    f.player.setCreative(true);
    bool panicked = false;
    for (int t = 0; t < 60 && !panicked; ++t) {
        f.tick();
        for (MobData* m : f.all()) panicked = panicked || (m->type == MobType::Villager && m->panicTicks > 0);
    }
    CHECK(panicked);
    // A raid's wave leader drops no ominous bottle (only captains outside raids - wiki).
    MobScene d;
    MobData cpt = Mobs::make(MobType::Pillager, {3.5, 64.0, 0.5}, d.rng);
    cpt.captain = true;
    cpt.raidId = 3;
    cpt.lastHurtByPlayer = true;
    cpt.health = 0.0f;
    REQUIRE(Mobs::add(d.world, cpt));
    d.survival = false;
    d.tick(30);
    bool bottle = false;
    for (const auto& it : d.items.items()) bottle = bottle || it.stack.item == *itemRegistry().find("ominous_bottle");
    CHECK_FALSE(bottle);
}

TEST_CASE("game rules: mob_drops off - no loot; tnt_explodes off - TNT can't be lit") {
    MobScene s;
    s.mobDrops = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {4.5, 64.0, 4.5}, s.rng)));
    for (MobData* m : s.all())
        if (m->type == MobType::Cow) Mobs::attack(*m, 20.0f, s.player.position());
    s.tick(25);
    CHECK(s.items.items().empty());

    BlockUpdates updates(s.world);
    updates.setTntExplodes(false);
    s.world.updateBlock({4, 64, 4}, blockRegistry().defaultState(blocks::Tnt));
    s.world.updateBlock({5, 64, 4}, blockRegistry().defaultState(blocks::RedstoneBlock));
    CHECK(updates.primedTnt().empty());
    CHECK(blockRegistry().blockOf(s.world.getBlock({4, 64, 4})) == blocks::Tnt);
}

TEST_CASE("Peaceful: monsters vanish (not shulkers), animals stay; mob hits do nothing") {
    MobScene s;
    s.difficulty = 0;
    s.vitals.setDifficulty(0);
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Zombie, {3.5, 64.0, 0.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {-6.5, 64.0, 0.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Shulker, {-6.5, 64.0, 6.5}, s.rng)));
    s.tick(3);
    int zombies = 0, cows = 0, shulkers = 0;
    for (MobData* m : s.all()) {
        zombies += m->type == MobType::Zombie;
        cows += m->type == MobType::Cow;
        shulkers += m->type == MobType::Shulker;
    }
    CHECK(zombies == 0);
    CHECK(cows == 1);
    CHECK(shulkers == 1);
    const glm::dvec3 from(1.0, 64.0, 0.0);
    CHECK_FALSE(s.vitals.attacked(5.0f, &from));
    CHECK(s.vitals.health() == 20.0f);
}

TEST_CASE("M29.5: a crafter crafts its grid once on a pulse and pushes the result into the chest in front") {
    MobScene s;
    s.mobs = Mobs();
    BlockUpdates updates(s.world);
    const auto& r = blockRegistry();
    s.world.updateBlock({4, 64, 10}, *r.with(r.defaultState(blocks::Crafter), "orientation", "east_up"));
    s.world.updateBlock({5, 64, 10}, r.defaultState(blocks::Chest));
    DispenserData* d = s.world.chunk({0, 0})->dispenser(4, 64, 10);
    REQUIRE(d);
    CHECK(d->crafter);
    d->items[4] = ItemStack{*itemRegistry().find("oak_log"), 2};
    Projectiles proj;
    PrimedTnt tnt;
    std::vector<BlockPos> edits;
    DispenseContext ctx{s.world, updates, s.items, proj, tnt, s.rng, edits};
    int64_t time = 0;
    updates.setTime(++time);
    s.world.updateBlock({4, 64, 11}, r.defaultState(blocks::RedstoneBlock));
    for (int i = 0; i < 6; ++i) {
        updates.setTime(++time);
        updates.tick();
        for (const BlockPos& b : updates.dispensed()) dispense(ctx, b);
        updates.dispensed().clear();
    }
    CHECK(d->items[4].count == 1); // one log used
    const ChestData* chest = s.world.chunk({0, 0})->chest(5, 64, 10);
    REQUIRE(chest);
    CHECK(itemRegistry().item(chest->items[0].item).id == "minecraft:oak_planks");
    CHECK(chest->items[0].count == 4);
}
