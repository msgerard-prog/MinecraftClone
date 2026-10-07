#pragma once

#include <cstdint>

namespace mc {

// Health and hunger (wiki: Health, Hunger, Damage › Fall damage), survival only.
// Values in half-hearts / half-drumsticks like vanilla (max 20). Normal difficulty.
class Vitals {
public:
    static constexpr float kMaxHealth = 20.0f;
    static constexpr int kMaxFood = 20;

    float health() const { return m_health; }
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
    int foodTimer() const { return m_foodTimer; }
    void setFoodTimer(int t) { m_foodTimer = t; }
    // Eats `food` points with `saturation` (wiki: Food), capped like vanilla.
    void eat(int food, float saturation);
    void reset(); // respawn: full health and food, fresh saturation
    void kill() { m_health = 0.0f; } // /kill (ignores invulnerability)
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
    float breathe(bool eyesInWater);
    int air() const { return m_air; }
    void setAir(int a) { m_air = a; }
    // Burning (wiki: Fire, Lava): lava sets the player on fire for 15 s; burning hurts
    // 1 every second; water puts it out. Returns damage taken.
    void setOnFire(int ticks) { m_fire = ticks > m_fire ? ticks : m_fire; }
    float tickFire(bool inWater);
    bool burning() const { return m_fire > 0; }
    int fireTicks() const { return m_fire; }
    void setFireTicks(int ticks) { m_fire = ticks; }

    // Saved state.
    void setState(float health, int food, float saturation, float exhaustion);

private:
    float m_health = kMaxHealth;
    int m_food = kMaxFood;
    float m_saturation = 5.0f;
    float m_exhaustion = 0.0f;
    int m_foodTimer = 0;     // regeneration / starvation clock
    int m_invulnerable = 0;  // ticks left after a hit
    int m_air = kMaxAir;
    int m_fire = 0;          // burning ticks left
    double m_voidY = -128.0;
    double m_fallStartY = 0.0;
    double m_lastY = 0.0;
    bool m_falling = false;
    bool m_started = false;
};

} // namespace mc
