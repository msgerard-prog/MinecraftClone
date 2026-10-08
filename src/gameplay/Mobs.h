#pragma once

#include "gameplay/Aabb.h"
#include "gameplay/Explosion.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Pathfinder.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Direction.h"
#include "world/Mob.h"
#include "world/Random.h"
#include "world/Weather.h"
#include "world/World.h"

#include <optional>
#include <array>
#include <span>
#include <utility>
#include <vector>

namespace mc {

// Mob simulation (M10, wiki: Mob, Zombie, Cow, Spawn): physics, simple AI goals,
// damage and death with loot, despawning and natural spawning. Mobs live in their
// chunk's `mobs()` (saved with it); this moves them between chunks as they walk.
class Mobs {
public:
    struct Context {
        world::World& world;
        Player& player;
        Vitals& vitals;
        bool survival;
        bool playerDead;
        int64_t dayTime;
        float skyDarken; // sky light levels lost (0..11)
        world::Xoroshiro& rng;
        ItemEntities& items;
        bool naturalSpawning = true; // Overworld and Nether monsters (none in the End yet)
        world::ItemId heldItem = 0;  // what the player holds (animals follow their food)
        std::vector<world::BlockPos>* edits = nullptr; // blocks mobs changed (sheep, creepers, endermen)
        class Projectiles* projectiles = nullptr;      // skeletons shoot into it
        class ExperienceOrbs* orbs = nullptr;          // experience from kills and breeding
        bool wearsGold = false; // a piece of golden armor on: piglins leave the player be (M19.2)
        class PrimedTnt* tnt = nullptr; // explosions set off TNT (M21.1b)
        uint64_t worldSeed = 0;         // slime chunks (M21.5)
        const world::Weather* weather = nullptr; // rain: undead don't burn, endermen get hurt (M22.1)
        bool thundering = false; // monsters spawn as if sky light were 10 lower (any time of day)
        const glm::ivec3* raidCentre = nullptr; // a raid is on (M24.5): raiders head there, villagers hide
        uint64_t playerTargetUuid = 0;   // (M26.1) the mob the player hit last: tamed wolves join in
        uint64_t playerAttackerUuid = 0; // (M26.1) the mob that last hurt the player
        int32_t raidId = 0;                     // (its id: raiders of older raids stay out of it)
        int timeSinceRest = 0;                  // (M26.4a) the player's: phantoms after 3 days awake
        // Game rules (M28.1): mob_drops, mob_griefing, spawn_phantoms; the difficulty
        // (0 peaceful .. 3 hard).
        bool mobDrops = true, mobGriefing = true, spawnPhantoms = true;
        int difficulty = 2;
    };

    // Chunks farther than this (Chebyshev, in chunks) from the player don't tick mobs
    // (vanilla's simulation distance option; 12 on the default Fancy preset, 6 on Fast).
    static constexpr int kDefaultSimulationDistance = 12;
    void setSimulationDistance(int chunks) { m_simulationDistance = chunks; }
    int simulationDistance() const { return m_simulationDistance; }

    Mobs() {
        m_moves.reserve(64);
        m_boxes.reserve(256);
        m_births.reserve(64); // (babies and spawner mobs)
        m_scratchEdits.reserve(4096);
        m_dragonDeaths.reserve(4);
    }

    void tick(Context& ctx);

    // The ender dragon (M20.2): where its head is, and the damage a hit at `at` does
    // (the head takes it all, the rest a quarter + 1; wiki: Ender Dragon).
    static glm::dvec3 dragonHead(const world::MobData& m);
    static float dragonDamage(const world::MobData& m, float damage, const glm::dvec3& at);
    // The dragon's health this tick for the boss bar (below 0: no dragon ticking).
    float bossHealth() const { return m_bossHealth; }
    // Which boss the bar shows (the ender dragon or the Wither - M26.4b) and its top health.
    world::MobType bossType() const { return m_bossType; }
    // Where dragons finished dying this tick (the fight ends there: main).
    const std::vector<glm::dvec3>& dragonDeaths() const { return m_dragonDeaths; }
    // A minecart item used on a rail (M21.4): a cart on it. False if not a rail.
    static bool placeMinecart(world::World& world, const world::BlockPos& rail, world::Xoroshiro& rng);
    // An end crystal item used on the top of obsidian or bedrock (wiki: End Crystal):
    // needs two free blocks above and no entity there. False: nothing placed.
    static bool placeEndCrystal(world::World& world, const world::BlockPos& on, world::Xoroshiro& rng);
    // A mob at `pos` (spawn eggs, commands, natural spawning).
    static world::MobData make(world::MobType type, const glm::dvec3& pos, world::Xoroshiro& rng);
    static uint8_t naturalWoolColour(world::Xoroshiro& rng);
    // Adds a mob to the chunk it stands in (false if that chunk isn't loaded).
    // A boat of `wood` (kBoatWoods) facing `yaw` (M25.2b).
    static bool placeBoat(world::World& world, const glm::dvec3& at, float yaw, int wood, world::Xoroshiro& rng,
                          bool chest = false);
    static bool add(world::World& world, const world::MobData& mob);
    // A lightning bolt at `at` (wiki: Lightning): mobs within 3 blocks (6 up) take 5
    // damage and burn 8 s; creepers become charged, pigs zombified piglins. Returns
    // whether any mob was hit.
    static bool strikeLightning(world::World& world, const glm::dvec3& at);

    // The mob the player's look ray hits first within `reach` blocks, if it's nearer
    // than `blockDistance` (attacks prefer the mob in front of a block).
    struct MobHit {
        world::ChunkPos chunk;
        int index;
        double distance;
    };
    // `skipUuidHi`: a mob to ignore (an arrow's shooter); 0 = none.
    static std::optional<MobHit> raycast(world::World& world, const glm::dvec3& eye, const glm::dvec3& dir,
                                         double reach, uint64_t skipUuidHi = 0);
    // Right-click on a mob with `held` (M16.3; wiki: Breeding, Sheep): feeding its food
    // puts an adult in love mode (or speeds a baby's growth by 10%), shears shear a
    // sheep (1-3 wool). Returns what happened so the caller uses up / wears the item.
    // (Sat: a pet sat down or stood up - M26.1; Ride: the player got on a mount - M26.2)
    enum class Use { None, Fed, Sheared, Sat, Ride };
    static Use interact(world::MobData& mob, world::ItemId held, world::Xoroshiro& rng, ItemEntities& items);
    // A pet's top health (tamed wolves: 40 - M26.1).
    static float petMaxHealth(const world::MobData& m);
    // The mob that last hurt the player (tamed wolves go for it).
    uint64_t playerAttacker() const { return m_playerAttacker; }
    static bool isFood(world::MobType type, world::ItemId item); // breeding / tempting food
    // Mounts (Mounts.cpp, M26.2): where the rider's feet sit above the mount (boats and
    // minecarts too), and the gear a dead mount leaves (saddle, armor, carpet, its chest).
    static double seatHeight(const world::MobData& m);
    // A foal's type (a horse and a donkey: a mule) and stats from its parents.
    static void mountOffspring(const world::MobData& a, const world::MobData& b, world::MobData& baby,
                               world::Xoroshiro& rng);
    // A wild animal's young (M26.3): panda genes, trusting fox kits, rabbit coats.
    static void wildlifeOffspring(const world::MobData& a, const world::MobData& b, world::MobData& baby,
                                  world::Xoroshiro& rng);
    // The player hits a mob for `damage` (knockback away from the player).
    static void attack(world::MobData& mob, float damage, const glm::dvec3& from);

    int hostileCount() const { return m_hostiles; }
    static Aabb box(const world::MobData& m);
    // Statistics (M28.1d): mobs the player killed and animals bred since the last call.
    std::span<const world::MobType> playerKills() const { return {m_kills.data(), size_t(m_killCount)}; }
    void clearPlayerKills() { m_killCount = 0; }
    int takeBred() { return std::exchange(m_bred, 0); }
    static int takeCured(); // (M28.5c) zombie villagers cured since the last call
    // Item frames and paintings (M28.3a, Hanging.cpp; wiki: Item Frame, Painting): hung on
    // the face `face` of `support` (paintings on walls only: the biggest canvases that fit,
    // one at random); false if there is no room.
    static bool placeHanging(world::World& world, world::MobType type, const world::BlockPos& support,
                             world::Direction face, world::Xoroshiro& rng);
    static bool hangingSurvives(const world::World& world, const world::MobData& m);
    static Aabb hangingBox(const world::MobData& m);
    // Right-click on an item frame: an empty one takes one of `held` (returned true: use it
    // up), a full one turns its item 45 degrees.
    static bool useItemFrame(world::World& world, world::MobData& frame, const world::ItemStack& held);
    static world::ItemStack frameItem(const world::World& world, const world::MobData& frame);
    // A hit on an item frame holding something drops the item instead of breaking it.
    static bool popFrameItem(world::World& world, world::MobData& frame, ItemEntities& items, world::Xoroshiro& rng);
    // Armor stands (M28.3b, ArmorStands.cpp; wiki: Armor Stand): placed in `cell` facing
    // the player; a right-click puts `held` on (or, empty-handed, takes the piece at the
    // clicked height `hitY` back into `held`); a hit returns true when it broke.
    static bool placeArmorStand(world::World& world, const world::BlockPos& cell, float playerYaw, world::Xoroshiro& rng);
    static bool useArmorStand(world::World& world, world::MobData& m, world::ItemStack& held, double hitY);
    static bool hitArmorStand(world::World& world, world::MobData& m, bool creative);
    static void refreshWorn(world::World& world, world::MobData& m);
    // Leads (M28.3c, Leads.cpp; wiki: Lead): put a mob on the player's lead; tie the
    // player's mobs within 7 blocks to a fence (a knot appears; returns how many); take
    // the mobs tied to a knot back; break a knot (their leads drop).
    static bool leashToPlayer(world::MobData& m);
    static int tieToFence(world::World& world, const world::BlockPos& fence, const glm::dvec3& player,
                          world::Xoroshiro& rng);
    static int takeFromKnot(world::World& world, const world::MobData& knot);
    static void breakKnot(world::World& world, world::MobData& knot, ItemEntities& items, world::Xoroshiro& rng);
    // The mob with this UUID, searched from the chunks around `near` outward.
    static world::MobData* mobByUuid(world::World& world, const glm::dvec3& near, uint64_t uuid);

private:
    std::array<world::MobType, 32> m_kills{};
    int m_killCount = 0, m_bred = 0;
    world::Xoroshiro m_soundRng{0xa3b1'e47cull}; // ambient sound timing only
    void ai(Context& ctx, world::MobData& m);
    void physics(const world::World& world, world::MobData& m, const glm::dvec3& wish, bool jump);
    void spawnHostiles(Context& ctx);
    void tickSpawners(Context& ctx, world::Chunk& chunk); // M18.3
    // Nether mobs (M19.2, NetherMobs.cpp): ghasts, blazes and magma cubes move and
    // attack on their own (true: handled); zombified piglins' anger runs down.
    bool netherAi(Context& ctx, world::MobData& m);
    bool waterAi(Context& ctx, world::MobData& m); // fish and squid (WaterMobs.cpp, M25.2)
    void boatTick(Context& ctx, world::MobData& m); // (Boats.cpp, M25.2b)
    // Pets (Pets.cpp, M26.1).
    static Use petInteract(world::MobData& m, world::ItemId held, world::Xoroshiro& rng);
    bool petGoal(Context& ctx, world::MobData& m, double& speed);
    void spawnCreatures(Context& ctx);
    // Mounts (Mounts.cpp, M26.2).
    static bool isMountFood(world::MobType type, world::ItemId item);
    static void initMount(world::MobData& m, world::Xoroshiro& rng);
    static bool canMate(const world::MobData& a, const world::MobData& b);
    static Use mountInteract(world::MobData& m, world::ItemId held, world::Xoroshiro& rng, ItemEntities& items);
    bool mountTick(Context& ctx, world::MobData& m); // ridden: steering, jumps, taming (true: handled)
    bool mountGoal(Context& ctx, world::MobData& m, double& speed); // camels sitting, trader llamas
    void llamaTick(Context& ctx, world::MobData& m);                // spitting
    void dropMountGear(Context& ctx, world::MobData& m);
    void spawnMounts(Context& ctx, world::Biome biome, int x, int y, int z);
    // Wildlife (Wildlife.cpp, M26.3): rabbits, foxes, polar bears, pandas, goats, armadillos.
    static void initWildlife(world::MobData& m, world::Xoroshiro& rng);
    bool wildlifeGoal(Context& ctx, world::MobData& m, double& speed);
    void wildlifeTick(Context& ctx, world::MobData& m, bool blockedAhead); // (after moving)
    void spawnWildlife(Context& ctx, world::Biome biome, world::BlockId ground, int x, int y, int z);
    // Bees (Bees.cpp, M26.3b): flying, pollen, crops, going home; hives letting them out.
    bool beeAi(Context& ctx, world::MobData& m);
    // Phantoms (Phantoms.cpp, M26.4a).
    bool phantomAi(Context& ctx, world::MobData& m);
    // The Wither (Wither.cpp, M26.4b).
    bool witherAi(Context& ctx, world::MobData& m);
    // Allays (Allays.cpp, M26.5a).
    bool allayAi(Context& ctx, world::MobData& m);
    static Use allayInteract(world::MobData& m, world::ItemId held, world::Xoroshiro& rng, ItemEntities& items);
    // Happy ghasts and copper golems (HappyGhasts.cpp, CopperGolems.cpp, M26.5b).
    bool happyGhastAi(Context& ctx, world::MobData& m);
    static Use happyGhastInteract(world::MobData& m, world::ItemId held, world::Xoroshiro& rng, ItemEntities& items);
    bool copperGolemGoal(Context& ctx, world::MobData& m, double& speed);
    static Use copperGolemInteract(world::MobData& m, world::ItemId held, world::Xoroshiro& rng, ItemEntities& items);
    // The creaking (Creakings.cpp, M27.1c): frozen while watched, bound to its heart.
    bool creakingTick(Context& ctx, world::MobData& m);
    // The warden (Wardens.cpp, M27.3c): emerging/digging, hearing, anger, darkness, the
    // sonic boom; investigating what it heard.
    bool wardenTick(Context& ctx, world::MobData& m);
    bool wardenGoal(Context& ctx, world::MobData& m, double& speed);
    // Sniffers (Sniffers.cpp, M27.5c): digging up seeds now and then.
    bool snifferTick(Context& ctx, world::MobData& m);
    // Hanging entities (Hanging.cpp, M28.3a): dropping off when their wall goes.
    void hangingTick(Context& ctx, world::MobData& m);
    // Leads (Leads.cpp, M28.3c): pulling and snapping, knots, llama caravans.
    void leashTick(Context& ctx, world::MobData& m);
    void caravanTick(Context& ctx, world::MobData& m);
    void knotTick(Context& ctx, world::MobData& k);
    // Armor stands (ArmorStands.cpp, M28.3b).
    void armorStandTick(Context& ctx, world::MobData& m);
    void dropArmorStand(Context& ctx, world::MobData& m);
    void dropHanging(Context& ctx, world::MobData& m);
    // Trial spawners (TrialChambers.cpp, M27.4d).
    void tickTrialSpawner(Context& ctx, world::Chunk& chunk, const world::BlockPos& p, world::SpawnerData& s);

public:
    // An awake creaking heart calls its creaking within 16 blocks, if a player is within
    // 32 and none of its own is out (M27.1c; false: none came).
    static bool spawnCreaking(world::World& world, const world::BlockPos& heart, const glm::dvec3& player,
                              world::Xoroshiro& rng);
    // A shrieker's 4th warning calls a warden out of the ground near it (M27.3; false: none
    // came - one is already within 48 blocks, or no room).
    static bool summonWarden(world::World& world, const world::BlockPos& shrieker, world::Xoroshiro& rng);
    // Whether a player at `eye` looking along `look` sees the creaking (M27.1c).
    static bool watched(const world::World& world, const world::MobData& m, const glm::dvec3& eye,
                        const glm::dvec3& look);
    // A carved pumpkin on a block of copper: a copper golem, and the copper becomes a
    // copper chest (false: not on copper).
    static bool buildCopperGolem(world::World& world, const world::BlockPos& pumpkin, world::Xoroshiro& rng);

private:

public:
    // Wither skeleton skulls on a T of soul sand / soil (the last skull just placed at
    // `skull`): the blocks vanish and a Wither appears (wiki: Wither › Summoning).
    static bool buildWither(world::World& world, const world::BlockPos& skull, world::Xoroshiro& rng);
    // Blocks a Wither or its skulls can't destroy (bedrock, end portal parts, barriers).
    static bool witherProof(world::BlockId b);

private:
    void spawnPhantoms(Context& ctx);
    int m_phantomTicks = 0;
    void tickHives(Context& ctx, world::Chunk& chunk);
    void spawnWater(Context& ctx);
    void dragonAi(Context& ctx, world::MobData& m); // EnderDragon.cpp
    void minecartTick(Context& ctx, world::MobData& m); // Minecarts.cpp
    void spawnNether(Context& ctx);
    // Villagers (M24, Villagers.cpp): the daily schedule (home, work, the bell; true if
    // it chose the goal) and running from zombies.
    bool villagerGoal(Context& ctx, world::MobData& m, double& speed);
    void villagerFear(Context& ctx, world::MobData& m);
    void villagerUpkeep(Context& ctx, world::MobData& v); // (M24.3) food, bread, sharing
    // Zombies hunting villagers, zombie villagers' cure (M24.3, Villagers.cpp).
    bool villageHunt(Context& ctx, world::MobData& z);
    static void zombieVillagerTick(world::MobData& m);
    // Iron golems (M24.3, Golems.cpp): fighting monsters / patrolling (true: chasing),
    // villagers calling one.
    bool golemGoal(Context& ctx, world::MobData& g, double& speed);
    void villagersCallGolem(Context& ctx, world::MobData& v);

public:
    // A carved pumpkin on a T of iron blocks: an iron golem (false: not a golem shape).
    static bool buildIronGolem(world::World& world, const world::BlockPos& pumpkin, world::Xoroshiro& rng);

private:
    void die(Context& ctx, world::MobData& m);
    // Animals (Animals.cpp): per-tick upkeep (growing, eggs, eating grass) and goals
    // (breeding partner, tempting food, parent); true if a goal was set.
    void animalUpkeep(Context& ctx, world::MobData& m);
    bool animalGoal(Context& ctx, world::MobData& m, double& speed);
    world::MobData* findMob(world::World& world, const world::MobData& self, double range, bool wantLove, bool wantAdult);
    // Monsters (Monsters.cpp): whether it may pick the player as a target (spiders only
    // in the dark, endermen only when angry), and its own per-tick behaviour after
    // moving (creeper fuse, skeleton bow, spider leap, enderman stare/teleport/blocks).
    bool mayTarget(Context& ctx, const world::MobData& m) const;
    void monsterTick(Context& ctx, world::MobData& m, bool chase, double playerDist2);
    bool teleport(world::World& world, world::MobData& m, const glm::dvec3& around, world::Xoroshiro& rng);

    struct Move {
        world::ChunkPos to, from;
        world::MobData mob;
    };
    std::vector<Move> m_moves; // reused: mobs crossing chunk borders this tick
    std::vector<world::MobData> m_births; // reused: babies born this tick
    std::vector<Aabb> m_boxes; // reused collision boxes
    Pathfinder m_pathfinder;
    Explosion m_explosion;
    std::vector<world::BlockPos> m_scratchEdits; // (explosions without an edit list)
    int m_hostiles = 0;
    int m_fish = 0, m_squid = 0, m_glowSquid = 0, m_axolotls = 0; // (M25.2: water mob caps; M26.3c axolotls)
    int m_felines = 0, m_felinesLastTick = 0; // (cats + ocelots: creepers skip their cat scan without any)
    int m_creatures = 0, m_cats = 0, m_creatureTicks = 0; // (M26.1: animal spawning)
    uint64_t m_playerAttacker = 0; // (M26.1) the mob that last hurt the player
    int m_playerAttackerTicks = 0; // (forgotten after 100 ticks, like vanilla's last-hurt-by memory)
    void setPlayerAttacker(uint64_t uuid) {
        m_playerAttacker = uuid;
        m_playerAttackerTicks = 0;
    }
    int m_striders = 0;
    float m_bossHealth = -1.0f;
    world::MobType m_bossType = world::MobType::EnderDragon;
    std::vector<glm::dvec3> m_dragonDeaths; // (counted in the tick's mob pass, for strider spawning)
    // Zombified piglins hit this tick (gathered in the mob pass; their herd joins in).
    std::array<glm::dvec3, 8> m_angerAlerts{};
    int m_angerAlertCount = 0;
    int m_simulationDistance = kDefaultSimulationDistance;
};

} // namespace mc
