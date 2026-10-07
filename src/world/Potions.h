#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mc::world {

// Status effects (M19.4; wiki: Effect) - the ones potions give so far.
enum class Effect : uint8_t {
    None,
    Speed,
    Slowness,
    Strength,
    Weakness,
    FireResistance,
    Regeneration,
    Poison,
    InstantHealth,
    InstantDamage,
    NightVision,
    Invisibility,
    WaterBreathing,
    JumpBoost,
    SlowFalling,
    Count
};
struct EffectInfo {
    std::string_view id; // "minecraft:speed"
    bool instant;        // healing/harming: applied at once
    uint32_t colour;     // 0xRRGGBB (potion liquid colour, wiki: Effect colours)
};
const EffectInfo& effectInfo(Effect e);
std::optional<Effect> findEffect(std::string_view id);

// Potion types (wiki: Potion › Data values): base potions, then each effect's normal,
// long (redstone) and strong (glowstone) kinds. Stored on potion items as vanilla's
// minecraft:potion_contents { potion: "minecraft:<id>" }.
enum class Potion : uint8_t {
    None, // (not a potion item)
    Water,
    Awkward,
    Mundane,
    Thick,
    NightVision,
    LongNightVision,
    Invisibility,
    LongInvisibility,
    Leaping,
    LongLeaping,
    StrongLeaping,
    FireResistance,
    LongFireResistance,
    Swiftness,
    LongSwiftness,
    StrongSwiftness,
    Slowness,
    LongSlowness,
    StrongSlowness,
    WaterBreathing,
    LongWaterBreathing,
    Healing,
    StrongHealing,
    Harming,
    StrongHarming,
    Poison,
    LongPoison,
    StrongPoison,
    Regeneration,
    LongRegeneration,
    StrongRegeneration,
    Strength,
    LongStrength,
    StrongStrength,
    Weakness,
    LongWeakness,
    SlowFalling,
    LongSlowFalling,
    Count
};
struct PotionInfo {
    std::string_view id;  // "long_swiftness"
    Effect effect;        // None: no effect (water, awkward...)
    uint8_t amplifier;    // 0 = level I
    int duration;         // ticks (instant effects: 1)
};
const PotionInfo& potionInfo(Potion p);
std::optional<Potion> findPotion(std::string_view id); // with or without "minecraft:"
uint32_t potionColour(Potion p); // liquid colour (water blue for no effect)

} // namespace mc::world
