#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace mc::world {

enum class MobType : uint8_t {
    Zombie,
    Cow,
    Sheep,
    Pig,
    Chicken,
    Skeleton,
    Creeper,
    Spider,
    Enderman,
    // Nether (M19.2).
    Ghast,
    Blaze,
    MagmaCube,
    ZombifiedPiglin,
    Piglin,
    Hoglin,
    Strider,
    // The End (M20).
    EndCrystal, // not a mob in vanilla but an entity; it lives with the mobs here
    EnderDragon,
    Shulker,
    Minecart, // (M21.4: a vehicle, kept with the mobs)
    Slime,    // (M21.5)
    Count
};

// Static facts per mob type (wiki: Zombie, Cow - health, hitbox, speed, damage).
struct MobInfo {
    std::string_view id;  // "minecraft:zombie"
    float maxHealth;
    double width, height; // hitbox
    double speed;         // movement_speed attribute (blocks/tick scale)
    float attackDamage;   // normal difficulty; 0 = passive
    bool hostile;
    bool fireImmune = false; // fire and lava don't hurt it (Nether mobs)
    bool flies = false;      // no gravity: ghasts, blazes hover
    float modelScale = 1.0f; // drawn this much larger than its model (ghast 4.5)
};
const MobInfo& mobInfo(MobType t);

// Inside the world bounds vanilla accepts for entities (+-30,000,000 horizontally,
// +-20,000,000 vertically; wiki: World boundary) and finite.
inline bool isValidMobPosition(const glm::dvec3& p) {
    return std::abs(p.x) < 3.0e7 && std::abs(p.z) < 3.0e7 && std::abs(p.y) < 2.0e7; // false for NaN
}

// A mob's state: saved fields first, then AI / animation state (transient, but
// kept here so a mob is one plain value that moves between chunks).
struct MobData {
    MobType type = MobType::Cow;
    uint64_t uuidHi = 0, uuidLo = 0; // vanilla UUID (saved as 4 ints)
    glm::dvec3 pos{0.0}, prevPos{0.0}, vel{0.0};
    float yaw = 0.0f, prevYaw = 0.0f; // body
    float headYaw = 0.0f, pitch = 0.0f;
    float prevHeadYaw = 0.0f, prevPitch = 0.0f;
    float health = 10.0f;
    float fallDistance = 0.0f;
    int16_t hurtTime = 0;  // red tint ticks after damage
    int16_t deathTime = 0; // death animation ticks (removed at 20)
    int16_t fireTicks = 0;
    bool onGround = false;
    bool persistent = false; // never despawns (named, picked up items...)
    int noPlayerTicks = 0;   // despawn clock (wiki: Spawn › Despawning)
    // Animals (M16.3; wiki: Breeding, Sheep, Chicken). Saved as Age, InLove, Color,
    // Sheared, EggLayTime.
    int age = 0;            // < 0: a baby growing up (-24000 at birth); > 0: breeding cooldown
    int loveTicks = 0;      // in love mode after being fed (600)
    uint8_t woolColour = 0; // sheep: dye index (0 white .. 15 black)
    bool sheared = false;
    bool showBottom = true; // end crystals: drawn on a bedrock base (ShowBottom)
    int eggTicks = 6000;    // chicken: ticks until the next egg
    int eatTicks = 0;       // sheep: eating-grass animation (40)
    int16_t breedTicks = 0; // time spent next to a partner in love
    // Hostiles 2 (M16.5; wiki: Creeper, Skeleton, Spider, Enderman).
    int16_t fuse = 0;           // creeper: swelling ticks (explodes at 30)
    int16_t shootTicks = 0;     // skeleton: drawing the bow
    bool angry = false;         // enderman stared at / spider or enderman hit
    uint16_t carried = 0;       // enderman: the block state it holds (0 = none)
    bool wantsTeleport = false; // enderman: hit by an arrow / in water - teleport away
    bool climbing = false;      // spider: against a wall last tick
    bool isBaby() const { return age < 0; }
    // AI
    glm::dvec3 goal{0.0};    // wander / chase target
    int goalTicks = 0;       // time spent on the current goal
    int panicTicks = 0;
    bool targeting = false; // a hostile chasing the player (seen it)
    uint8_t sightCheck = 0; // ticks since the last line-of-sight check
    int attackCooldown = 0;
    // Path (M16.2): block cells (feet) to walk through, from the pathfinder; not saved.
    static constexpr int kMaxPath = 32;
    std::array<glm::ivec3, kMaxPath> path{};
    uint8_t pathLength = 0, pathIndex = 0;
    int16_t repathTicks = 0;
    glm::ivec3 pathRequest{0, -100000, 0}; // the goal cell the current path was asked for
    bool lastHurtByPlayer = false;          // (spider eyes drop only for player kills)
    uint8_t looting = 0;                    // Looting level of the player's last hit
    int16_t stareTicks = 0;                 // enderman: ticks the player has looked at it
    int16_t angerTicks = 0;                 // enderman: anger left (calms down at 0)
    // Nether mobs (M19.2).
    uint8_t size = 1;       // magma cube: 1, 2 or 4 (saved as Size 0, 1, 3)
    int16_t chargeTicks = 0; // ghast/blaze: charging a shot; blaze: volley timing
    uint8_t volley = 0;      // blaze: fireballs left in this volley
    int16_t jumpTicks = 0;   // magma cube: ticks to its next jump
    bool angerAlert = false; // zombified piglin: just hit - the ones around join in
    int16_t admireTicks = 0; // piglin: inspecting a gold ingot (barters at the end)
    // The ender dragon (M20.2): its phase uses vanilla's DragonPhase numbers (saved).
    uint8_t phase = 0;
    int16_t phaseTicks = 0;
    uint8_t node = 0;         // holding pattern: the ring node it flies to
    int8_t nodeStep = 1;      // around the ring clockwise or not
    float lastHealth = 0.0f;  // health at the last tick (damage taken while perched)
    float perchDamage = 0.0f; // damage taken since it landed (takes off at 50)
    bool hasBeam = false;     // an end crystal heals it: the beam starts at `beam`
    uint8_t peek = 0;         // shulker: how far its lid is open, 0..100 (saved as Peek)
    bool ridden = false;      // minecart: the player sits in it (not saved: vanilla saves passengers)
    glm::dvec3 beam{0.0};
    float limbSwing = 0.0f, limbSwingAmount = 0.0f; // walk animation
};

} // namespace mc::world
