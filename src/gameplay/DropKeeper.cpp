#include "gameplay/DropKeeper.h"

#include "gameplay/FallingBlocks.h"
#include "gameplay/PrimedTnt.h"
#include "gameplay/Projectiles.h"

#include <cmath>

namespace mc {

using namespace world;

void DropKeeper::chunkLoaded(Chunk& c) {
    m_items.unpark(c, m_rng);
    m_orbs.unpark(c);
    unparkEntities(c);
    c.savedInhabitedTicks = c.inhabitedTicks;
}

void DropKeeper::chunkUnloading(Chunk& c) {
    m_items.park(c); // (appended to whatever the pools couldn't take back, still parked here)
    m_orbs.park(c);
    parkEntities(nullptr, &c);
    if (c.dropsHash() != c.savedDropsHash || c.inhabitedChanged())
        c.markDirty(); // (M31.3: only when they changed)
}

void DropKeeper::beforeSave(World& world) {
    m_touched.clear();
    m_items.parkAll(world, m_touched);
    m_orbs.parkAll(world, m_touched);
    parkEntities(&world, nullptr);
    world.forEachChunk([&](Chunk& c) {
        if (c.dropsHash() != c.savedDropsHash || c.inhabitedChanged())
            c.markDirty(); // (M31.3: only when they changed)
    });
}

void DropKeeper::afterSave(World& world) {
    // (every chunk whose drops changed was written; the others hold what is on disk)
    world.forEachChunk([&](Chunk& c) {
        c.savedDropsHash = c.dropsHash();
        if (!c.dirty()) c.savedInhabitedTicks = c.inhabitedTicks; // (written: clearDirty ran)
    });
    for (const ChunkPos& p : m_touched)
        if (Chunk* c = world.chunk(p)) chunkLoaded(*c);
}

namespace {

ChunkPos chunkAt(const glm::dvec3& p) {
    return {blockToChunk(int(std::floor(p.x))), blockToChunk(int(std::floor(p.z)))};
}

} // namespace

void DropKeeper::parkEntities(World* world, Chunk* only) {
    using K = Chunk::ParkedEntity::Kind;
    // The chunk an entity at `p` parks in (null: leave it in its pool).
    auto target = [&](const glm::dvec3& p) -> Chunk* {
        const ChunkPos at = chunkAt(p);
        if (only) return at == only->pos() ? only : nullptr;
        Chunk* c = world->chunk(at);
        if (c && !c->holdsParked()) m_touched.push_back(at); // (put back after the save)
        return c;
    };
    if (m_projectiles) {
        auto& v = m_projectiles->mutableItems();
        for (size_t i = 0; i < v.size();) {
            const Projectile& p = v[i];
            // (arrows and tridents only: other things in flight are lost, as before)
            Chunk* c = p.kind == ProjectileKind::Arrow || p.kind == ProjectileKind::Trident
                           ? target(p.pos)
                           : nullptr;
            if (!c) {
                ++i;
                continue;
            }
            Chunk::ParkedEntity e;
            e.kind = p.kind == ProjectileKind::Trident ? K::Trident : K::Arrow;
            e.pos = p.pos;
            e.vel = p.vel;
            e.facing = p.facing;
            e.stack = p.stack;
            e.time = p.life;
            e.shooter = p.shooter;
            e.potion = p.potion;
            e.pierce = p.pierce;
            e.power = p.power;
            e.punch = p.punch;
            e.stuck = p.stuck;
            e.pickup = p.pickup;
            e.critical = p.critical;
            e.fromPlayer = p.fromPlayer;
            e.dealt = p.dealt;
            e.spectral = p.spectral;
            e.flame = p.flame;
            e.hitEffect = uint8_t(p.hitEffect.effect);
            e.hitTicks = p.hitEffect.ticks;
            e.skeleton = p.skeleton;
            c->parkedEntities().push_back(e);
            v[i] = v.back();
            v.pop_back();
        }
    }
    if (m_tnt) {
        auto& v = m_tnt->mutableItems();
        for (size_t i = 0; i < v.size();) {
            Chunk* c = target(v[i].pos);
            if (!c) {
                ++i;
                continue;
            }
            Chunk::ParkedEntity e;
            e.kind = K::Tnt;
            e.pos = v[i].pos;
            e.vel = v[i].vel;
            e.time = v[i].fuse;
            c->parkedEntities().push_back(e);
            v[i] = v.back();
            v.pop_back();
        }
    }
    if (m_falling) {
        auto& v = m_falling->mutableBlocks();
        for (size_t i = 0; i < v.size();) {
            Chunk* c = target(v[i].pos);
            if (!c) {
                ++i;
                continue;
            }
            Chunk::ParkedEntity e;
            e.kind = K::FallingBlock;
            e.pos = v[i].pos;
            e.vel = v[i].vel;
            e.time = v[i].time;
            e.state = v[i].state;
            e.startY = v[i].startY;
            c->parkedEntities().push_back(e);
            v[i] = v.back();
            v.pop_back();
        }
    }
}

void DropKeeper::unparkEntities(Chunk& c) {
    using K = Chunk::ParkedEntity::Kind;
    auto& parked = c.parkedEntities();
    size_t kept = 0; // (what a full pool can't take stays parked)
    for (size_t i = 0; i < parked.size(); ++i) {
        const Chunk::ParkedEntity& e = parked[i];
        bool taken = false;
        if ((e.kind == K::Arrow || e.kind == K::Trident) && m_projectiles &&
            m_projectiles->mutableItems().size() < size_t(Projectiles::kMax)) {
            Projectile p;
            p.kind = e.kind == K::Trident ? ProjectileKind::Trident : ProjectileKind::Arrow;
            p.pos = p.prevPos = e.pos;
            p.vel = e.vel;
            p.facing = e.facing;
            p.stack = e.stack;
            p.life = e.time;
            p.shooter = p.owner = e.shooter;
            p.potion = e.potion;
            p.pierce = e.pierce;
            p.power = e.power;
            p.punch = e.punch;
            p.stuck = e.stuck;
            p.pickup = e.pickup;
            p.critical = e.critical;
            p.fromPlayer = e.fromPlayer;
            p.dealt = e.dealt;
            p.spectral = e.spectral;
            p.flame = e.flame;
            p.hitEffect = {static_cast<world::Effect>(e.hitEffect), e.hitTicks};
            p.skeleton = e.skeleton;
            m_projectiles->mutableItems().push_back(p);
            taken = true;
        } else if (e.kind == K::Tnt && m_tnt &&
                   m_tnt->mutableItems().size() < size_t(PrimedTnt::kMax)) {
            PrimedTntEntity t;
            t.pos = t.prevPos = e.pos;
            t.vel = e.vel;
            t.fuse = e.time;
            m_tnt->mutableItems().push_back(t);
            taken = true;
        } else if (e.kind == K::FallingBlock && m_falling &&
                   m_falling->mutableBlocks().size() < size_t(FallingBlocks::kMax)) {
            FallingBlock f;
            f.pos = f.prevPos = e.pos;
            f.vel = e.vel;
            f.time = e.time;
            f.state = e.state;
            f.startY = e.startY;
            m_falling->mutableBlocks().push_back(f);
            taken = true;
        }
        if (!taken) parked[kept++] = e;
    }
    parked.resize(kept);
}

} // namespace mc
