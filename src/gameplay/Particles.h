#pragma once

#include "world/Direction.h"
#include "world/ParticleSprite.h"
#include "world/Random.h"
#include "world/Weather.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace mc {

// One particle (wiki: Particles). Simulated at 20 TPS like everything else (vanilla
// ticks particles on the client tick) and interpolated when drawn.
struct Particle {
    glm::dvec3 pos{0.0}, prevPos{0.0}, vel{0.0};
    glm::vec3 color{1.0f};
    float size = 0.1f;     // half the quad's side, blocks
    float gravity = 0.0f;  // blocks/tick^2 pulled down (negative: rises)
    float friction = 0.98f;
    int16_t age = 0, lifetime = 20;
    world::BlockStateId state = 0; // Terrain: whose texture
    uint8_t u = 0, v = 0;          // Terrain: the piece's corner in 1/4 of the sprite
    ParticleSprite sprite = ParticleSprite::Generic0;
    uint8_t frames = 1;        // >1: steps sprite..sprite+frames-1 over its life
    bool reverseFrames = false; // large to small (smoke) instead of small to large
    bool physics = true;       // stops against blocks
    bool onGround = false;
    bool fullBright = false;   // flames, lava, portal sparks glow
    bool hangs = false;        // drips: wait under the block, then fall
    bool toOrigin = false;     // portal sparks: drift back toward their start
    glm::dvec3 origin{0.0};
    uint8_t skyLight = 15, blockLight = 0;
    // The current sprite cell (animated ones step through their frames).
    ParticleSprite frame() const {
        if (frames <= 1) return sprite;
        int f = age * frames / std::max<int>(1, lifetime);
        f = std::min(f, frames - 1);
        if (reverseFrames) f = frames - 1 - f;
        return static_cast<ParticleSprite>(int(sprite) + f);
    }
};

// Particles (M22.3): spawned from level events (block breaking and mining, explosions,
// mob deaths, potion splashes, crits, teleports), from vanilla's "animate ticks" of
// blocks near the player (torches, fire, lava, furnaces, portals, drips, end rods,
// redstone) and from rain (splashes). A reserved pool; the oldest are replaced when
// it is full (vanilla caps at 16,384).
class Particles {
public:
    static constexpr int kMax = 4096;

    Particles() { m_particles.reserve(kMax); }

    // One tick: spawn from this tick's level events (not cleared here), animate blocks
    // around `player`, rain splashes, then move and age everything.
    void tick(const world::World& world, const std::vector<world::LevelEvent>& events, const glm::dvec3& player,
              const world::Weather* weather, world::Xoroshiro& rng);
    // Status effect swirls around an entity (the player: colour 0xRRGGBB).
    void effectSwirl(const glm::dvec3& feet, double width, double height, uint32_t rgb, world::Xoroshiro& rng);

    const std::vector<Particle>& all() const { return m_particles; }
    void clear() {
        m_particles.clear();
        m_emitterCount = 0;
    }

    // Spawning (also used by tests).
    Particle& add(const Particle& p);
    void blockBreak(world::BlockStateId state, const world::BlockPos& p, world::Xoroshiro& rng);
    void blockHit(world::BlockStateId state, const world::BlockPos& p, world::Direction face, world::Xoroshiro& rng);
    void explosion(const glm::dvec3& at, float power, world::Xoroshiro& rng);
    void poof(const glm::dvec3& feet, double width, double height, world::Xoroshiro& rng);
    void splashPotion(const glm::dvec3& at, uint32_t rgb, world::Xoroshiro& rng);
    void crit(const glm::dvec3& at, world::Xoroshiro& rng);
    void smoke(const glm::dvec3& at, bool large, world::Xoroshiro& rng);
    void flame(const glm::dvec3& at, world::Xoroshiro& rng);
    void portal(const glm::dvec3& at, world::Xoroshiro& rng);

private:
    void animate(const world::World& world, const world::BlockPos& p, world::Xoroshiro& rng);
    void rain(const world::World& world, const glm::dvec3& player, const world::Weather& weather, world::Xoroshiro& rng);
    void move(const world::World& world, Particle& p);
    void puff(const glm::dvec3& at, world::Xoroshiro& rng);
    std::vector<Particle> m_particles;
    struct Emitter { // explosions of power 2+: 6 puffs a tick for 8 ticks
        glm::dvec3 at;
        int ticks;
    };
    std::array<Emitter, 32> m_emitters{};
    int m_emitterCount = 0;
    size_t m_next = 0; // the slot replaced next when full
};

} // namespace mc
