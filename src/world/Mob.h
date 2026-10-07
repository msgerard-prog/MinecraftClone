#pragma once

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <string_view>

namespace mc::world {

enum class MobType : uint8_t { Zombie, Cow, Count };

// Static facts per mob type (wiki: Zombie, Cow - health, hitbox, speed, damage).
struct MobInfo {
    std::string_view id;  // "minecraft:zombie"
    float maxHealth;
    double width, height; // hitbox
    double speed;         // movement_speed attribute (blocks/tick scale)
    float attackDamage;   // normal difficulty; 0 = passive
    bool hostile;
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
    // AI
    glm::dvec3 goal{0.0};    // wander / chase target
    int goalTicks = 0;       // time spent on the current goal
    int panicTicks = 0;
    bool targeting = false; // a hostile chasing the player (seen it)
    uint8_t sightCheck = 0; // ticks since the last line-of-sight check
    int attackCooldown = 0;
    float limbSwing = 0.0f, limbSwingAmount = 0.0f; // walk animation
};

} // namespace mc::world
