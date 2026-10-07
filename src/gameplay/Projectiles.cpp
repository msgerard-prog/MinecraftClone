#include "gameplay/Projectiles.h"

#include "gameplay/FluidContact.h"
#include "gameplay/Mobs.h"
#include "world/Blocks.h"
#include "world/Raycast.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

double gaussian(Xoroshiro& rng) {
    // Box-Muller: vanilla's spread uses a gaussian.
    const double u1 = std::max(1e-12, rng.nextDouble()), u2 = rng.nextDouble();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
}

// Where a segment first enters a box: t in [0, len], or a negative number.
double enter(const glm::dvec3& o, const glm::dvec3& dir, double len, const Aabb& b) {
    double t0 = 0.0, t1 = len;
    for (int a = 0; a < 3; ++a) {
        if (std::abs(dir[a]) < 1e-12) {
            if (o[a] < b.min[a] || o[a] > b.max[a]) return -1.0;
            continue;
        }
        double ta = (b.min[a] - o[a]) / dir[a], tb = (b.max[a] - o[a]) / dir[a];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1) return -1.0;
    }
    return t0;
}

} // namespace

float bowPower(int ticks) {
    const float f = float(ticks) / 20.0f;
    return std::min(1.0f, (f * f + f * 2.0f) / 3.0f);
}

void Projectiles::shoot(ProjectileKind kind, const glm::dvec3& from, const glm::dvec3& dir, double speed,
                        double inaccuracy, bool fromPlayer, bool critical, Xoroshiro& rng) {
    if (m_items.size() >= size_t(kMax)) return;
    glm::dvec3 d = glm::normalize(dir);
    d += glm::dvec3(gaussian(rng), gaussian(rng), gaussian(rng)) * 0.0075 * inaccuracy;
    Projectile p;
    p.kind = kind;
    p.pos = p.prevPos = from;
    p.vel = d * speed;
    p.fromPlayer = fromPlayer;
    p.critical = critical;
    m_items.push_back(p);
}

Projectiles::Hits Projectiles::tick(World& world, Player& player, Vitals* vitals, Inventory& inventory,
                                    bool survival, Xoroshiro& rng) {
    Hits hits;
    m_chicks.clear();
    static const ItemId arrowItem = *itemRegistry().find("arrow");
    for (size_t i = 0; i < m_items.size();) {
        Projectile& p = m_items[i];
        p.prevPos = p.pos;
        ++p.life;
        bool remove = false;
        const BlockPos cell{int(std::floor(p.pos.x)), int(std::floor(p.pos.y)), int(std::floor(p.pos.z))};
        if (const Chunk* c = world.chunk(cell.chunk()); c && c->lit() && world.isInHeight(cell.y)) {
            p.skyLight = c->skyLight(blockToLocal(cell.x), cell.y, blockToLocal(cell.z));
            p.blockLight = c->blockLight(blockToLocal(cell.x), cell.y, blockToLocal(cell.z));
        }
        if (p.stuck) {
            // Stuck arrows: picked up by a survival player who shot them (wiki: Arrow).
            if (p.fromPlayer && player.box().intersects(Aabb{p.pos - glm::dvec3(1.0), p.pos + glm::dvec3(1.0)})) {
                if (!survival || inventory.add({arrowItem, 1}) == 0) remove = true;
            }
            if (p.life > 1200 || blockRegistry().blockOf(world.getBlock(cell)) == 0) remove = true; // its block gone
        } else {
            const double speed = glm::length(p.vel);
            const glm::dvec3 dir = speed > 1e-9 ? p.vel / speed : glm::dvec3(0, -1, 0);
            p.facing = dir;
            const auto block = speed > 1e-9 ? raycastBlocks(world, p.pos, dir, speed) : std::nullopt;
            double reach = block ? block->distance : speed;
            // Entities in the way, nearer than the block.
            enum class Target { None, Player, Mob } target = Target::None;
            Mobs::MobHit mob{};
            if (const auto mh = Mobs::raycast(world, p.pos, dir, reach)) {
                mob = *mh;
                reach = mh->distance;
                target = Target::Mob;
            }
            if (!(p.fromPlayer && p.life < 5)) { // (doesn't hit its shooter as it leaves)
                const double t = enter(p.pos, dir, reach, player.box().inflated(0.3));
                if (t >= 0.0) {
                    reach = t;
                    target = Target::Player;
                }
            }
            if (target != Target::None) {
                if (p.kind == ProjectileKind::Arrow) {
                    float damage = float(std::ceil(speed * 2.0));
                    if (p.critical) damage += float(rng.nextInt(uint32_t(damage / 2.0f + 2.0f)));
                    if (target == Target::Player) {
                        if (vitals && survival && vitals->damage(damage)) {
                            player.knockback(p.vel.x, p.vel.z, 0.6 * 0.5); // wiki: Arrow knockback
                            hits.playerDamage += damage;
                        }
                    } else {
                        MobData& m = world.chunk(mob.chunk)->mobs()[size_t(mob.index)];
                        if (m.hurtTime == 0) {
                            m.health -= damage;
                            m.hurtTime = 10;
                            const glm::dvec2 h(p.vel.x, p.vel.z);
                            if (glm::length(h) > 1e-6) m.vel += glm::dvec3(h.x, 0, h.y) / glm::length(h) * 0.3;
                            ++hits.mobsHit;
                        }
                    }
                } else if (target == Target::Mob) {
                    ++hits.mobsHit; // eggs only knock (no damage)
                }
                if (p.kind == ProjectileKind::Egg) m_chicks.push_back(p.pos + dir * reach);
                remove = true;
            } else if (block) {
                if (p.kind == ProjectileKind::Arrow) { // sticks just inside the face it hit
                    p.pos += dir * (block->distance + 0.05);
                    p.vel = glm::dvec3(0.0);
                    p.stuck = true;
                    p.life = 0;
                } else {
                    m_chicks.push_back(p.pos + dir * block->distance);
                    remove = true;
                }
            } else {
                p.pos += p.vel;
                const bool inWater = blockRegistry().blockOf(world.getBlock(cell)) == blocks::Water;
                const double drag = inWater ? 0.6 : 0.99;
                p.vel *= drag;
                p.vel.y -= p.kind == ProjectileKind::Arrow ? 0.05 : 0.03;
                if (p.pos.y < world.height().minY - 64 || p.life > 1200) remove = true;
            }
        }
        if (remove) {
            m_items[i] = m_items.back();
            m_items.pop_back();
        } else {
            ++i;
        }
    }
    // Eggs: 1 in 8 hatches a chick, 1 in 32 of those four (wiki: Egg).
    for (const glm::dvec3& at : m_chicks) {
        if (rng.nextInt(8) != 0) continue;
        const int n = rng.nextInt(32) == 0 ? 4 : 1;
        for (int k = 0; k < n; ++k) {
            MobData chick = Mobs::make(MobType::Chicken, at, rng);
            chick.age = -24000;
            Mobs::add(world, chick);
        }
    }
    return hits;
}

} // namespace mc
