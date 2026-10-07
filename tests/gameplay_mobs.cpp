// Mobs (wiki: Zombie, Cow, Spawn, Entity format).
#include "gameplay/Inventory.h"
#include "gameplay/Mobs.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
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
            mobs.tick(ctx);
        }
    }
    std::vector<MobData*> all() {
        std::vector<MobData*> out;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs())
                out.push_back(&m);
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
    dark.tick(2000);
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
    for (int cz = -12; cz <= 12 && grassy < 80; ++cz)
        for (int cx = -12; cx <= 12 && grassy < 80; ++cx) {
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
    for (int z = -8; z <= 8; ++z) // a 3-high wall between spider and player
        for (int y = 64; y <= 66; ++y)
            s.world.setBlock({4, y, z}, blockRegistry().defaultState(blocks::Stone));
    for (int z = -8; z <= 8; ++z)
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
    MobData* first = s.all()[0];
    Mobs::attack(*first, 1.0f, s.player.position());
    s.tick(1);
    int angry = 0;
    for (MobData* m : s.all())
        angry += m->angry;
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
