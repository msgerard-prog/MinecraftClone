#pragma once

#include "world/Potions.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <utility>

#include <cstdint>

namespace mc {

// Health and hunger (wiki: Health, Hunger, Damage › Fall damage), survival only.
// Values in half-hearts / half-drumsticks like vanilla (max 20). Normal difficulty.
class Vitals {
public:
    static constexpr float kMaxHealth = 20.0f;
    static constexpr int kMaxFood = 20;

    float health() const { return m_health; }
    // (M29.2a) Health Boost: +4 a level; Absorption's golden health (taken first by damage).
    float maxHealth() const { return kMaxHealth + 4.0f * float(effectLevel(world::Effect::HealthBoost)); }
    float absorption() const { return m_absorption; }
    void setAbsorption(float a) { m_absorption = std::max(0.0f, a); } // (loading: AbsorptionAmount)
    int takeInfestedHits() { return std::exchange(m_infestedHits, 0); }
    // The fall damage factor of the block being landed on (hay bale 0.2), set each tick.
    void setLandingFactor(float f) { m_landingFactor = f; }
    // Difficulty (M28.1b; wiki: Difficulty): 0 peaceful, 1 easy, 2 normal, 3 hard. Hits
    // caused by mobs (and every explosion) scale with it; starving stops at 10 health on
    // Easy, at 1 on Normal, never on Hard; Peaceful refills health and food.
    void setDifficulty(int d) { m_difficulty = d; }
    // Damage taken since the last call (statistics, M28.1d).
    float takeDamageTaken() { return std::exchange(m_damageTaken, 0.0f); }
    int difficulty() const { return m_difficulty; }
    static float scaledDamage(float amount, int difficulty);
    // Game rules (M28.1): fall_damage, fire_damage, drowning_damage, natural_health_regeneration.
    void setRules(bool fall, bool fire, bool drowning, bool regen) {
        m_fallDamage = fall;
        m_fireDamage = fire;
        m_drowningDamage = drowning;
        m_naturalRegen = regen;
    }
    // Landing on a stalagmite's point (M27.2b; wiki: Pointed Dripstone): ceil(2 x fall - 2).
    void setStalagmite(bool on) { m_stalagmite = on; }
    void setVoidY(double y) { m_voidY = y; } // per dimension
    int food() const { return m_food; }
    float saturation() const { return m_saturation; }
    float exhaustion() const { return m_exhaustion; }
    bool dead() const { return m_health <= 0.0f; }
    // Sprinting needs more than 6 food points (wiki: Hunger).
    bool canSprint() const { return m_food > 6; }

    // Per tick, in survival. `feetY` and `onGround` track falls; `inWater` and
    // `flying` reset the fall distance. Returns damage taken this tick (0 if none).
    float tick(double feetY, bool onGround, bool inWater, bool flying);

    // Exhaustion from actions (wiki: Hunger › Exhaustion level increase).
    void exhaust(float amount) { m_exhaustion += amount; }
    // Hurts unless invulnerable (10 ticks after a hit). Returns true if it applied.
    // `exhausts`: false for falls and the void (wiki: they cause no exhaustion).
    bool damage(float amount, bool exhausts = true);
    // An attack (mobs, arrows, explosions, lava, fire blocks): a raised shield blocks
    // it from the front (no damage); otherwise armor reduces it (wiki: Armor):
    //   damage x (1 - min(20, max(armor / 5, armor - 4 x damage / (toughness + 8))) / 25)
    // and each worn piece wears floor(damage / 4), at least 1. `from`: where it came
    // from (null: no direction, the shield can't help). Returns true if it hurt.
    enum class Hit : uint8_t { Generic, Fire, Explosion, Projectile };
    bool attacked(float amount, const glm::dvec3* from = nullptr, Hit kind = Hit::Generic);
    // Protection enchantments worn (levels summed over the pieces): each hit adds up
    // "enchantment protection" - Protection 1 per level, Fire/Blast/Projectile
    // Protection 2 for their kind, Feather Falling 3 for falls - capped at 20, and
    // the damage left after armor is cut by EPF/25 (wiki: Armor › Enchantments).
    void setProtection(int all, int fire, int blast, int projectile, int feather) {
        m_protection = {all, fire, blast, projectile, feather};
    }
    float protectionReduced(float amount, Hit kind, bool fall) const;
    static float armorReduced(float amount, int armor, float toughness);
    // Set each tick from the inventory and the player.
    void setArmor(int points, float toughness) {
        m_armorPoints = points;
        m_armorToughness = toughness;
    }
    // A shield held up for 5+ ticks blocks attacks from in front of `eye` (horizontal
    // `facing`).
    void setShield(bool raised, const glm::dvec3& eye, const glm::dvec3& facing) {
        m_shieldRaised = raised;
        m_eye = eye;
        m_facing = facing;
    }
    // Durability owed by the worn armor / the shield since the last call.
    int takeArmorWear() { return std::exchange(m_armorWear, 0); }
    int takeShieldWear() { return std::exchange(m_shieldWear, 0); }
    int foodTimer() const { return m_foodTimer; }
    void setFoodTimer(int t) { m_foodTimer = t; }
    // Eats `food` points with `saturation` (wiki: Food), capped like vanilla.
    void eat(int food, float saturation);
    void reset(bool keepExperience = false); // respawn: full health and food, fresh saturation
                                             // (keep_inventory: the levels stay)
    void kill() { m_health = 0.0f; }         // /kill (ignores invulnerability)
    void setHealth(float h) {
        m_health = h < 0.0f ? 0.0f : h > maxHealth() ? maxHealth() : h;
    } // (totems, M24.5)
    // Forget the fall in progress (teleports, game-mode changes, respawn): the next
    // tick measures from where the player is now.
    void resetFall() {
        m_falling = false;
        m_started = false;
    }

    // Drowning (wiki: Drowning): 300 ticks of air; with the eyes under water it drops
    // 1 a tick, and from -20 on (one empty second) 2 damage, then back to 0. Out of
    // water it refills 4 a tick (public write-ups; the wiki's "1 bubble every 0.2 s"
    // would be 7.5 a tick - in-game check). Returns damage taken.
    static constexpr int kMaxAir = 300;
    // `keepBreath`: this tick's air loss is skipped (Respiration: chance level/(level+1)).
    float breathe(bool eyesInWater, bool keepBreath = false);
    int air() const { return m_air; }
    void setAir(int a) { m_air = a; }
    // Burning (wiki: Fire, Lava): lava sets the player on fire for 15 s; burning hurts
    // 1 every second; water puts it out. Returns damage taken.
    void setOnFire(int ticks) { m_fire = ticks > m_fire ? ticks : m_fire; }
    // Standing in a fire block (wiki: Fire › Burning): 1 damage a tick (the hurt cooldown
    // makes it one every half second); after a second in it the player catches fire for
    // 8 s (the player's Fire tag starts at -20). Call each tick, `inFire` or not.
    float touchFire(bool inFire);
    float tickFire(bool inWater);
    bool burning() const { return m_fire > 0; }
    int fireTicks() const { return m_fire; }
    void setFireTicks(int ticks) { m_fire = ticks; }

    // Experience (M17.5; wiki: Experience): points fill the bar to the next level -
    // 2L + 7 points below level 16, 5L - 38 up to 30, 9L - 158 above.
    static int pointsForLevel(int level) {
        return level < 16 ? 2 * level + 7 : level < 31 ? 5 * level - 38 : 9 * level - 158;
    }
    void addExperience(int points);
    // Ticks since the player last slept (M26.4a; vanilla's time_since_rest statistic):
    // phantoms come for players awake 3 days or more. Counted by main while alive; sleeping
    // and dying reset it.
    int timeSinceRest() const { return m_timeSinceRest; }
    // The warden's warnings (M27.3; wiki: Sculk Shrieker): a shrieker that can summon
    // raises the level by 1 (at most once in 10 s, up to 4) and the 4th calls a warden;
    // the level drops by 1 after 10 minutes without a warning. True: a warden comes.
    // (-1: too soon - the shrieker does nothing; 0: warned; 1: the warden comes)
    int wardenWarn() {
        if (m_wardenCooldown > 0) return -1;
        m_wardenCooldown = 200;
        m_wardenTicks = 0;
        m_wardenLevel = std::min(4, m_wardenLevel + 1);
        return m_wardenLevel >= 4 ? 1 : 0;
    }
    void tickWardenTracker() {
        if (m_wardenCooldown > 0) --m_wardenCooldown;
        if (m_wardenLevel > 0 && ++m_wardenTicks >= 12000) {
            --m_wardenLevel;
            m_wardenTicks = 0;
        }
    }
    int wardenLevel() const { return m_wardenLevel; }
    int wardenTicks() const { return m_wardenTicks; }
    int wardenCooldown() const { return m_wardenCooldown; }
    void setWardenTracker(int level, int ticks, int cooldown) {
        m_wardenLevel = std::clamp(level, 0, 4);
        m_wardenTicks = std::max(0, ticks);
        m_wardenCooldown = std::max(0, cooldown);
    }
    void setTimeSinceRest(int t) { m_timeSinceRest = std::max(0, t); }
    void addRestTime() {
        if (m_timeSinceRest < 0x7fffffff) ++m_timeSinceRest;
    }
    // Spends whole levels (enchanting, anvils); false if there aren't enough.
    bool spendLevels(int levels);
    int xpLevel() const { return m_xpLevel; }
    float xpProgress() const { return m_xpProgress; } // 0..1 of the bar
    int xpTotal() const { return m_xpTotal; }
    void setExperience(int level, float progress, int total) {
        m_xpLevel = level;
        m_xpProgress = progress;
        m_xpTotal = total;
    }
    // The enchantment seed (vanilla XpSeed): fixes the table's offers until used.
    uint64_t enchantSeed() const { return m_enchantSeed; }
    void setEnchantSeed(uint64_t s) { m_enchantSeed = s; }
    // Dropped on death (wiki): 7 x level, at most 100 points; then all is lost.
    int deathExperience() const { return std::min(m_xpLevel * 7, 100); }

    // Status effects (M19.4; wiki: Effect). A new effect replaces one of the same kind
    // when it is stronger, or as strong and longer. Instant health heals 4 x 2^level,
    // instant damage hurts 6 x 2^level (armor doesn't help); regeneration heals 1
    // every 50 >> level ticks, poison hurts 1 every 25 >> level ticks but never below
    // 1 health; fire resistance stops fire and lava damage; water breathing keeps the
    // air; slow falling stops fall damage. Speed, jump boost and the rest are read by
    // the player and the renderer (effectLevel).
    struct ActiveEffect {
        world::Effect type = world::Effect::None;
        uint8_t amplifier = 0;
        int duration = 0; // ticks left
    };
    static constexpr int kMaxEffects = 16;
    // `scale`: splash potions' distance factor (instant effects' amount).
    void addEffect(world::Effect type, int amplifier, int duration, double scale = 1.0);
    // 0 = not active, else level (amplifier + 1).
    int effectLevel(world::Effect type) const {
        for (const ActiveEffect& e : m_effects)
            if (e.type == type && e.duration > 0) return e.amplifier + 1;
        return 0;
    }
    int effectTicks(world::Effect type) const { // ticks left (0: not active)
        for (const ActiveEffect& e : m_effects)
            if (e.type == type && e.duration > 0) return e.duration;
        return 0;
    }
    const std::array<ActiveEffect, kMaxEffects>& effects() const { return m_effects; }
    void clearEffects() { m_effects = {}; } // death, milk
    void removeEffect(world::Effect type) { // (Bad Omen turning into Raid Omen)
        for (ActiveEffect& e : m_effects)
            if (e.type == type) e = {};
    }
    void tickEffects();

    // Saved state.
    void setState(float health, int food, float saturation, float exhaustion);

private:
    float m_landingFactor = 1.0f;
    bool m_stalagmite = false;
    bool m_fallDamage = true, m_fireDamage = true, m_drowningDamage = true,
         m_naturalRegen = true; // (M28.1)
    int m_difficulty = 2;
    float m_damageTaken = 0.0f;
    int m_peacefulTicks = 0;
    int m_wardenLevel = 0, m_wardenTicks = 0, m_wardenCooldown = 0; // (M27.3)
    float m_health = kMaxHealth;
    float m_absorption = 0.0f; // (M29.2a)
    int m_infestedHits = 0;
    int m_food = kMaxFood;
    float m_saturation = 5.0f;
    float m_exhaustion = 0.0f;
    int m_foodTimer = 0;    // regeneration / starvation clock
    int m_invulnerable = 0; // ticks left after a hit
    int m_air = kMaxAir;
    int m_fire = 0;        // burning ticks left
    int m_fireContact = 0; // ticks spent in fire blocks (catches fire at 20)
    int m_timeSinceRest = 0;
    int m_xpLevel = 0;
    float m_xpProgress = 0.0f;
    int m_xpTotal = 0;
    uint64_t m_enchantSeed = 0x5EED;
    std::array<int, 5> m_protection{};
    int m_armorPoints = 0;
    float m_armorToughness = 0.0f;
    int m_armorWear = 0, m_shieldWear = 0;
    bool m_shieldRaised = false;
    glm::dvec3 m_eye{0.0}, m_facing{0.0, 0.0, 1.0};
    double m_voidY = -128.0;
    double m_fallStartY = 0.0;
    double m_lastY = 0.0;
    bool m_falling = false;
    bool m_started = false;
    std::array<ActiveEffect, kMaxEffects> m_effects{};
};

} // namespace mc
