// Mobs of M29 (completeness; wiki pages of each mob).
#include "gameplay/Inventory.h"
#include "gameplay/Projectiles.h"
#include "gameplay/Commands.h"
#include "gameplay/Mobs.h"
#include "world/Weather.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Villagers.h"
#include "world/ChunkSerializer.h"
#include "world/OverworldGenerator.h"
#include "world/Potions.h"

#include <doctest/doctest.h>

#include <filesystem>

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

// M29.1a: husks, strays, bogged and parched (wiki: Husk, Stray, Bogged, Parched).
namespace {
void brighten(MobScene& s) {
    s.skyDarken = 0.0f; // noon
    std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
    auto bright = std::make_shared<SectionLight>();
    bright->sky.fill(15);
    light.fill(bright);
    s.world.forEachChunk([&](Chunk& c) { c.setLight(light); });
}
MobData* findType(MobScene& s, MobType t) {
    for (MobData* m : s.all())
        if (m->type == t) return m;
    return nullptr;
}
} // namespace

TEST_CASE("husks and parched don't burn in the sun; strays and bogged do") {
    MobScene s;
    brighten(s);
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Husk, {8.5, 64.0, 8.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Parched, {-8.5, 64.0, 8.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Stray, {8.5, 64.0, -8.5}, s.rng)));
    s.tick(60);
    CHECK(findType(s, MobType::Husk)->health == doctest::Approx(20.0f));
    CHECK(findType(s, MobType::Parched)->health == doctest::Approx(16.0f));
    CHECK(findType(s, MobType::Stray)->health < 20.0f);
    CHECK(isUndead(MobType::Husk));
    CHECK(isSkeleton(MobType::Bogged));
    CHECK(mobInfo(MobType::Bogged).maxHealth == 16.0f);
}

TEST_CASE("a drowning husk turns into a zombie (45 s under water)") {
    MobScene s;
    s.survival = false;
    for (int y = 64; y <= 67; ++y)
        for (int z = -32; z < 48; ++z)
            for (int x = -32; x < 48; ++x)
                s.world.setBlock({x, y, z}, blockRegistry().defaultState(blocks::Water));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Husk, {8.5, 64.0, 8.5}, s.rng)));
    s.tick(950);
    CHECK(findType(s, MobType::Husk) == nullptr);
    CHECK(findType(s, MobType::Zombie) != nullptr);
}

TEST_CASE("a husk's hit starves; a stray's arrow slows") {
    MobScene s;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Husk, {2.0, 64.0, 0.5}, s.rng)));
    for (int i = 0; i < 100 && s.vitals.effectLevel(Effect::Hunger) == 0; ++i) s.tick();
    CHECK(s.vitals.effectLevel(Effect::Hunger) > 0);

    MobScene t;
    Projectiles shots;
    REQUIRE(Mobs::add(t.world, Mobs::make(MobType::Stray, {8.5, 64.0, 0.5}, t.rng)));
    for (int i = 0; i < 200 && t.vitals.effectLevel(Effect::Slowness) == 0; ++i) {
        t.player.tick(t.world, {});
        Mobs::Context ctx{t.world, t.player, t.vitals, true, false, t.dayTime, t.skyDarken, t.rng, t.items};
        ctx.projectiles = &shots;
        t.mobs.tick(ctx);
        Inventory inv;
        shots.tick(t.world, t.player, &t.vitals, inv, true, t.rng);
    }
    CHECK(t.vitals.effectLevel(Effect::Slowness) > 0);
}

// M29.1b: undead mounts and jockeys (wiki: Jockey, Skeleton Horse, Zombie Horse, Camel Husk,
// Zombie Nautilus).
TEST_CASE("jockeys: a rider sits on its mount and falls off when the mount dies") {
    MobScene s;
    s.survival = false;
    MobData spider = Mobs::make(MobType::Spider, {8.5, 64.0, 8.5}, s.rng);
    MobData rider = Mobs::make(MobType::Skeleton, {8.5, 64.0, 8.5}, s.rng);
    rider.vehicle = spider.uuidHi;
    REQUIRE(Mobs::add(s.world, spider));
    REQUIRE(Mobs::add(s.world, rider));
    s.tick(5);
    MobData* sp = findType(s, MobType::Spider);
    MobData* sk = findType(s, MobType::Skeleton);
    REQUIRE((sp && sk));
    CHECK(sk->pos.y == doctest::Approx(sp->pos.y + Mobs::seatHeight(*sp)));
    CHECK(sk->pos.x == doctest::Approx(sp->pos.x));
    CHECK(sp->mobRidden);
    sp->health = 0.0f;
    sp->deathTime = 19;
    s.tick(2);
    CHECK(findType(s, MobType::Skeleton)->vehicle == 0);
}

TEST_CASE("a skeleton trap springs into four skeleton horsemen near the player") {
    MobScene s;
    MobData trap = Mobs::make(MobType::SkeletonHorse, {5.5, 64.0, 0.5}, s.rng);
    trap.skeletonTrap = true;
    REQUIRE(Mobs::add(s.world, trap));
    s.tick(2);
    int horses = 0, skeletons = 0, tamed = 0, riders = 0;
    for (MobData* m : s.all()) {
        horses += m->type == MobType::SkeletonHorse;
        tamed += m->type == MobType::SkeletonHorse && m->tamed;
        skeletons += m->type == MobType::Skeleton;
        riders += m->type == MobType::Skeleton && m->vehicle != 0;
    }
    CHECK(horses == 4);
    CHECK(tamed == 4);
    CHECK(skeletons == 4);
    CHECK(riders == 4);
    // Bolts: 1% Easy .. 4.5% Hard of storms' strikes, none on Peaceful.
    World w;
    w.createChunk({0, 0});
    Xoroshiro rng{3};
    int traps = 0;
    for (int i = 0; i < 2000; ++i) traps += Mobs::spawnSkeletonTrap(w, {8.5, 64.0, 8.5}, 2, rng) ? 1 : 0;
    CHECK(traps > 20);
    CHECK(traps < 90);
    CHECK_FALSE(Mobs::spawnSkeletonTrap(w, {8.5, 64.0, 8.5}, 0, rng));
}

TEST_CASE("undead mounts: no breeding; zombie horses eat red mushrooms; stats") {
    MobScene s;
    MobData a = Mobs::make(MobType::ZombieHorse, {0, 64, 0}, s.rng);
    MobData b = Mobs::make(MobType::ZombieHorse, {0, 64, 0}, s.rng);
    CHECK(isUndeadMount(b.type)); // (never mate: Mounts.cpp canMate)
    CHECK(a.maxHealth == 25.0f);
    CHECK(Mobs::make(MobType::SkeletonHorse, {0, 64, 0}, s.rng).maxHealth == 15.0f);
    a.health = 10.0f;
    ItemEntities items;
    CHECK(Mobs::interact(a, *itemRegistry().find("red_mushroom"), s.rng, items) == Mobs::Use::Fed);
    CHECK(a.health == doctest::Approx(13.0f));
    MobData sk = Mobs::make(MobType::SkeletonHorse, {0, 64, 0}, s.rng);
    sk.health = 5.0f;
    CHECK(Mobs::interact(sk, *itemRegistry().find("wheat"), s.rng, items) != Mobs::Use::Fed);
    CHECK(isUndead(MobType::CamelHusk));
    CHECK(burnsInDaylight(MobType::ZombieHorse));
    CHECK_FALSE(burnsInDaylight(MobType::SkeletonHorse));
    MobData n = Mobs::make(MobType::ZombieNautilus, {0, 64, 0}, s.rng);
    int fed = 0;
    for (int i = 0; i < 30 && !n.tamed; ++i, ++fed)
        Mobs::interact(n, *itemRegistry().find("pufferfish"), s.rng, items);
    CHECK(n.tamed);
}

// M29.1c: the rest of the roster.
TEST_CASE("piglins and hoglins zombify after 15 s outside the Nether") {
    MobScene s;
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Piglin, {8.5, 64.0, 8.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Hoglin, {-8.5, 64.0, 8.5}, s.rng)));
    s.tick(290);
    CHECK(findType(s, MobType::Piglin) != nullptr);
    s.tick(20);
    CHECK(findType(s, MobType::Piglin) == nullptr);
    CHECK(findType(s, MobType::ZombifiedPiglin) != nullptr);
    CHECK(findType(s, MobType::Zoglin) != nullptr);
}

TEST_CASE("endermites crumble after 2 minutes; bats fly and don't fall") {
    MobScene s;
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Endermite, {8.5, 64.0, 8.5}, s.rng)));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Bat, {-8.5, 70.0, 8.5}, s.rng)));
    s.tick(100);
    MobData* bat = findType(s, MobType::Bat);
    REQUIRE(bat);
    CHECK(bat->pos.y > 64.5); // (airborne)
    s.tick(2310);
    CHECK(findType(s, MobType::Endermite) == nullptr);
}

TEST_CASE("snow golems are built of snow; mooshrooms give stew and shear into cows") {
    MobScene s;
    s.world.setBlock({4, 64, 4}, blockRegistry().defaultState(blocks::SnowBlock));
    s.world.setBlock({4, 65, 4}, blockRegistry().defaultState(blocks::SnowBlock));
    s.world.setBlock({4, 66, 4}, blockRegistry().defaultState(blocks::CarvedPumpkin));
    CHECK(Mobs::buildSnowGolem(s.world, {4, 66, 4}, s.rng));
    CHECK(findType(s, MobType::SnowGolem) != nullptr);
    CHECK(s.world.getBlock({4, 65, 4}) == 0);

    MobData moo = Mobs::make(MobType::Mooshroom, {0, 64, 0}, s.rng);
    ItemEntities items;
    CHECK(Mobs::interact(moo, *itemRegistry().find("bowl"), s.rng, items) == Mobs::Use::Stew);
    CHECK(Mobs::interact(moo, *itemRegistry().find("shears"), s.rng, items) == Mobs::Use::Sheared);
    CHECK(moo.type == MobType::Cow);
    REQUIRE(items.items().size() == 1);
    CHECK(items.items()[0].stack.count == 5);
    CHECK(itemRegistry().item(items.items()[0].stack.item).id == "minecraft:red_mushroom");
}

// M29.1d: 1.21.5 farm animal variants.
TEST_CASE("farm animal variants: warm, cold and temperate biomes; cold chickens lay blue eggs") {
    CHECK(farmVariant(Biome::Desert) == 1);
    CHECK(farmVariant(Biome::MangroveSwamp) == 1);
    CHECK(farmVariant(Biome::SnowyPlains) == 2);
    CHECK(farmVariant(Biome::Taiga) == 2);
    CHECK(farmVariant(Biome::Plains) == 0);
    MobScene s;
    s.survival = false;
    MobData hen = Mobs::make(MobType::Chicken, {8.5, 64.0, 8.5}, s.rng);
    hen.woolColour = 2;
    hen.color2 = 1;
    hen.eggTicks = 1;
    REQUIRE(Mobs::add(s.world, hen));
    s.tick(2);
    bool blue = false;
    for (const auto& it : s.items.items()) blue = blue || itemRegistry().item(it.stack.item).id == "minecraft:blue_egg";
    CHECK(blue);
    // Saved as vanilla's variant.
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(*s.world.chunk({0, 0}), 0));
    bool saved = false;
    for (const nbt::Tag& t : n.list("Entities")->items)
        if (const std::string* v = t.get<nbt::Compound>()->string("variant")) saved = saved || *v == "minecraft:cold";
    CHECK(saved);
}

TEST_CASE("spawn eggs: one for every mob (not decorations, vehicles or illusioners), each with a texture") {
    int eggs = 0;
    for (int t = 0; t < int(MobType::Count); ++t) {
        const MobType type = MobType(t);
        const auto id = itemRegistry().find(std::string(mobInfo(type).id.substr(10)) + "_spawn_egg");
        const bool none = isHanging(type) || type == MobType::ArmorStand || type == MobType::LeashKnot ||
                          type == MobType::Boat || type == MobType::Minecart || type == MobType::EndCrystal ||
                          type == MobType::Illusioner || type == MobType::Giant || type == MobType::Mannequin ||
                          type == MobType::Cushion || isTechnical(type); // (M33.3d: placed from its item)
        CHECK(id.has_value() == !none);
        if (id) {
            ++eggs;
            CHECK(itemRegistry().item(*id).spawnEgg == t + 1);
            CHECK(std::filesystem::exists(std::filesystem::path(MC_ASSETS_DIR) / "minecraft/textures" /
                                          (itemRegistry().item(*id).texture + ".png")));
        }
    }
    CHECK(eggs >= 85);
}

// M29.2c: lasting effects on mobs.
TEST_CASE("mob effects: poison wears down (not the undead), speed, saved as active_effects") {
    MobScene s;
    s.survival = false;
    MobData cow = Mobs::make(MobType::Cow, {8.5, 64.0, 8.5}, s.rng);
    Mobs::addEffect(cow, Effect::Poison, 0, 200);
    Mobs::addEffect(cow, Effect::Glowing, 0, 200);
    MobData zombie = Mobs::make(MobType::Zombie, {-8.5, 64.0, 8.5}, s.rng);
    Mobs::addEffect(zombie, Effect::Poison, 0, 200); // (undead: no effect)
    CHECK(zombie.effectLevel(uint8_t(Effect::Poison)) == 0);
    REQUIRE(Mobs::add(s.world, cow));
    REQUIRE(Mobs::add(s.world, zombie));
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(*s.world.chunk({0, 0}), 0));
    bool saved = false;
    for (const nbt::Tag& t : n.list("Entities")->items)
        if (const nbt::List* fx = t.get<nbt::Compound>()->list("active_effects")) saved = saved || fx->items.size() == 2;
    CHECK(saved);
    s.tick(100);
    MobData* c = findType(s, MobType::Cow);
    REQUIRE(c);
    CHECK(c->health < 10.0f);
    CHECK(c->health >= 1.0f);
    CHECK(c->effectLevel(uint8_t(Effect::Glowing)) == 1);
}

// M29.3a: gear items.
#include "gameplay/Jukebox.h"
#include "gameplay/Recipes.h"

TEST_CASE("nautilus armor goes on a tamed nautilus; mount armor cuts damage; gear melts to nuggets; new discs") {
    MobScene s;
    ItemEntities items;
    MobData n = Mobs::make(MobType::Nautilus, {0, 64, 0}, s.rng);
    n.tamed = true;
    CHECK(Mobs::interact(n, *itemRegistry().find("diamond_nautilus_armor"), s.rng, items) == Mobs::Use::Fed);
    CHECK(n.horseArmor == 4);
    MobData h = Mobs::make(MobType::Horse, {0, 64, 0}, s.rng);
    h.tamed = true;
    CHECK(Mobs::interact(h, *itemRegistry().find("netherite_horse_armor"), s.rng, items) == Mobs::Use::Fed);
    const float before = h.health;
    Mobs::attack(h, 10.0f, {0, 64, 5});
    CHECK(before - h.health < 10.0f * 0.4f); // (19 points: about 76% off)
    CHECK(itemRegistry().item(smelt({*itemRegistry().find("chainmail_helmet"), 1})->item).id == "minecraft:iron_nugget");
    CHECK(itemRegistry().item(smelt({*itemRegistry().find("copper_sword"), 1})->item).id == "minecraft:copper_nugget");
    CHECK(itemRegistry().item(*itemRegistry().find("chainmail_chestplate")).armor == 5);
    const int relic = discIndex(*itemRegistry().find("music_disc_relic"));
    REQUIRE(relic >= 0);
    CHECK(discInfo(relic).comparator == 14);
}

// M29.3b: names.
#include "gameplay/Anvil.h"
#include "world/ItemExtras.h"

TEST_CASE("anvil renaming costs a level; names save on items and mobs") {
    const ItemStack sword{*itemRegistry().find("iron_sword"), 1};
    AnvilResult r = anvilCombine(sword, {}, false, std::string_view("Slicer"));
    REQUIRE_FALSE(r.out.empty());
    CHECK(r.cost == 1);
    CHECK(nameText(r.out.name) == "Slicer");
    CHECK(anvilCombine(sword, {}, false).out.empty()); // (nothing to do)
    // Saved as minecraft:custom_name, read back.
    MobScene s;
    MobData pig = Mobs::make(MobType::Pig, {8.5, 64.0, 8.5}, s.rng);
    pig.nameId = addName("Wilbur");
    REQUIRE(Mobs::add(s.world, pig));
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(*s.world.chunk({0, 0}), 0));
    bool saved = false;
    for (const nbt::Tag& t : n.list("Entities")->items)
        if (const std::string* cn = t.get<nbt::Compound>()->string("CustomName")) saved = saved || *cn == "Wilbur";
    CHECK(saved);
}

TEST_CASE("pigs take saddles and walk where a rider with a carrot on a stick looks (M29.3d)") {
    MobScene s;
    s.survival = false;
    ItemEntities items;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Pig, {8.5, 64.0, 8.5}, s.rng)));
    MobData* pig = findType(s, MobType::Pig);
    CHECK(Mobs::interact(*pig, *itemRegistry().find("saddle"), s.rng, items) == Mobs::Use::Fed);
    CHECK(pig->saddled);
    CHECK(Mobs::interact(*pig, kNoItem, s.rng, items) == Mobs::Use::Ride);
    CHECK(pig->ridden);
    const glm::dvec3 start = pig->pos;
    for (int i = 0; i < 40; ++i) {
        pig = findType(s, MobType::Pig);
        pig->headYaw = 0.0f; // (looking south, +Z)
        pig->paddleForward = 1;
        s.tick();
    }
    pig = findType(s, MobType::Pig);
    CHECK(pig->pos.z - start.z > 2.0);
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(*s.world.chunk(BlockPos{int(pig->pos.x), 64, int(pig->pos.z)}.chunk()), 0));
    bool saddleSaved = false;
    for (const nbt::Tag& t : n.list("Entities")->items)
        if (const nbt::Compound* eq = t.get<nbt::Compound>()->compound("equipment")) saddleSaved = saddleSaved || eq->compound("saddle");
    CHECK(saddleSaved);
}

TEST_CASE("minecart kinds: furnace carts push themselves, hopper carts pick up items, saved by kind (M29.3e)") {
    MobScene s;
    s.survival = false;
    const auto& r = blockRegistry();
    for (int z = -8; z < 24; ++z) s.world.setBlock({4, 64, z}, r.defaultState(blocks::Rail)); // (north-south)
    REQUIRE(Mobs::placeMinecart(s.world, {4, 64, 2}, s.rng, 2));
    MobData* f = findType(s, MobType::Minecart);
    REQUIRE(f);
    f->temper = 400;
    f->home = {0, 0, 1};
    const double z0 = f->pos.z;
    s.tick(40);
    f = findType(s, MobType::Minecart);
    CHECK(f->pos.z - z0 > 2.0);
    // A hopper cart standing still takes a dropped stack over it into its 5 slots.
    MobScene h;
    h.survival = false;
    h.world.setBlock({8, 64, 8}, r.defaultState(blocks::Rail));
    REQUIRE(Mobs::placeMinecart(h.world, {8, 64, 8}, h.rng, 3));
    h.items.spawn({8.5, 64.6, 8.5}, {*itemRegistry().find("diamond"), 2}, h.rng);
    h.tick(30);
    MobData* hc = findType(h, MobType::Minecart);
    REQUIRE(hc);
    const ItemContents* slots = h.world.chunk({0, 0})->mobStore(hc->uuidHi);
    REQUIRE(slots);
    CHECK((*slots)[0].count >= 1);
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(*h.world.chunk({0, 0}), 0));
    bool saved = false;
    for (const nbt::Tag& t : n.list("Entities")->items)
        if (const std::string* id = t.get<nbt::Compound>()->string("id")) saved = saved || *id == "minecraft:hopper_minecart";
    CHECK(saved);
}

TEST_CASE("M29.7: a command block minecart on a powered activator rail reports its command every 4 ticks") {
    MobScene s;
    s.survival = false;
    const auto& r = blockRegistry();
    s.world.setBlock({8, 64, 8}, *r.with(r.defaultState(blocks::ActivatorRail), "powered", "true"));
    REQUIRE(Mobs::placeMinecart(s.world, {8, 64, 8}, s.rng, 5));
    MobData* c = findType(s, MobType::Minecart);
    REQUIRE(c);
    c->commandId = addName("say hi");
    int fired = 0;
    for (int i = 0; i < 12; ++i) {
        s.tick(1);
        fired += int(s.mobs.cartCommands().size());
        s.mobs.cartCommands().clear();
    }
    CHECK(fired == 3);
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(*s.world.chunk({0, 0}), 0));
    bool saved = false;
    for (const nbt::Tag& t : n.list("Entities")->items)
        if (const std::string* cmd = t.get<nbt::Compound>()->string("Command")) saved = saved || *cmd == "say hi";
    CHECK(saved);
}

TEST_CASE("M29.7e: displays, giants, markers and spawner carts from /summon; displays keep what they show") {
    MobScene s;
    s.survival = false;
    int64_t dayTime = 0;
    Inventory inv;
    CommandContext ctx{s.player, inv, dayTime, 0, 42};
    ctx.world = &s.world;
    ctx.rng = &s.rng;
    CHECK(runCommand("/summon block_display 4 65 4 {block_state:{Name:\"minecraft:diamond_block\"}}", ctx).ok);
    CHECK(runCommand("/summon text_display 5 65 4 {text:\"Hello world\"}", ctx).ok);
    CHECK(runCommand("/summon giant 8 64 8", ctx).ok);
    CHECK(runCommand("/summon marker 6 65 4", ctx).ok);
    CHECK(runCommand("/summon spawner_minecart 10 64 10", ctx).ok);
    const MobData* bd = findType(s, MobType::BlockDisplay);
    REQUIRE(bd);
    CHECK(blockRegistry().blockOf(BlockStateId(bd->commandId)) == blocks::DiamondBlock);
    CHECK(nameText(findType(s, MobType::TextDisplay)->commandId) == "Hello world");
    s.tick(20); // they stay put; the marker can't be hit
    CHECK(findType(s, MobType::BlockDisplay)->pos.y == doctest::Approx(65.0));
    CHECK_FALSE(Mobs::raycast(s.world, {6.0, 65.0, 2.0}, {0.0, 0.0, 1.0}, 4.0));
    const nbt::Compound n = entitiesToNbt(ChunkSnapshot::of(*s.world.chunk({0, 0}), 0));
    bool block = false, cart = false;
    for (const nbt::Tag& t : n.list("Entities")->items) {
        const nbt::Compound* c = t.get<nbt::Compound>();
        if (const nbt::Compound* b = c->compound("block_state")) block = block || *b->string("Name") == "minecraft:diamond_block";
        if (const std::string* id = c->string("id")) cart = cart || *id == "minecraft:spawner_minecart";
    }
    CHECK(block);
    CHECK(cart);
}

TEST_CASE("M29 review: a hopper cart crossing a chunk border keeps the item it picked up that tick") {
    const auto& r = blockRegistry();
    const uint16_t diamond = uint16_t(*itemRegistry().find("diamond"));
    for (int offset = 0; offset < 4; ++offset) { // (one of these picks up on the crossing tick)
        MobScene s;
        s.survival = false;
        for (int x = 2; x < 34; ++x) s.world.setBlock({x, 64, 8}, *r.with(r.defaultState(blocks::Rail), "shape", "east_west"));
        REQUIRE(Mobs::placeMinecart(s.world, {12, 64, 8}, s.rng, 3));
        MobData* c = findType(s, MobType::Minecart);
        REQUIRE(c);
        c->age = offset;
        c->vel = {0.4, 0.0, 0.0};
        for (int x = 13; x < 33; ++x) s.items.spawn({x + 0.5, 64.2, 8.5}, {diamond, 1}, s.rng);
        s.tick(30);
        int total = 0;
        for (const auto& it : s.items.items()) total += it.stack.item == diamond ? it.stack.count : 0;
        c = findType(s, MobType::Minecart);
        REQUIRE(c);
        for (int cx = -2; cx <= 2; ++cx)
            if (const ItemContents* slots = s.world.chunk({cx, 0})->mobStore(c->uuidHi))
                for (const ItemStack& st : *slots) total += st.item == diamond ? st.count : 0;
        CHECK(total == 20);
        CHECK(c->pos.x > 16.0); // (it crossed)
    }
}

TEST_CASE("M29 review: a fully oxidized copper golem soon turns into a statue, dropping what it carried") {
    MobScene s;
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::CopperGolem, {8.5, 64.0, 8.5}, s.rng)));
    MobData* g = findType(s, MobType::CopperGolem);
    REQUIRE(g);
    g->woolColour = 3; // (oxidized)
    g->mouthItem = *itemRegistry().find("apple");
    g->allayCount = 2;
    int ticks = 0;
    while (findType(s, MobType::CopperGolem) && ticks < 1500) {
        s.tick();
        ++ticks;
    }
    MESSAGE("statue after " << ticks << " ticks");
    CHECK(findType(s, MobType::CopperGolem) == nullptr);
    CHECK(ticks < 1500);
    CHECK(blockRegistry().block(blockRegistry().blockOf(s.world.getBlock({8, 64, 8}))).id ==
          "minecraft:oxidized_copper_golem_statue");
    int apples = 0;
    for (const auto& it : s.items.items()) apples += it.stack.item == *itemRegistry().find("apple") ? it.stack.count : 0;
    CHECK(apples == 2);
}

TEST_CASE("M30.3: a mob with its head in a block suffocates (1 a hurt cooldown)") {
    MobScene s;
    s.survival = false;
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Cow, {8.5, 64.0, 8.5}, s.rng)));
    s.world.setBlock({8, 65, 8}, blockRegistry().defaultState(blocks::Stone)); // (a cow's head ~1.19 up)
    MobData* cow = findType(s, MobType::Cow);
    REQUIRE(cow);
    const float before = cow->health;
    s.tick(25);
    cow = findType(s, MobType::Cow);
    REQUIRE(cow);
    CHECK(before - cow->health >= 2.0f);
    CHECK(before - cow->health <= 3.0f);
}

TEST_CASE("M30.5: a villager opens a wooden door on its way and shuts it behind it") {
    MobScene s;
    s.survival = false;
    s.dayTime = 6000; // (day: it strolls)
    const auto& r = blockRegistry();
    for (int z = -32; z <= 47; ++z)
        for (int y = 64; y <= 66; ++y) s.world.setBlock({10, y, z}, r.defaultState(blocks::Stone));
    const BlockStateId door = *r.with(r.defaultState(blocks::OakDoor), "facing", "east"); // (across the way)
    s.world.setBlock({10, 64, 8}, *r.with(door, "half", "lower"));
    s.world.setBlock({10, 65, 8}, *r.with(door, "half", "upper"));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Villager, {7.5, 64.0, 8.5}, s.rng)));
    bool opened = false, through = false;
    for (int t = 0; t < 400 && !(through && r.get(s.world.getBlock({10, 64, 8}), properties::open) == 1); ++t) {
        MobData* v = findType(s, MobType::Villager);
        REQUIRE(v);
        if (!through) {
            v->goal = {13.5, 64.0, 8.5}; // (held: its own goals would wander off)
            v->goalTicks = 0;
        }
        s.tick();
        v = findType(s, MobType::Villager);
        opened = opened || r.get(s.world.getBlock({10, 64, 8}), properties::open) == 0;
        through = through || v->pos.x > 11.5;
    }
    CHECK(opened);
    CHECK(through);
    CHECK(r.get(s.world.getBlock({10, 64, 8}), properties::open) == 1); // (shut again)
}

TEST_CASE("M30 review: small slimes and baby animals fit under a 1-block ceiling without suffocating") {
    MobScene s;
    s.survival = false;
    for (int z = 4; z <= 12; ++z)
        for (int x = 4; x <= 12; ++x) s.world.setBlock({x, 65, z}, blockRegistry().defaultState(blocks::Stone));
    MobData slime = Mobs::make(MobType::Slime, {8.5, 64.0, 8.5}, s.rng);
    slime.size = 1;
    slime.health = 1.0f;
    REQUIRE(Mobs::add(s.world, slime));
    MobData calf = Mobs::make(MobType::Cow, {6.5, 64.0, 6.5}, s.rng);
    calf.age = -24000;
    REQUIRE(Mobs::add(s.world, calf));
    const float calfHealth = calf.health;
    s.tick(30);
    const MobData* sl = findType(s, MobType::Slime);
    REQUIRE(sl);
    CHECK(sl->health == doctest::Approx(1.0f));
    const MobData* c = findType(s, MobType::Cow);
    REQUIRE(c);
    CHECK(c->health == doctest::Approx(calfHealth));
}

TEST_CASE("M30 review: a villager staying just past its door still shuts it (the 10 s fallback)") {
    MobScene s;
    s.survival = false;
    s.dayTime = 6000;
    const auto& r = blockRegistry();
    for (int z = -32; z <= 47; ++z)
        for (int y = 64; y <= 66; ++y) s.world.setBlock({10, y, z}, r.defaultState(blocks::Stone));
    const BlockStateId door = *r.with(r.defaultState(blocks::OakDoor), "facing", "east");
    s.world.setBlock({10, 64, 8}, *r.with(door, "half", "lower"));
    s.world.setBlock({10, 65, 8}, *r.with(door, "half", "upper"));
    REQUIRE(Mobs::add(s.world, Mobs::make(MobType::Villager, {8.5, 64.0, 8.5}, s.rng)));
    bool opened = false, shutAgain = false; // (its own schedule may take it back through later)
    for (int t = 0; t < 400 && !shutAgain; ++t) {
        MobData* v = findType(s, MobType::Villager);
        REQUIRE(v);
        v->goal = {11.6, 64.0, 8.5}; // (stays 1.1 from the door)
        v->goalTicks = 0;
        s.tick();
        const bool open = r.get(s.world.getBlock({10, 64, 8}), properties::open) == 0;
        shutAgain = opened && !open;
        opened = opened || open;
    }
    CHECK(opened);
    CHECK(shutAgain);
}

TEST_CASE("M31 review: a wide mob suffocates when its middle is in a block, even with its corners free") {
    MobScene s;
    s.survival = false;
    MobData g = Mobs::make(MobType::IronGolem, {8.5, 64.0, 8.5}, s.rng); // 1.4 wide, 2.7 tall
    REQUIRE(Mobs::add(s.world, g));
    s.world.setBlock({8, 66, 8}, blockRegistry().defaultState(blocks::Stone)); // (its eyes' block, corners in 7/9)
    const float before = findType(s, MobType::IronGolem)->health;
    s.tick(15);
    CHECK(findType(s, MobType::IronGolem)->health < before);
}
