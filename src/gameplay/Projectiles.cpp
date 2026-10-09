#include "gameplay/Projectiles.h"

#include "gameplay/Fireworks.h"

#include "gameplay/FluidContact.h"
#include "gameplay/Mining.h"
#include "gameplay/Mobs.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Direction.h"
#include "world/Enchantments.h"
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

int ammoSlot(const Inventory& inventory) {
    static const ItemId arrow = *itemRegistry().find("arrow"),
                        tipped = *itemRegistry().find("tipped_arrow"),
                        spectral = *itemRegistry().find("spectral_arrow");
    auto isAmmo = [&](const ItemStack& s) {
        return !s.empty() && (s.item == arrow || s.item == tipped || s.item == spectral);
    };
    if (isAmmo(inventory.offhand())) return -1;
    for (int i = 0; i < Inventory::kSlots; ++i)
        if (isAmmo(inventory.slot(i))) return i;
    return -2;
}

namespace {
// Takes one of the ammunition (survival) and returns it; creative shoots plain arrows
// unless it holds others.
ItemStack takeAmmo(Inventory& inventory, bool survival, bool keep) {
    static const ItemId arrow = *itemRegistry().find("arrow");
    const int slot = ammoSlot(inventory);
    if (slot == -2) return survival ? ItemStack{} : ItemStack{arrow, 1};
    ItemStack s = slot == -1 ? inventory.offhand() : inventory.slot(slot);
    ItemStack one = s;
    one.count = 1;
    if (survival && !keep) {
        s.count = uint8_t(s.count - 1);
        if (s.count == 0) s = {};
        if (slot == -1)
            inventory.setOffhand(s);
        else
            inventory.setSlot(slot, s);
    }
    return one;
}
// What a fired arrow carries from its ammunition: a tipped arrow's potion, spectral glow.
void loadArrow(Projectile& p, const ItemStack& ammo) {
    static const ItemId tipped = *itemRegistry().find("tipped_arrow"),
                        spectral = *itemRegistry().find("spectral_arrow");
    if (ammo.item == tipped) p.potion = ammo.potion;
    p.spectral = ammo.item == spectral;
    p.stack = ammo; // (picked up as itself)
}
} // namespace

bool canDrawBow(const Inventory& inventory, bool survival) {
    return !survival || ammoSlot(inventory) != -2;
}
bool canLoadCrossbow(const Inventory& inventory, bool survival) {
    static const ItemId rocket = *itemRegistry().find("firework_rocket");
    return !survival || ammoSlot(inventory) != -2 ||
           (inventory.offhand().item == rocket && !inventory.offhand().empty());
}

bool releaseBow(Inventory& inventory, int ticks, bool survival, const glm::dvec3& eye,
                const glm::dvec3& look, Projectiles& projectiles, Xoroshiro& rng) {
    static const ItemId arrow = *itemRegistry().find("arrow");
    const float power = bowPower(ticks);
    const ItemStack bow = inventory.selectedStack();
    // Infinity: needs an arrow but doesn't use it up (wiki: Infinity) - plain arrows only.
    const bool infinity = enchantLevel(bow, Enchantment::Infinity) > 0;
    if (power < 0.1f || (survival && ammoSlot(inventory) == -2)) return false;
    const int slot = ammoSlot(inventory);
    const ItemStack peek = slot == -2   ? ItemStack{arrow, 1}
                           : slot == -1 ? inventory.offhand()
                                        : inventory.slot(slot);
    if (!projectiles.shoot(ProjectileKind::Arrow, eye, look, power * 3.0, 1.0, true, power >= 1.0f,
                           rng))
        return false;
    const ItemStack ammo =
        takeAmmo(inventory, survival, infinity && peek.item == arrow); // (only once it flew)
    Projectile& p = projectiles.last();
    loadArrow(p, ammo);
    p.power = static_cast<uint8_t>(enchantLevel(bow, Enchantment::Power));
    p.punch = static_cast<uint8_t>(enchantLevel(bow, Enchantment::Punch));
    p.flame = enchantLevel(bow, Enchantment::Flame) > 0;
    p.pickup = !(infinity && ammo.item == arrow) && survival;
    if (survival)
        inventory.setSlot(inventory.selected(), wearItem(inventory.selectedStack(), 1, rng));
    return true;
}

int crossbowChargeTicks(const ItemStack& crossbow) {
    return std::max(0, 25 - 5 * enchantLevel(crossbow, Enchantment::QuickCharge));
}

bool loadCrossbow(Inventory& inventory, bool survival) {
    static const ItemId tipped = *itemRegistry().find("tipped_arrow"),
                        spectral = *itemRegistry().find("spectral_arrow");
    static const ItemId rocket = *itemRegistry().find("firework_rocket");
    ItemStack bow = inventory.selectedStack();
    if (bow.state != 0) return false;
    if (inventory.offhand().item == rocket &&
        !inventory.offhand().empty()) { // (M28.4c) a rocket first
        bow.state = kCrossbowFirework;
        bow.extra = inventory.offhand().extra;
        if (survival) {
            ItemStack off = inventory.offhand();
            off.count = uint8_t(off.count - 1);
            inventory.setOffhand(off.count ? off : ItemStack{});
        }
        inventory.setSlot(inventory.selected(), bow);
        return true;
    }
    if (survival && ammoSlot(inventory) == -2) return false;
    const ItemStack ammo = takeAmmo(inventory, survival, false);
    bow.state = ammo.item == spectral ? kCrossbowSpectral
                : ammo.item == tipped ? kCrossbowTipped
                                      : kCrossbowArrow;
    bow.potion = ammo.item == tipped ? ammo.potion : 0;
    inventory.setSlot(inventory.selected(), bow);
    return true;
}

bool fireCrossbow(Inventory& inventory, bool survival, const glm::dvec3& eye,
                  const glm::dvec3& look, Projectiles& projectiles, Xoroshiro& rng) {
    static const ItemId arrowItem = *itemRegistry().find("arrow"),
                        tipped = *itemRegistry().find("tipped_arrow"),
                        spectralItem = *itemRegistry().find("spectral_arrow");
    ItemStack bow = inventory.selectedStack();
    if (bow.state == 0) return false;
    ItemStack ammo{bow.state == kCrossbowSpectral ? spectralItem
                   : bow.state == kCrossbowTipped ? tipped
                                                  : arrowItem,
                   1};
    ammo.potion = bow.state == kCrossbowTipped ? bow.potion : 0;
    const bool multishot = enchantLevel(bow, Enchantment::Multishot) > 0;
    const uint8_t pierce = static_cast<uint8_t>(enchantLevel(bow, Enchantment::Piercing));
    if (bow.state ==
        kCrossbowFirework) { // (M28.4c) rockets fly straight and burst on what they hit
        static const ItemId rocket = *itemRegistry().find("firework_rocket");
        ItemStack r{rocket, 1};
        r.extra = bow.extra;
        for (const float turn : {0.0f, -10.0f, 10.0f}) {
            if (turn != 0.0f && !multishot) break;
            const float a = glm::radians(turn);
            const glm::dvec3 dir(look.x * std::cos(a) - look.z * std::sin(a), look.y,
                                 look.x * std::sin(a) + look.z * std::cos(a));
            projectiles.launchFirework(eye, r, true, dir, rng);
        }
        bow.state = 0;
        bow.extra = 0;
        if (survival) bow = wearItem(bow, multishot ? 3 : 1, rng);
        inventory.setSlot(inventory.selected(), bow);
        return true;
    }
    for (const float turn : {0.0f, -10.0f, 10.0f}) {
        if (turn != 0.0f && !multishot) break;
        const float a = glm::radians(turn); // about the vertical, like vanilla's side shots
        const glm::dvec3 dir(look.x * std::cos(a) - look.z * std::sin(a), look.y,
                             look.x * std::sin(a) + look.z * std::cos(a));
        if (!projectiles.shoot(ProjectileKind::Arrow, eye, dir, 3.15, 1.0, true, false, rng))
            continue;
        Projectile& p = projectiles.last();
        loadArrow(p, ammo);
        p.pickup = turn == 0.0f && survival;
        p.pierce = pierce;
    }
    bow.state = 0;
    bow.potion = 0;
    if (survival) bow = wearItem(bow, multishot ? 3 : 1, rng); // (wiki: Multishot wears it 3)
    inventory.setSlot(inventory.selected(), bow);
    return true;
}

double releaseTrident(Inventory& inventory, int ticks, bool survival, bool wet,
                      const glm::dvec3& eye, const glm::dvec3& look, Projectiles& projectiles,
                      Xoroshiro& rng) {
    if (ticks < 10) return 0.0; // (held back at least half a second)
    ItemStack held = inventory.selectedStack();
    const int riptide = enchantLevel(held, Enchantment::Riptide);
    if (riptide > 0) {
        if (!wet) return 0.0;
        if (survival) inventory.setSlot(inventory.selected(), wearItem(held, 1, rng));
        return 3.0 * (1.0 + riptide) / 4.0; // (wiki: Riptide - the player flies along the look)
    }
    if (!projectiles.shoot(ProjectileKind::Trident, eye, look, 2.5, 1.0, true, false, rng))
        return 0.0;
    Projectile& p = projectiles.last();
    if (survival) {
        p.stack = wearItem(held, 1, rng);
        inventory.setSlot(inventory.selected(), {});
        if (p.stack.empty()) p.pickup = false; // (it broke with that throw)
    } else {
        p.stack = held;
        p.pickup = false; // (creative keeps its own; the copy just goes)
    }
    return 0.0;
}

void throwWindCharge(Inventory& inventory, bool survival, const glm::dvec3& eye,
                     const glm::dvec3& look, Projectiles& projectiles, Xoroshiro& rng) {
    if (projectiles.shoot(ProjectileKind::WindCharge, eye + look * 0.3, look, 1.5, 1.0, true, false,
                          rng) &&
        survival)
        inventory.consumeSelected(1);
}

void throwSnowball(Inventory& inventory, bool survival, const glm::dvec3& eye,
                   const glm::dvec3& look, Projectiles& projectiles, Xoroshiro& rng) {
    projectiles.shoot(ProjectileKind::Snowball, eye, look, 1.5, 1.0, true, false, rng);
    if (survival) inventory.consumeSelected(1);
}

void throwEgg(Inventory& inventory, bool survival, const glm::dvec3& eye, const glm::dvec3& look,
              Projectiles& projectiles, Xoroshiro& rng) {
    const std::string_view id = itemRegistry().item(inventory.selectedStack().item).id;
    if (projectiles.shoot(ProjectileKind::Egg, eye, look, 1.5, 1.0, true, false, rng))
        projectiles.last().eggVariant = id == "minecraft:brown_egg" ? 1 : id == "minecraft:blue_egg" ? 2 : 0;
    if (survival) inventory.consumeSelected(1);
}

void throwEye(Inventory& inventory, bool survival, const glm::dvec3& eye, glm::ivec2 stronghold,
              Projectiles& projectiles) {
    world::Xoroshiro unused(0);
    if (!projectiles.shoot(ProjectileKind::EyeOfEnder, eye, glm::dvec3(0, 1, 0), 0.0, 0.0, true,
                           false, unused))
        return;
    Projectile& p = projectiles.last();
    const glm::dvec2 to(stronghold.x + 0.5 - eye.x, stronghold.y + 0.5 - eye.z);
    const double dist = glm::length(to);
    const glm::dvec2 dir = dist > 1e-6 ? to / dist : glm::dvec2(0.0);
    const double reach = std::min(12.0, dist);
    p.target = glm::dvec3(eye.x + dir.x * reach, eye.y + (dist > 12.0 ? 8.0 : -4.0),
                          eye.z + dir.y * reach);
    p.pickup = false;
    if (survival) inventory.consumeSelected(1);
}

void throwPearl(Inventory& inventory, bool survival, const glm::dvec3& eye, float yaw, float pitch,
                Projectiles& projectiles, Xoroshiro& rng) {
    if (!projectiles.shoot(ProjectileKind::EnderPearl, eye, glm::dvec3(lookVector(yaw, pitch)), 1.5,
                           1.0, true, false, rng))
        return;
    projectiles.last().pickup = false;
    if (survival) inventory.consumeSelected(1);
}

void throwSplashPotion(Inventory& inventory, bool survival, const glm::dvec3& eye, float yaw,
                       float pitch, Projectiles& projectiles, Xoroshiro& rng) {
    const ItemStack held = inventory.selectedStack();
    const glm::dvec3 dir(lookVector(yaw, pitch - 20.0f));
    const bool lingering = itemRegistry().item(held.item).id == "minecraft:lingering_potion";
    if (!projectiles.shoot(lingering ? ProjectileKind::LingeringPotion
                                     : ProjectileKind::SplashPotion,
                           eye, dir, 0.5, 1.0, true, false, rng))
        return;
    projectiles.last().potion = held.potion;
    projectiles.last().pickup = false;
    if (survival) inventory.consumeSelected(1);
}

bool Projectiles::launchFirework(const glm::dvec3& at, const ItemStack& rocket, bool straight,
                                 const glm::dvec3& dir, Xoroshiro& rng) {
    const int flight = fireworks(rocket.extra).value_or(Fireworks{}).flight;
    if (!shoot(ProjectileKind::Firework, at, straight ? dir : glm::dvec3(0, 1, 0),
               straight ? 1.6 : 0.05, 0.0, true, false, rng))
        return false;
    Projectile& p = last();
    if (!straight) // (a little sideways drift: wiki)
        p.vel +=
            glm::dvec3((rng.nextDouble() - 0.5) * 0.002, 0.0, (rng.nextDouble() - 0.5) * 0.002);
    p.straight = straight;
    p.stack = rocket;
    p.stack.count = 1;
    p.pickup = false;
    p.fuse = int16_t(rocketLifetime(flight, rng.nextInt(6), rng.nextInt(7)));
    return true;
}

bool Projectiles::shoot(ProjectileKind kind, const glm::dvec3& from, const glm::dvec3& dir,
                        double speed, double inaccuracy, bool fromPlayer, bool critical,
                        Xoroshiro& rng, uint64_t owner) {
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
    p.owner = p.shooter = owner;
    m_items.push_back(p);
    return true;
}

Projectiles::Hits Projectiles::tick(World& world, Player& player, Vitals* vitals,
                                    Inventory& inventory, bool survival, Xoroshiro& rng) {
    Hits hits;
    m_chicks.clear();
    m_eyeDrops.clear();
    m_explosions.clear();
    m_witherBlasts.clear();
    m_windBursts.clear();
    m_fireworkBursts.clear();
    m_pearls.clear();
    m_channeled.clear();
    // Breath clouds: Instant Damage once a second to a survival player standing in one.
    // Lingering clouds (M28.4b; wiki: Lingering Potion): their effect, a quarter as long,
    // once a second, shrinking from 3 blocks to nothing over 30 s.
    for (size_t i = 0; i < m_clouds.size();) {
        BreathCloud& c = m_clouds[i];
        if (c.cooldown > 0) --c.cooldown;
        const glm::dvec3 feet = player.position();
        const double dx = feet.x - c.pos.x, dz = feet.z - c.pos.z;
        if (vitals && c.cooldown == 0 && dx * dx + dz * dz < double(c.radius) * c.radius &&
            feet.y > c.pos.y - 1.0 && feet.y < c.pos.y + 1.5) {
            if (c.potion == 0) {
                if (survival) {
                    vitals->addEffect(Effect::InstantDamage, 0, 1);
                    c.cooldown = 20;
                }
            } else if (const PotionInfo& info = potionInfo(static_cast<Potion>(c.potion));
                       info.effect != Effect::None) {
                const bool hurts =
                    info.effect == Effect::InstantDamage || info.effect == Effect::Poison;
                if (survival || !hurts) {
                    // Instant effects at half potency (wiki: Lingering Potion): ours, one level
                    // less.
                    const bool instant = effectInfo(info.effect).instant;
                    vitals->addEffect(info.effect,
                                      instant ? std::max(0, int(info.amplifier) - 1)
                                              : info.amplifier,
                                      instant ? 1 : std::max(1, info.duration / 4));
                    if (info.effect2 != Effect::None) // (M29.2a: Turtle Master)
                        vitals->addEffect(info.effect2, info.amplifier2, std::max(1, info.duration / 4));
                    c.radius -= 0.5f; // (each use takes some of it - wiki: 0.5 radius and 5 s)
                    c.ticks -= 100;
                }
                c.cooldown = 20;
            }
        }
        c.radius -= c.shrink;
        if (--c.ticks <= 0 || c.radius < 0.5f) {
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
        const BlockPos cell{int(std::floor(p.pos.x)), int(std::floor(p.pos.y)),
                            int(std::floor(p.pos.z))};
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
        } else if (p.kind == ProjectileKind::Firework) {
            // (M28.4c; wiki: Firework Rocket) rises faster and faster - a crossbow's flies
            // straight - and bursts when its time is up or it hits something.
            if (!p.straight) {
                p.vel.x *= 1.15;
                p.vel.z *= 1.15;
                p.vel.y += 0.04;
            }
            const double speed = glm::length(p.vel);
            const glm::dvec3 dir = speed > 1e-9 ? p.vel / speed : glm::dvec3(0, 1, 0);
            p.facing = dir;
            bool burst = p.life >= p.fuse;
            if (speed > 1e-9 && raycastBlocks(world, p.pos, dir, speed)) burst = true;
            if (p.straight && Mobs::raycast(world, p.pos, dir, speed, p.owner)) burst = true;
            if (burst) {
                if (m_fireworkBursts.size() < m_fireworkBursts.capacity())
                    m_fireworkBursts.push_back({p.pos, p.stack.extra});
                const int stars = fireworks(p.stack.extra).value_or(Fireworks{}).count;
                if (stars > 0) { // (rockets with stars hurt what's around: wiki)
                    const double dp =
                        glm::length(player.position() + glm::dvec3(0.0, 0.9, 0.0) - p.pos);
                    if (vitals && survival)
                        if (const float d = fireworkDamage(stars, dp); d > 0.0f)
                            vitals->attacked(d, &p.pos, Vitals::Hit::Explosion);
                    const ChunkPos c0{blockToChunk(int(std::floor(p.pos.x))),
                                      blockToChunk(int(std::floor(p.pos.z)))};
                    for (int dz = -1; dz <= 1; ++dz)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (Chunk* ch = world.chunk({c0.x + dx, c0.z + dz}))
                                for (MobData& m : ch->mobs()) {
                                    if (m.health <= 0.0f || isHanging(m.type)) continue;
                                    const Aabb mb = Mobs::box(m);
                                    const float d = fireworkDamage(
                                        stars, glm::length((mb.min + mb.max) * 0.5 - p.pos));
                                    if (d > 0.0f && m.hurtTime == 0) {
                                        m.health -= d;
                                        m.hurtTime = 10;
                                        if (p.fromPlayer) m.lastHurtByPlayer = true;
                                    }
                                }
                }
                remove = true;
            } else {
                p.pos += p.vel;
            }
        } else if (p.kind == ProjectileKind::EyeOfEnder) {
            // Glides toward its target (through blocks), then comes down (wiki).
            p.vel = (p.target - p.pos) * 0.06;
            p.pos += p.vel;
            p.facing = p.vel;
            if (p.life >= 80) {
                if (rng.nextInt(5) != 0) m_eyeDrops.push_back(p.pos);
                remove = true;
            }
        } else if (p.kind == ProjectileKind::Trident && p.fromPlayer && (p.stuck || p.dealt) &&
                   enchantLevel(p.stack, Enchantment::Loyalty) > 0) {
            // Loyalty (wiki): once it has hit something it flies back to the thrower,
            // through blocks, faster for higher levels; caught, it goes back in the pack.
            const glm::dvec3 to = player.eyePosition(1.0) - glm::dvec3(0.0, 0.4, 0.0) - p.pos;
            const double len = glm::length(to);
            const int level = enchantLevel(p.stack, Enchantment::Loyalty);
            p.stuck = false;
            p.vel = p.vel * 0.95 + (len > 1e-6 ? to / len : glm::dvec3(0.0)) * (0.05 * level);
            p.facing = -p.vel;
            p.pos += p.vel;
            if (len < 1.5) {
                if (!survival || !p.pickup || inventory.add(p.stack) == 0) remove = true;
            }
            if (p.life > 2400) remove = true;
        } else if (p.stuck) {
            // Stuck tridents: back into the pack of the player who threw them (M25.3).
            if (p.kind == ProjectileKind::Trident) {
                if (p.fromPlayer &&
                    player.box().intersects(Aabb{p.pos - glm::dvec3(1.0), p.pos + glm::dvec3(1.0)}))
                    if (!survival || !p.pickup || inventory.add(p.stack) == 0) remove = true;
                if ((!p.fromPlayer && p.life > 1200) ||
                    blockRegistry().blockOf(world.getBlock(cell)) == 0) {
                    p.stuck = false; // (its block gone: it falls)
                    if (!p.fromPlayer) remove = true;
                }
                if (remove) {
                    m_items[i] = m_items.back();
                    m_items.pop_back();
                } else {
                    ++i;
                }
                continue;
            }
            // Stuck arrows: picked up by a survival player who shot them (wiki: Arrow).
            if (p.fromPlayer && p.pickup &&
                player.box().intersects(Aabb{p.pos - glm::dvec3(1.0), p.pos + glm::dvec3(1.0)})) {
                if (!survival ||
                    inventory.add(p.stack.empty() ? ItemStack{arrowItem, 1} : p.stack) == 0)
                    remove = true;
            }
            if (p.life > 1200 || blockRegistry().blockOf(world.getBlock(cell)) == 0)
                remove = true; // its block gone
        } else {
            const double speed = glm::length(p.vel);
            const glm::dvec3 dir = speed > 1e-9 ? p.vel / speed : glm::dvec3(0, -1, 0);
            p.facing = dir;
            const auto block =
                speed > 1e-9 ? raycastBlocks(world, p.pos, dir, speed) : std::nullopt;
            double reach = block ? block->distance : speed;
            // Entities in the way, nearer than the block.
            enum class Target { None, Player, Mob } target = Target::None;
            bool pierced = false; // (M28.4a) went on through a mob
            Mobs::MobHit mob{};
            if (const auto mh =
                    p.dealt ? std::nullopt : Mobs::raycast(world, p.pos, dir, reach, p.owner)) {
                mob = *mh;
                reach = mh->distance;
                target = Target::Mob;
            }
            if (!(p.fromPlayer && p.life < 5) &&
                !p.dealt) { // (doesn't hit its shooter as it leaves)
                const double t = enter(p.pos, dir, reach, player.box().inflated(0.3));
                if (t >= 0.0) {
                    reach = t;
                    target = Target::Player;
                }
            }
            const bool fireball =
                p.kind == ProjectileKind::GhastFireball || p.kind == ProjectileKind::BlazeFireball;
            if (p.kind == ProjectileKind::EnderPearl && (target != Target::None || block)) {
                PearlLanding l;
                l.pos = p.pos + dir * std::max(0.0, reach - 0.3); // (just short of what it hit)
                if (block && target == Target::None &&
                    blockRegistry().blockOf(world.getBlock(block->block)) == blocks::EndGateway) {
                    l.gateway = true;
                    l.gatewayBlock = block->block;
                }
                if (m_pearls.size() < m_pearls.capacity()) m_pearls.push_back(l);
                // (M29.1c; wiki: Endermite) 1 in 20 thrown pearls leaves an endermite behind.
                if (rng.nextInt(20) == 0) world.queueMob(Mobs::make(MobType::Endermite, l.pos, rng));
                remove = true;
            } else if (p.kind == ProjectileKind::DragonFireball &&
                       (target != Target::None || block)) {
                // Its breath lingers where it burst, on the floor below (wiki: Dragon Fireball).
                glm::dvec3 at = p.pos + dir * reach;
                for (int k = 0; k < 8 && !blockRegistry().collides(world.getBlock(
                                             {int(std::floor(at.x)), int(std::floor(at.y - 0.5)),
                                              int(std::floor(at.z))}));
                     ++k)
                    at.y -= 1.0;
                addCloud(at, 3.0f, 600);
                remove = true;
            } else if (p.kind == ProjectileKind::LingeringPotion &&
                       (target != Target::None || block)) {
                // (M28.4b) a cloud of its effect on the floor where it broke: 3 blocks, 30 s
                glm::dvec3 at = p.pos + dir * reach;
                world.levelEvent(LevelEvent::Type::PotionSplash, at.x, at.y, at.z,
                                 potionColour(static_cast<Potion>(p.potion)));
                for (int k = 0; k < 8 && !blockRegistry().collides(world.getBlock(
                                             {int(std::floor(at.x)), int(std::floor(at.y - 0.5)),
                                              int(std::floor(at.z))}));
                     ++k)
                    at.y -= 1.0;
                addCloud(at, 3.0f, 600, p.potion, 3.0f / 600.0f);
                remove = true;
            } else if (p.kind == ProjectileKind::SplashPotion &&
                       (target != Target::None || block)) {
                // Splash (wiki: Splash Potion): entities whose hitbox touches an
                // 8.25 x 4.25 x 8.25 box around the impact and whose nearest point is
                // within 4 blocks get the effect, scaled by 1 - distance / 4 (a direct hit:
                // full); healing and harming swap on the undead (wiki: Undead).
                const glm::dvec3 at = p.pos + dir * reach;
                world.levelEvent(LevelEvent::Type::PotionSplash, at.x, at.y, at.z,
                                 potionColour(static_cast<Potion>(p.potion)));
                const Aabb area{at - glm::dvec3(4.125, 2.125, 4.125),
                                at + glm::dvec3(4.125, 2.125, 4.125)};
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
                const MobData* direct = target == Target::Mob
                                            ? &world.chunk(mob.chunk)->mobs()[size_t(mob.index)]
                                            : nullptr;
                if (info.effect != Effect::None) {
                    const double sp = scaleFor(player.box(), target == Target::Player);
                    const bool hurtsPlayer =
                        info.effect == Effect::InstantDamage || info.effect == Effect::Poison;
                    if (sp > 0.0 && vitals &&
                        (survival || !hurtsPlayer)) { // (creative takes no harm)
                        const int duration = int(info.duration * sp + 0.5);
                        if (effectInfo(info.effect).instant ||
                            duration > 20) // (1 s or less: dropped)
                            vitals->addEffect(info.effect, info.amplifier, duration, sp);
                        if (info.effect2 != Effect::None && duration > 20) // (M29.2a)
                            vitals->addEffect(info.effect2, info.amplifier2, duration, sp);
                    }
                }
                // Weakness on zombie villagers (M24.3: the first half of curing them).
                if (info.effect == Effect::Weakness) {
                    const ChunkPos c0{blockToChunk(int(std::floor(at.x))),
                                      blockToChunk(int(std::floor(at.z)))};
                    for (int dz = -1; dz <= 1; ++dz)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (Chunk* ch = world.chunk({c0.x + dx, c0.z + dz}))
                                for (MobData& m : ch->mobs())
                                    if (m.type == MobType::ZombieVillager && m.health > 0.0f) {
                                        const double sm = scaleFor(Mobs::box(m), &m == direct);
                                        if (sm > 0.0)
                                            m.weaknessTicks = int16_t(std::max<int>(
                                                m.weaknessTicks, int(info.duration * sm)));
                                    }
                }
                // Mobs: instant health/damage (other effects reach only the player, our
                // simplification); water hurts blazes, endermen and striders by 1.
                if (info.effect == Effect::InstantHealth || info.effect == Effect::InstantDamage ||
                    water) {
                    const ChunkPos c0{blockToChunk(int(std::floor(at.x))),
                                      blockToChunk(int(std::floor(at.z)))};
                    for (int dz = -1; dz <= 1; ++dz)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (Chunk* ch = world.chunk({c0.x + dx, c0.z + dz}))
                                for (MobData& m : ch->mobs()) {
                                    const double sm = scaleFor(Mobs::box(m), &m == direct);
                                    if (sm <= 0.0 || m.health <= 0.0f) continue;
                                    // (the dragon and crystals take no effects)
                                    if (m.type == MobType::EnderDragon ||
                                        m.type == MobType::EndCrystal)
                                        continue;
                                    float amount = 0.0f;
                                    bool harm = true;
                                    if (water) {
                                        if (m.type != MobType::Blaze &&
                                            m.type != MobType::Enderman &&
                                            m.type != MobType::Strider)
                                            continue;
                                        amount = 1.0f;
                                    } else {
                                        const bool undead = isUndead(m.type);
                                        harm = (info.effect == Effect::InstantDamage) != undead;
                                        amount =
                                            float((harm ? 6 : 4) << info.amplifier) * float(sm);
                                    }
                                    if (harm) {
                                        m.health -= amount;
                                        m.hurtTime = 10;
                                        if (p.fromPlayer) m.lastHurtByPlayer = true;
                                        m.lastHurtBySkeleton = p.skeleton;
                                    } else {
                                        m.health =
                                            std::min(mobInfo(m.type).maxHealth, m.health + amount);
                                    }
                                }
                }
                if (water) { // puts out fire in the cell it broke in and the 4 beside it
                    const BlockPos c = block && target == Target::None
                                           ? BlockPos{block->block.x + normal(block->face).x,
                                                      block->block.y + normal(block->face).y,
                                                      block->block.z + normal(block->face).z}
                                           : BlockPos{int(std::floor(at.x)), int(std::floor(at.y)),
                                                      int(std::floor(at.z))};
                    const BlockPos cells[5] = {c,
                                               {c.x + 1, c.y, c.z},
                                               {c.x - 1, c.y, c.z},
                                               {c.x, c.y, c.z + 1},
                                               {c.x, c.y, c.z - 1}};
                    for (const BlockPos& f : cells)
                        if (world.isInHeight(f.y) &&
                            blockRegistry().blockOf(world.getBlock(f)) == blocks::Fire) {
                            world.updateBlock(f, 0);
                            if (m_edits.size() < m_edits.capacity()) m_edits.push_back(f);
                        }
                }
                remove = true;
            } else if (fireball && (target != Target::None || block)) {
                const glm::dvec3 at = p.pos + dir * reach;
                if (target == Target::Player && vitals && survival) {
                    const float damage =
                        p.kind == ProjectileKind::GhastFireball ? 6.0f : 5.0f; // (wiki)
                    if (vitals->attacked(damage, &p.pos, Vitals::Hit::Fire) &&
                        p.kind == ProjectileKind::BlazeFireball)
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
                    if (world.isInHeight(front.y) && world.getBlock(front) == 0 &&
                        BlockUpdates::fireCanStay(world, front)) {
                        world.updateBlock(front, BlockUpdates::fireState(0));
                        if (m_edits.size() < m_edits.capacity()) m_edits.push_back(front);
                    }
                }
                remove = true;
            } else if (p.kind == ProjectileKind::WindCharge && (target != Target::None || block)) {
                // A wind charge (M26.4c; wiki: Wind Charge): 1 damage to what it hits, then
                // its burst (main: knockback, no harm to blocks).
                if (target == Target::Player) {
                    if (vitals && survival && !p.fromPlayer &&
                        vitals->attacked(1.0f, &p.pos, Vitals::Hit::Projectile))
                        hits.playerDamage += 1.0f;
                } else if (target == Target::Mob) {
                    MobData& m = world.chunk(mob.chunk)->mobs()[size_t(mob.index)];
                    if (m.hurtTime == 0 && m.type != MobType::Breeze) {
                        m.health -= 1.0f;
                        m.hurtTime = 10;
                        if (p.fromPlayer) m.lastHurtByPlayer = true;
                        ++hits.mobsHit;
                    }
                }
                const glm::dvec3 at = block && target == Target::None
                                          ? p.pos + dir * std::max(0.0, block->distance - 0.1)
                                          : p.pos + dir * reach;
                if (m_windBursts.size() < m_windBursts.capacity())
                    m_windBursts.push_back({at, p.fromPlayer});
                remove = true;
            } else if (p.kind == ProjectileKind::WitherSkull && (target != Target::None || block)) {
                // A wither skull (M26.4b; wiki: Wither › Wither skulls): 8 damage and
                // Wither II for 10 s on Normal (40 s on Hard) to what it hits, then a power-1
                // blast.
                if (target == Target::Player) {
                    if (vitals && survival &&
                        vitals->attacked(8.0f, &p.pos, Vitals::Hit::Projectile)) {
                        // Wither II 10 s on Normal, 40 s on Hard, none on Easy (wiki: Wither ›
                        // Wither skull).
                        if (vitals->difficulty() >= 2)
                            vitals->addEffect(Effect::Wither, 1,
                                              vitals->difficulty() == 3 ? 800 : 200);
                        hits.playerDamage += 8.0f;
                    }
                } else if (target == Target::Mob) {
                    MobData& m = world.chunk(mob.chunk)->mobs()[size_t(mob.index)];
                    if (m.hurtTime == 0 && m.type != MobType::Wither) {
                        m.health -= 8.0f;
                        m.hurtTime = 10;
                        ++hits.mobsHit;
                    }
                }
                if (m_witherBlasts.size() < m_witherBlasts.capacity())
                    m_witherBlasts.push_back(p.pos + dir * reach);
                remove = true;
            } else if (p.kind == ProjectileKind::LlamaSpit && (target != Target::None || block)) {
                // A llama's spit (M26.2; wiki: Llama): 1 damage, gone on whatever it hits.
                if (target == Target::Player) {
                    if (vitals && survival &&
                        vitals->attacked(1.0f, &p.pos, Vitals::Hit::Projectile))
                        hits.playerDamage += 1.0f;
                } else if (target == Target::Mob) {
                    MobData& m = world.chunk(mob.chunk)->mobs()[size_t(mob.index)];
                    if (m.hurtTime == 0) {
                        m.health -= 1.0f;
                        m.hurtTime = 10;
                        if (!mobInfo(m.type).hostile) m.panicTicks = 100; // (wolves run off)
                        ++hits.mobsHit;
                    }
                }
                remove = true;
            } else if (target != Target::None && p.kind == ProjectileKind::Trident) {
                // A trident (M25.3; wiki: Trident): 8 damage, Impaling +2.5 a level on
                // water mobs; Channeling calls lightning on a mob in a thunderstorm where
                // the sky is open. Then it drops away (and Loyalty brings it back).
                float damage = 8.0f;
                if (target == Target::Player) {
                    if (vitals && survival &&
                        vitals->attacked(damage, &p.pos, Vitals::Hit::Projectile)) {
                        player.knockback(p.vel.x, p.vel.z, 0.3);
                        hits.playerDamage += damage;
                    }
                } else {
                    MobData& m = world.chunk(mob.chunk)->mobs()[size_t(mob.index)];
                    if (mobInfo(m.type).swims || m.type == MobType::Turtle)
                        damage += 2.5f * float(enchantLevel(p.stack, Enchantment::Impaling));
                    if (m.hurtTime == 0 && m.type != MobType::Enderman) {
                        if (p.fromPlayer) m.lastHurtByPlayer = true;
                        m.health -= m.type == MobType::EnderDragon
                                        ? Mobs::dragonDamage(m, damage, p.pos + dir * reach)
                                        : damage;
                        m.hurtTime = 10;
                        ++hits.mobsHit;
                    }
                    if (m_thundering && enchantLevel(p.stack, Enchantment::Channeling) > 0 &&
                        m_channeled.size() < m_channeled.capacity()) {
                        const BlockPos at{int(std::floor(m.pos.x)), int(std::floor(m.pos.y)),
                                          int(std::floor(m.pos.z))};
                        const Chunk* c = world.chunk(at.chunk());
                        if (c && c->lit() &&
                            c->skyLight(blockToLocal(at.x), at.y + 1, blockToLocal(at.z)) == 15)
                            m_channeled.push_back(at);
                    }
                }
                world.playSound(Sound::ArrowHit, p.pos.x, p.pos.y, p.pos.z);
                p.pos += dir * std::max(0.0, reach - 0.3);
                p.vel = glm::dvec3(-p.vel.x * 0.01, -0.1, -p.vel.z * 0.01);
                p.dealt = true;
            } else if (target != Target::None) {
                if (p.kind == ProjectileKind::Arrow) {
                    world.playSound(Sound::ArrowHit, p.pos.x, p.pos.y, p.pos.z);
                    // Base damage 2, Power adds 0.5 per level + 0.5 (wiki: Power).
                    const double base = 2.0 + (p.power ? 0.5 * p.power + 0.5 : 0.0);
                    float damage = float(std::ceil(speed * base));
                    if (p.critical) {
                        damage += float(rng.nextInt(uint32_t(damage / 2.0f + 2.0f)));
                        world.levelEvent(LevelEvent::Type::Crit, p.pos.x, p.pos.y, p.pos.z);
                    }
                    if (target == Target::Player) {
                        if (vitals && survival &&
                            vitals->attacked(damage, &p.pos, Vitals::Hit::Projectile)) {
                            player.knockback(p.vel.x, p.vel.z, 0.6 * 0.5); // wiki: Arrow knockback
                            hits.playerDamage += damage;
                            // (M28.4b) a tipped arrow's effect, an eighth as long; spectral:
                            // Glowing 10 s
                            if (p.potion)
                                if (const PotionInfo& info =
                                        potionInfo(static_cast<Potion>(p.potion));
                                    info.effect != Effect::None)
                                    vitals->addEffect(info.effect, info.amplifier,
                                                      effectInfo(info.effect).instant
                                                          ? 1
                                                          : std::max(1, info.duration / 8));
                            if (p.potion && potionInfo(static_cast<Potion>(p.potion)).effect2 != Effect::None) {
                                const PotionInfo& info2 = potionInfo(static_cast<Potion>(p.potion));
                                vitals->addEffect(info2.effect2, info2.amplifier2, std::max(1, info2.duration / 8));
                            }
                            if (p.spectral) vitals->addEffect(Effect::Glowing, 0, 200);
                            if (p.hitEffect.effect != Effect::None)
                                vitals->addEffect(p.hitEffect.effect, 0, p.hitEffect.ticks);
                        }
                    } else {
                        MobData& m = world.chunk(mob.chunk)->mobs()[size_t(mob.index)];
                        const bool perchedDragon =
                            (m.type == MobType::EnderDragon && (m.phase == 5 || m.phase == 6)) ||
                            (m.type == MobType::Shulker && m.peek == 0); // (closed shells too)
                        if (m.type == MobType::Breeze) {
                            // (M26.4c) arrows glance off a breeze (wiki: it deflects projectiles)
                        } else if (m.type == MobType::Wither &&
                                   (m.health < mobInfo(m.type).maxHealth * 0.5f ||
                                    m.spellTicks > 0)) {
                            // (M26.4b) below half health its armour turns arrows (wiki: Wither)
                        } else if (m.type == MobType::Enderman) {
                            m.wantsTeleport =
                                true; // arrows can't hurt endermen: they teleport away (wiki)
                        } else if (perchedDragon) {
                            // A perched dragon shrugs arrows off (wiki: Ender Dragon).
                        } else if (m.hurtTime == 0) {
                            if (p.fromPlayer) m.lastHurtByPlayer = true;
                            m.lastHurtBySkeleton = p.skeleton;
                            m.health -=
                                m.type == MobType::EnderDragon
                                    ? Mobs::dragonDamage(m, float(damage), p.pos + dir * reach)
                                    : float(damage);
                            m.hurtTime = 10;
                            const glm::dvec2 h(p.vel.x, p.vel.z);
                            if (glm::length(h) > 1e-6)
                                m.vel += glm::dvec3(h.x, 0, h.y) / glm::length(h) *
                                         (0.3 + 0.6 * p.punch); // Punch
                            if (p.flame)
                                m.fireTicks = std::max<int16_t>(m.fireTicks, 100); // Flame: 5 s
                            if (p.potion) { // (M28.4b) a tipped arrow's instant effects on mobs
                                            // (healing hurts the undead)
                                const PotionInfo& info = potionInfo(static_cast<Potion>(p.potion));
                                const bool undead = isUndead(m.type);
                                const float amount = float(1 << info.amplifier);
                                if ((info.effect == Effect::InstantDamage) != undead &&
                                    (info.effect == Effect::InstantDamage ||
                                     info.effect == Effect::InstantHealth))
                                    m.health -= 6.0f * amount;
                                else if (info.effect == Effect::InstantDamage ||
                                         info.effect == Effect::InstantHealth)
                                    m.health = std::min(maxHealthOf(m), m.health + 4.0f * amount);
                            }
                            ++hits.mobsHit;
                        }
                        if (p.pierce >
                            0) { // (M28.4a) Piercing: on through it, never hitting it again
                            --p.pierce;
                            p.owner = m.uuidHi;
                            p.pos += dir * (reach + 0.05);
                            pierced = true;
                        }
                    }
                } else if (target == Target::Mob) {
                    ++hits.mobsHit; // eggs only knock (no damage)
                    MobData& m = world.chunk(mob.chunk)->mobs()[size_t(mob.index)];
                    if (p.kind == ProjectileKind::Snowball && m.type == MobType::Blaze &&
                        m.hurtTime == 0) { // (M26.5b)
                        m.health -= 3.0f;
                        m.hurtTime = 10;
                    }
                    const glm::dvec2 h(p.vel.x, p.vel.z);
                    if (p.kind == ProjectileKind::Snowball && glm::length(h) > 1e-6)
                        m.vel += glm::dvec3(h.x, 0.0, h.y) / glm::length(h) * 0.3;
                } else if (target == Target::Player && p.kind == ProjectileKind::Snowball &&
                           !p.fromPlayer) {
                    player.knockback(p.vel.x, p.vel.z, 0.3);
                }
                if (p.kind == ProjectileKind::Egg) m_chicks.push_back({p.pos + dir * reach, p.eggVariant});
                remove = !pierced;
            } else if (block) {
                if (p.kind == ProjectileKind::Arrow ||
                    p.kind == ProjectileKind::Trident) { // sticks just inside the face it hit
                    p.pos += dir * (block->distance + 0.05);
                    world.playSound(Sound::ArrowHit, p.pos.x, p.pos.y, p.pos.z);
                    p.vel = glm::dvec3(0.0);
                    p.stuck = true;
                    p.life = 0;
                } else {
                    if (p.kind == ProjectileKind::Egg)
                        m_chicks.push_back({p.pos + dir * block->distance, p.eggVariant}); // (snowballs just break)
                    remove = true;
                }
            } else {
                p.pos += p.vel;
                const bool inWater = blockRegistry().blockOf(world.getBlock(cell)) == blocks::Water;
                if (p.kind != ProjectileKind::GhastFireball &&
                    p.kind != ProjectileKind::BlazeFireball &&
                    p.kind != ProjectileKind::DragonFireball &&
                    p.kind != ProjectileKind::WitherSkull &&
                    p.kind !=
                        ProjectileKind::WindCharge) { // (fireballs and wind charges fly straight)
                    const double drag = inWater && p.kind != ProjectileKind::Trident
                                            ? 0.6
                                            : 0.99; // (tridents keep going in water)
                    p.vel *= drag;
                    p.vel.y -= p.kind == ProjectileKind::Arrow ||
                                       p.kind == ProjectileKind::SplashPotion ||
                                       p.kind == ProjectileKind::LingeringPotion ||
                                       p.kind == ProjectileKind::Trident
                                   ? 0.05
                                   : 0.03; // (pearls 0.03)
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
    for (const auto& [at, variant] : m_chicks) {
        if (rng.nextInt(8) != 0) continue;
        const int n = rng.nextInt(32) == 0 ? 4 : 1;
        for (int k = 0; k < n; ++k) {
            MobData chick = Mobs::make(MobType::Chicken, at, rng);
            chick.age = -24000;
            chick.woolColour = variant; // (M29.1d) the egg's kind
            chick.color2 = 1;
            Mobs::add(world, chick);
        }
    }
    return hits;
}

} // namespace mc
