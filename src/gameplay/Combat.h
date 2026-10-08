#pragma once

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

} // namespace mc
