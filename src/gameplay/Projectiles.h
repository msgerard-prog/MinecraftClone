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
// Eyes of ender (M18.5) fly to a point and then drop or shatter (see throwEye).
// Ghast fireballs (M19.2) fly straight and explode (power 1, fire); blaze fireballs
// hit for 5 and set fire (wiki: Fireball, Small Fireball).
// Splash potions (M19.4) fly with gravity 0.05 and break on anything (wiki: Splash
// Potion): within 4 blocks effects scale with 1 - distance / 4.
// Dragon fireballs (M20.2) fly straight and leave a cloud of dragon's breath where they
// hit: radius 3 for 30 s, Instant Damage to the player in it once a second (wiki:
// Dragon Fireball, Dragon's Breath; the cloud's numbers are our reading).
// Ender pearls (M20.3) fly like eggs and take their thrower where they land (wiki: Ender
// Pearl - 5 damage on landing); into an end gateway, through it.
enum class ProjectileKind : uint8_t {
    Arrow,
    Egg,
    EyeOfEnder,
    GhastFireball,
    BlazeFireball,
    SplashPotion,
    DragonFireball,
    EnderPearl,
    ShulkerBullet, // (M20.4: homes in on the player; 4 damage + Levitation for 10 s)
    Trident        // (M25.3: 8 damage, + Impaling on water mobs; sticks, Loyalty brings it back)
};

// Where a thrown ender pearl came down: the player goes there (main).
struct PearlLanding {
    glm::dvec3 pos{0.0};
    bool gateway = false; // it went into an end gateway at `gatewayBlock`
    world::BlockPos gatewayBlock{0, 0, 0};
};

// A lingering cloud of dragon's breath (vanilla: an area effect cloud).
struct BreathCloud {
    glm::dvec3 pos{0.0}; // the middle of its floor
    float radius = 3.0f;
    int ticks = 0;    // left
    int cooldown = 0; // until it can hurt the player again
};

struct Projectile {
    ProjectileKind kind = ProjectileKind::Arrow;
    glm::dvec3 pos{0.0}, prevPos{0.0}, vel{0.0};
    bool fromPlayer = false; // shot by the player (can be picked up; doesn't hit them at once)
    uint64_t owner = 0;      // the shooting mob's UUID (high half): never hit by its own arrow
    bool critical = false;
    bool skeleton = false; // shot by a skeleton (creepers it kills drop a music disc; pillagers' bolts don't)
    uint8_t power = 0, punch = 0; // bow enchantments (damage, knockback)
    bool flame = false;           // sets what it hits on fire
    bool pickup = true;           // (Infinity arrows can't be picked up)
    bool stuck = false;
    glm::dvec3 facing{0.0, -1.0, 0.0}; // flight direction (kept when stuck, for drawing)
    int life = 0; // ticks alive (stuck arrows vanish at 1200)
    uint8_t skyLight = 15, blockLight = 0;
    glm::dvec3 target{0.0}; // eyes of ender: where they fly
    uint8_t potion = 0;     // splash potions: the potion (world::Potion)
    world::ItemStack stack{}; // tridents: the item itself (enchantments, wear), given back on pickup
    bool dealt = false;       // tridents: has hit something (drops away, hits nothing more)
};

class Projectiles {
public:
    static constexpr int kMax = 512;

    Projectiles() {
        m_items.reserve(kMax);
        m_chicks.reserve(16);
        m_eyeDrops.reserve(16);
        m_explosions.reserve(16);
        m_edits.reserve(64);
        m_clouds.reserve(kMaxClouds);
        m_pearls.reserve(16);
        m_channeled.reserve(8);
    }
    static constexpr int kMaxClouds = 32;
    // A breath cloud (dragon fireballs, the perched dragon's flames).
    void addCloud(const glm::dvec3& at, float radius, int ticks) {
        if (int(m_clouds.size()) < kMaxClouds) m_clouds.push_back({at, radius, ticks, 0});
    }
    const std::vector<BreathCloud>& clouds() const { return m_clouds; }
    // Ender pearls that came down this tick.
    const std::vector<PearlLanding>& pearls() const { return m_pearls; }
    // Launch along `dir` at `speed` blocks/tick with vanilla's inaccuracy spread
    // (gaussian x 0.0075 x inaccuracy per axis).
    // Returns false if the pool is full of flying arrows (nothing was shot).
    bool shoot(ProjectileKind kind, const glm::dvec3& from, const glm::dvec3& dir, double speed, double inaccuracy,
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
    // Eyes of ender that came down this tick (the caller drops an eye item there).
    const std::vector<glm::dvec3>& eyeDrops() const { return m_eyeDrops; }
    // Ghast fireballs that hit this tick (the caller explodes them, power 1, with fire)
    // and blocks set alight by blaze fireballs (to relight and re-mesh).
    const std::vector<glm::dvec3>& explosions() const { return m_explosions; }
    std::vector<world::BlockPos>& edits() { return m_edits; }
    Projectile& last() { return m_items.back(); } // the one just shot
    // Channeling strikes this tick (M25.3): main calls lightning down there. `thundering`
    // must be set by main each tick.
    const std::vector<world::BlockPos>& channeled() const { return m_channeled; }
    void setThundering(bool on) { m_thundering = on; }
    void clear() { // (travelling: nothing follows the player into another dimension)
        m_items.clear();
        m_clouds.clear();
        m_pearls.clear();
    }

private:
    std::vector<Projectile> m_items;
    std::vector<world::BlockPos> m_channeled;
    bool m_thundering = false;
    std::vector<glm::dvec3> m_chicks;   // reused
    std::vector<glm::dvec3> m_eyeDrops; // reused
    std::vector<glm::dvec3> m_explosions;
    std::vector<world::BlockPos> m_edits;
    std::vector<BreathCloud> m_clouds;
    std::vector<PearlLanding> m_pearls;
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
// Releasing a trident held back for `ticks` (M25.3; wiki: Trident): at least 10 ticks;
// thrown at 2.5 blocks a tick (survival: it leaves the hand and wears by 1; creative:
// a copy that can't be picked up). With Riptide it can't be thrown: returns the
// player's launch speed instead (when in water or rain), 0 otherwise.
double releaseTrident(Inventory& inventory, int ticks, bool survival, bool wet, const glm::dvec3& eye,
                      const glm::dvec3& look, Projectiles& projectiles, world::Xoroshiro& rng);
// Throwing the held egg (speed 1.5); survival uses it up.
void throwEgg(Inventory& inventory, bool survival, const glm::dvec3& eye, const glm::dvec3& look,
              Projectiles& projectiles, world::Xoroshiro& rng);
// Throwing an eye of ender toward the nearest stronghold at (x, z) (wiki: Eye of
// Ender): it flies about 12 blocks that way, climbing while the stronghold is farther
// than 12 blocks and sinking once it is nearer, hovers, and after 80 ticks drops as an
// item (80%) or shatters (20%). Survival uses it up.
void throwEye(Inventory& inventory, bool survival, const glm::dvec3& eye, glm::ivec2 stronghold,
              Projectiles& projectiles);
// Throwing the held ender pearl (speed 1.5, like a snowball; wiki).
void throwPearl(Inventory& inventory, bool survival, const glm::dvec3& eye, float yaw, float pitch,
                Projectiles& projectiles, world::Xoroshiro& rng);
// Throwing the held splash potion (speed 0.5, aimed 20 degrees up; wiki).
void throwSplashPotion(Inventory& inventory, bool survival, const glm::dvec3& eye, float yaw, float pitch,
                       Projectiles& projectiles, world::Xoroshiro& rng);

} // namespace mc
