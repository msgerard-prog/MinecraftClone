// Raids (M24.5; wiki: Raid).
#include "gameplay/Raids.h"

#include "gameplay/Mobs.h"
#include "world/Blocks.h"

#include <cmath>
#include <numbers>

namespace mc {

using namespace world;

std::optional<glm::ivec3> findBell(const World& world, const glm::dvec3& at, int range) {
    std::optional<glm::ivec3> best;
    double bestD = double(range) * range;
    const ChunkPos pc{blockToChunk(int(std::floor(at.x))), blockToChunk(int(std::floor(at.z)))};
    const int cr = (range >> 4) + 1;
    for (int dz = -cr; dz <= cr; ++dz)
        for (int dx = -cr; dx <= cr; ++dx) {
            const Chunk* ch = world.chunk({pc.x + dx, pc.z + dz});
            if (!ch) continue;
            for (int si = 0; si < ch->sectionCount(); ++si) {
                const Section& sec = ch->section(si);
                if (sec.isEmpty() ||
                    sec.allPaletteStates([](BlockStateId s) { return blockRegistry().blockOf(s) != blocks::Bell; }))
                    continue;
                for (int i = 0; i < 4096; ++i) {
                    if (blockRegistry().blockOf(sec.getIndex(i)) != blocks::Bell) continue;
                    const glm::ivec3 p{ch->pos().x * 16 + (i & 15), ch->height().minY + si * 16 + (i >> 8),
                                       ch->pos().z * 16 + ((i >> 4) & 15)};
                    const glm::dvec3 d = glm::dvec3(p) + 0.5 - at;
                    if (glm::dot(d, d) < bestD) bestD = glm::dot(d, d), best = p;
                }
            }
        }
    return best;
}

void Raid::start(const glm::ivec3& centre, int level) {
    m_active = true;
    m_centre = centre;
    m_level = std::max(1, level);
    m_waves = 5 + (m_level > 1 ? 1 : 0); // Normal: 5 waves, a bonus one at Bad Omen II+ (wiki)
    m_wave = 0;
    m_ticks = 0;
    m_cooldown = 300; // (15 s before the first wave)
    m_waveHealth = m_aliveHealth = 0.0f;
}

// Raiders per wave on Normal (wiki: Raid › Waves - pillagers, vindicators, ravagers,
// witches, evokers for waves 1-7; ours uses the first `waves`).
void Raid::spawnWave(World& world, Xoroshiro& rng) {
    static constexpr int kWaves[5][7] = {{4, 3, 3, 4, 4, 4, 2},
                                         {0, 2, 0, 1, 4, 2, 5},
                                         {0, 0, 1, 0, 1, 0, 2},
                                         {0, 0, 1, 3, 0, 1, 0},
                                         {0, 0, 0, 0, 1, 1, 2}};
    static constexpr MobType kTypes[5] = {MobType::Pillager, MobType::Vindicator, MobType::Ravager, MobType::Witch,
                                          MobType::Evoker};
    // A spot about 32 blocks from the bell on the surface (vanilla tries closer if not).
    const auto& r = blockRegistry();
    glm::ivec3 spot = m_centre;
    for (int tries = 0; tries < 30; ++tries) {
        const double angle = rng.nextDouble() * 2.0 * std::numbers::pi, dist = tries < 20 ? 32.0 : 20.0;
        const int x = m_centre.x + int(std::lround(std::cos(angle) * dist));
        const int z = m_centre.z + int(std::lround(std::sin(angle) * dist));
        if (!world.chunk(BlockPos{x, 0, z}.chunk())) continue;
        bool ok = false;
        for (int y = m_centre.y + 20; y >= m_centre.y - 20 && !ok; --y) {
            const BlockStateId below = world.getBlock({x, y - 1, z});
            if (!r.collides(below) || r.blockOf(below) == blocks::Water) continue;
            if (r.collides(world.getBlock({x, y, z})) || r.collides(world.getBlock({x, y + 1, z}))) continue;
            spot = {x, y, z};
            ok = true;
        }
        if (ok) break;
    }
    m_waveHealth = 0.0f;
    bool captain = false;
    for (int k = 0; k < 5; ++k)
        for (int n = 0; n < kWaves[k][std::min(m_wave, 6)]; ++n) {
            MobData m = Mobs::make(kTypes[k], {spot.x + 0.5 + rng.nextDouble() * 2 - 1, double(spot.y),
                                               spot.z + 0.5 + rng.nextDouble() * 2 - 1},
                                   rng);
            m.raider = true;
            m.persistent = true;
            if (kTypes[k] == MobType::Pillager && !captain) m.captain = captain = true; // (the wave's leader)
            if (Mobs::add(world, m)) m_waveHealth += m.health;
        }
    m_aliveHealth = m_waveHealth;
    ++m_wave;
}

void Raid::tick(World& world, Vitals& vitals, const glm::dvec3& player, Xoroshiro& rng) {
    // Bad Omen near a village bell: Raid Omen for 30 s, then the raid (wiki, 1.21).
    if (!m_active && m_pendingTicks == 0 && vitals.effectLevel(Effect::BadOmen) > 0 && ++m_counter % 20 == 0)
        if (const auto bell = findBell(world, player, 32)) {
            m_pendingLevel = vitals.effectLevel(Effect::BadOmen);
            vitals.removeEffect(Effect::BadOmen);
            vitals.addEffect(Effect::RaidOmen, m_pendingLevel - 1, 600);
            m_pendingCentre = *bell;
            m_pendingTicks = 600;
        }
    if (m_pendingTicks > 0 && --m_pendingTicks == 0) start(m_pendingCentre, m_pendingLevel);
    if (!m_active) return;
    ++m_ticks;
    if (m_ticks % 20 != 0) return;
    // The raiders left, and the villagers near the bell.
    m_alive = 0;
    m_aliveHealth = 0.0f;
    int villagers = 0;
    world.forEachChunk([&](Chunk& c) {
        for (const MobData& m : c.mobs()) {
            if (m.health <= 0.0f) continue;
            if (m.raider) ++m_alive, m_aliveHealth += m.health;
            if (m.type == MobType::Villager && glm::length(glm::dvec3(m_centre) - m.pos) < 96.0) ++villagers;
        }
    });
    if (villagers == 0 || m_ticks > 48000) { // lost, or given up (wiki)
        m_active = false;
        return;
    }
    if (m_alive > 0) return;
    if (m_wave >= m_waves) { // won: the hero's reward (wiki: Hero of the Village, 40 min)
        if (glm::length(glm::dvec3(m_centre) - player) < 96.0)
            vitals.addEffect(Effect::HeroOfTheVillage, m_level - 1, 48000);
        m_active = false;
        return;
    }
    if ((m_cooldown -= 20) <= 0) {
        spawnWave(world, rng);
        m_cooldown = 300;
    }
}

} // namespace mc
