#pragma once

#include "gameplay/Aabb.h"
#include "gameplay/Explosion.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Pathfinder.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Mob.h"
#include "world/Random.h"
#include "world/Weather.h"
#include "world/World.h"

#include <optional>
#include <array>
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
    enum class Use { None, Fed, Sheared };
    static Use interact(world::MobData& mob, world::ItemId held, world::Xoroshiro& rng, ItemEntities& items);
    static bool isFood(world::MobType type, world::ItemId item); // breeding / tempting food
    // The player hits a mob for `damage` (knockback away from the player).
    static void attack(world::MobData& mob, float damage, const glm::dvec3& from);

    int hostileCount() const { return m_hostiles; }
    static Aabb box(const world::MobData& m);

private:
    void ai(Context& ctx, world::MobData& m);
    void physics(const world::World& world, world::MobData& m, const glm::dvec3& wish, bool jump);
    void spawnHostiles(Context& ctx);
    void tickSpawners(Context& ctx, world::Chunk& chunk); // M18.3
    // Nether mobs (M19.2, NetherMobs.cpp): ghasts, blazes and magma cubes move and
    // attack on their own (true: handled); zombified piglins' anger runs down.
    bool netherAi(Context& ctx, world::MobData& m);
    void dragonAi(Context& ctx, world::MobData& m); // EnderDragon.cpp
    void minecartTick(Context& ctx, world::MobData& m); // Minecarts.cpp
    void spawnNether(Context& ctx);
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
        world::ChunkPos to;
        world::MobData mob;
    };
    std::vector<Move> m_moves; // reused: mobs crossing chunk borders this tick
    std::vector<world::MobData> m_births; // reused: babies born this tick
    std::vector<Aabb> m_boxes; // reused collision boxes
    Pathfinder m_pathfinder;
    Explosion m_explosion;
    std::vector<world::BlockPos> m_scratchEdits; // (explosions without an edit list)
    int m_hostiles = 0;
    int m_striders = 0;
    float m_bossHealth = -1.0f;
    std::vector<glm::dvec3> m_dragonDeaths; // (counted in the tick's mob pass, for strider spawning)
    // Zombified piglins hit this tick (gathered in the mob pass; their herd joins in).
    std::array<glm::dvec3, 8> m_angerAlerts{};
    int m_angerAlertCount = 0;
    int m_simulationDistance = kDefaultSimulationDistance;
};

} // namespace mc
