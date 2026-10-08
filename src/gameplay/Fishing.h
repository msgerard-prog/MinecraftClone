#pragma once

#include "gameplay/ExperienceOrbs.h"
#include "gameplay/ItemEntities.h"
#include "world/Random.h"
#include "world/World.h"

#include <glm/glm.hpp>

namespace mc {

// Fishing (M25.2; wiki: Fishing, Fishing Rod). A cast bobber flies like a thrown item
// and floats on water; after 5-30 s (5 s less per Lure level) a fish nibbles for 1-4 s,
// then bites: reeling in within the next 1-2 s catches something - fish 85%, junk 10%,
// treasure 5% (Luck of the Sea moves 2.1% from junk and 1% from fish... ours: +2% to
// treasure per level, from junk) - pulled toward the player, plus 1-6 experience.
class Fishing {
public:
    bool active() const { return m_active; }
    const glm::dvec3& bobber() const { return m_pos; }
    const glm::dvec3& prevBobber() const { return m_prev; }
    bool biting() const { return m_state == State::Bite; }
    bool nibbling() const { return m_state == State::Nibble; } // (bubbles toward the bobber)

    void cast(const glm::dvec3& eye, const glm::dvec3& look, int lure, int luck,
              world::Xoroshiro& rng);
    // Reel in: the catch (when biting) flies to the player. Returns the rod's wear
    // (1 for a catch, 2 when the bobber was on the ground, else 0).
    int reel(const world::World& world, const glm::dvec3& player, ItemEntities& items,
             ExperienceOrbs* orbs, world::Xoroshiro& rng);
    // One tick; the line breaks beyond 32 blocks (vanilla).
    void tick(const world::World& world, const glm::dvec3& player, world::Xoroshiro& rng);
    void cancel() { m_active = false; }

    // The catch for a bite (public for tests): a loot roll from vanilla's tables.
    static world::ItemStack rollCatch(int luck, world::Xoroshiro& rng);

private:
    enum class State : uint8_t { Flying, Ground, Waiting, Nibble, Bite };
    bool m_active = false;
    State m_state = State::Flying;
    glm::dvec3 m_pos{0.0}, m_prev{0.0}, m_vel{0.0};
    int m_timer = 0, m_lure = 0, m_luck = 0;
};

} // namespace mc
