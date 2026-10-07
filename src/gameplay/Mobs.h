#pragma once

#include "gameplay/Aabb.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Mob.h"
#include "world/Random.h"
#include "world/World.h"

#include <optional>
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
        bool naturalSpawning = true; // Overworld zombies only (no Nether/End mobs yet)
    };

    // Chunks farther than this (Chebyshev, in chunks) from the player don't tick mobs
    // (vanilla's simulation distance option; its default is 12).
    static constexpr int kSimulationDistance = 12;

    Mobs() { m_moves.reserve(64); m_boxes.reserve(256); }

    void tick(Context& ctx);

    // A mob at `pos` (spawn eggs, commands, natural spawning).
    static world::MobData make(world::MobType type, const glm::dvec3& pos, world::Xoroshiro& rng);
    // Adds a mob to the chunk it stands in (false if that chunk isn't loaded).
    static bool add(world::World& world, const world::MobData& mob);

    // The mob the player's look ray hits first within `reach` blocks, if it's nearer
    // than `blockDistance` (attacks prefer the mob in front of a block).
    struct MobHit {
        world::ChunkPos chunk;
        int index;
        double distance;
    };
    static std::optional<MobHit> raycast(world::World& world, const glm::dvec3& eye, const glm::dvec3& dir,
                                         double reach);
    // The player hits a mob for `damage` (knockback away from the player).
    static void attack(world::MobData& mob, float damage, const glm::dvec3& from);

    int hostileCount() const { return m_hostiles; }
    static Aabb box(const world::MobData& m);

private:
    void ai(Context& ctx, world::MobData& m);
    void physics(const world::World& world, world::MobData& m, const glm::dvec3& wish, bool jump);
    void spawnHostiles(Context& ctx);
    void die(Context& ctx, world::MobData& m);

    struct Move {
        world::ChunkPos to;
        world::MobData mob;
    };
    std::vector<Move> m_moves; // reused: mobs crossing chunk borders this tick
    std::vector<Aabb> m_boxes; // reused collision boxes
    int m_hostiles = 0;
};

} // namespace mc
