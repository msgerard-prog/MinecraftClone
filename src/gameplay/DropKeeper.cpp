#include "gameplay/DropKeeper.h"

namespace mc {

using namespace world;

void DropKeeper::chunkLoaded(Chunk& c) {
    m_items.unpark(c, m_rng);
    m_orbs.unpark(c);
}

void DropKeeper::chunkUnloading(Chunk& c) {
    // (whatever the pools couldn't take back is still parked here: it stays and saves)
    const int kept = int(c.droppedItems().size() + c.droppedOrbs().size());
    const int drops = m_items.park(c) + m_orbs.park(c) + kept;
    if (drops > 0 || c.savedDrops > 0) c.markDirty();
}

void DropKeeper::beforeSave(World& world) {
    m_touched.clear();
    m_items.parkAll(world, m_touched);
    m_orbs.parkAll(world, m_touched);
    world.forEachChunk([&](Chunk& c) {
        if (!c.droppedItems().empty() || !c.droppedOrbs().empty() || c.savedDrops > 0) c.markDirty();
    });
}

void DropKeeper::afterSave(World& world) {
    world.forEachChunk([&](Chunk& c) { c.savedDrops = int(c.droppedItems().size() + c.droppedOrbs().size()); });
    for (const ChunkPos& p : m_touched)
        if (Chunk* c = world.chunk(p)) chunkLoaded(*c);
}

} // namespace mc
