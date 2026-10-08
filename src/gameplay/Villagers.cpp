// Villager behaviour (M24.1; wiki: Villager › Behavior, Schedules). Part of Mobs.
#include "gameplay/Mobs.h"

#include "gameplay/ExperienceOrbs.h"

#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Trades.h"
#include "world/Villagers.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
bool hasPoint(const glm::ivec3& p) { return p.y != kNoPoint; }
glm::dvec3 centre(const glm::ivec3& p) { return {p.x + 0.5, double(p.y), p.z + 0.5}; }

enum class Poi { Bed, JobSite, Bell };
bool isPoi(BlockStateId s, Poi kind) {
    const BlockId b = R().blockOf(s);
    switch (kind) {
    case Poi::Bed:
        return b == blocks::RedBed && R().get(s, properties::bedPart) == 0; // (the head half)
    case Poi::JobSite:
        return isJobSite(b);
    case Poi::Bell:
        return b == blocks::Bell;
    }
    return false;
}

// Whether another villager already remembers this point as its home or job site
// (vanilla keeps POI tickets; ours asks the villagers around).
bool claimed(World& world, const MobData* self, const glm::ivec3& p, Poi kind) {
    const ChunkPos c{blockToChunk(p.x), blockToChunk(p.z)};
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx)
            if (const Chunk* ch = world.chunk({c.x + dx, c.z + dz}))
                for (const MobData& o : ch->mobs()) {
                    if (&o == self || o.type != MobType::Villager || o.health <= 0.0f) continue;
                    if ((kind == Poi::Bed && o.home == p) ||
                        (kind == Poi::JobSite && o.jobSite == p))
                        return true;
                }
    return false;
}

// The nearest free point of a kind within 48 blocks (vanilla's POI search range),
// skipping sections whose palette has no such block.
// (`countSelf`: its own home counts as taken - a free bed for a baby.)
std::optional<glm::ivec3> findPoi(World& world, const MobData& self, Poi kind, bool countSelf = false) {
    const glm::ivec3 at{int(std::floor(self.pos.x)), int(std::floor(self.pos.y)),
                        int(std::floor(self.pos.z))};
    constexpr int kRange = 48;
    std::optional<glm::ivec3> best;
    int bestD = kRange * kRange + 1;
    const ChunkPos c{blockToChunk(at.x), blockToChunk(at.z)};
    for (int dz = -3; dz <= 3; ++dz)
        for (int dx = -3; dx <= 3; ++dx) {
            const Chunk* ch = world.chunk({c.x + dx, c.z + dz});
            if (!ch) continue;
            const int minY = ch->height().minY;
            for (int si = 0; si < ch->sectionCount(); ++si) {
                const int sy = minY + si * 16;
                if (sy + 16 < at.y - 16 || sy > at.y + 16)
                    continue; // (16 up and down is plenty for a village)
                const Section& sec = ch->section(si);
                if (sec.isEmpty() ||
                    sec.allPaletteStates([&](BlockStateId s) { return !isPoi(s, kind); }))
                    continue;
                for (int y = 0; y < 16; ++y)
                    for (int z = 0; z < 16; ++z)
                        for (int x = 0; x < 16; ++x) {
                            if (!isPoi(sec.get(x, y, z), kind)) continue;
                            const glm::ivec3 p{ch->pos().x * 16 + x, sy + y, ch->pos().z * 16 + z};
                            const glm::ivec3 d = p - at;
                            const int d2 = d.x * d.x + d.y * d.y + d.z * d.z;
                            if (d2 >= bestD || (kind != Poi::Bell && claimed(world, countSelf ? nullptr : &self, p, kind)))
                                continue;
                            bestD = d2;
                            best = p;
                        }
            }
        }
    return best;
}

// Villager food (wiki: Villager › Breeding): bread is worth 4, carrots, potatoes and
// beetroots 1; 12 makes a villager willing.
enum FoodSlot { kBread, kCarrot, kPotato, kBeetroot, kWheat, kSeeds };
constexpr std::string_view kFoodItems[6] = {"bread", "carrot", "potato", "beetroot", "wheat", "wheat_seeds"};
int foodPoints(const MobData& v) { return v.food[kBread] * 4 + v.food[kCarrot] + v.food[kPotato] + v.food[kBeetroot]; }
void eatForBreeding(MobData& v) { // uses up 12 points, bread first
    int need = 12;
    while (need >= 4 && v.food[kBread] > 0) --v.food[kBread], need -= 4;
    for (int slot : {kCarrot, kPotato, kBeetroot, kBread})
        while (need > 0 && v.food[size_t(slot)] > 0) {
            --v.food[size_t(slot)];
            need -= slot == kBread ? 4 : 1;
        }
}
bool willing(const MobData& v) {
    return v.type == MobType::Villager && !v.isBaby() && v.age == 0 && !v.sleeping && v.health > 0.0f &&
           foodPoints(v) >= 12;
}

bool stillThere(const World& world, const glm::ivec3& p, Poi kind) {
    return hasPoint(p) && isPoi(world.getBlock({p.x, p.y, p.z}), kind);
}

} // namespace

// The villager's day (wiki: Villager › Schedules, adults): 10-2000 wander, 2000-9000
// work at the job site (nitwits and the unemployed wander), 9000-11000 gather at the
// bell, 11000-12000 wander, 12000-24000 sleep in its bed. Babies play instead of
// working. Returns true when it chose the goal (or sleeps).
bool Mobs::villagerGoal(Context& ctx, MobData& m, double& speed) {
    World& world = ctx.world;
    // Points it remembers vanish with their blocks.
    if (hasPoint(m.home) && !stillThere(world, m.home, Poi::Bed)) m.home = {0, kNoPoint, 0};
    if (hasPoint(m.meetingPoint) && !stillThere(world, m.meetingPoint, Poi::Bell))
        m.meetingPoint = {0, kNoPoint, 0};
    if (hasPoint(m.jobSite) && !stillThere(world, m.jobSite, Poi::JobSite)) {
        m.jobSite = {0, kNoPoint, 0};
        // An untrained villager loses its profession with its job site (wiki).
        if (m.villagerXp == 0 && m.villagerLevel <= 1 &&
            m.profession != uint8_t(Profession::Nitwit)) {
            m.profession = uint8_t(Profession::None);
            m.offerCount = 0;
        }
    }
    // Look for what's missing now and then (every 10-20 s).
    if (--m.poiSearch <= 0) {
        m.poiSearch = int16_t(200 + ctx.rng.nextInt(200));
        if (!m.isBaby()) {
            if (!hasPoint(m.home))
                if (const auto bed = findPoi(world, m, Poi::Bed)) m.home = *bed;
            const bool canWork = m.profession != uint8_t(Profession::Nitwit);
            if (canWork && !hasPoint(m.jobSite))
                if (const auto job = findPoi(world, m, Poi::JobSite)) {
                    const Profession p =
                        professionForJobSite(R().blockOf(world.getBlock({job->x, job->y, job->z})));
                    // A job site of another trade only suits the unemployed.
                    if (m.profession == uint8_t(Profession::None) || m.profession == uint8_t(p)) {
                        m.jobSite = *job;
                        m.profession = uint8_t(p);
                        if (m.offerCount == 0)
                            addLevelTrades(m, ctx.rng); // (M24.2: its novice trades)
                    }
                }
        }
        if (!hasPoint(m.meetingPoint))
            if (const auto bell = findPoi(world, m, Poi::Bell)) m.meetingPoint = *bell;
        // Nothing found: look less often (a village with too few beds or job sites
        // would otherwise rescan its sections every 10-20 s per villager).
        const bool wantsJob = m.profession != uint8_t(Profession::Nitwit) && !hasPoint(m.jobSite);
        if (!m.isBaby() && (!hasPoint(m.home) || wantsJob)) m.poiSearch = int16_t(600 + ctx.rng.nextInt(600));
    }
    const int64_t t = ((ctx.dayTime % 24000) + 24000) % 24000;
    const bool night = t >= 12000;
    // Asleep: lies in the bed until morning (or until the bed is gone or it's hurt).
    if (m.sleeping) {
        if (!night || !hasPoint(m.home) || m.panicTicks > 0) {
            m.sleeping = false;
            m.pos.y += 0.6; // (out of the bed)
        } else {
            m.vel = glm::dvec3(0.0);
            m.goal = m.pos;
            return true;
        }
    }
    if (m.tradingTicks > 0) { // trading: stands and looks at the player (wiki)
        --m.tradingTicks;
        m.goal = m.pos;
        return true;
    }
    // A raid on the village: everyone runs home and stays inside, awake, rather than
    // panicking in the open (wiki: Raid ›
    // villagers hide in their houses until it is over).
    if (ctx.raidCentre && hasPoint(m.home) &&
        glm::length(glm::dvec3(*ctx.raidCentre) - m.pos) < 96.0) {
        m.goal = centre(m.home);
        if (glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 1.2) m.goal = m.pos;
        return true;
    }
    if (m.panicTicks > 0) return false; // (running from a zombie or a hit: the general panic)
    // Breeding (wiki: Villager › Breeding): two willing villagers meet; when a free bed is
    // there for the child, a baby is born after 3 s together. Both use 12 food points
    // and rest 5 minutes.
    if (willing(m)) {
        MobData* partner = nullptr;
        double best = 8.0 * 8.0;
        const ChunkPos c{blockToChunk(int(std::floor(m.pos.x))), blockToChunk(int(std::floor(m.pos.z)))};
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (Chunk* ch = world.chunk({c.x + dx, c.z + dz}))
                    for (MobData& o : ch->mobs()) {
                        if (&o == &m || !willing(o)) continue;
                        const double d = glm::dot(o.pos - m.pos, o.pos - m.pos);
                        if (d < best) best = d, partner = &o;
                    }
        if (partner) {
            m.goal = partner->pos;
            if (best < 2.0 * 2.0 && ++m.breedTogether >= 60) {
                m.breedTogether = 0;
                partner->breedTogether = 0;
                eatForBreeding(m);
                eatForBreeding(*partner);
                m.age = partner->age = 6000;
                const auto freeBed = findPoi(world, m, Poi::Bed, true);
                if (freeBed && m_births.size() < m_births.capacity()) {
                    MobData baby = make(MobType::Villager, (m.pos + partner->pos) * 0.5, ctx.rng);
                    baby.age = -24000;
                    baby.home = *freeBed; // (the child claims the bed that let it be born)
                    baby.villagerType = m.villagerType;
                    m_births.push_back(baby);
                }
            }
            return true;
        }
    }
    const double stroll = 0.6;          // (wiki: villagers stroll at 0.6 of their speed)
    if (night && hasPoint(m.home)) {
        const glm::dvec3 bed = centre(m.home);
        m.goal = bed;
        speed *= stroll;
        const glm::dvec2 d(bed.x - m.pos.x, bed.z - m.pos.z);
        if (glm::length(d) < 1.6 && std::abs(bed.y - m.pos.y) < 1.5) {
            // Lie down: feet at the foot end, head on the pillow (lying, the model's head
            // points along its back, which yaw turns toward the bed's facing).
            const BlockStateId bs = world.getBlock({m.home.x, m.home.y, m.home.z});
            const int f = R().get(bs, properties::facing); // north, south, west, east
            static constexpr int kDx[4] = {0, 0, -1, 1}, kDz[4] = {-1, 1, 0, 0};
            static constexpr float kYaw[4] = {0.0f, 180.0f, 270.0f,
                                              90.0f}; // (yaw 0: the head lies north)
            m.sleeping = true;
            m.pos = {bed.x - kDx[f] * 1.5, m.home.y + 0.5625, bed.z - kDz[f] * 1.5};
            m.vel = glm::dvec3(0.0);
            m.yaw = m.prevYaw = kYaw[f];
        }
        return true;
    }
    auto wanderNear = [&](const glm::ivec3& around, double radius) {
        if (++m.goalTicks > 160 ||
            glm::length(glm::dvec2(m.goal.x - m.pos.x, m.goal.z - m.pos.z)) < 0.8) {
            if (ctx.rng.nextInt(60) == 0) {
                const glm::dvec3 c = hasPoint(around) ? centre(around) : m.pos;
                m.goal = c + glm::dvec3(ctx.rng.nextDouble() * 2 * radius - radius, 0.0,
                                        ctx.rng.nextDouble() * 2 * radius - radius);
                m.goalTicks = 0;
            } else if (m.goalTicks > 160) {
                m.goal = m.pos;
            }
        }
        speed *= stroll;
        return true;
    };
    const bool works = !m.isBaby() && hasPoint(m.jobSite) &&
                       m.profession != uint8_t(Profession::None) &&
                       m.profession != uint8_t(Profession::Nitwit);
    if (t >= 2000 && t < 9000 && works) {
        // Stand by the job site, now and then stepping around it.
        const glm::dvec3 js = centre(m.jobSite);
        // Working there restocks used trades, up to twice a day (wiki: Trading › Restocking).
        const int64_t day = ctx.dayTime / 24000;
        if (m.lastRestockDay != day) m.restocksToday = 0; // (a new day: two restocks again)
        if (glm::length(glm::dvec2(js.x - m.pos.x, js.z - m.pos.z)) < 2.5 && m.restocksToday < 2) {
            bool used = false;
            for (int i = 0; i < m.offerCount; ++i)
                used = used || m.offers[size_t(i)].uses > 0;
            if (used && (m.lastRestockDay != day || ctx.rng.nextInt(1200) == 0)) {
                restock(m);
                ++m.restocksToday;
                m.lastRestockDay = day;
            }
        }
        if (glm::length(glm::dvec2(js.x - m.pos.x, js.z - m.pos.z)) > 2.5) {
            m.goal = js;
            speed *= stroll;
            return true;
        }
        // Farmers harvest ripe crops around their job site and replant them (wiki:
        // Farmer); what they gather goes into their food.
        if (m.profession == uint8_t(Profession::Farmer)) {
            if (hasPoint(m.workTarget)) {
                const BlockStateId cs = world.getBlock({m.workTarget.x, m.workTarget.y, m.workTarget.z});
                const BlockId cb = R().blockOf(cs);
                if (!BlockUpdates::isCrop(cb) || BlockUpdates::cropAge(cs) < BlockUpdates::cropMaxAge(cb)) {
                    m.workTarget = {0, kNoPoint, 0};
                } else if (glm::length(centre(m.workTarget) - m.pos) < 1.8) {
                    const int extra = 1 + int(ctx.rng.nextInt(3));
                    auto add = [&](int slot, int n) { m.food[size_t(slot)] = uint8_t(std::min(64, m.food[size_t(slot)] + n)); };
                    if (cb == blocks::Wheat) add(kWheat, 1), add(kSeeds, extra);
                    else if (cb == blocks::Carrots) add(kCarrot, extra);
                    else if (cb == blocks::Potatoes) add(kPotato, extra);
                    else add(kBeetroot, 1);
                    const BlockPos at{m.workTarget.x, m.workTarget.y, m.workTarget.z};
                    world.updateBlock(at, R().defaultState(cb)); // replanted (age 0)
                    if (ctx.edits) ctx.edits->push_back(at);
                    m.workTarget = {0, kNoPoint, 0};
                } else {
                    m.goal = centre(m.workTarget);
                    speed *= stroll;
                    return true;
                }
            } else if (ctx.rng.nextInt(40) == 0) {
                for (int i = 0; i < 24 && !hasPoint(m.workTarget); ++i) {
                    const glm::ivec3 p{m.jobSite.x + int(ctx.rng.nextInt(17)) - 8, m.jobSite.y + int(ctx.rng.nextInt(5)) - 2,
                                       m.jobSite.z + int(ctx.rng.nextInt(17)) - 8};
                    const BlockStateId cs = world.getBlock({p.x, p.y, p.z});
                    const BlockId cb = R().blockOf(cs);
                    if (BlockUpdates::isCrop(cb) && BlockUpdates::cropAge(cs) >= BlockUpdates::cropMaxAge(cb)) m.workTarget = p;
                }
            }
        }
        return wanderNear(m.jobSite, 1.5);
    }
    if (t >= 9000 && t < 11000 && hasPoint(m.meetingPoint)) return wanderNear(m.meetingPoint, 4.0);
    if (hasPoint(m.meetingPoint)) return wanderNear(m.meetingPoint, 16.0); // (the village)
    if (hasPoint(m.home)) return wanderNear(m.home, 10.0);
    return false; // the general random stroll
}

MobData* Mobs::mobByUuid(World& world, const glm::dvec3& near, uint64_t uuid) {
    const ChunkPos c{blockToChunk(int(std::floor(near.x))), blockToChunk(int(std::floor(near.z)))};
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx)
            if (Chunk* ch = world.chunk({c.x + dx, c.z + dz}))
                for (MobData& o : ch->mobs())
                    if (o.uuidHi == uuid) return &o;
    return nullptr;
}

// Zombies (and zombie villagers) go after villagers too (wiki: Zombie › Behavior), and
// raiders - pillagers, vindicators, ravagers - after villagers, iron golems and
// wandering traders (wiki: Raid): the nearest within 16 blocks, looked for about once a
// second, chased while within 35. A villager a zombie kills turns into a zombie
// villager half the time (Normal difficulty), keeping its profession and trades.
// Returns true while hunting one.
bool Mobs::villageHunt(Context& ctx, MobData& z) {
    const bool zombie = isZombie(z.type);
    auto wanted = [&](const MobData& o) {
        if (o.health <= 0.0f) return false;
        if (o.type == MobType::Villager) return true;
        return !zombie && (o.type == MobType::IronGolem || o.type == MobType::WanderingTrader);
    };
    MobData* v = z.targetUuid ? mobByUuid(ctx.world, z.pos, z.targetUuid) : nullptr;
    if (v && (!wanted(*v) || glm::length(v->pos - z.pos) > 35.0)) v = nullptr;
    if (!v) z.targetUuid = 0;
    if (!v && ctx.rng.nextInt(20) == 0) { // (about once a second)
        double best = 16.0 * 16.0;
        const ChunkPos c{blockToChunk(int(std::floor(z.pos.x))), blockToChunk(int(std::floor(z.pos.z)))};
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                    for (MobData& o : ch->mobs()) {
                        if (!wanted(o)) continue;
                        const double d = glm::dot(o.pos - z.pos, o.pos - z.pos);
                        if (d < best) best = d, v = &o;
                    }
        if (v) z.targetUuid = v->uuidHi;
    }
    if (!v) return false;
    z.goal = v->pos;
    if (z.type == MobType::Pillager) {
        // (shoots it from about 8 blocks: Monsters.cpp)
        if (glm::length(v->pos - z.pos) < 8.0) z.goal = z.pos;
        return true;
    }
    const MobInfo& info = mobInfo(z.type);
    const double reach = info.width * 0.5 + mobInfo(v->type).width * 0.5 + 0.9;
    if (info.attackDamage > 0.0f && z.attackCooldown == 0 &&
        glm::length(glm::dvec2(v->pos.x - z.pos.x, v->pos.z - z.pos.z)) < reach && std::abs(v->pos.y - z.pos.y) < 1.5) {
        z.attackCooldown = 20;
        v->health -= info.attackDamage;
        v->hurtTime = 10;
        if (v->type == MobType::Villager) {
            v->panicTicks = 100;
            v->sleeping = false;
        }
        if (v->type == MobType::IronGolem) v->targetUuid = z.uuidHi; // (it fights back)
        // Infected: always on Hard, half the time on Normal, never on Easy (wiki: Zombie Villager).
        if (zombie && v->type == MobType::Villager && v->health <= 0.0f &&
            (ctx.difficulty == 3 || (ctx.difficulty == 2 && ctx.rng.nextInt(2) == 0))) {
            v->type = MobType::ZombieVillager; // infected: a zombie villager from now on
            v->health = mobInfo(MobType::ZombieVillager).maxHealth;
            v->hurtTime = 0;
            v->panicTicks = 0;
            v->persistent = true; // (vanilla: infected villagers never despawn)
            z.targetUuid = 0;
        }
    }
    return true;
}

// A zombie villager's Weakness wears off; a curing one becomes a villager when its
// countdown ends, with lower prices out of gratitude (wiki: Zombie Villager › Curing:
// the cure's 125 reputation takes price multiplier x 125 off - 0.05 trades 6, 0.2 trades
// 25 - so many trades fall to 1 emerald; ours only on the trades it has now).
void Mobs::zombieVillagerTick(MobData& m) {
    if (m.weaknessTicks > 0) --m.weaknessTicks;
    if (m.convertTicks <= 0 || --m.convertTicks > 0) return;
    m.type = MobType::Villager;
    m.health = mobInfo(MobType::Villager).maxHealth;
    m.targeting = false;
    m.targetUuid = 0;
    m.fireTicks = 0;
    for (int i = 0; i < m.offerCount; ++i)
        m.offers[size_t(i)].specialPrice =
            int16_t(-std::max(1, int(std::floor(m.offers[size_t(i)].priceMultiplier * 125.0f))));
}

// A villager picks up food lying near it; farmers bake 3 wheat into bread; one with
// plenty shares with a hungry neighbour (wiki: Villager › Gathering food, Sharing).
void Mobs::villagerUpkeep(Context& ctx, MobData& v) {
    if (v.isBaby() || ctx.rng.nextInt(10) != 0) return; // (twice a second)
    static const std::array<ItemId, 6> ids = [] {
        std::array<ItemId, 6> a{};
        for (size_t i = 0; i < a.size(); ++i) a[i] = itemRegistry().find(kFoodItems[i]).value_or(kNoItem);
        return a;
    }();
    for (ItemEntity& it : ctx.items.mutableItems()) {
        if (it.stack.empty() || it.pickupDelay > 0) continue;
        const glm::dvec3 d = it.pos - v.pos;
        if (std::abs(d.x) > 1.5 || std::abs(d.z) > 1.5 || d.y < -0.5 || d.y > 2.0) continue;
        for (size_t slot = 0; slot < ids.size(); ++slot)
            if (ids[slot] == it.stack.item && ids[slot] != kNoItem) {
                const int room = 64 - v.food[slot];
                const int n = std::min<int>(room, it.stack.count);
                v.food[slot] = uint8_t(v.food[slot] + n);
                it.stack.count = uint8_t(it.stack.count - n);
                if (it.stack.count == 0) it.stack = {};
            }
    }
    if (v.profession == uint8_t(Profession::Farmer))
        while (v.food[kWheat] >= 3 && v.food[kBread] < 64) v.food[kWheat] -= 3, ++v.food[kBread];
    if (foodPoints(v) >= 24 && ctx.rng.nextInt(10) == 0) {
        const ChunkPos c{blockToChunk(int(std::floor(v.pos.x))), blockToChunk(int(std::floor(v.pos.z)))};
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                    for (MobData& o : ch->mobs()) {
                        if (&o == &v || o.type != MobType::Villager || o.isBaby() || foodPoints(o) >= 12) continue;
                        if (glm::length(o.pos - v.pos) > 4.0) continue;
                        if (v.food[kBread] >= 3) v.food[kBread] -= 3, o.food[kBread] = uint8_t(o.food[kBread] + 3);
                        return;
                    }
    }
}

// Villagers run from zombies within 8 blocks (wiki: Villager › Behavior).
void Mobs::villagerFear(Context& ctx, MobData& m) {
    if (m.sleeping || ctx.rng.nextInt(10) != 0) return; // (about twice a second)
    const ChunkPos c{blockToChunk(int(std::floor(m.pos.x))),
                     blockToChunk(int(std::floor(m.pos.z)))};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                for (const MobData& o : ch->mobs()) {
                    // (wiki: Villager - zombies and vexes within 8, vindicators 10, evokers
                    // and ravagers 12, pillagers 15)
                    const double r = isZombie(o.type) || o.type == MobType::Vex ? 8.0
                                     : o.type == MobType::Vindicator             ? 10.0
                                     : o.type == MobType::Evoker || o.type == MobType::Ravager ? 12.0
                                     : o.type == MobType::Pillager                             ? 15.0
                                                                                               : 0.0;
                    if (r == 0.0 || o.health <= 0.0f) continue;
                    const glm::dvec3 away = m.pos - o.pos;
                    if (glm::dot(away, away) > r * r) continue;
                    m.panicTicks = 60;
                    const double l = std::max(0.1, glm::length(glm::dvec2(away.x, away.z)));
                    m.goal = m.pos + glm::dvec3(away.x / l * 8.0, 0.0, away.z / l * 8.0);
                    m.goalTicks = 0;
                    return;
                }
}

} // namespace mc
