// Villager behaviour (M24.1; wiki: Villager › Behavior, Schedules). Part of Mobs.
#include "gameplay/Mobs.h"

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
bool claimed(World& world, const MobData& self, const glm::ivec3& p, Poi kind) {
    const ChunkPos c{blockToChunk(p.x), blockToChunk(p.z)};
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx)
            if (const Chunk* ch = world.chunk({c.x + dx, c.z + dz}))
                for (const MobData& o : ch->mobs()) {
                    if (&o == &self || o.type != MobType::Villager || o.health <= 0.0f) continue;
                    if ((kind == Poi::Bed && o.home == p) ||
                        (kind == Poi::JobSite && o.jobSite == p))
                        return true;
                }
    return false;
}

// The nearest free point of a kind within 48 blocks (vanilla's POI search range),
// skipping sections whose palette has no such block.
std::optional<glm::ivec3> findPoi(World& world, const MobData& self, Poi kind) {
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
                            if (d2 >= bestD || (kind != Poi::Bell && claimed(world, self, p, kind)))
                                continue;
                            bestD = d2;
                            best = p;
                        }
            }
        }
    return best;
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
    if (m.panicTicks > 0) return false; // (running from a zombie or a hit: the general panic)
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
        if (m.lastRestockDay != day && m.restocksToday > 0 && t < 2100) m.restocksToday = 0;
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
        return wanderNear(m.jobSite, 1.5);
    }
    if (t >= 9000 && t < 11000 && hasPoint(m.meetingPoint)) return wanderNear(m.meetingPoint, 4.0);
    if (hasPoint(m.meetingPoint)) return wanderNear(m.meetingPoint, 16.0); // (the village)
    if (hasPoint(m.home)) return wanderNear(m.home, 10.0);
    return false; // the general random stroll
}

// Villagers run from zombies within 8 blocks (wiki: Villager › Behavior).
void Mobs::villagerFear(Context& ctx, MobData& m) {
    if ((uint64_t(ctx.dayTime) + m.uuidLo) % 10 != 0 || m.sleeping) return;
    const ChunkPos c{blockToChunk(int(std::floor(m.pos.x))),
                     blockToChunk(int(std::floor(m.pos.z)))};
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                for (const MobData& o : ch->mobs()) {
                    if (o.type != MobType::Zombie || o.health <= 0.0f) continue;
                    const glm::dvec3 away = m.pos - o.pos;
                    if (glm::dot(away, away) > 8.0 * 8.0) continue;
                    m.panicTicks = 60;
                    const double l = std::max(0.1, glm::length(glm::dvec2(away.x, away.z)));
                    m.goal = m.pos + glm::dvec3(away.x / l * 8.0, 0.0, away.z / l * 8.0);
                    m.goalTicks = 0;
                    return;
                }
}

} // namespace mc
