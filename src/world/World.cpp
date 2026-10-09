#include "world/World.h"

#include "world/Beehives.h"
#include "world/Blocks.h"

namespace mc::world {

Chunk& World::createChunk(ChunkPos pos) {
    ++m_chunkEpoch;
    auto& slot = m_chunks[pos];
    slot = std::make_unique<Chunk>(pos, m_height);
    return *slot;
}

Chunk& World::insertChunk(std::unique_ptr<Chunk> chunk) {
    ++m_chunkEpoch;
    auto& slot = m_chunks[chunk->pos()];
    slot = std::move(chunk);
    slot->inTickingList = false;
    if (!slot->furnaces().empty() || !slot->mobs().empty() || !slot->blockTicks().empty() || !slot->spawners().empty() ||
        !slot->brewingStands().empty() || !slot->comparators().empty() || !slot->hoppers().empty() ||
        !slot->campfires().empty() || !slot->beacons().empty() || !slot->jukeboxes().empty() ||
        !slot->beehives().empty())
        markTicking(slot->pos());
    return *slot;
}

void World::markTicking(ChunkPos pos) {
    Chunk* c = chunk(pos);
    if (!c || c->inTickingList) return;
    c->inTickingList = true;
    m_ticking.push_back(pos);
}

std::unique_ptr<Chunk> World::removeChunk(ChunkPos pos) {
    const auto it = m_chunks.find(pos);
    if (it == m_chunks.end()) return nullptr;
    std::unique_ptr<Chunk> chunk = std::move(it->second);
    m_chunks.erase(it);
    ++m_chunkEpoch;
    if (chunk->inTickingList) { // a chunk loaded here again must not be listed twice
        std::erase(m_ticking, pos);
        chunk->inTickingList = false;
    }
    return chunk;
}

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
    Chunk* c = chunk(p.chunk());
    if (!c) return;
    const int x = blockToLocal(p.x), z = blockToLocal(p.z);
    const BlockId was = blockRegistry().blockOf(c->get(x, p.y, z));
    c->set(x, p.y, z, state);
    // Block entities follow their block (a furnace's contents are dropped by the
    // caller before it breaks it).
    const BlockId b = blockRegistry().blockOf(state);
    const auto isSign = [](BlockId id) {
        const BlockKind k = blockRegistry().kind(id);
        return k == BlockKind::Sign || k == BlockKind::WallSign || k == BlockKind::HangingSign ||
               k == BlockKind::WallHangingSign;
    };
    if (was != b && isSign(was)) c->removeBlockEntity(x, p.y, z); // (M23.3c)
    const auto isCampfire = [](BlockId id) { return id == blocks::Campfire || id == blocks::SoulCampfire; };
    if (was != b && isCampfire(was)) c->removeBlockEntity(x, p.y, z); // (M23.4c)
    if (b != was && isCampfire(b)) {
        c->addCampfire(x, p.y, z);
        markTicking(c->pos());
    }
    if (was != b && was == blocks::Jukebox) c->removeBlockEntity(x, p.y, z); // (M23.6)
    if (was != b && isSuspicious(was)) c->removeBlockEntity(x, p.y, z); // (M27.5: its loot goes)
    if (isSuspicious(b)) c->addBrushable(x, p.y, z);                    // (kept while it is dusted)
    const auto isHive = [](BlockId id) { return id == blocks::BeeNest || id == blocks::Beehive; };
    if (was != b && isHive(was)) { // (M26.3b) the bees inside come out angry
        if (BeehiveData* h = c->beehive(x, p.y, z)) releaseBees(*this, p, *h, true);
        c->removeBlockEntity(x, p.y, z);
    }
    if (b != was && isHive(b)) {
        c->addBeehive(x, p.y, z);
        markTicking(c->pos());
    }
    if (b != was && b == blocks::Jukebox) {
        c->addJukebox(x, p.y, z);
        markTicking(c->pos());
    }
    const auto isBeacon = [](BlockId id) { return id == blocks::Beacon || id == blocks::Conduit; };
    if (was != b && isBeacon(was)) c->removeBlockEntity(x, p.y, z); // (M23.6)
    if (b != was && isBeacon(b)) {
        c->addBeacon(x, p.y, z).conduit = b == blocks::Conduit;
        markTicking(c->pos());
    }
    const auto isBanner = [](BlockId id) {
        const BlockKind k = blockRegistry().kind(id);
        return k == BlockKind::Banner || k == BlockKind::WallBanner;
    };
    if (was != b && isBanner(was)) c->removeBlockEntity(x, p.y, z); // (M28.3d)
    if (b != was && isBanner(b)) c->addBanner(x, p.y, z);
    if (b != was && isSign(b)) {
        const BlockKind k = blockRegistry().kind(b);
        c->addSign(x, p.y, z).hanging = k == BlockKind::HangingSign || k == BlockKind::WallHangingSign;
    }
    // (M26.5b: copper chests are chests - `like` - and keep their contents as they age)
    const bool chestNow = blockRegistry().likeOf(b) == blocks::Chest;
    if (was != b && (blockRegistry().likeOf(was) == blocks::Furnace ||
                     (blockRegistry().likeOf(was) == blocks::Chest && !chestNow) || was == blocks::Barrel ||
                     (was == blocks::ChiseledBookshelf && b != blocks::ChiseledBookshelf) ||
                     (blockRegistry().likeOf(was) == blocks::Shelf && blockRegistry().likeOf(b) != blocks::Shelf) ||
                     blockRegistry().likeOf(was) == blocks::ShulkerBox || (was == blocks::Spawner && b != blocks::Spawner) ||
                     (was == blocks::TrialSpawner && b != blocks::TrialSpawner) ||
                     was == blocks::BrewingStand || was == blocks::Comparator || was == blocks::Hopper ||
                     ((was == blocks::Dispenser || was == blocks::Dropper) && b != blocks::Dispenser && b != blocks::Dropper) ||
                     (was == blocks::Crafter && b != blocks::Crafter)))
        c->removeBlockEntity(x, p.y, z); // replaced
    if (blockRegistry().likeOf(b) == blocks::Furnace) { // (smokers and blast furnaces too: M23.5)
        c->addFurnace(x, p.y, z).kind = b == blocks::Smoker ? 1 : b == blocks::BlastFurnace ? 2 : 0;
        markTicking(c->pos());
    } else if (chestNow || b == blocks::Barrel || blockRegistry().likeOf(b) == blocks::ShulkerBox ||
               b == blocks::ChiseledBookshelf || blockRegistry().likeOf(b) == blocks::Shelf) {
        if (c->chest(x, p.y, z) == nullptr) {
            ChestData& d = c->addChest(x, p.y, z);
            d.barrel = b == blocks::Barrel;
            d.shulker = blockRegistry().likeOf(b) == blocks::ShulkerBox;
            d.trapped = b == blocks::TrappedChest;      // (M29.5)
            d.bookshelf = b == blocks::ChiseledBookshelf;
            d.shelf = blockRegistry().likeOf(b) == blocks::Shelf; // (M29.6)
        }
    } else if (b == blocks::Spawner || b == blocks::TrialSpawner) {
        c->addSpawner(x, p.y, z).trial = b == blocks::TrialSpawner; // (M27.4d: kept through state changes)
        markTicking(c->pos());
    } else if (b == blocks::BrewingStand) {
        c->addBrewing(x, p.y, z);
        markTicking(c->pos());
    } else if (b == blocks::Comparator) { // (ticking: they watch containers, M21.2)
        c->addComparator(x, p.y, z);
        markTicking(c->pos());
    } else if (b == blocks::Hopper) { // (ticking: they move items, M21.3)
        c->addHopper(x, p.y, z);
        markTicking(c->pos());
    } else if (b == blocks::Dispenser || b == blocks::Dropper || b == blocks::Crafter) {
        if (c->dispenser(x, p.y, z) == nullptr) { // (kept while its state changes)
            DispenserData& d = c->addDispenser(x, p.y, z);
            d.dropper = b == blocks::Dropper;
            d.crafter = b == blocks::Crafter; // (M29.5)
        }
    }
}

} // namespace mc::world
