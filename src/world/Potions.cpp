#include "world/Potions.h"

namespace mc::world {

namespace {

// wiki: Effect (ids and colours).
constexpr EffectInfo kEffects[] = {
    {"", false, 0x385DC6},
    {"minecraft:speed", false, 0x33EBFF},
    {"minecraft:slowness", false, 0x8BAFE0},
    {"minecraft:strength", false, 0xFFC700},
    {"minecraft:weakness", false, 0x484D48},
    {"minecraft:fire_resistance", false, 0xFF9900},
    {"minecraft:regeneration", false, 0xCD5CAB},
    {"minecraft:poison", false, 0x87A363},
    {"minecraft:instant_health", true, 0xF82423},
    {"minecraft:instant_damage", true, 0xA9656A},
    {"minecraft:night_vision", false, 0xC2FF66},
    {"minecraft:invisibility", false, 0xF6F6F6},
    {"minecraft:water_breathing", false, 0x98DAC0},
    {"minecraft:jump_boost", false, 0xFDFF84},
    {"minecraft:slow_falling", false, 0xF3CFB9},
    {"minecraft:levitation", false, 0xCEFFFF},
    {"minecraft:haste", false, 0xD9C043},
    {"minecraft:resistance", false, 0x9146F0},
    {"minecraft:conduit_power", false, 0x1DC2D1},
    {"minecraft:bad_omen", false, 0x0B6138},
    {"minecraft:raid_omen", false, 0xDE4058},
    {"minecraft:hero_of_the_village", false, 0x44FF44},
    {"minecraft:hunger", false, 0x587653},
    {"minecraft:dolphins_grace", false, 0x88A3BE},
    {"minecraft:mining_fatigue", false, 0x4A4217},
    {"minecraft:wither", false, 0x736156}, // (M26.4a)
    {"minecraft:breath_of_the_nautilus", false, 0x00AACC}, // (M26.5a)
    {"minecraft:darkness", false, 0x292721},                // (M27.3)
    {"minecraft:glowing", false, 0x94A061},                 // (M28.4b)
    {"minecraft:trial_omen", false, 0x16A6A6},              // (M28.4d)
    {"minecraft:nausea", false, 0x551D4A},                  // (M29.2a)
    {"minecraft:blindness", false, 0x1F1F23},
    {"minecraft:health_boost", false, 0xF87D23},
    {"minecraft:absorption", false, 0x2552A5},
    {"minecraft:saturation", false, 0xF82423},
    {"minecraft:luck", false, 0x59C106},
    {"minecraft:unluck", false, 0xC0A44D},
    {"minecraft:wind_charged", false, 0xBDC9FF},
    {"minecraft:weaving", false, 0x78695A},
    {"minecraft:oozing", false, 0x99FFA3},
    {"minecraft:infested", false, 0x8C9B8C},
};
static_assert(std::size(kEffects) == size_t(Effect::Count));

using E = Effect;
// wiki: Potion › Java Edition durations (3:00 = 3600 ticks, 8:00 = 9600...).
constexpr PotionInfo kPotions[] = {
    {"", E::None, 0, 0},
    {"water", E::None, 0, 0},
    {"awkward", E::None, 0, 0},
    {"mundane", E::None, 0, 0},
    {"thick", E::None, 0, 0},
    {"night_vision", E::NightVision, 0, 3600},
    {"long_night_vision", E::NightVision, 0, 9600},
    {"invisibility", E::Invisibility, 0, 3600},
    {"long_invisibility", E::Invisibility, 0, 9600},
    {"leaping", E::JumpBoost, 0, 3600},
    {"long_leaping", E::JumpBoost, 0, 9600},
    {"strong_leaping", E::JumpBoost, 1, 1800},
    {"fire_resistance", E::FireResistance, 0, 3600},
    {"long_fire_resistance", E::FireResistance, 0, 9600},
    {"swiftness", E::Speed, 0, 3600},
    {"long_swiftness", E::Speed, 0, 9600},
    {"strong_swiftness", E::Speed, 1, 1800},
    {"slowness", E::Slowness, 0, 1800},
    {"long_slowness", E::Slowness, 0, 4800},
    {"strong_slowness", E::Slowness, 3, 400},
    {"water_breathing", E::WaterBreathing, 0, 3600},
    {"long_water_breathing", E::WaterBreathing, 0, 9600},
    {"healing", E::InstantHealth, 0, 1},
    {"strong_healing", E::InstantHealth, 1, 1},
    {"harming", E::InstantDamage, 0, 1},
    {"strong_harming", E::InstantDamage, 1, 1},
    {"poison", E::Poison, 0, 900},
    {"long_poison", E::Poison, 0, 1800},
    {"strong_poison", E::Poison, 1, 432},
    {"regeneration", E::Regeneration, 0, 900},
    {"long_regeneration", E::Regeneration, 0, 1800},
    {"strong_regeneration", E::Regeneration, 1, 450},
    {"strength", E::Strength, 0, 3600},
    {"long_strength", E::Strength, 0, 9600},
    {"strong_strength", E::Strength, 1, 1800},
    {"weakness", E::Weakness, 0, 1800},
    {"long_weakness", E::Weakness, 0, 4800},
    {"slow_falling", E::SlowFalling, 0, 1800},
    {"long_slow_falling", E::SlowFalling, 0, 4800},
    // (M29.2a; wiki: Potion) Turtle Master: Slowness IV + Resistance III 20 s (long 40 s;
    // strong: Slowness VI + Resistance IV 20 s); Luck 5 min; the 1.21 ones 3 min.
    {"turtle_master", E::Slowness, 3, 400, E::Resistance, 2},
    {"long_turtle_master", E::Slowness, 3, 800, E::Resistance, 2},
    {"strong_turtle_master", E::Slowness, 5, 400, E::Resistance, 3},
    {"luck", E::Luck, 0, 6000},
    {"wind_charged", E::WindCharged, 0, 3600},
    {"weaving", E::Weaving, 0, 3600},
    {"oozing", E::Oozing, 0, 3600},
    {"infested", E::Infested, 0, 3600},
};
static_assert(std::size(kPotions) == size_t(Potion::Count));

} // namespace

const EffectInfo& effectInfo(Effect e) { return kEffects[size_t(e)]; }

std::optional<Effect> findEffect(std::string_view id) {
    if (!id.starts_with("minecraft:")) return std::nullopt;
    for (size_t i = 1; i < std::size(kEffects); ++i)
        if (kEffects[i].id == id) return static_cast<Effect>(i);
    return std::nullopt;
}

const PotionInfo& potionInfo(Potion p) { return kPotions[size_t(p)]; }

std::optional<Potion> findPotion(std::string_view id) {
    if (id.starts_with("minecraft:")) id.remove_prefix(10);
    for (size_t i = 1; i < std::size(kPotions); ++i)
        if (kPotions[i].id == id) return static_cast<Potion>(i);
    return std::nullopt;
}

uint32_t potionColour(Potion p) { return effectInfo(potionInfo(p).effect).colour; }

} // namespace mc::world
