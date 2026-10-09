// Trial spawners (M27.4d; wiki: Trial Spawner). Part of Mobs.
//
// A trial spawner wakes when a player comes within 14 blocks and sends out its mobs - 6
// for one player - at most 2 at a time, one every 2 s, near it. When every one of them is
// beaten it ejects a trial key or (half the time) a consumable, and rests for 30 minutes.
#include "gameplay/Mobs.h"

#include "gameplay/ItemEntities.h"
#include "gameplay/Player.h"
#include "world/Blocks.h"
#include "world/Loot.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

// Room for a mob: a solid block under two free cells (no fluids).
bool roomAt(const World& w, int x, int y, int z) {
    const auto& r = blockRegistry();
    if (!r.collides(w.getBlock({x, y - 1, z}))) return false;
    for (int dy = 0; dy < 2; ++dy) {
        const BlockStateId s = w.getBlock({x, y + dy, z});
        if (r.collides(s) || r.blockOf(s) == blocks::Water || r.blockOf(s) == blocks::Lava)
            return false;
    }
    return true;
}

} // namespace

void Mobs::tickTrialSpawner(Context& ctx, Chunk& chunk, const BlockPos& p, SpawnerData& s) {
    const auto& r = blockRegistry();
    const BlockStateId state = ctx.world.getBlock(p);
    if (r.blockOf(state) != blocks::TrialSpawner) return;
    auto setState = [&](int st) {
        if (r.get(state, properties::trialState) != st)
            ctx.world.updateBlock(p, r.set(state, properties::trialState, st));
    };
    const glm::dvec3 centre(p.x + 0.5, p.y + 0.5, p.z + 0.5);
    const glm::dvec3 player = ctx.player.position();
    const bool near = !ctx.playerDead && ctx.survival &&
                      glm::dot(player - centre, player - centre) <= 14.0 * 14.0;
    // (saved when something changes; while resting, every minute - M27 review)
    const SpawnerData before = s;
    struct Dirty {
        Chunk& c;
        const SpawnerData& was;
        const SpawnerData& now;
        ~Dirty() {
            if (was.total != now.total || was.spawned != now.spawned ||
                (was.cooldown > 0) != (now.cooldown > 0) ||
                (now.cooldown > 0 && now.cooldown % 1200 == 0))
                c.markDirty();
        }
    } dirty{chunk, before, s};
    // Trial Omen (M28.4d; wiki: Trial Omen, Ominous Trial): Bad Omen turns into it near a
    // trial spawner (15 minutes a level); with it, idle spawners nearby turn ominous - and,
    // ours, the vaults within 16 blocks too - and their reward is an ominous trial key.
    if (near && ctx.vitals.effectLevel(Effect::BadOmen) > 0) {
        const int level = ctx.vitals.effectLevel(Effect::BadOmen);
        ctx.vitals.removeEffect(Effect::BadOmen);
        ctx.vitals.addEffect(Effect::TrialOmen, 0, 18000 * level);
    }
    const bool ominousNow = r.get(state, properties::ominous) == 0;
    if (near && !ominousNow && ctx.vitals.effectLevel(Effect::TrialOmen) > 0 && s.spawned == 0 &&
        s.total == 0) {
        ctx.world.updateBlock(p, r.set(state, properties::ominous, 0));
        s.cooldown = 0;
        for (int dy = -8; dy <= 8; ++dy)
            for (int dz = -16; dz <= 16; ++dz)
                for (int dx = -16; dx <= 16; ++dx) {
                    const BlockPos q{p.x + dx, p.y + dy, p.z + dz};
                    const BlockStateId vs = ctx.world.getBlock(q);
                    if (r.blockOf(vs) == blocks::Vault && r.get(vs, properties::ominous) != 0)
                        ctx.world.updateBlock(q, r.set(vs, properties::ominous, 0));
                }
        return;
    }
    if (s.cooldown > 0) { // resting after its reward
        if (--s.cooldown == 0) {
            setState(1);
            if (ominousNow)
                ctx.world.updateBlock(
                    p, r.set(ctx.world.getBlock(p), properties::ominous, 1)); // (back to normal)
        } else {
            setState(5);
        }
        return;
    }
    if (s.total == 0) { // waiting for a player
        if (!near) {
            setState(1);
            return;
        }
        s.total = 6; // (one player: 6; vanilla adds 2 per extra player)
        s.spawned = 0;
        s.delay = 20;
        setState(2);
        return;
    }
    // A round: its living mobs (tagged with it as their home).
    int alive = 0;
    const ChunkPos c = p.chunk();
    for (int dz = -3; dz <= 3; ++dz) // (they may chase a player a little way)
        for (int dx = -3; dx <= 3; ++dx)
            if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                for (const MobData& m : ch->mobs())
                    alive +=
                        m.health > 0.0f && m.home == glm::ivec3(p.x, p.y, p.z) && m.type == s.mob;
    for (const MobData& m : m_births)
        alive += m.home == glm::ivec3(p.x, p.y, p.z);
    // (M29.7e; wiki: Ominous Item Spawner) an ominous trial now and then drops arrows on the
    // player from 3 blocks above (ours: 1 in 160 ticks; vanilla also potions and fire charges).
    if (ominousNow && near && s.spawned > 0 && ctx.rng.nextInt(160) == 0 && m_births.size() < m_births.capacity()) {
        MobData o = make(MobType::OminousItemSpawner, ctx.player.position() + glm::dvec3(0.0, 3.0, 0.0), ctx.rng);
        o.commandId = *itemRegistry().find("arrow");
        o.eggTicks = 60;
        m_births.push_back(o);
    }
    if (s.spawned < s.total) {
        if (alive < 2 && --s.delay <= 0) { // (wiki: 2 at a time for one player)
            for (int tries = 0; tries < 8; ++tries) {
                const int x = p.x + int(ctx.rng.nextInt(9)) - 4,
                          z = p.z + int(ctx.rng.nextInt(9)) - 4;
                const int y = p.y + int(ctx.rng.nextInt(3)) - 1;
                if (!roomAt(ctx.world, x, y, z)) continue;
                MobData m = make(s.mob, {x + 0.5, double(y), z + 0.5}, ctx.rng);
                m.home = {p.x, p.y, p.z};
                m.persistent = true;
                if (m_births.size() >= m_births.capacity()) break; // (hard rule 1: next tick)
                m_births.push_back(m);
                ++s.spawned;
                ctx.world.levelEvent(LevelEvent::Type::MobDeath, x + 0.5, y, z + 0.5,
                                     60 | 180 << 16); // (a puff)
                break;
            }
            s.delay = 40;
        }
        return;
    }
    if (alive > 0) {
        setState(3); // (waiting for the last of them)
        return;
    }
    // Beaten: the key and a reward, then the rest.
    static const ItemId plainKey = *itemRegistry().find("trial_key"),
                        ominousKey = *itemRegistry().find("ominous_trial_key");
    const ItemId key = ominousNow ? ominousKey : plainKey; // (M28.4d)
    const glm::dvec3 out = centre + glm::dvec3(0.0, 0.8, 0.0);
    // (wiki: Trial Spawner › Loot - half the time a trial key, else one consumable)
    if (ctx.rng.nextInt(2) == 0)
        ctx.items.spawn(out, {key, 1}, ctx.rng);
    else if (const ItemStack it = rollOne(LootTable::TrialReward, ctx.rng); !it.empty())
        ctx.items.spawn(out, it, ctx.rng);
    s.total = s.spawned = 0;
    s.cooldown = 36000;
    setState(5);
}

} // namespace mc
