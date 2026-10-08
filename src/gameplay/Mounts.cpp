// Mounts (M26.2; wiki: Horse, Donkey, Mule, Llama, Trader Llama, Camel, Taming). Part of
// Mobs.
//
// How vanilla's mounts work:
// - Each horse, donkey, mule and llama has its own stats (attributes): top health 15-30,
//   speed (horses 0.1125-0.3375, the others 0.175) and jump strength (horses 0.4-1.0,
//   donkeys and mules 0.5). A foal's stat is its parents' average plus a random spread
//   that grows with how far apart they are, kept inside the range.
// - Taming: get on with an empty hand. A wild one bucks for a while, then compares a
//   roll of 0-99 (llamas 0-29) with its temper: below it, it is tamed; else it throws
//   the rider and its temper rises by 5. Food raises temper too.
// - A tamed horse, donkey or mule wears a saddle to be steered; horses take horse armor,
//   llamas a carpet; donkeys, mules and llamas a chest. Llamas are never steered.
// - Riding: the mount faces where the rider looks; forward runs at its speed (backward a
//   quarter, sideways half); holding jump charges the jump bar, letting go jumps (a full
//   bar jumps with all its jump strength). Camels need no taming: their jump is a dash
//   (up to 12 blocks, every 2.75 s), sprinting doubles their pace, and they sit down now
//   and then.
// - Llamas spit (1 damage) at a player who hits them and at wolves; trader llamas walk
//   with a wandering trader (vanilla: on its leads) and leave with it.
#include "gameplay/Mobs.h"

#include "gameplay/Projectiles.h"
#include "world/Blocks.h"
#include "world/Items.h"
#include "world/Rotation.h"
#include "world/Sounds.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace mc {

using namespace world;

namespace {

struct MountFood {
    const char* id;
    float heal;
    int grow;   // ticks a foal grows
    int temper; // temper gained while wild
    bool love;  // puts a tamed adult in love
};
// wiki: Horse › Food (sugar, wheat, apples, carrots, golden carrots and apples, hay
// bales); Llama (wheat, hay bales); Camel (cactus).
constexpr MountFood kHorseFood[] = {
    {"sugar", 1.0f, 600, 3, false},         {"wheat", 2.0f, 400, 3, false},
    {"apple", 3.0f, 1200, 3, false},        {"carrot", 3.0f, 1200, 0, false},
    {"golden_carrot", 4.0f, 1200, 5, true}, {"golden_apple", 10.0f, 4800, 10, true},
    {"hay_block", 20.0f, 3600, 0, false}};
constexpr MountFood kLlamaFood[] = {{"wheat", 2.0f, 200, 3, false},
                                    {"hay_block", 10.0f, 1800, 6, true}};
constexpr MountFood kCamelFood[] = {{"cactus", 2.0f, 200, 0, true}};

const MountFood* mountFood(MobType t, std::string_view id) {
    auto find = [&](const auto& table) -> const MountFood* {
        for (const MountFood& f : table)
            if (id.size() > 10 && id.substr(10) == f.id) return &f;
        return nullptr;
    };
    if (isHorseKind(t)) return find(kHorseFood);
    if (isLlama(t)) return find(kLlamaFood);
    if (t == MobType::Camel) return find(kCamelFood);
    return nullptr;
}

double attrSpeed(const MobData& m) {
    return m.moveSpeed > 0.0f ? m.moveSpeed : mobInfo(m.type).speed;
}
double attrJump(const MobData& m) { return m.jumpStrength > 0.0f ? m.jumpStrength : 0.5; }
int maxTemper(MobType t) { return isLlama(t) ? 30 : 100; }
// (three rolls averaged: the middle of the range is the most likely)
double tri(Xoroshiro& rng) {
    return (rng.nextDouble() + rng.nextDouble() + rng.nextDouble()) / 3.0;
}

void dropNamed(ItemEntities& items, const glm::dvec3& at, const std::string& name, Xoroshiro& rng) {
    if (const auto id = itemRegistry().find(name)) items.spawn(at, {*id, 1}, rng);
}

bool solid(const World& w, int x, int y, int z) {
    return blockRegistry().collides(w.getBlock({x, y, z}));
}

float approachAngle(float from, float to, float maxStep) { // (as in Mobs.cpp)
    float d = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    d = std::clamp(d, -maxStep, maxStep);
    return from + d;
}

} // namespace

bool Mobs::isMountFood(MobType type, ItemId item) {
    if (item == kNoItem) return false;
    const MountFood* f = mountFood(type, itemRegistry().item(item).id);
    return f && f->love && type != MobType::Mule; // (what tempts them: their breeding food)
}

void Mobs::initMount(MobData& m, Xoroshiro& rng) {
    // Spawn stats (wiki: Horse › Statistics): health 15 + 0-7 + 0-8; speed and jump
    // strength from three averaged rolls.
    if (m.type == MobType::Camel || m.type == MobType::Nautilus || m.type == MobType::HappyGhast)
        return; // (fixed stats)
    m.maxHealth = 15.0f + float(rng.nextInt(8) + rng.nextInt(9));
    m.health = m.maxHealth;
    if (m.type == MobType::Horse) {
        m.moveSpeed = float((0.45 + 0.9 * tri(rng)) * 0.25);
        m.jumpStrength = float(0.4 + 0.6 * tri(rng));
        m.woolColour = uint8_t(rng.nextInt(7));
        m.color2 = uint8_t(rng.nextInt(5));
    } else if (isHorseKind(m.type)) {
        m.jumpStrength = 0.5f;
    } else { // llamas: strength 1-3 almost always, 4 and 5 rarely (wiki: 32.8% / 0.8%)
        const uint32_t r = rng.nextInt(1000);
        m.strength = uint8_t(r < 328 ? 1 : r < 656 ? 2 : r < 984 ? 3 : r < 992 ? 4 : 5);
        m.woolColour = uint8_t(rng.nextInt(4));
    }
}

void Mobs::mountOffspring(const MobData& a, const MobData& b, MobData& baby, Xoroshiro& rng) {
    // A horse and a donkey have a mule (wiki: Mule).
    if ((a.type == MobType::Horse && b.type == MobType::Donkey) ||
        (a.type == MobType::Donkey && b.type == MobType::Horse))
        baby.type = MobType::Mule;
    if (baby.type == MobType::Camel || baby.type == MobType::Nautilus ||
        baby.type == MobType::HappyGhast)
        return;
    // A foal's stat: the parents' average + (their difference + 30% of the range) x a
    // centred random factor, reflected back inside the range (wiki: Horse › Breeding).
    auto stat = [&](double x, double y, double lo, double hi) {
        double v = (x + y) * 0.5 + (std::abs(x - y) + (hi - lo) * 0.3) * (tri(rng) - 0.5);
        if (v > hi) v = 2.0 * hi - v;
        if (v < lo) v = 2.0 * lo - v;
        return std::clamp(v, lo, hi);
    };
    auto hp = [](const MobData& m) { return double(maxHealthOf(m)); };
    baby.maxHealth = float(stat(hp(a), hp(b), 15.0, 30.0));
    baby.health = baby.maxHealth;
    if (isHorseKind(baby.type)) {
        baby.moveSpeed = float(stat(attrSpeed(a), attrSpeed(b), 0.1125, 0.3375));
        baby.jumpStrength = float(stat(attrJump(a), attrJump(b), 0.4, 1.0));
        if (baby.type == MobType::Donkey) { // (donkeys keep theirs)
            baby.moveSpeed = 0.175f;
            baby.jumpStrength = 0.5f;
        }
    }
    if (baby.type == MobType::Horse) { // coat: 4/9 each parent's, else random; markings 2/5 each
        const uint32_t c = rng.nextInt(9), k = rng.nextInt(5);
        baby.woolColour = c < 4 ? a.woolColour : c < 8 ? b.woolColour : uint8_t(rng.nextInt(7));
        baby.color2 = k < 2 ? a.color2 : k < 4 ? b.color2 : uint8_t(rng.nextInt(5));
    }
    if (isLlama(baby.type)) { // strength 1..the stronger parent's, 3% one more (wiki: Llama)
        baby.type = MobType::Llama;
        baby.woolColour = rng.nextInt(2) ? a.woolColour : b.woolColour;
        baby.strength = uint8_t(1 + rng.nextInt(std::max(a.strength, b.strength)));
        if (rng.nextInt(100) < 3) baby.strength = uint8_t(std::min(5, baby.strength + 1));
    }
    baby.horseArmor = baby.decor = 0;
    baby.saddled = baby.hasChest = false;
}

bool Mobs::canMate(const MobData& a, const MobData& b) {
    if (a.type == MobType::Mule || b.type == MobType::Mule) return false; // (mules never breed)
    if (a.type == b.type) return true;
    return (a.type == MobType::Horse && b.type == MobType::Donkey) ||
           (a.type == MobType::Donkey && b.type == MobType::Horse) ||
           (isLlama(a.type) && isLlama(b.type));
}

Mobs::Use Mobs::mountInteract(MobData& m, ItemId held, Xoroshiro& rng, ItemEntities& items) {
    const std::string_view id = held != kNoItem ? itemRegistry().item(held).id : std::string_view{};
    if (m.type == MobType::HappyGhast) return happyGhastInteract(m, held, rng, items); // (M26.5b)
    if (m.type == MobType::Nautilus) { // (M26.5a; wiki: Nautilus)
        const bool puffer = id == "minecraft:pufferfish" || id == "minecraft:pufferfish_bucket";
        const bool fish = puffer || id == "minecraft:cod" || id == "minecraft:salmon" ||
                          id == "minecraft:tropical_fish" || id == "minecraft:cod_bucket" ||
                          id == "minecraft:salmon_bucket" || id == "minecraft:tropical_fish_bucket";
        if (!m.tamed) { // tamed with pufferfish, 1 in 3
            if (!puffer) return Use::None;
            if (rng.nextInt(3) == 0) {
                m.tamed = true;
                m.persistent = true;
                m.angry = false;
            }
            return Use::Fed;
        }
        if (fish) { // heals, grows, breeds
            if (m.health < maxHealthOf(m)) {
                m.health = std::min(maxHealthOf(m), m.health + 2.0f);
                return Use::Fed;
            }
            if (m.isBaby()) {
                m.age = std::min(0, m.age + 2400);
                return Use::Fed;
            }
            if (m.age == 0 && m.loveTicks == 0) {
                m.loveTicks = 600;
                return Use::Fed;
            }
            return Use::None;
        }
        if (m.isBaby()) return Use::None;
        if (id == "minecraft:shears" && m.saddled) {
            dropNamed(items, m.pos + glm::dvec3(0.0, 0.5, 0.0), "saddle", rng);
            m.saddled = false;
            return Use::Sheared;
        }
        if (id == "minecraft:saddle" && !m.saddled) {
            m.saddled = true;
            return Use::Fed;
        }
        if (m.ridden) return Use::None;
        m.ridden = true;
        return Use::Ride;
    }
    const bool tame = m.tamed || m.type == MobType::Camel; // (camels need no taming)
    const glm::dvec3 at = m.pos + glm::dvec3(0.0, 1.0, 0.0);
    // Shears take off its body armor or carpet, then its saddle (1.21.6).
    if (id == "minecraft:shears" && tame) {
        if (m.horseArmor > 0) {
            dropNamed(items, at, kHorseArmorItems[m.horseArmor], rng);
            m.horseArmor = 0;
            return Use::Sheared;
        }
        if (m.decor > 0) {
            dropNamed(items, at, std::string(kDyeColours[m.decor - 1]) + "_carpet", rng);
            m.decor = 0;
            return Use::Sheared;
        }
        if (m.saddled) {
            dropNamed(items, at, "saddle", rng);
            m.saddled = false;
            return Use::Sheared;
        }
        return Use::None;
    }
    if (const MountFood* f = mountFood(m.type, id)) {
        bool used = false;
        if (m.health < maxHealthOf(m)) {
            m.health = std::min(maxHealthOf(m), m.health + f->heal);
            used = true;
        }
        if (m.isBaby() && f->grow > 0) {
            m.age = std::min(0, m.age + f->grow);
            used = true;
        }
        if (!tame && f->temper > 0 && m.temper < maxTemper(m.type)) {
            m.temper = int16_t(std::min(maxTemper(m.type), m.temper + f->temper));
            used = true;
        }
        if (f->love && tame && !m.isBaby() && m.age == 0 && m.loveTicks == 0 &&
            m.type != MobType::Mule) {
            m.loveTicks = 600;
            used = true;
        }
        return used ? Use::Fed : Use::None;
    }
    if (m.isBaby()) return Use::None;
    if (tame) { // gear (it uses up the item: Fed)
        if (id == "minecraft:saddle" && !m.saddled &&
            (isHorseKind(m.type) || m.type == MobType::Camel)) {
            m.saddled = true;
            return Use::Fed;
        }
        for (int k = 1; k < 5; ++k)
            if (m.type == MobType::Horse && m.horseArmor == 0 &&
                id == std::string("minecraft:") + kHorseArmorItems[k]) {
                m.horseArmor = uint8_t(k);
                return Use::Fed;
            }
        for (int c = 0; c < 16; ++c)
            if (isLlama(m.type) && m.decor == 0 &&
                id == std::string("minecraft:") + kDyeColours[c] + "_carpet") {
                m.decor = uint8_t(c + 1);
                return Use::Fed;
            }
        if (id == "minecraft:chest" && canCarryChest(m.type) && !m.hasChest) {
            m.hasChest = true;
            return Use::Fed;
        }
    }
    if (m.ridden) return Use::None;
    // Getting on. A wild one bucks for 1.5-3.5 s before it decides.
    if (!tame) m.tameCheck = int16_t(30 + rng.nextInt(41));
    m.ridden = true;
    m.sitting = false;
    m.goal = m.pos;
    return Use::Ride;
}

double Mobs::seatHeight(const MobData& m) {
    // Where the rider's feet go above the mount's (our estimates of vanilla's
    // passenger attachment points).
    switch (m.type) {
    case MobType::Boat:
        return 0.15;
    case MobType::Minecart:
        return 0.3;
    case MobType::Camel:
        return 1.75;
    case MobType::Nautilus:
        return 0.55;
    case MobType::HappyGhast:
        return 4.0; // (on its back)
    case MobType::Llama:
    case MobType::TraderLlama:
        return 1.15;
    case MobType::Donkey:
    case MobType::Mule:
        return 0.85;
    default:
        return 0.95; // horses
    }
}

bool Mobs::mountTick(Context& ctx, MobData& m) {
    if (!m.ridden) return false;
    const MobInfo& info = mobInfo(m.type);
    m.sitting = false;
    if (m.dashCooldown > 0) --m.dashCooldown;
    if (m.type == MobType::HappyGhast) {
        // Ridden (M26.5b; wiki: Happy Ghast): forward flies where the rider looks (about
        // 3.6 blocks/s), back the other way, jump straight up.
        m.yaw = m.headYaw;
        const glm::dvec3 look(lookVector(m.headYaw, m.pitch));
        glm::dvec3 wish = look * (m.paddleForward > 0 ? 0.18 : m.paddleForward < 0 ? -0.18 : 0.0);
        if (m.riderJump > 0 || m.paddleTurn == 2) wish.y += 0.18;
        m.riderJump = 0;
        m.paddleForward = m.paddleTurn = 0;
        physics(ctx.world, m, wish, false);
        return true;
    }
    if (!m.tamed && m.type != MobType::Camel) {
        // Being tamed: it fidgets and turns about, then keeps the rider or throws them.
        m.yaw += (ctx.rng.nextFloat() - 0.5f) * 30.0f;
        m.headYaw = m.yaw;
        const glm::dvec3 f(forwardFlat(m.yaw));
        physics(ctx.world, m, f * (attrSpeed(m) * 0.6), false);
        if (--m.tameCheck <= 0) {
            if (int(ctx.rng.nextInt(uint32_t(maxTemper(m.type)))) < m.temper) {
                m.tamed = true;
                m.persistent = true;
                m.despawnDelay = 0; // (a trader llama the player tamed stays)
            } else {
                m.temper = int16_t(std::min(maxTemper(m.type), m.temper + 5));
                m.ridden = false; // (main sees it and puts the rider down)
                m.vel.y = 0.3;    // (rears up)
                m.ambientTime = 0;
                ctx.world.playSound(mobSound(m.type, MobSound::Hurt), m.pos.x, m.pos.y + 1.0,
                                    m.pos.z);
            }
        }
        return true;
    }
    if (m.type == MobType::Nautilus && m.saddled) {
        // Ridden under water (M26.5a; wiki: Nautilus): forward swims where the rider looks
        // (6.5 blocks/s), jump dashes up to 12 blocks (every 2 s).
        m.yaw = m.headYaw;
        const glm::dvec3 look(lookVector(m.headYaw, m.pitch));
        const glm::dvec3 wish = look * (m.paddleForward > 0   ? 0.325
                                        : m.paddleForward < 0 ? -0.08
                                                              : 0.0);
        if (m.riderJump > 0 && m.dashCooldown == 0) {
            m.vel += look * (1.4 * m.riderJump / 100.0);
            m.dashCooldown = 40;
        }
        m.riderJump = 0;
        m.paddleForward = m.paddleTurn = 0;
        physics(ctx.world, m, wish, false);
        return true;
    }
    const bool steered = m.saddled && (isHorseKind(m.type) || m.type == MobType::Camel);
    if (!steered) { // (an unsaddled horse or a llama only carries the rider: it stands)
        m.yaw = approachAngle(m.yaw, m.headYaw, 5.0f);
        physics(ctx.world, m, glm::dvec3(0.0), false);
        return true;
    }
    // The mount faces where the rider looks (main sets headYaw); W runs at its speed,
    // S backs at a quarter, A/D sidestep at half (wiki: Horse › Riding).
    m.yaw = m.headYaw;
    const double forward = m.paddleForward > 0 ? 1.0 : m.paddleForward < 0 ? -0.25 : 0.0;
    const double side = m.paddleTurn * 0.5;
    // A player's walk speed 0.1 is 4.317 blocks/s: 2.1585 blocks a tick per point.
    double speed = attrSpeed(m) * 2.1585;
    if (m.type == MobType::Camel && m.paddleForward == 2)
        speed *= 2.11; // (sprinting: 8.2 blocks/s)
    glm::dvec3 wish =
        (glm::dvec3(forwardFlat(m.yaw)) * forward + glm::dvec3(rightFlat(m.yaw)) * side) * speed;
    if (m.riderJump > 0 && m.onGround) {
        const double power = m.riderJump / 100.0;
        if (m.type == MobType::Camel) {
            if (m.dashCooldown == 0) { // a dash: forward and a little up (up to ~12 blocks)
                const glm::dvec3 f(forwardFlat(m.yaw));
                m.vel += f * (2.5 * power);
                m.vel.y = 0.5 * power + 0.1;
                m.dashCooldown = 55; // 2.75 s
            }
        } else {
            m.vel.y = attrJump(m) * power; // (strength 1.0 at a full bar: ~5.9 blocks high)
            const glm::dvec3 f(forwardFlat(m.yaw));
            m.vel += f * (0.4 * power * forward); // (a leap forward when running)
        }
    }
    m.riderJump = 0;
    m.paddleForward = m.paddleTurn = 0;
    physics(ctx.world, m, wish, false);
    (void)info;
    return true;
}

bool Mobs::mountGoal(Context& ctx, MobData& m, double& speed) {
    // Camels sit down now and then and rest (vanilla: about every 2 minutes or more).
    if (m.type == MobType::Camel) {
        if (m.sitting) {
            m.goal = m.pos;
            m.vel.x = m.vel.z = 0.0;
            if (ctx.rng.nextInt(1200) == 0 || m.panicTicks > 0) m.sitting = false;
            return true;
        }
        if (!m.isBaby() && m.loveTicks == 0 && ctx.rng.nextInt(2400) == 0) m.sitting = true;
        return false;
    }
    if (m.type != MobType::TraderLlama || m.tamed) return false;
    // A trader llama keeps to its wandering trader (targetUuid) and goes when it goes.
    if (m.despawnDelay > 0 && --m.despawnDelay == 0) {
        m.health = 0.0f;
        m.deathTime = 19; // (gone, no drops)
        m.lastHurtByPlayer = false;
        return true;
    }
    if (MobData* t = m.targetUuid ? mobByUuid(ctx.world, m.pos, m.targetUuid) : nullptr) {
        const double d = glm::length(t->pos - m.pos);
        if (d > 4.0) {
            m.goal = t->pos;
            speed *= 1.2;
            return true;
        }
        m.goal = m.pos;
        return true;
    }
    m.targetUuid = 0; // (its trader is gone: it wanders on its own, no lookups each tick)
    return false;
}

void Mobs::llamaTick(Context& ctx, MobData& m) {
    // Llamas spit at a player who hit them (for a while) and at wolves near them
    // (wiki: Llama › Behavior): a gob that hits for 1, about every 2 s.
    if (m.angry && --m.angerTicks <= 0) m.angry = false;
    if (m.attackCooldown > 0 || !ctx.projectiles || m.ridden) return;
    glm::dvec3 target(0.0);
    bool have = false;
    const glm::dvec3 eye = m.pos + glm::dvec3(0.0, mobInfo(m.type).height * 0.85, 0.0);
    if (m.angry && ctx.survival && !ctx.playerDead) {
        target = ctx.player.position() + glm::dvec3(0.0, 1.2, 0.0);
        have = glm::length(target - eye) < 12.0;
    }
    if (!have && ctx.rng.nextInt(20) == 0) { // (looks for wolves about once a second)
        const ChunkPos c{blockToChunk(int(std::floor(m.pos.x))),
                         blockToChunk(int(std::floor(m.pos.z)))};
        for (int dz = -1; dz <= 1 && !have; ++dz)
            for (int dx = -1; dx <= 1 && !have; ++dx)
                if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                    for (const MobData& o : ch->mobs())
                        if (o.type == MobType::Wolf && !o.tamed && o.health > 0.0f &&
                            glm::length(o.pos - m.pos) < 10.0) {
                            target = o.pos + glm::dvec3(0.0, 0.4, 0.0);
                            have = true;
                            break;
                        }
    }
    if (!have) return;
    const glm::dvec3 to = target - eye;
    const double d = glm::length(to);
    if (d < 1e-6) return;
    m.yaw = m.headYaw = float(std::atan2(-to.x, to.z) * 180.0 / 3.14159265358979);
    // (aimed a little high: the spit falls on its way)
    const glm::dvec3 dir = glm::normalize(to + glm::dvec3(0.0, d * 0.1, 0.0));
    ctx.projectiles->shoot(ProjectileKind::LlamaSpit, eye + dir * 0.6, dir, 1.5, 4.0, false, false,
                           ctx.rng, m.uuidHi);
    m.attackCooldown = 40;
}

void Mobs::dropMountGear(Context& ctx, MobData& m) {
    const glm::dvec3 at = m.pos + glm::dvec3(0.0, 0.5, 0.0);
    if (m.saddled) dropNamed(ctx.items, at, "saddle", ctx.rng);
    if (m.horseArmor > 0 && m.horseArmor < 5)
        dropNamed(ctx.items, at, kHorseArmorItems[m.horseArmor], ctx.rng);
    if (m.decor > 0 && m.decor <= 16)
        dropNamed(ctx.items, at,
                  std::string(kDyeColours[m.decor - 1]) +
                      (m.type == MobType::HappyGhast ? "_harness" : "_carpet"),
                  ctx.rng);
    if (m.hasChest && m.type != MobType::Boat) dropNamed(ctx.items, at, "chest", ctx.rng);
    m.saddled = m.hasChest = false;
    m.horseArmor = m.decor = 0;
    // The chest's stacks spill out.
    if (Chunk* c = ctx.world.chunk(
            {blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))}))
        if (ItemContents* slots = c->mobStore(m.uuidHi)) {
            for (const ItemStack& s : *slots)
                if (!s.empty()) ctx.items.spawn(at, s, ctx.rng);
            c->removeMobStore(m.uuidHi);
        }
}

void Mobs::spawnMounts(Context& ctx, Biome biome, int x, int y, int z) {
    // wiki: Horse (plains, savannas: herds of 2-6), Donkey (plains, savannas: 1-3),
    // Llama (windswept hills, savanna plateaus: 4-6), Camel (deserts; vanilla: only in
    // desert villages - ours wander in, see game-design.md).
    MobType kind = MobType::Count;
    int group = 1;
    const uint32_t r = ctx.rng.nextInt(10);
    if (biome == Biome::Plains || biome == Biome::Savanna) {
        kind = r < 6 ? MobType::Horse : MobType::Donkey;
        group = kind == MobType::Horse ? 2 + int(ctx.rng.nextInt(5)) : 1 + int(ctx.rng.nextInt(3));
    } else if (biome == Biome::WindsweptHills) {
        kind = MobType::Llama;
        group = 4 + int(ctx.rng.nextInt(3));
    } else if (biome == Biome::Desert && r == 0) {
        kind = MobType::Camel;
    } else {
        return;
    }
    // One herd shares a coat (horses: the markings vary) or a llama's look.
    uint8_t coat = 255;
    for (int i = 0; i < group; ++i) {
        const int gx = x + int(ctx.rng.nextInt(7)) - 3, gz = z + int(ctx.rng.nextInt(7)) - 3;
        if (!solid(ctx.world, gx, y - 1, gz) || solid(ctx.world, gx, y, gz) ||
            solid(ctx.world, gx, y + 1, gz))
            continue;
        MobData m = make(kind, {gx + 0.5, double(y), gz + 0.5}, ctx.rng);
        if (coat == 255) coat = m.woolColour;
        if (kind == MobType::Horse || kind == MobType::Llama) m.woolColour = coat;
        if (kind != MobType::Camel && ctx.rng.nextInt(5) == 0) m.age = -24000; // (20% foals)
        if (add(ctx.world, m)) ++m_creatures;
    }
}

} // namespace mc
