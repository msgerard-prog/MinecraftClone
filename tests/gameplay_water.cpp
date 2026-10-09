// Water mobs, fish buckets and fishing (M25.2).
#include "gameplay/Buckets.h"
#include "gameplay/Fishing.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Mobs.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Blocks.h"
#include "world/ChunkSerializer.h"
#include "world/Enchantments.h"
#include "world/Potions.h"

#include <doctest/doctest.h>

#include <map>

using namespace mc;
using namespace mc::world;

namespace {

// A pool: stone floor at y 63, water y 64..70, over 5x5 chunks; the player far off.
struct Pool {
    World world;
    Player player;
    Vitals vitals;
    Xoroshiro rng{21};
    ItemEntities items;
    Mobs mobs;
    Pool() {
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
                        for (int y = 64; y <= 70; ++y) c.set(x, y, z, blockRegistry().defaultState(blocks::Water));
                    }
                std::array<std::shared_ptr<const SectionLight>, kMaxSections> light;
                light.fill(std::make_shared<const SectionLight>());
                c.setLight(light);
            }
        player.setPosition({30.5, 71.0, 30.5});
        player.setCreative(true);
    }
    void tick(int n, bool survival = false) {
        for (int i = 0; i < n; ++i) {
            player.tick(world, {});
            Mobs::Context ctx{world, player, vitals, survival, false, 6000, 0.0f, rng, items};
            ctx.naturalSpawning = false;
            mobs.tick(ctx);
        }
    }
    std::vector<MobData*> all() {
        std::vector<MobData*> out;
        world.forEachChunk([&](Chunk& c) {
            for (auto& m : c.mobs()) out.push_back(&m);
        });
        return out;
    }
};

} // namespace

TEST_CASE("fish swim about in the water without leaving it; out of water they flop and suffocate (M25.2)") {
    Pool p;
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Cod, {0.5, 66.0, 0.5}, p.rng)));
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Squid, {4.5, 66.0, 4.5}, p.rng)));
    p.tick(400);
    double moved = 0.0;
    for (MobData* m : p.all()) {
        CHECK(m->pos.y > 63.9);
        CHECK(m->pos.y < 71.0);
        CHECK(m->health == mobInfo(m->type).maxHealth);
        moved += glm::length(m->pos - glm::dvec3(m->type == MobType::Cod ? 0.5 : 4.5, 66.0, m->type == MobType::Cod ? 0.5 : 4.5));
    }
    CHECK(moved > 1.0);
    // On dry land: after 15 s of air, 2 damage a second.
    for (int y = 64; y <= 70; ++y)
        for (int z = 2; z < 22; ++z)
            for (int x = 2; x < 22; ++x) p.world.setBlock({x, y, z}, 0);
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Salmon, {11.5, 64.0, 11.5}, p.rng)));
    p.tick(340);
    bool hurt = false;
    for (MobData* m : p.all()) hurt = hurt || (m->type == MobType::Salmon && m->health < 3.0f);
    CHECK(hurt);
}

TEST_CASE("a pufferfish puffs up near a survival player and stings with poison (M25.2)") {
    Pool p;
    p.player.setCreative(false);
    p.player.setPosition({0.5, 64.0, 0.5});
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Pufferfish, {0.5, 64.5, 1.0}, p.rng)));
    p.tick(60, true);
    int puff = 0;
    for (MobData* m : p.all()) puff = m->size;
    CHECK(puff == 2);
    CHECK(p.vitals.effectLevel(Effect::Poison) > 0);
    CHECK(p.vitals.health() < Vitals::kMaxHealth);
}

TEST_CASE("fish buckets: which fish each holds; fish save their variant, puff state and bucket origin (M25.2)") {
    CHECK(bucketFish(*itemRegistry().find("salmon_bucket")) == MobType::Salmon);
    CHECK(bucketFish(*itemRegistry().find("water_bucket")) == MobType::Count);
    CHECK(fishBucketFor(MobType::Pufferfish) == *itemRegistry().find("pufferfish_bucket"));
    CHECK(fishBucketFor(MobType::Squid) == 0); // (squid can't be bucketed)
    Chunk c({0, 0});
    Xoroshiro rng(3);
    MobData t = Mobs::make(MobType::TropicalFish, {3.5, 64.0, 3.5}, rng);
    t.fromBucket = true;
    MobData f = Mobs::make(MobType::Pufferfish, {5.5, 64.0, 3.5}, rng);
    f.size = 2;
    c.mobs().push_back(t);
    c.mobs().push_back(f);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 2);
    CHECK(back.mobs()[0].size == t.size);
    CHECK(back.mobs()[0].woolColour == t.woolColour);
    CHECK(back.mobs()[0].color2 == t.color2);
    CHECK(back.mobs()[0].fromBucket);
    CHECK(back.mobs()[1].size == 2);
}

TEST_CASE("fishing: the bobber floats, a fish bites after 5-30 s, reeling in then catches mostly fish (M25.2)") {
    Pool p;
    Fishing f;
    const glm::dvec3 eye{0.5, 73.0, 0.5};
    f.cast(eye, glm::normalize(glm::dvec3(0.0, -0.4, 1.0)), 0, 0, p.rng);
    bool bit = false;
    int t = 0;
    for (; t < 800 && !bit; ++t) {
        f.tick(p.world, eye, p.rng);
        bit = f.biting();
    }
    REQUIRE(bit);
    CHECK(t >= 100); // (5 s at least without Lure)
    CHECK(f.bobber().y > 69.0); // floating near the surface
    ExperienceOrbs orbs;
    CHECK(f.reel(p.world, eye, p.items, &orbs, p.rng) == 1); // the rod wears 1
    REQUIRE(p.items.items().size() == 1);
    CHECK(glm::length(p.items.items()[0].vel) > 0.1); // flying to the player
    // The catch table: fish 85%, the rest junk and treasure.
    std::map<std::string, int> counts;
    int fish = 0;
    for (int i = 0; i < 2000; ++i) {
        const ItemStack s = Fishing::rollCatch(0, p.rng);
        const std::string id(itemRegistry().item(s.item).id);
        fish += id == "minecraft:cod" || id == "minecraft:salmon" || id == "minecraft:pufferfish" ||
                id == "minecraft:tropical_fish";
        ++counts[id];
    }
    CHECK(fish > 1650);
    CHECK(fish < 1900);
    CHECK(counts["minecraft:cod"] > counts["minecraft:salmon"]);
    // Lure: bites 5 s sooner per level; a rod takes Lure and Luck of the Sea.
    CHECK(canEnchant(*itemRegistry().find("fishing_rod"), Enchantment::Lure));
    CHECK_FALSE(canEnchant(*itemRegistry().find("bow"), Enchantment::LuckOfTheSea));
}

TEST_CASE("boats float, paddle up to 0.4 blocks a tick, turn 1 degree a tick more, keep their wood when saved (M25.2b)") {
    Pool p;
    REQUIRE(Mobs::placeBoat(p.world, {0.5, 70.8, 0.5}, 0.0f, 7, p.rng)); // cherry
    MobData* boat = nullptr;
    auto find = [&] {
        boat = nullptr;
        for (MobData* m : p.all())
            if (m->type == MobType::Boat) boat = m;
        return boat != nullptr;
    };
    REQUIRE(find());
    p.tick(60);
    REQUIRE(find());
    CHECK(boat->pos.y > 70.0); // afloat (the water's top is y 71)
    CHECK(boat->pos.y < 71.2);
    for (int t = 0; t < 120; ++t) {
        REQUIRE(find());
        boat->paddleForward = 1;
        p.tick(1);
    }
    REQUIRE(find());
    const double speed = glm::length(glm::dvec2(boat->vel.x, boat->vel.z));
    CHECK(speed > 0.3);
    CHECK(speed < 0.41);
    CHECK(boat->vel.z > 0.0); // yaw 0: toward +Z (south)
    const float yaw0 = boat->yaw;
    for (int t = 0; t < 10; ++t) {
        REQUIRE(find());
        boat->paddleTurn = 1;
        p.tick(1);
    }
    REQUIRE(find());
    CHECK(boat->yaw - yaw0 > 20.0f); // (turning builds up: 1, 1.9, 2.7...)
    Chunk c({0, 0});
    c.mobs().push_back(*boat);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 1);
    CHECK(back.mobs()[0].type == MobType::Boat);
    CHECK(back.mobs()[0].woolColour == 7);
    CHECK(boatId(9) == "minecraft:bamboo_raft");
    CHECK(itemRegistry().find("cherry_boat").has_value());
}

#include "gameplay/Combat.h"
#include "gameplay/Inventory.h"
#include "gameplay/Projectiles.h"

TEST_CASE("a zombie under water becomes a drowned after 45 s; drowned swim up to a player in the water (M25.3)") {
    Pool p;
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Zombie, {0.5, 64.0, 0.5}, p.rng)));
    p.tick(905);
    int drowned = 0;
    for (MobData* m : p.all()) drowned += m->type == MobType::Drowned;
    CHECK(drowned == 1);
    // A survival player swimming nearby at night: it comes for them.
    p.player.setCreative(false);
    p.player.setPosition({6.5, 68.0, 6.5});
    double before = 0.0, after = 0.0;
    for (MobData* m : p.all())
        if (m->type == MobType::Drowned) before = glm::length(m->pos - p.player.position());
    for (int t = 0; t < 100; ++t) {
        p.player.setPosition({6.5, 68.0, 6.5});
        p.player.tick(p.world, {});
        Mobs::Context ctx{p.world, p.player, p.vitals, true, false, 18000, 11.0f, p.rng, p.items};
        ctx.naturalSpawning = false;
        p.mobs.tick(ctx);
    }
    for (MobData* m : p.all())
        if (m->type == MobType::Drowned) after = glm::length(m->pos - p.player.position());
    CHECK(after < before);
    CHECK(isZombie(MobType::Drowned)); // (undead: Smite works, it burns in the sun)
}

TEST_CASE("tridents: thrown at 2.5, 8 damage, stick and come back with Loyalty; Riptide launches the player instead (M25.3)") {
    Pool p;
    for (int y = 64; y <= 70; ++y) // (dry air for the throw)
        for (int z = -16; z < 32; ++z)
            for (int x = -16; x < 32; ++x) p.world.setBlock({x, y, z}, 0);
    Inventory inv;
    const ItemId tridentId = *itemRegistry().find("trident");
    inv.setSlot(0, {tridentId, 1});
    Projectiles proj;
    const glm::dvec3 eye{0.5, 65.6, 0.5};
    p.player.setPosition({0.5, 64.0, 0.5});
    CHECK(releaseTrident(inv, 5, true, false, eye, {0, 0, 1}, proj, p.rng) == 0.0); // (too short a draw)
    CHECK(proj.items().empty());
    releaseTrident(inv, 20, true, false, eye, glm::normalize(glm::dvec3(0, -0.08, 1)), proj, p.rng);
    REQUIRE(proj.items().size() == 1);
    CHECK(inv.slot(0).empty()); // (it left the hand)
    CHECK(proj.items()[0].stack.damage == 1);
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Cow, {0.5, 64.0, 6.5}, p.rng)));
    for (int t = 0; t < 60; ++t) proj.tick(p.world, p.player, &p.vitals, inv, true, p.rng);
    float cowHealth = 0.0f;
    for (MobData* m : p.all()) cowHealth = m->health;
    CHECK(cowHealth == doctest::Approx(10.0f - 8.0f));
    // Loyalty: thrown again, it hits a wall and flies back into the pack.
    ItemStack held{tridentId, 1};
    setEnchantment(held, Enchantment::Loyalty, 3);
    inv.setSlot(0, held);
    for (int y = 60; y < 72; ++y) p.world.setBlock({-6, y, 0}, blockRegistry().defaultState(blocks::Stone));
    releaseTrident(inv, 20, true, false, eye, {-1, 0, 0}, proj, p.rng);
    bool back = false;
    for (int t = 0; t < 400 && !back; ++t) {
        proj.tick(p.world, p.player, &p.vitals, inv, true, p.rng);
        for (int sl = 0; sl < Inventory::kSlots; ++sl) back = back || inv.slot(sl).item == tridentId;
    }
    CHECK(back);
    // Riptide: no throw; in water it launches the player along the look.
    ItemStack rip{tridentId, 1};
    setEnchantment(rip, Enchantment::Riptide, 3);
    inv.setSlot(1, rip);
    inv.select(1);
    const size_t flying = proj.items().size();
    CHECK(releaseTrident(inv, 20, true, false, eye, {0, 0, 1}, proj, p.rng) == 0.0); // dry: nothing
    CHECK(releaseTrident(inv, 20, true, true, eye, {0, 0, 1}, proj, p.rng) == doctest::Approx(3.0));
    CHECK(proj.items().size() == flying);
    CHECK(conflicts(Enchantment::Riptide, Enchantment::Loyalty));
    CHECK_FALSE(conflicts(Enchantment::Loyalty, Enchantment::Channeling));
    // Impaling: +2.5 a level on water mobs.
    MeleeHit h;
    h.itemDamage = 9.0f;
    h.impaling = 2;
    h.aquatic = true;
    CHECK(meleeDamage(h) == doctest::Approx(14.0f));
}

#include "world/BlockUpdates.h"

TEST_CASE("dolphins give swimming players Dolphin's Grace, which keeps their speed in water (M25.3b)") {
    Pool p;
    p.player.setCreative(false);
    p.player.setPosition({0.5, 66.0, 0.5});
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Dolphin, {2.5, 67.0, 0.5}, p.rng)));
    p.tick(5, true);
    CHECK(p.vitals.effectLevel(Effect::DolphinsGrace) == 1);
    // Swimming forward for 2 s with and without it.
    auto swim = [&](bool grace) {
        Player pl;
        pl.setCreative(false);
        pl.setPosition({0.5, 66.0, 0.5});
        pl.setDolphinsGrace(grace);
        PlayerInput in;
        in.forward = 1.0f;
        for (int t = 0; t < 40; ++t) pl.tick(p.world, in);
        return glm::length(pl.position() - glm::dvec3(0.5, 66.0, 0.5));
    };
    CHECK(swim(true) > swim(false) * 1.5);
}

TEST_CASE("turtles fed seagrass carry eggs home and lay them in the sand; eggs hatch into babies that grow a scute (M25.3b)") {
    Pool p;
    for (int z = -16; z < 32; ++z) // a beach: sand floor, no water
        for (int x = -16; x < 32; ++x) {
            for (int y = 64; y <= 70; ++y) p.world.setBlock({x, y, z}, 0);
            p.world.setBlock({x, 63, z}, blockRegistry().defaultState(blocks::Sand));
            p.world.setBlock({x, 62, z}, blockRegistry().defaultState(blocks::Stone)); // (sand falls over air)
        }
    for (int i = 0; i < 2; ++i) {
        MobData t = Mobs::make(MobType::Turtle, {2.5 + i, 64.0, 2.5}, p.rng);
        t.home = {8, 64, 8};
        t.loveTicks = 600; // (fed seagrass)
        REQUIRE(Mobs::add(p.world, t));
    }
    CHECK(Mobs::isFood(MobType::Turtle, itemRegistry().blockItem(blocks::Seagrass)));
    p.tick(900);
    int eggsAtHome = 0; // (laid within 2 blocks of home)
    for (int z = 6; z <= 10; ++z)
        for (int x = 6; x <= 10; ++x) eggsAtHome += blockRegistry().blockOf(p.world.getBlock({x, 64, z})) == blocks::TurtleEgg;
    CHECK(eggsAtHome == 1);
    // Eggs hatch at night on sand: two cracks, then babies.
    BlockUpdates updates(p.world);
    updates.setDayTime(21500); // (just before dawn: eggs crack)
    p.world.setBlock({2, 64, 12}, blockRegistry().set(blockRegistry().defaultState(blocks::TurtleEgg), properties::eggs, 2));
    updates.setRandomTicks({0, 0}, 1, 1000); // (not 4096: the random's low bits repeat every 4096 draws)
    bool three = false; // (the 3-egg clutch; the turtles' own may hatch too, before or after)
    for (int t = 0; t < 400 && !three; ++t) {
        updates.setTime(t);
        updates.tick();
        for (const auto& h : updates.hatched()) three = three || (h.pos == BlockPos{2, 64, 12} && h.count == 3);
    }
    CHECK(three);
    // A baby about to grow up drops a scute.
    MobData baby = Mobs::make(MobType::Turtle, {12.5, 64.0, 2.5}, p.rng);
    baby.age = -2;
    REQUIRE(Mobs::add(p.world, baby));
    p.tick(3);
    bool scute = false;
    for (const auto& it : p.items.items()) scute = scute || it.stack.item == *itemRegistry().find("turtle_scute");
    CHECK(scute);
}

#include "gameplay/Mining.h"
#include "gameplay/Recipes.h"

TEST_CASE("guardians charge their laser for 4 s then hit for 6; elders give Mining Fatigue III, which slows mining (M25.5)") {
    Pool p;
    p.player.setCreative(false);
    p.player.setPosition({0.5, 66.0, 0.5});
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Guardian, {6.5, 66.0, 0.5}, p.rng)));
    p.tick(70, true);
    CHECK(p.vitals.health() == doctest::Approx(Vitals::kMaxHealth)); // still charging
    bool beam = false;
    for (MobData* m : p.all()) beam = beam || (m->type == MobType::Guardian && m->hasBeam);
    CHECK(beam);
    p.tick(15, true);
    CHECK(p.vitals.health() < Vitals::kMaxHealth - 4.0f); // 6 (no armor)
    // An elder: within a minute, Mining Fatigue III.
    Pool e;
    e.player.setCreative(false);
    e.player.setPosition({0.5, 66.0, 0.5});
    for (int y = 64; y <= 70; ++y) // (out of its sight: a wall)
        for (int z = -3; z <= 3; ++z) e.world.setBlock({3, y, z}, blockRegistry().defaultState(blocks::Stone));
    REQUIRE(Mobs::add(e.world, Mobs::make(MobType::ElderGuardian, {10.5, 66.0, 0.5}, e.rng)));
    e.tick(1210, true);
    CHECK(e.vitals.effectLevel(Effect::MiningFatigue) == 3);
    const BlockStateId stone = blockRegistry().defaultState(blocks::Stone);
    const ItemStack pick{*itemRegistry().find("iron_pickaxe"), 1};
    CHECK(breakTicks(stone, pick, true, false, 0, 3) > breakTicks(stone, pick, true, false) * 30);
}

TEST_CASE("a sponge soaks up the water around it and turns wet; a wet sponge dries in a furnace (M25.5)") {
    World w;
    BlockUpdates updates(w);
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) {
            Chunk& c = w.createChunk({cx, cz});
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x) {
                    c.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
                    for (int y = 64; y <= 66; ++y) c.set(x, y, z, blockRegistry().defaultState(blocks::Water));
                }
        }
    w.updateBlock({0, 64, 0}, blockRegistry().defaultState(blocks::Sponge));
    CHECK(blockRegistry().blockOf(w.getBlock({0, 64, 0})) == blocks::WetSponge);
    int removed = 0;
    for (int y = 64; y <= 66; ++y)
        for (int z = -8; z <= 8; ++z)
            for (int x = -8; x <= 8; ++x) removed += w.getBlock({x, y, z}) == 0;
    CHECK(removed == 65); // (its limit)
    CHECK(w.getBlock({1, 64, 0}) == 0);
    CHECK(blockRegistry().blockOf(w.getBlock({8, 64, 8})) == blocks::Water); // beyond its reach
    const auto dried = smelt({itemRegistry().blockItem(blocks::WetSponge), 1});
    REQUIRE(dried.has_value());
    CHECK(dried->item == itemRegistry().blockItem(blocks::Sponge));
}

TEST_CASE("review fixes (M25): turtles and drowned save their data; guardians live on land; fatigue III is 0.27%") {
    Chunk c({0, 0});
    Xoroshiro rng(4);
    MobData t = Mobs::make(MobType::Turtle, {3.5, 64.0, 3.5}, rng);
    t.hasEgg = true;
    t.home = {12, 64, -7};
    MobData d = Mobs::make(MobType::Drowned, {5.5, 64.0, 3.5}, rng);
    d.heldTrident = true;
    c.mobs().push_back(t);
    c.mobs().push_back(d);
    Chunk back({0, 0});
    entitiesFromNbt(entitiesToNbt(ChunkSnapshot::of(c, 0)), back);
    REQUIRE(back.mobs().size() == 2);
    CHECK(back.mobs()[0].hasEgg);
    CHECK(back.mobs()[0].home == glm::ivec3{12, 64, -7});
    CHECK(back.mobs()[1].heldTrident);
    // A guardian beached for a minute stays unhurt (wiki: it never suffocates).
    Pool p;
    for (int y = 64; y <= 70; ++y)
        for (int z = -8; z <= 8; ++z)
            for (int x = -8; x <= 8; ++x) p.world.setBlock({x, y, z}, 0);
    REQUIRE(Mobs::add(p.world, Mobs::make(MobType::Guardian, {0.5, 64.0, 0.5}, p.rng)));
    p.tick(1200);
    for (MobData* m : p.all())
        if (m->type == MobType::Guardian) CHECK(m->health == doctest::Approx(30.0f));
    // Mining Fatigue III: 0.27% of the speed.
    const BlockStateId stone = blockRegistry().defaultState(blocks::Stone);
    const ItemStack pick{*itemRegistry().find("iron_pickaxe"), 1};
    CHECK(breakTicks(stone, pick, true, false, 0, 3) > breakTicks(stone, pick, true, false) * 300);
}

TEST_CASE("review fixes (M25): pistons break kelp leaving its water; coral plants live beside water") {
    World w;
    BlockUpdates updates(w);
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) {
            Chunk& ch = w.createChunk({cx, cz});
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x) {
                    ch.set(x, 63, z, blockRegistry().defaultState(blocks::Stone));
                    for (int y = 64; y <= 66; ++y) ch.set(x, y, z, blockRegistry().defaultState(blocks::Water));
                }
        }
    w.setBlock({1, 64, 0}, blockRegistry().defaultState(blocks::Kelp));
    w.setBlock({0, 64, 0}, *blockRegistry().parse("minecraft:piston[facing=east]"));
    w.updateBlock({0, 65, 0}, blockRegistry().defaultState(blocks::RedstoneBlock));
    for (int t = 0; t < 6; ++t) {
        updates.setTime(t);
        updates.tick();
    }
    CHECK(blockRegistry().blockOf(w.getBlock({1, 64, 0})) == blocks::PistonHead); // the head took its place
    CHECK(blockRegistry().blockOf(w.getBlock({2, 64, 0})) != blocks::Kelp); // broken off, not carried
    CHECK_FALSE(updates.drops().empty());
    // A dry coral fan with water beside it lives.
    const BlockStateId dryFan = blockRegistry().set(blockRegistry().defaultState(*blockRegistry().findBlock("tube_coral_fan")),
                                                    properties::waterlogged, 1);
    w.updateBlock({6, 64, 6}, dryFan);
    for (int t = 10; t < 130; ++t) {
        updates.setTime(t);
        updates.tick();
    }
    CHECK(blockRegistry().blockOf(w.getBlock({6, 64, 6})) == *blockRegistry().findBlock("tube_coral_fan"));
}
