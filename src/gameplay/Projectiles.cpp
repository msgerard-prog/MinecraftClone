#include "gameplay/Projectiles.h"

#include "gameplay/FluidContact.h"
#include "gameplay/Mining.h"
#include "gameplay/Mobs.h"
#include "world/Blocks.h"
#include "world/Enchantments.h"
#include "world/BlockUpdates.h"
#include "world/Direction.h"
#include "world/Potions.h"
#include "world/Raycast.h"
#include "world/Rotation.h"

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

bool canDrawBow(const Inventory& inventory, bool survival) {
    static const ItemId arrow = *itemRegistry().find("arrow");
    return !survival || inventory.has(arrow);
}

bool releaseBow(Inventory& inventory, int ticks, bool survival, const glm::dvec3& eye, const glm::dvec3& look,
                Projectiles& projectiles, Xoroshiro& rng) {
    static const ItemId arrow = *itemRegistry().find("arrow");
    const float power = bowPower(ticks);
    const ItemStack bow = inventory.selectedStack();
    // Infinity: needs an arrow but doesn't use it up (wiki: Infinity).
    const bool infinity = enchantLevel(bow, Enchantment::Infinity) > 0;
    if (power < 0.1f || (survival && !inventory.has(arrow))) return false;
    if (!projectiles.shoot(ProjectileKind::Arrow, eye, look, power * 3.0, 1.0, true, power >= 1.0f, rng)) return false;
    if (survival && !infinity) inventory.takeOne(arrow); // (only once it flew)
    Projectile& p = projectiles.last();
    p.power = static_cast<uint8_t>(enchantLevel(bow, Enchantment::Power));
    p.punch = static_cast<uint8_t>(enchantLevel(bow, Enchantment::Punch));
    p.flame = enchantLevel(bow, Enchantment::Flame) > 0;
    p.pickup = !infinity;
    if (survival) inventory.setSlot(inventory.selected(), wearItem(inventory.selectedStack(), 1, rng));
    return true;
}

void throwEgg(Inventory& inventory, bool survival, const glm::dvec3& eye, const glm::dvec3& look,
              Projectiles& projectiles, Xoroshiro& rng) {
    projectiles.shoot(ProjectileKind::Egg, eye, look, 1.5, 1.0, true, false, rng);
    if (survival) inventory.consumeSelected(1);
}

void throwEye(Inventory& inventory, bool survival, const glm::dvec3& eye, glm::ivec2 stronghold,
              Projectiles& projectiles) {
    world::Xoroshiro unused(0);
    if (!projectiles.shoot(ProjectileKind::EyeOfEnder, eye, glm::dvec3(0, 1, 0), 0.0, 0.0, true, false, unused))
        return;
    Projectile& p = projectiles.last();
    const glm::dvec2 to(stronghold.x + 0.5 - eye.x, stronghold.y + 0.5 - eye.z);
    const double dist = glm::length(to);
    const glm::dvec2 dir = dist > 1e-6 ? to / dist : glm::dvec2(0.0);
    const double reach = std::min(12.0, dist);
    p.target = glm::dvec3(eye.x + dir.x * reach, eye.y + (dist > 12.0 ? 8.0 : -4.0), eye.z + dir.y * reach);
    p.pickup = false;
    if (survival) inventory.consumeSelected(1);
}

void throwPearl(Inventory& inventory, bool survival, const glm::dvec3& eye, float yaw, float pitch,
                Projectiles& projectiles, Xoroshiro& rng) {
    if (!projectiles.shoot(ProjectileKind::EnderPearl, eye, glm::dvec3(lookVector(yaw, pitch)), 1.5, 1.0, true, false, rng))
        return;
    projectiles.last().pickup = false;
    if (survival) inventory.consumeSelected(1);
}

void throwSplashPotion(Inventory& inventory, bool survival, const glm::dvec3& eye, float yaw, float pitch,
                       Projectiles& projectiles, Xoroshiro& rng) {
    const ItemStack held = inventory.selectedStack();
    const glm::dvec3 dir(lookVector(yaw, pitch - 20.0f));
    if (!projectiles.shoot(ProjectileKind::SplashPotion, eye, dir, 0.5, 1.0, true, false, rng)) return;
    projectiles.last().potion = held.potion;
    projectiles.last().pickup = false;
    if (survival) inventory.consumeSelected(1);
}

bool Projectiles::shoot(ProjectileKind kind, const glm::dvec3& from, const glm::dvec3& dir, double speed,
                        double inaccuracy, bool fromPlayer, bool critical, Xoroshiro& rng, uint64_t owner) {
    if (m_items.size() >= size_t(kMax)) {
        // Full: the oldest arrow stuck in a block (not the player's) makes room.
        size_t oldest = m_items.size();
        for (size_t i = 0; i < m_items.size(); ++i)
            if (m_items[i].stuck && !m_items[i].fromPlayer &&
                (oldest == m_items.size() || m_items[i].life > m_items[oldest].life))
                oldest = i;
        if (oldest == m_items.size()) return false;
        m_items[oldest] = m_items.back();
        m_items.pop_back();
    }
    glm::dvec3 d = glm::normalize(dir);
    d += glm::dvec3(gaussian(rng), gaussian(rng), gaussian(rng)) * 0.0075 * inaccuracy;
    Projectile p;
    p.kind = kind;
    p.pos = p.prevPos = from;
    p.vel = d * speed;
    p.fromPlayer = fromPlayer;
    p.critical = critical;
    p.owner = owner;
    m_items.push_back(p);
    return true;
}

Projectiles::Hits Projectiles::tick(World& world, Player& player, Vitals* vitals, Inventory& inventory,
                                    bool survival, Xoroshiro& rng) {
    Hits hits;
    m_chicks.clear();
    m_eyeDrops.clear();
    m_explosions.clear();
    m_pearls.clear();
    // Breath clouds: Instant Damage once a second to a survival player standing in one.
    for (size_t i = 0; i < m_clouds.size();) {
        BreathCloud& c = m_clouds[i];
        if (c.cooldown > 0) --c.cooldown;
        const glm::dvec3 feet = player.position();
        const double dx = feet.x - c.pos.x, dz = feet.z - c.pos.z;
        if (vitals && survival && c.cooldown == 0 && dx * dx + dz * dz < double(c.radius) * c.radius &&
            feet.y > c.pos.y - 1.0 && feet.y < c.pos.y + 1.5) {
            vitals->addEffect(Effect::InstantDamage, 0, 1);
            c.cooldown = 20;
        }
        if (--c.ticks <= 0) {
            m_clouds[i] = m_clouds.back();
            m_clouds.pop_back();
        } else {
            ++i;
        }
    }
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
        if (p.kind == ProjectileKind::ShulkerBullet) {
            // Homes in on the player's middle, turning a little each tick (vanilla moves
            // it axis by axis; ours steers smoothly); hits blocks and the player only.
            const glm::dvec3 aim = player.position() + glm::dvec3(0.0, 0.9, 0.0) - p.pos;
            const double len = glm::length(aim);
            if (len > 1e-6) p.vel += (aim / len * 0.2 - p.vel) * 0.1;
            p.facing = p.vel;
            const double speed = glm::length(p.vel);
            if (player.box().intersects(Aabb{p.pos - glm::dvec3(0.15), p.pos + glm::dvec3(0.15)})) {
                if (vitals && survival && vitals->attacked(4.0f, &p.pos, Vitals::Hit::Projectile)) {
                    vitals->addEffect(Effect::Levitation, 0, 200);
                    hits.playerDamage += 4.0f;
                }
                remove = true;
            } else if (speed > 1e-9 && raycastBlocks(world, p.pos, p.vel / speed, speed)) {
                remove = true;
            } else {
                p.pos += p.vel;
            }
            if (p.life > 400) remove = true;
        } else if (p.kind == ProjectileKind::EyeOfEnder) {
            // Glides toward its target (through blocks), then comes down (wiki).
            p.vel = (p.target - p.pos) * 0.06;
            p.pos += p.vel;
            p.facing = p.vel;
            if (p.life >= 80) {
                if (rng.nextInt(5) != 0) m_eyeDrops.push_back(p.pos);
                remove = true;
            }
        } else if (p.stuck) {
            // Stuck arrows: picked up by a survival player who shot them (wiki: Arrow).
            if (p.fromPlayer && p.pickup && player.box().intersects(Aabb{p.pos - glm::dvec3(1.0), p.pos + glm::dvec3(1.0)})) {
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
            if (const auto mh = Mobs::raycast(world, p.pos, dir, reach, p.owner)) {
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
            const bool fireball = p.kind == ProjectileKind::GhastFireball || p.kind == ProjectileKind::BlazeFireball;
            if (p.kind == ProjectileKind::EnderPearl && (target != Target::None || block)) {
                PearlLanding l;
                l.pos = p.pos + dir * std::max(0.0, reach - 0.3); // (just short of what it hit)
                if (block && target == Target::None &&
                    blockRegistry().blockOf(world.getBlock(block->block)) == blocks::EndGateway) {
                    l.gateway = true;
                    l.gatewayBlock = block->block;
                }
                if (m_pearls.size() < m_pearls.capacity()) m_pearls.push_back(l);
                remove = true;
            } else if (p.kind == ProjectileKind::DragonFireball && (target != Target::None || block)) {
                // Its breath lingers where it burst, on the floor below (wiki: Dragon Fireball).
                glm::dvec3 at = p.pos + dir * reach;
                for (int k = 0; k < 8 && !blockRegistry().collides(world.getBlock(
                                             {int(std::floor(at.x)), int(std::floor(at.y - 0.5)), int(std::floor(at.z))}));
                     ++k)
                    at.y -= 1.0;
                addCloud(at, 3.0f, 600);
                remove = true;
            } else if (p.kind == ProjectileKind::SplashPotion && (target != Target::None || block)) {
                // Splash (wiki: Splash Potion): entities whose hitbox touches an
                // 8.25 x 4.25 x 8.25 box around the impact and whose nearest point is
                // within 4 blocks get the effect, scaled by 1 - distance / 4 (a direct hit:
                // full); healing and harming swap on the undead (wiki: Undead).
                const glm::dvec3 at = p.pos + dir * reach;
                const Aabb area{at - glm::dvec3(4.125, 2.125, 4.125), at + glm::dvec3(4.125, 2.125, 4.125)};
                auto scaleFor = [&](const Aabb& box, bool direct) {
                    if (direct) return 1.0;
                    if (!box.intersects(area)) return 0.0;
                    const glm::dvec3 nearest = glm::clamp(at, box.min, box.max);
                    const double d = glm::length(nearest - at);
                    return d < 4.0 ? 1.0 - d / 4.0 : 0.0;
                };
                const Potion potion = static_cast<Potion>(p.potion);
                const PotionInfo& info = potionInfo(potion);
                const bool water = potion == Potion::Water;
                const MobData* direct =
                    target == Target::Mob ? &world.chunk(mob.chunk)->mobs()[size_t(mob.index)] : nullptr;
                if (info.effect != Effect::None) {
                    const double sp = scaleFor(player.box(), target == Target::Player);
                    const bool hurtsPlayer = info.effect == Effect::InstantDamage || info.effect == Effect::Poison;
                    if (sp > 0.0 && vitals && (survival || !hurtsPlayer)) { // (creative takes no harm)
                        const int duration = int(info.duration * sp + 0.5);
                        if (effectInfo(info.effect).instant || duration > 20) // (1 s or less: dropped)
                            vitals->addEffect(info.effect, info.amplifier, duration, sp);
                    }
                }
                // Mobs: instant health/damage (other effects reach only the player, our
                // simplification); water hurts blazes, endermen and striders by 1.
                if (info.effect == Effect::InstantHealth || info.effect == Effect::InstantDamage || water) {
                    const ChunkPos c0{blockToChunk(int(std::floor(at.x))), blockToChunk(int(std::floor(at.z)))};
                    for (int dz = -1; dz <= 1; ++dz)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (Chunk* ch = world.chunk({c0.x + dx, c0.z + dz}))
                                for (MobData& m : ch->mobs()) {
                                    const double sm = scaleFor(Mobs::box(m), &m == direct);
                                    if (sm <= 0.0 || m.health <= 0.0f) continue;
                                    // (the dragon and crystals take no effects)
                                    if (m.type == MobType::EnderDragon || m.type == MobType::EndCrystal) continue;
                                    float amount = 0.0f;
                                    bool harm = true;
                                    if (water) {
                                        if (m.type != MobType::Blaze && m.type != MobType::Enderman &&
                                            m.type != MobType::Strider)
                                            continue;
                                        amount = 1.0f;
                                    } else {
                                        const bool undead = m.type == MobType::Zombie || m.type == MobType::Skeleton ||
                                                            m.type == MobType::ZombifiedPiglin;
                                        harm = (info.effect == Effect::InstantDamage) != undead;
                                        amount = float((harm ? 6 : 4) << info.amplifier) * float(sm);
                                    }
                                    if (harm) {
                                        m.health -= amount;
                                        m.hurtTime = 10;
                                        if (p.fromPlayer) m.lastHurtByPlayer = true;
                                    } else {
                                        m.health = std::min(mobInfo(m.type).maxHealth, m.health + amount);
                                    }
                                }
                }
                if (water) { // puts out fire in the cell it broke in and the 4 beside it
                    const BlockPos c = block && target == Target::None
                                           ? BlockPos{block->block.x + normal(block->face).x,
                                                      block->block.y + normal(block->face).y,
                                                      block->block.z + normal(block->face).z}
                                           : BlockPos{int(std::floor(at.x)), int(std::floor(at.y)), int(std::floor(at.z))};
                    const BlockPos cells[5] = {c, {c.x + 1, c.y, c.z}, {c.x - 1, c.y, c.z}, {c.x, c.y, c.z + 1},
                                               {c.x, c.y, c.z - 1}};
                    for (const BlockPos& f : cells)
                        if (world.isInHeight(f.y) && blockRegistry().blockOf(world.getBlock(f)) == blocks::Fire) {
                            world.updateBlock(f, 0);
                            if (m_edits.size() < m_edits.capacity()) m_edits.push_back(f);
                        }
                }
                remove = true;
            } else if (fireball && (target != Target::None || block)) {
                const glm::dvec3 at = p.pos + dir * reach;
                if (target == Target::Player && vitals && survival) {
                    const float damage = p.kind == ProjectileKind::GhastFireball ? 6.0f : 5.0f; // (wiki)
                    if (vitals->attacked(damage, &p.pos, Vitals::Hit::Fire) && p.kind == ProjectileKind::BlazeFireball)
                        vitals->setOnFire(100); // 5 s alight
                    hits.playerDamage += damage;
                } else if (target == Target::Mob) {
                    MobData& m = world.chunk(mob.chunk)->mobs()[size_t(mob.index)];
                    if (m.hurtTime == 0 && !mobInfo(m.type).fireImmune) {
                        m.health -= p.kind == ProjectileKind::GhastFireball ? 6.0f : 5.0f;
                        m.hurtTime = 10;
                        m.fireTicks = std::max<int16_t>(m.fireTicks, 100);
                        ++hits.mobsHit;
                    }
                }
                if (p.kind == ProjectileKind::GhastFireball) {
                    m_explosions.push_back(at);
                } else if (target == Target::None && block) { // fire where it landed
                    const BlockPos f = block->block;
                    const glm::ivec3 n = normal(block->face);
                    const BlockPos front{f.x + n.x, f.y + n.y, f.z + n.z};
                    if (world.isInHeight(front.y) && world.getBlock(front) == 0 && BlockUpdates::fireCanStay(world, front)) {
                        world.updateBlock(front, BlockUpdates::fireState(0));
                        if (m_edits.size() < m_edits.capacity()) m_edits.push_back(front);
                    }
                }
                remove = true;
            } else if (target != Target::None) {
                if (p.kind == ProjectileKind::Arrow) {
                    // Base damage 2, Power adds 0.5 per level + 0.5 (wiki: Power).
                    const double base = 2.0 + (p.power ? 0.5 * p.power + 0.5 : 0.0);
                    float damage = float(std::ceil(speed * base));
                    if (p.critical) damage += float(rng.nextInt(uint32_t(damage / 2.0f + 2.0f)));
                    if (target == Target::Player) {
                        if (vitals && survival && vitals->attacked(damage, &p.pos, Vitals::Hit::Projectile)) {
                            player.knockback(p.vel.x, p.vel.z, 0.6 * 0.5); // wiki: Arrow knockback
                            hits.playerDamage += damage;
                        }
                    } else {
                        MobData& m = world.chunk(mob.chunk)->mobs()[size_t(mob.index)];
                        const bool perchedDragon = (m.type == MobType::EnderDragon && (m.phase == 5 || m.phase == 6)) ||
                                                   (m.type == MobType::Shulker && m.peek == 0); // (closed shells too)
                        if (m.type == MobType::Enderman) {
                            m.wantsTeleport = true; // arrows can't hurt endermen: they teleport away (wiki)
                        } else if (perchedDragon) {
                            // A perched dragon shrugs arrows off (wiki: Ender Dragon).
                        } else if (m.hurtTime == 0) {
                            if (p.fromPlayer) m.lastHurtByPlayer = true;
                            m.health -= m.type == MobType::EnderDragon
                                            ? Mobs::dragonDamage(m, float(damage), p.pos + dir * reach)
                                            : float(damage);
                            m.hurtTime = 10;
                            const glm::dvec2 h(p.vel.x, p.vel.z);
                            if (glm::length(h) > 1e-6)
                                m.vel += glm::dvec3(h.x, 0, h.y) / glm::length(h) * (0.3 + 0.6 * p.punch); // Punch
                            if (p.flame) m.fireTicks = std::max<int16_t>(m.fireTicks, 100); // Flame: 5 s
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
                if (p.kind != ProjectileKind::GhastFireball && p.kind != ProjectileKind::BlazeFireball &&
                    p.kind != ProjectileKind::DragonFireball) { // (fireballs fly straight)
                    const double drag = inWater ? 0.6 : 0.99;
                    p.vel *= drag;
                    p.vel.y -= p.kind == ProjectileKind::Arrow || p.kind == ProjectileKind::SplashPotion ? 0.05 : 0.03; // (pearls 0.03)
                }
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
