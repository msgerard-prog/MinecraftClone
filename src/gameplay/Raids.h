#pragma once

#include "gameplay/Vitals.h"
#include "world/Random.h"
#include "world/World.h"

#include <glm/glm.hpp>

namespace mc {

// Raids (M24.5; wiki: Raid). A player with Bad Omen who comes within 32 blocks of a
// village bell gets Raid Omen; 30 s later a raid starts there: 5 waves (Normal; one more
// at Bad Omen II+) of raiders spawning ~32 blocks from the bell, 15 s apart once the
// last wave is beaten. Winning gives Hero of the Village (40 min); the raid is lost
// when no villager is left near the bell, and given up after 40 minutes.
class Raid {
public:
    bool active() const { return m_active; }
    bool pending() const { return m_pendingTicks > 0; }
    const glm::ivec3& centre() const { return m_centre; }
    int wave() const { return m_wave; }
    int waves() const { return m_waves; }
    // The raid bar: the living raiders' health over the wave's starting health.
    float progress() const { return m_waveHealth > 0.0f ? m_aliveHealth / m_waveHealth : 0.0f; }
    // One game tick. `player` is where the player stands; `vitals` carries the omen.
    void tick(world::World& world, Vitals& vitals, const glm::dvec3& player, world::Xoroshiro& rng);
    // Saved state (level.dat, our tag).
    struct State {
        bool active = false;
        glm::ivec3 centre{0};
        int wave = 0, waves = 0, level = 1, ticks = 0, cooldown = 0;
        float waveHealth = 0.0f;
    };
    State state() const {
        return {m_active, m_centre, m_wave, m_waves, m_level, m_ticks, m_cooldown, m_waveHealth};
    }
    void restore(const State& s) {
        m_active = s.active;
        m_centre = s.centre;
        m_wave = s.wave;
        m_waves = s.waves;
        m_level = s.level;
        m_ticks = s.ticks;
        m_cooldown = s.cooldown;
        m_waveHealth = s.waveHealth;
    }
    void start(const glm::ivec3& centre, int level); // (public for tests and /raid-like starts)

private:
    void spawnWave(world::World& world, world::Xoroshiro& rng);
    bool m_active = false;
    glm::ivec3 m_centre{0}, m_pendingCentre{0};
    int m_pendingTicks = 0, m_pendingLevel = 1;
    int m_wave = 0, m_waves = 0, m_level = 1, m_ticks = 0, m_cooldown = 0, m_counter = 0;
    float m_waveHealth = 0.0f, m_aliveHealth = 0.0f;
    int m_alive = 0;
};

// The nearest bell within `range` blocks of `at` (a village's meeting point), if any.
std::optional<glm::ivec3> findBell(const world::World& world, const glm::dvec3& at, int range);

} // namespace mc
