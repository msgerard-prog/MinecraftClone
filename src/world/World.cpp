#include "world/World.h"

namespace mc::world {

Chunk& World::createChunk(ChunkPos pos) {
    auto& slot = m_chunks[pos];
    slot = std::make_unique<Chunk>(pos);
    return *slot;
}

Chunk& World::insertChunk(std::unique_ptr<Chunk> chunk) {
    auto& slot = m_chunks[chunk->pos()];
    slot = std::move(chunk);
    return *slot;
}

void World::removeChunk(ChunkPos pos) { m_chunks.erase(pos); }

Chunk* World::chunk(ChunkPos pos) {
    const auto it = m_chunks.find(pos);
    return it == m_chunks.end() ? nullptr : it->second.get();
}

const Chunk* World::chunk(ChunkPos pos) const {
    const auto it = m_chunks.find(pos);
    return it == m_chunks.end() ? nullptr : it->second.get();
}

BlockStateId World::getBlock(const BlockPos& p) const {
    const Chunk* c = chunk(p.chunk());
    return c ? c->get(blockToLocal(p.x), p.y, blockToLocal(p.z)) : BlockStateId{0};
}

void World::setBlock(const BlockPos& p, BlockStateId state) {
    if (Chunk* c = chunk(p.chunk())) c->set(blockToLocal(p.x), p.y, blockToLocal(p.z), state);
}

} // namespace mc::world
