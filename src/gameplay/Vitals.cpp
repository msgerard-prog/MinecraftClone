#include "gameplay/Vitals.h"

#include <algorithm>
#include <cmath>

namespace mc {

float Vitals::breathe(bool eyesInWater, bool keepBreath) {
    if (!eyesInWater) {
        m_air = m_air + 4 > kMaxAir ? kMaxAir : m_air + 4;
        return 0.0f;
    }
    if (keepBreath) return 0.0f;
    if (--m_air <= -20) {
        m_air = 0;
        if (damage(2.0f, false)) return 2.0f;
    }
    return 0.0f;
}

float Vitals::touchFire(bool inFire) {
    if (!inFire) {
        m_fireContact = 0;
        return 0.0f;
    }
    if (++m_fireContact >= 20) setOnFire(160);
    return attacked(1.0f, nullptr, Hit::Fire) ? 1.0f : 0.0f; // (standing in fire: armor helps; burning doesn't)
}

float Vitals::tickFire(bool inWater) {
    if (inWater) m_fire = 0;
    if (m_fire <= 0) return 0.0f;
    const bool hurt = m_fire % 20 == 0 && damage(1.0f, false);
    --m_fire;
    return hurt ? 1.0f : 0.0f;
}

void Vitals::reset() {
    m_health = kMaxHealth;
    m_food = kMaxFood;
    m_saturation = 5.0f;
    m_exhaustion = 0.0f;
    m_foodTimer = 0;
    m_invulnerable = 0;
    m_air = kMaxAir;
    m_fire = 0;
    m_fireContact = 0;
    m_xpLevel = 0; // (dropped as orbs by the caller)
    m_xpProgress = 0.0f;
    m_xpTotal = 0;
    m_falling = false;
    m_started = false;
}

void Vitals::setState(float health, int food, float saturation, float exhaustion) {
    m_health = std::clamp(health, 0.0f, kMaxHealth);
    m_food = std::clamp(food, 0, kMaxFood);
    m_saturation = std::clamp(saturation, 0.0f, float(m_food));
    m_exhaustion = std::clamp(exhaustion, 0.0f, 40.0f);
}

bool Vitals::damage(float amount, bool exhausts) {
    if (amount <= 0.0f || m_invulnerable > 0 || dead()) return false;
    m_health = std::max(0.0f, m_health - amount);
    m_invulnerable = 10;
    if (exhausts) exhaust(0.1f); // wiki: taking damage
    return true;
}

float Vitals::armorReduced(float amount, int armor, float toughness) {
    const float a = float(armor);
    const float reduction = std::min(20.0f, std::max(a / 5.0f, a - 4.0f * amount / (std::min(toughness, 20.0f) + 8.0f)));
    return amount * (1.0f - reduction / 25.0f);
}

float Vitals::protectionReduced(float amount, Hit kind, bool fall) const {
    int epf = m_protection[0];
    if (kind == Hit::Fire) epf += 2 * m_protection[1];
    if (kind == Hit::Explosion) epf += 2 * m_protection[2];
    if (kind == Hit::Projectile) epf += 2 * m_protection[3];
    if (fall) epf += 3 * m_protection[4];
    return amount * (1.0f - float(std::min(epf, 20)) / 25.0f);
}

bool Vitals::attacked(float amount, const glm::dvec3* from, Hit kind) {
    if (amount <= 0.0f || m_invulnerable > 0 || dead()) return false;
    if (m_shieldRaised && from) {
        glm::dvec3 to = *from - m_eye;
        to.y = 0.0;
        if (glm::dot(to, m_facing) > 0.0) { // in front: blocked (wiki: Shield)
            if (amount >= 3.0f) m_shieldWear += 1 + int(std::floor(amount));
            return false;
        }
    }
    if (m_armorPoints > 0) m_armorWear += std::max(1, int(amount / 4.0f));
    return damage(protectionReduced(armorReduced(amount, m_armorPoints, m_armorToughness), kind, false));
}

void Vitals::addExperience(int points) {
    if (points <= 0) return;
    m_xpTotal += points;
    float left = float(points);
    while (left > 0.0f) {
        const float need = float(pointsForLevel(m_xpLevel)) * (1.0f - m_xpProgress);
        if (left < need) {
            m_xpProgress += left / float(pointsForLevel(m_xpLevel));
            break;
        }
        left -= need;
        ++m_xpLevel;
        m_xpProgress = 0.0f;
    }
}

bool Vitals::spendLevels(int levels) {
    if (levels > m_xpLevel) return false;
    m_xpLevel -= levels;
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
        // Armor doesn't help with falls; Feather Falling and Protection do.
        const float reduced = protectionReduced(amount, Hit::Generic, true);
        if (reduced > 0.0f && damage(reduced, false)) hurt += reduced;
        m_falling = false;
    }
    m_lastY = feetY;

    // The void (wiki: Void): 4 damage every half second, 64 below the dimension's
    // bottom (Overworld -128; the Nether and End -64).
    if (feetY < m_voidY && m_invulnerable == 0 && damage(4.0f, false)) hurt += 4.0f;

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
