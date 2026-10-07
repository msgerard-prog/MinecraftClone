#pragma once

#include "gameplay/Inventory.h"
#include "gameplay/ItemEntities.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Random.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <vector>

namespace mc {

// Projectiles (M16.4; wiki: Arrow, Egg, Bow). Arrows fall with gravity 0.05 and drag
// 0.99 a tick (0.6 in water) and deal ceil(speed x 2) damage (fully drawn bows: a
// critical adds up to half again); they stick in blocks for 1200 ticks and can be
// picked up if the player shot them. Thrown eggs (gravity 0.03) break on anything;
// 1 in 8 hatches a chick (1 in 32 of those, four). Pooled (hard rule 1).
enum class ProjectileKind : uint8_t { Arrow, Egg };

struct Projectile {
    ProjectileKind kind = ProjectileKind::Arrow;
    glm::dvec3 pos{0.0}, prevPos{0.0}, vel{0.0};
    bool fromPlayer = false; // shot by the player (can be picked up; doesn't hit them at once)
    uint64_t owner = 0;      // the shooting mob's UUID (high half): never hit by its own arrow
    bool critical = false;
    bool stuck = false;
    glm::dvec3 facing{0.0, -1.0, 0.0}; // flight direction (kept when stuck, for drawing)
    int life = 0; // ticks alive (stuck arrows vanish at 1200)
    uint8_t skyLight = 15, blockLight = 0;
};

class Projectiles {
public:
    static constexpr int kMax = 512;

    Projectiles() {
        m_items.reserve(kMax);
        m_chicks.reserve(16);
    }
    // Launch along `dir` at `speed` blocks/tick with vanilla's inaccuracy spread
    // (gaussian x 0.0075 x inaccuracy per axis).
    void shoot(ProjectileKind kind, const glm::dvec3& from, const glm::dvec3& dir, double speed, double inaccuracy,
               bool fromPlayer, bool critical, world::Xoroshiro& rng, uint64_t owner = 0);

    struct Hits {
        float playerDamage = 0.0f; // (applied here; reported for tests)
        int mobsHit = 0;
    };
    // One tick: flight, hits on blocks, the player (survival: `vitals`) and mobs;
    // pickup of stuck player arrows into `inventory`. Chicks hatched from eggs are
    // added to the world.
    Hits tick(world::World& world, Player& player, Vitals* vitals, Inventory& inventory, bool survival,
              world::Xoroshiro& rng);

    const std::vector<Projectile>& items() const { return m_items; }
    void clear() { m_items.clear(); }

private:
    std::vector<Projectile> m_items;
    std::vector<glm::dvec3> m_chicks; // reused
};

// The bow's draw (wiki: Bow): after `ticks` of drawing, power 0..1 =
// min(1, (f^2 + 2f) / 3) with f = ticks / 20; launch speed power x 3; full = critical.
float bowPower(int ticks);

// The player's bow (wiki: Bow): may it be drawn (survival needs an arrow)?
bool canDrawBow(const Inventory& inventory, bool survival);
// Releasing a bow drawn for `ticks`: below power 0.1 nothing happens; otherwise an
// arrow flies from `eye` along `look` (speed power x 3, critical at full draw), and
// in survival an arrow is used up and the held bow wears by 1 (breaking at its
// durability). Returns true if it shot.
bool releaseBow(Inventory& inventory, int ticks, bool survival, const glm::dvec3& eye, const glm::dvec3& look,
                Projectiles& projectiles, world::Xoroshiro& rng);
// Throwing the held egg (speed 1.5); survival uses it up.
void throwEgg(Inventory& inventory, bool survival, const glm::dvec3& eye, const glm::dvec3& look,
              Projectiles& projectiles, world::Xoroshiro& rng);

} // namespace mc
