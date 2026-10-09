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
    Levitation, // (M20.4: shulker bullets)
    Haste,        // (M23.6: beacons) mining +20% per level
    Resistance,   // (M23.6: beacons) damage -20% per level
    ConduitPower, // (M23.6: conduits) breath and mining under water
    BadOmen,          // (M24.5) from an ominous bottle: a raid when entering a village
    RaidOmen,         // (M24.5) 30 s, then the raid starts
    HeroOfTheVillage, // (M24.5) won a raid: villagers trade cheaper
    Hunger,           // (M25.2: pufferfish) 0.005 exhaustion a tick per level
    DolphinsGrace,    // (M25.3b) swimming near a dolphin: much less water drag
    MiningFatigue,    // (M25.5: elder guardians) mining x 0.3 per level
    Wither,           // (M26.4a: wither skeletons) like Poison, but it can kill
    BreathOfTheNautilus, // (M26.5a: riding a nautilus) the air bar doesn't drop under water
    Darkness,            // (M27.3: shriekers, wardens) the world pulses dark around you
    Glowing,             // (M28.4b: spectral arrows) outlined through walls (ours: no outline yet)
    TrialOmen,           // (M28.4d) Bad Omen turned near a trial spawner: nearby trials are ominous
    // M29.2a (wiki pages of each)
    Nausea,       // the view wobbles
    Blindness,    // fog closes in to 5 blocks; no sprinting
    HealthBoost,  // +4 max health a level
    Absorption,   // 4 extra (golden) health a level, taken first
    Saturation,   // food +1 and saturation +2 a tick a level
    Luck,         // better fishing (and loot) by a level
    Unluck,       // worse
    WindCharged,  // bursts like a wind charge on death
    Weaving,      // spreads cobwebs on death; walks through them
    Oozing,       // two slimes on death
    Infested,     // when hurt, 1 in 10: silverfish come out
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
    // M29.2a (wiki: Potion): Turtle Master gives two effects; Luck can't be brewed.
    TurtleMaster,
    LongTurtleMaster,
    StrongTurtleMaster,
    Luck,
    WindCharging,
    Weaving,
    Oozing,
    Infestation,
    Count
};
struct PotionInfo {
    std::string_view id;  // "long_swiftness"
    Effect effect;        // None: no effect (water, awkward...)
    uint8_t amplifier;    // 0 = level I
    int duration;         // ticks (instant effects: 1)
    Effect effect2 = Effect::None; // (M29.2a) a second effect (Turtle Master's Resistance)
    uint8_t amplifier2 = 0;
};
const PotionInfo& potionInfo(Potion p);
std::optional<Potion> findPotion(std::string_view id); // with or without "minecraft:"
uint32_t potionColour(Potion p); // liquid colour (water blue for no effect)

} // namespace mc::world
