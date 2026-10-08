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
    CHECK(most <= 2);
    int rewards = 0; // (a key, or half the time a consumable)
    for (const auto& it : d.items.items()) {
        const std::string_view id = itemRegistry().item(it.stack.item).id;
        rewards += id == "minecraft:trial_key" || id == "minecraft:cooked_chicken" || id == "minecraft:bread" ||
                   id == "minecraft:baked_potato" || id == "minecraft:potion";
    }
    CHECK(rewards == 1);
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
    d.tick(400);
    int eggs = 0; // (dropped as an item - wiki)
    for (const auto& it : d.items.items()) eggs += it.stack.item == itemRegistry().blockItem(blocks::SnifferEgg);
    CHECK(eggs == 1);
}

TEST_CASE("a brush gets a scute from an armadillo (M27.5c)") {
    Deep d;
    MobData a = Mobs::make(MobType::Armadillo, {0.5, 60.0, 0.5}, d.rng);
    CHECK(Mobs::interact(a, *itemRegistry().find("brush"), d.rng, d.items) == Mobs::Use::Sheared);
    int scutes = 0;
    for (const auto& it : d.items.items()) scutes += it.stack.item == *itemRegistry().find("armadillo_scute");
    CHECK(scutes == 1);
}

#include "gameplay/Combat.h"
#include "gameplay/Recipes.h"
#include "world/Loot.h"

TEST_CASE("Trial Omen: Bad Omen near a trial spawner turns it and the vaults nearby ominous (M28.4d)") {
    Deep d;
    const auto& r = blockRegistry();
    d.world.updateBlock({0, 60, 0}, r.defaultState(blocks::TrialSpawner));
    d.world.chunk({0, 0})->spawner(0, 60, 0)->mob = MobType::Zombie;
    d.world.updateBlock({4, 60, 3}, r.defaultState(blocks::Vault));
    d.world.markTicking({0, 0});
    d.player.setPosition({0.5, 60.0, 8.5});
    d.vitals.addEffect(Effect::BadOmen, 1, 6000); // (level II)
    d.tick(2);
    CHECK(d.vitals.effectLevel(Effect::BadOmen) == 0);
    CHECK(d.vitals.effectLevel(Effect::TrialOmen) == 1);
    CHECK(r.get(d.world.getBlock({0, 60, 0}), properties::ominous) == 0);
    CHECK(r.get(d.world.getBlock({4, 60, 3}), properties::ominous) == 0);
    // The ominous vault's table can give the heavy core.
    bool core = false;
    Xoroshiro rng{12};
    for (int i = 0; i < 400 && !core; ++i) {
        std::array<ItemStack, 27> loot{};
        fillChest(LootTable::TrialVaultOminous, rng, loot);
        for (const auto& s : loot) core = core || (!s.empty() && itemRegistry().item(s.item).id == "minecraft:heavy_core");
    }
    CHECK(core);
}

TEST_CASE("the mace: smash damage by the height fallen, Density, Wind Burst; the recipe (M28.4d)") {
    CHECK(maceSmashBonus(1.0, 0) == 0.0f);
    CHECK(maceSmashBonus(3.0, 0) == doctest::Approx(12.0f));
    CHECK(maceSmashBonus(8.0, 0) == doctest::Approx(22.0f));
    CHECK(maceSmashBonus(10.0, 0) == doctest::Approx(24.0f));
    CHECK(maceSmashBonus(4.0, 2) == doctest::Approx(14.0f + 4.0f));
    CHECK(windBurstLift(1) > 1.0);
    CHECK(windBurstLift(3) > windBurstLift(1));
    std::array<ItemStack, 4> g{};
    g[0] = {*itemRegistry().find("heavy_core"), 1};
    g[2] = {*itemRegistry().find("breeze_rod"), 1};
    const auto m = craft(g, 2);
    REQUIRE(m);
    CHECK(itemRegistry().item(m->item).id == "minecraft:mace");
}

#include "world/Enchantments.h"

TEST_CASE("spears: seven tiers, a long jab, a charge by speed, Lunge; crafted on the diagonal (M28.4e)") {
    const auto& items = itemRegistry();
    for (const char* id : {"wooden_spear", "stone_spear", "copper_spear", "iron_spear", "golden_spear", "diamond_spear",
                           "netherite_spear"}) {
        const auto s = items.find(id);
        REQUIRE(s);
        CHECK(items.item(*s).tool == ToolType::Spear);
        CHECK(items.item(*s).durability > 0);
    }
    CHECK(kSpearReach > 3.0);
    CHECK(spearChargeDamage(5.0f, 0.1) == 0.0f);   // (walking: no charge)
    CHECK(spearChargeDamage(5.0f, 0.3) == doctest::Approx(6.0f)); // (sprinting)
    CHECK(spearChargeDamage(5.0f, 0.6) == doctest::Approx(12.0f)); // (on a horse)
    CHECK(canEnchant(*items.find("iron_spear"), Enchantment::Lunge));
    CHECK_FALSE(canEnchant(*items.find("iron_sword"), Enchantment::Lunge));
    std::array<ItemStack, 9> g{};
    g[2] = {*items.find("iron_ingot"), 1};
    g[4] = g[6] = {*items.find("stick"), 1};
    const auto r = craft(g, 3);
    REQUIRE(r);
    CHECK(items.item(r->item).id == "minecraft:iron_spear");
}
