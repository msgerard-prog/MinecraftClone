#include "gameplay/Explosion.h"

#include "gameplay/PrimedTnt.h"

#include "gameplay/Mining.h"
#include "gameplay/Mobs.h"
#include "world/Blocks.h"
#include "world/ItemContainers.h"
#include "world/Raycast.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

double Explosion::exposure(const World& world, const glm::dvec3& centre, const Aabb& box) {
    // Sample points across the box (spacing 1 / (2 x size + 1) of each side), a ray
    // from each to the centre (wiki: Explosion › Exposure).
    const glm::dvec3 size = box.max - box.min;
    const double sx = 1.0 / (size.x * 2.0 + 1.0), sy = 1.0 / (size.y * 2.0 + 1.0), sz = 1.0 / (size.z * 2.0 + 1.0);
    int open = 0, total = 0;
    for (double fx = 0.0; fx <= 1.0; fx += sx)
        for (double fy = 0.0; fy <= 1.0; fy += sy)
            for (double fz = 0.0; fz <= 1.0; fz += sz) {
                const glm::dvec3 p = box.min + size * glm::dvec3(fx, fy, fz);
                const glm::dvec3 d = centre - p;
                const double len = glm::length(d);
                ++total;
                if (len < 1e-6 || !raycastBlocks(world, p, d / len, len)) ++open;
            }
    return total ? double(open) / total : 0.0;
}

int Explosion::explode(World& world, const glm::dvec3& centre, float power, Xoroshiro& rng, ItemEntities& items,
                       std::vector<BlockPos>& changed, const ExplosionTargets& targets) {
    world.levelEvent(LevelEvent::Type::Explosion, centre.x, centre.y, centre.z, uint32_t(power * 10.0f));
    const auto& reg = blockRegistry();
    m_hits.clear();
    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 16; ++j)
            for (int k = 0; k < 16; ++k) {
                if (i != 0 && i != 15 && j != 0 && j != 15 && k != 0 && k != 15) continue; // surface only
                glm::dvec3 dir(i / 15.0 * 2.0 - 1.0, j / 15.0 * 2.0 - 1.0, k / 15.0 * 2.0 - 1.0);
                dir /= glm::length(dir);
                double intensity = power * (0.7 + rng.nextFloat() * 0.6);
                glm::dvec3 p = centre;
                while (intensity > 0.0) {
                    const BlockPos b{int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z))};
                    if (!world.isInHeight(b.y)) break;
                    const BlockStateId s = world.getBlock(b);
                    if (s != 0) {
                        intensity -= (reg.block(reg.blockOf(s)).settings.resistance + 0.3) * 0.3;
                        const BlockId id = reg.blockOf(s);
                        if (intensity > 0.0 && id != blocks::Water && id != blocks::Lava &&
                            reg.block(id).settings.hardness >= 0.0f)
                            m_hits.push_back(b);
                    }
                    p += dir * 0.3;
                    intensity -= 0.22500001;
                }
            }
    // Each block once.
    std::sort(m_hits.begin(), m_hits.end(), [](const BlockPos& a, const BlockPos& b) {
        return a.x != b.x ? a.x < b.x : a.y != b.y ? a.y < b.y : a.z < b.z;
    });
    m_hits.erase(std::unique(m_hits.begin(), m_hits.end()), m_hits.end());
    // Entities first, while the cover the blast is about to destroy still stands
    // (vanilla's order).
    // Entities within 2 x power.
    const double reach = 2.0 * power;
    auto hurt = [&](const Aabb& box, const glm::dvec3& feet, double eyeHeight, auto&& apply) {
        const double dist = glm::length(feet - centre) / reach;
        if (dist > 1.0) return;
        const double impact = (1.0 - dist) * exposure(world, centre, box);
        const double damage = (impact * impact + impact) / 2.0 * 7.0 * reach + 1.0;
        glm::dvec3 away = feet + glm::dvec3(0, eyeHeight, 0) - centre; // pushed away from the eyes (wiki)
        const double len = glm::length(away);
        away = len > 1e-6 ? away / len : glm::dvec3(0, 1, 0);
        apply(float(std::floor(damage)), away * impact);
    };
    if (targets.player && targets.vitals)
        hurt(targets.player->box(), targets.player->position(), targets.player->eyeHeight(),
             [&](float dmg, const glm::dvec3& push) {
            targets.vitals->attacked(dmg, &centre, Vitals::Hit::Explosion);
            targets.player->push(push);
        });
    if (targets.damageMobs) {
        const ChunkPos c{blockToChunk(int(std::floor(centre.x))), blockToChunk(int(std::floor(centre.z)))};
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (Chunk* ch = world.chunk({c.x + dx, c.z + dz}))
                    for (MobData& m : ch->mobs()) {
                        if (m.health <= 0.0f) continue;
                        if (m.type == MobType::EndCrystal) { // (Java: blown away without exploding, MC-118429)
                            m.health = 0.0f;
                            m.deathTime = 19;
                            continue;
                        }
                        hurt(Mobs::box(m), m.pos, mobInfo(m.type).height * 0.85, [&](float dmg, const glm::dvec3& push) {
                            m.health -= m.type == MobType::EnderDragon ? Mobs::dragonDamage(m, dmg, centre) : dmg;
                            m.hurtTime = 10;
                            m.vel += push;
                        });
                    }
    }
    if (targets.tnt) targets.tnt->push(centre, power);
    int destroyed = 0;
    for (const BlockPos& b : m_hits) {
        const BlockStateId s = world.getBlock(b);
        if (s == 0) continue;
        if (reg.blockOf(s) == blocks::Tnt && targets.tnt) { // set off with a short fuse (wiki: TNT)
            world.updateBlock(b, 0);
            changed.push_back(b);
            targets.tnt->prime(b, 10 + static_cast<int>(rng.nextInt(21)), rng);
            continue;
        }
        // A shulker box always drops, keeping what it holds (wiki: Shulker Box - its drop
        // isn't subject to explosion decay).
        uint32_t boxContents = 0;
        const bool shulker = reg.likeOf(reg.blockOf(s)) == blocks::ShulkerBox;
        if (shulker)
            if (const Chunk* ch = world.chunk(b.chunk()))
                if (const ChestData* cd = ch->chest(blockToLocal(b.x), b.y, blockToLocal(b.z)))
                    boxContents = addItemContents(cd->items);
        if (shulker) {
            ItemStack box{itemRegistry().blockItem(reg.blockOf(s)), 1};
            box.contents = boxContents;
            items.spawn({b.x + 0.5, b.y + 0.5, b.z + 0.5}, box, rng);
        } else if (targets.dropAll || rng.nextFloat() < 1.0f / power) { // its loot (mob explosions: 1 in power)
            m_loot.clear();
            blockDrops(s, {}, rng, m_loot, true);
            for (const ItemStack& st : m_loot)
                items.spawn({b.x + 0.5, b.y + 0.5, b.z + 0.5}, st, rng);
        }
        if (reg.blockOf(s) == blocks::Chest || reg.blockOf(s) == blocks::Barrel) // contents fall out (wiki: Chest)
            if (const Chunk* ch = world.chunk(b.chunk()))
                if (const ChestData* cd = ch->chest(blockToLocal(b.x), b.y, blockToLocal(b.z)))
                    for (const ItemStack& st : cd->items)
                        if (!st.empty()) items.spawn({b.x + 0.5, b.y + 0.5, b.z + 0.5}, st, rng);
        if (const Chunk* ch = world.chunk(b.chunk())) { // hoppers, dispensers, droppers too (M21.3)
            Chunk* mc = const_cast<Chunk*>(ch);
            if (const HopperData* h = mc->hopper(blockToLocal(b.x), b.y, blockToLocal(b.z)))
                for (const ItemStack& st : h->items)
                    if (!st.empty()) items.spawn({b.x + 0.5, b.y + 0.5, b.z + 0.5}, st, rng);
            if (const DispenserData* d = mc->dispenser(blockToLocal(b.x), b.y, blockToLocal(b.z)))
                for (const ItemStack& st : d->items)
                    if (!st.empty()) items.spawn({b.x + 0.5, b.y + 0.5, b.z + 0.5}, st, rng);
            if (const JukeboxData* j = mc->jukebox(blockToLocal(b.x), b.y, blockToLocal(b.z)); j && !j->record.empty())
                items.spawn({b.x + 0.5, b.y + 0.5, b.z + 0.5}, j->record, rng); // (M23 review: its disc)
            if (const CampfireData* cf = mc->campfire(blockToLocal(b.x), b.y, blockToLocal(b.z)))
                for (const ItemStack& st : cf->items)
                    if (!st.empty()) items.spawn({b.x + 0.5, b.y + 0.5, b.z + 0.5}, st, rng);
        }
        world.updateBlock(b, leftAfterBreaking(s)); // (waterlogged: the water stays)
        changed.push_back(b);
        ++destroyed;
    }
    return destroyed;
}

} // namespace mc
