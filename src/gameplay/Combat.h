#pragma once

#include <algorithm>
#include <cmath>

namespace mc {

// Melee damage (wiki: Damage, Critical hit, Sharpness, Smite, Bane of Arthropods,
// Strength, Weakness): the item's attack damage, +3 per Strength level, -4 per Weakness
// level (not below 0), x1.5 for a critical hit, then the enchantments' bonus - Sharpness
// 0.5 x level + 0.5, Smite / Bane 2.5 x level against their mobs.
struct MeleeHit {
    float itemDamage = 1.0f; // hand: 1
    int strength = 0, weakness = 0;
    bool critical = false;
    int sharpness = 0, smite = 0, bane = 0;
    bool undead = false, arthropod = false;
    int impaling = 0;     // (M25.3, tridents) +2.5 a level
    bool aquatic = false; // ... against water mobs (Java Edition)
};
inline float meleeDamage(const MeleeHit& h) {
    float d = h.itemDamage + 3.0f * float(h.strength);
    d -= 4.0f * float(h.weakness);
    if (d < 0.0f) d = 0.0f;
    if (h.critical) d *= 1.5f;
    if (h.sharpness > 0) d += 0.5f * float(h.sharpness) + 0.5f;
    if (h.undead) d += 2.5f * float(h.smite);
    if (h.arthropod) d += 2.5f * float(h.bane);
    if (h.aquatic) d += 2.5f * float(h.impaling);
    return d;
}

// The mace's smash attack (M28.4d; wiki: Mace): a hit while falling more than 1.5 blocks
// adds 4 damage per block for the first 3 blocks, 2 per block for the next 5, then 1 per
// block; Density adds 0.5 per block fallen a level.
inline float maceSmashBonus(double fallen, int density) {
    if (fallen <= 1.5) return 0.0f;
    const double b = std::min(fallen, 3.0) * 4.0 + std::clamp(fallen - 3.0, 0.0, 5.0) * 2.0 + std::max(0.0, fallen - 8.0);
    return float(b + 0.5 * density * fallen);
}
// Wind Burst (wiki: about 7, 8, 9 blocks up for levels I-III): the upward speed that
// throws a player that high against our gravity and drag (ours).
inline double windBurstLift(int level) {
    return level <= 0 ? 0.0 : std::sqrt(2.0 * 0.08 * double(6 + level) / 0.9);
}

// Spears (M28.4e; 1.21.11 - our assumptions): a jab reaches 4.5 blocks; held out while
// moving at least 0.25 blocks a tick (a sprint is about 0.28, a horse more), the charge hits
// what is in front for the jab damage x speed x 4; Lunge throws the player 0.5 blocks a tick
// forward a level when jabbing.
inline constexpr double kSpearReach = 4.5;
inline float spearChargeDamage(float jab, double speed) { return speed < 0.25 ? 0.0f : jab * float(speed) * 4.0f; }
inline double lungeImpulse(int level) { return 0.5 * double(level); }

} // namespace mc
