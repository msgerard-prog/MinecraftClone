#include "gameplay/Vitals.h"

#include <algorithm>
#include <cmath>

namespace mc {

void Vitals::reset() {
    m_health = kMaxHealth;
    m_food = kMaxFood;
    m_saturation = 5.0f;
    m_exhaustion = 0.0f;
    m_foodTimer = 0;
    m_invulnerable = 0;
    m_falling = false;
    m_started = false;
}

void Vitals::setState(float health, int food, float saturation, float exhaustion) {
    m_health = std::clamp(health, 0.0f, kMaxHealth);
    m_food = std::clamp(food, 0, kMaxFood);
    m_saturation = std::clamp(saturation, 0.0f, float(m_food));
    m_exhaustion = std::clamp(exhaustion, 0.0f, 40.0f);
}

bool Vitals::damage(float amount) {
    if (amount <= 0.0f || m_invulnerable > 0 || dead()) return false;
    m_health = std::max(0.0f, m_health - amount);
    m_invulnerable = 10;
    exhaust(0.1f); // wiki: taking damage
    return true;
}

void Vitals::eat(int food, float saturation) {
    m_food = std::min(kMaxFood, m_food + food);
    m_saturation = std::min(float(m_food), m_saturation + saturation);
}

float Vitals::tick(double feetY, bool onGround, bool inWater, bool flying) {
    float hurt = 0.0f;
    if (m_invulnerable > 0) --m_invulnerable;
    if (!m_started) { // first tick after (re)spawn or load: no fall from nowhere
        m_started = true;
        m_lastY = feetY;
    }

    // Falls (wiki: Fall damage): 1 per block fallen beyond 3, on landing, measured
    // from the highest point since leaving the ground. Water and flight cancel it.
    if (inWater || flying) {
        m_falling = false;
    } else if (!onGround) {
        if (!m_falling) {
            m_falling = true;
            m_fallStartY = std::max(feetY, m_lastY);
        }
        m_fallStartY = std::max(m_fallStartY, feetY);
    } else if (m_falling) {
        const float amount = static_cast<float>(std::ceil(m_fallStartY - feetY - 3.0));
        if (amount > 0.0f && damage(amount)) hurt += amount;
        m_falling = false;
    }
    m_lastY = feetY;

    // The void (wiki: Void): 4 damage every half second below y -128.
    if (feetY < -128.0 && m_invulnerable == 0 && damage(4.0f)) hurt += 4.0f;

    // Exhaustion drains saturation first, then food (wiki: Hunger).
    while (m_exhaustion >= 4.0f) {
        m_exhaustion -= 4.0f;
        if (m_saturation > 0.0f) m_saturation = std::max(0.0f, m_saturation - 1.0f);
        else m_food = std::max(0, m_food - 1);
    }

    // Regeneration and starvation (wiki: Hunger › Mechanics, normal difficulty).
    if (dead()) return hurt;
    if (m_food >= kMaxFood && m_saturation > 0.0f && m_health < kMaxHealth) {
        if (++m_foodTimer >= 10) { // fast: saturation-fuelled, every half second
            const float heal = std::min(m_saturation, 6.0f) / 6.0f;
            m_health = std::min(kMaxHealth, m_health + heal);
            exhaust(heal * 6.0f);
            m_foodTimer = 0;
        }
    } else if (m_food >= 18 && m_health < kMaxHealth) {
        if (++m_foodTimer >= 80) {
            m_health = std::min(kMaxHealth, m_health + 1.0f);
            exhaust(6.0f);
            m_foodTimer = 0;
        }
    } else if (m_food <= 0) {
        if (++m_foodTimer >= 80) {
            if (m_health > 1.0f) { // normal difficulty: starving stops at half a heart
                m_health -= 1.0f;
                hurt += 1.0f;
            }
            m_foodTimer = 0;
        }
    } else {
        m_foodTimer = 0;
    }
    return hurt;
}

} // namespace mc
