#pragma once

#include "world/Items.h"

#include <algorithm>
#include <cmath>

namespace mc {

// Melee damage (wiki: Damage, Critical hit, Sharpness, Smite, Bane of Arthropods,
// Strength, Weakness): the item's attack damage, +3 per Strength level, -4 per Weakness
// level (not below 0), x1.5 for a critical hit, then the enchantments' bonus - Sharpness
// 0.5 x level + 0.5, Smite / Bane 2.5 x level against their mobs.
struct MeleeHit {
    float itemDamage = 1.0f; // hand: 1
    // (M30.2) how charged the attack is, 0..1 (see attackCharge): the base damage is scaled
    // by 0.2 + 0.8 x charge^2, enchantment damage by the charge (wiki: Damage).
    float charge = 1.0f;
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
    const float c = std::clamp(h.charge, 0.0f, 1.0f);
    d *= 0.2f + 0.8f * c * c;
    if (h.critical) d *= 1.5f;
    float e = 0.0f;
    if (h.sharpness > 0) e += 0.5f * float(h.sharpness) + 0.5f;
    if (h.undead) e += 2.5f * float(h.smite);
    if (h.arthropod) e += 2.5f * float(h.bane);
    if (h.aquatic) e += 2.5f * float(h.impaling);
    return d + e * c;
}

// Attack cooldown (M30.2; wiki: Attack cooldown, each weapon's page): attacks per second
// by item - a hand or any non-weapon 4, swords 1.6, axes 0.8 (wood, stone, copper) / 0.9
// (iron) / 1.0, pickaxes 1.2, shovels 1.0, hoes 1 (wood, gold) / 2 (stone, copper) / 3
// (iron) / 4, the trident 1.1, the mace 0.6; spears (1.21.11) by tier, our reading of the
// wiki (wood 1.54, stone and copper 1.33, iron 1.18, gold and diamond 1.05, netherite 0.95).
inline float attackSpeed(const world::ItemDef& d) {
    using T = world::ToolTier;
    switch (d.tool) {
    case world::ToolType::Sword: return 1.6f;
    case world::ToolType::Axe:
        return d.tier == T::Wood || d.tier == T::Stone || d.tier == T::Copper ? 0.8f : d.tier == T::Iron ? 0.9f : 1.0f;
    case world::ToolType::Pickaxe: return 1.2f;
    case world::ToolType::Shovel: return 1.0f;
    case world::ToolType::Hoe:
        return d.tier == T::Wood || d.tier == T::Gold                         ? 1.0f
               : d.tier == T::Stone || d.tier == T::Copper                    ? 2.0f
               : d.tier == T::Iron                                            ? 3.0f
                                                                              : 4.0f;
    case world::ToolType::Spear: // (wiki: Spear, Melee attack table)
        return d.tier == T::Wood                         ? 1.54f
               : d.tier == T::Stone                      ? 1.33f
               : d.tier == T::Copper                     ? 1.18f
               : d.tier == T::Iron || d.tier == T::Gold  ? 1.05f
               : d.tier == T::Diamond                    ? 0.95f
                                                         : 0.87f; // netherite
    default: break;
    }
    if (d.id == "minecraft:trident") return 1.1f;
    if (d.id == "minecraft:mace") return 0.6f;
    return 4.0f;
}
inline float attackSpeedWith(const world::ItemDef& d, int haste, int fatigue) {
    return attackSpeed(d) * std::max(0.1f, (1.0f + 0.1f * float(haste)) * (1.0f - 0.1f * float(fatigue)));
}
// The charge after `ticksSince` ticks since the last swing or item switch (vanilla
// getAttackStrengthScale(0.5)): (ticks + 0.5) / (20 / speed), at most 1.
inline float attackCharge(int ticksSince, float speed) {
    return std::clamp((float(ticksSince) + 0.5f) * speed / 20.0f, 0.0f, 1.0f);
}
// A charged attack (above 0.9) is needed for critical hits, sweeps and sprint knockback.
inline constexpr float kFullCharge = 0.9f;

// The mace's smash attack (M28.4d; wiki: Mace): a hit while falling more than 1.5 blocks
// adds 4 damage per block for the first 3 blocks, 2 per block for the next 5, then 1 per
// block; Density adds 0.5 per block fallen a level.
inline float maceSmashBonus(double fallen, int density) {
    if (fallen <= 1.5) return 0.0f;
    const double b = std::min(fallen, 3.0) * 4.0 + std::clamp(fallen - 3.0, 0.0, 5.0) * 2.0 +
                     std::max(0.0, fallen - 8.0);
    return float(b + 0.5 * density * fallen);
}
// Wind Burst (wiki: about 7, 8, 9 blocks up for levels I-III): the upward speed that
// throws a player that high against our gravity and drag (ours).
inline double windBurstLift(int level) {
    return level <= 0 ? 0.0 : std::sqrt(2.0 * 0.08 * double(6 + level) / 0.9);
}

// Spears (M28.4e; 1.21.11, wiki: Spear, Lunge): a jab reaches 2 to 4.5 blocks; held out
// while moving at least 4.6 blocks a second (0.23 a tick: a sprint is about 0.28, a horse
// more), the charge hits what is in front; the wiki's damage multiplier by tier (wood/gold
// 0.7, stone/copper 0.82, iron 0.95, diamond 1.075, netherite 1.2) times the speed in
// blocks a second is our formula. Lunge throws the player 0.458 blocks a tick forward a level.
inline constexpr double kSpearReach = 4.5, kSpearMinReach = 2.0;
inline float spearChargeMultiplier(world::ToolTier t) {
    using T = world::ToolTier;
    switch (t) {
    case T::Stone:
    case T::Copper:
        return 0.82f;
    case T::Iron:
        return 0.95f;
    case T::Diamond:
        return 1.075f;
    case T::Netherite:
        return 1.2f;
    default:
        return 0.7f; // wood, gold
    }
}
inline float spearChargeDamage(world::ToolTier t, double speed) {
    return speed < 0.23 ? 0.0f : spearChargeMultiplier(t) * float(speed * 20.0);
}
inline double lungeImpulse(int level) { return 0.458 * double(level); }

} // namespace mc
