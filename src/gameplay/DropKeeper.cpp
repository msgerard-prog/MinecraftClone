#include "gameplay/DropKeeper.h"

namespace mc {

using namespace world;

void DropKeeper::chunkLoaded(Chunk& c) {
    m_items.unpark(c, m_rng);
    m_orbs.unpark(c);
}

void DropKeeper::chunkUnloading(Chunk& c) {
    m_items.park(c); // (appended to whatever the pools couldn't take back, still parked here)
    m_orbs.park(c);
    if (c.dropsHash() != c.savedDropsHash) c.markDirty(); // (M31.3: only when they changed)
}

void DropKeeper::beforeSave(World& world) {
    m_touched.clear();
    m_items.parkAll(world, m_touched);
    m_orbs.parkAll(world, m_touched);
    world.forEachChunk([&](Chunk& c) {
        if (c.dropsHash() != c.savedDropsHash) c.markDirty(); // (M31.3: only when they changed)
    });
}

void DropKeeper::afterSave(World& world) {
    // (every chunk whose drops changed was written; the others hold what is on disk)
    world.forEachChunk([&](Chunk& c) {
        c.savedDropsHash = c.dropsHash();
    });
    for (const ChunkPos& p : m_touched)
        if (Chunk* c = world.chunk(p)) chunkLoaded(*c);
}

} // namespace mc
