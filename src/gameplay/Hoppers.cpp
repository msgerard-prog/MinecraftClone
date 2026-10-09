#include "gameplay/Hoppers.h"

#include "gameplay/Brewing.h"
#include "gameplay/Jukebox.h"
#include "gameplay/Recipes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

namespace mc {

using namespace world;

namespace {

Direction oppositeOf(Direction d) { return static_cast<Direction>(static_cast<int>(d) ^ 1); }
int maxStackOf(const ItemStack& s) {
    return std::max(1, int(itemRegistry().item(s.item).maxStack));
}

// Puts one item into the first slot of `slots` it merges with, else the first empty one.
// Puts one item into the leftmost slot that takes it: a matching stack with room or an
// empty slot (wiki: Hopper - "leftmost available slot").
template <size_t N> bool putIn(std::array<ItemStack, N>& slots, const ItemStack& one) {
    for (ItemStack& t : slots) {
        if (t.empty()) {
            t = one;
            t.count = 1;
            return true;
        }
        if (t.sameKind(one) && t.count < maxStackOf(t)) {
            ++t.count;
            return true;
        }
    }
    return false;
}
template <size_t N> bool fits(const std::array<ItemStack, N>& slots, const ItemStack& one) {
    for (const ItemStack& t : slots)
        if (t.empty() || (t.sameKind(one) && t.count < maxStackOf(t))) return true;
    return false;
}
bool putInto(ItemStack& t, const ItemStack& one) {
    if (t.empty()) {
        t = one;
        t.count = 1;
        return true;
    }
    if (t.sameKind(one) && t.count < maxStackOf(t)) {
        ++t.count;
        return true;
    }
    return false;
}
ItemStack* g_lastSlot = nullptr;   // (the slot the last take came from; main thread only)
BlockUpdates* g_updates = nullptr; // (composters; main thread only)
bool takeFrom(ItemStack& s, ItemStack& out) {
    if (s.empty()) return false;
    out = s;
    out.count = 1;
    if (--s.count == 0) s = {};
    g_lastSlot = &s;
    return true;
}
template <size_t N> bool takeFirst(std::array<ItemStack, N>& slots, ItemStack& out) {
    for (ItemStack& s : slots)
        if (takeFrom(s, out)) return true;
    return false;
}

} // namespace

void setHopperBlockUpdates(BlockUpdates* updates) { g_updates = updates; }

bool isContainer(const World& world, const BlockPos& p) {
    const BlockId b =
        blockRegistry().likeOf(blockRegistry().blockOf(world.getBlock(p))); // (smokers: furnaces)
    return ((b == blocks::Composter || b == blocks::Jukebox) && g_updates) ||
           b == blocks::ShulkerBox || b == blocks::Chest || b == blocks::Barrel ||
           b == blocks::Hopper || b == blocks::Dispenser || b == blocks::Dropper ||
           b == blocks::Furnace || b == blocks::BrewingStand || (b == blocks::ChiseledBookshelf && g_updates);
}

bool insertOne(World& world, const BlockPos& p, Direction from, const ItemStack& one) {
    Chunk* c = world.chunk(p.chunk());
    if (!c || one.empty()) return false;
    const int x = blockToLocal(p.x), z = blockToLocal(p.z);
    const BlockId b = blockRegistry().likeOf(blockRegistry().blockOf(world.getBlock(p)));
    bool ok = false;
    switch (b) {
    case blocks::Jukebox: // a disc into an empty jukebox starts it (wiki: Jukebox, M23 review)
        if (JukeboxData* d = c->jukebox(x, p.y, z);
            d && g_updates && d->record.empty() && discIndex(one.item) >= 0) {
            d->record = one;
            d->record.count = 1;
            d->ticks = 0;
            d->playing = true;
            world.updateBlock(p, blockRegistry().set(world.getBlock(p), properties::hasRecord, 0));
            g_updates->jukeboxChanged(p);
            c->markDirty();
            ok = true;
        }
        break;
    case blocks::ChiseledBookshelf: // (M29.5) books into the first free slot
        if (g_updates)
            for (int i = 0; i < 6 && !ok; ++i) ok = g_updates->putBook(p, i, one);
        break;
    case blocks::Composter: // only from above, while it takes compost (wiki: Composter)
        ok = g_updates && from == Direction::Up && g_updates->compost(p, one.item);
        break;
    case blocks::ShulkerBox: // (M23.6: like a barrel, but never another shulker box - wiki)
        if (const BlockId ib = itemRegistry().item(one.item).block;
            ib != 0 && blockRegistry().likeOf(ib) == blocks::ShulkerBox)
            break;
        [[fallthrough]];
    case blocks::Barrel: // (M23.5: 27 slots like a chest)
        if (ChestData* d = c->chest(x, p.y, z)) ok = putIn(d->items, one);
        break;
    case blocks::Chest:
        if (ChestData* d = c->chest(x, p.y, z)) ok = putIn(d->items, one);
        if (!ok)
            if (const auto partner = BlockUpdates::chestPartner(world, p))
                if (Chunk* pc = world.chunk(partner->chunk()))
                    if (ChestData* d = pc->chest(blockToLocal(partner->x), partner->y,
                                                 blockToLocal(partner->z))) {
                        ok = putIn(d->items, one);
                        if (ok) pc->markDirty();
                    }
        break;
    case blocks::Hopper:
        if (HopperData* d = c->hopper(x, p.y, z)) {
            bool wasEmpty = true;
            for (const ItemStack& st : d->items)
                wasEmpty = wasEmpty && st.empty();
            ok = putIn(d->items, one);
            if (ok && wasEmpty)
                d->cooldown = 7; // (wiki: so an item doesn't cross a chain in one tick)
        }
        break;
    case blocks::Dispenser:
    case blocks::Dropper:
        if (DispenserData* d = c->dispenser(x, p.y, z)) ok = putIn(d->items, one);
        break;
    case blocks::Furnace:
        if (FurnaceData* d = c->furnace(x, p.y, z)) {
            if (from == Direction::Up)
                ok = putInto(d->input, one);
            else if (fuelTicks(one) > 0)
                ok = putInto(d->fuel, one);
        }
        break;
    case blocks::BrewingStand:
        if (BrewingData* d = c->brewing(x, p.y, z)) {
            static const ItemId powder = *itemRegistry().find("blaze_powder");
            static const ItemId potion = *itemRegistry().find("potion"),
                                splash = *itemRegistry().find("splash_potion");
            if (from == Direction::Up) {
                if (isBrewingIngredient(one)) ok = putInto(d->ingredient, one);
            } else if (one.item == powder) {
                ok = putInto(d->fuel, one);
            } else if (one.item == potion || one.item == splash) {
                for (ItemStack& bottle : d->bottles)
                    if (bottle.empty()) {
                        bottle = one;
                        bottle.count = 1;
                        ok = true;
                        break;
                    }
            }
        }
        break;
    default:
        break;
    }
    if (ok) c->markDirty();
    return ok;
}

bool extractOne(World& world, const BlockPos& p, Direction from, ItemStack& out,
                ItemStack** fromSlot) {
    g_lastSlot = nullptr;
    Chunk* c = world.chunk(p.chunk());
    if (!c) return false;
    const int x = blockToLocal(p.x), z = blockToLocal(p.z);
    const BlockId b = blockRegistry().likeOf(blockRegistry().blockOf(world.getBlock(p)));
    bool ok = false;
    switch (b) {
    case blocks::Jukebox: // the disc, once its song has ended, out of the bottom
        if (JukeboxData* d = c->jukebox(x, p.y, z);
            d && g_updates && from == Direction::Down && !d->playing && !d->record.empty()) {
            out = d->record;
            d->record = {};
            world.updateBlock(p, blockRegistry().set(world.getBlock(p), properties::hasRecord, 1));
            c->markDirty();
            ok = true;
        }
        break;
    case blocks::ChiseledBookshelf: // (M29.5) the first book
        if (g_updates)
            for (int i = 0; i < 6 && !ok; ++i) {
                out = g_updates->takeBook(p, i);
                ok = !out.empty();
            }
        break;
    case blocks::Composter: // its bone meal, only out of the bottom
        if (g_updates && from == Direction::Down) {
            out = g_updates->takeCompost(p);
            ok = !out.empty();
        }
        break;
    case blocks::Barrel:
    case blocks::ShulkerBox:
        if (ChestData* d = c->chest(x, p.y, z)) ok = takeFirst(d->items, out);
        break;
    case blocks::Chest:
        if (ChestData* d = c->chest(x, p.y, z)) ok = takeFirst(d->items, out);
        if (!ok)
            if (const auto partner = BlockUpdates::chestPartner(world, p))
                if (Chunk* pc = world.chunk(partner->chunk()))
                    if (ChestData* d = pc->chest(blockToLocal(partner->x), partner->y,
                                                 blockToLocal(partner->z))) {
                        ok = takeFirst(d->items, out);
                        if (ok) pc->markDirty();
                    }
        break;
    case blocks::Hopper:
        if (HopperData* d = c->hopper(x, p.y, z)) ok = takeFirst(d->items, out);
        break;
    case blocks::Dispenser:
    case blocks::Dropper:
        if (DispenserData* d = c->dispenser(x, p.y, z)) ok = takeFirst(d->items, out);
        break;
    case blocks::Furnace:
        if (FurnaceData* d = c->furnace(x, p.y, z); d && from == Direction::Down)
            ok = takeFrom(d->output, out);
        break;
    case blocks::BrewingStand:
        if (BrewingData* d = c->brewing(x, p.y, z); d && from == Direction::Down)
            for (ItemStack& bottle : d->bottles)
                if (takeFrom(bottle, out)) {
                    ok = true;
                    break;
                }
        break;
    default:
        break;
    }
    if (ok) c->markDirty();
    if (ok && fromSlot) *fromSlot = g_lastSlot;
    return ok;
}

void tickHoppers(World& world, ItemEntities& items) {
    const auto& r = blockRegistry();
    bool tookItems = false;
    // Which dropped items lie over a hopper (one pass: a hopper at (x, y, z) takes items
    // with y + 0.5 <= item y < y + 2 over its column).
    struct Pickup {
        BlockPos hopper;
        int item;
    };
    static std::array<Pickup, 1024> pickups;
    int pickupCount = 0;
    const auto& all = items.items();
    for (size_t i = 0; i < all.size() && pickupCount < int(pickups.size()); ++i) {
        const glm::dvec3 q = all[i].pos;
        const int bx = int(std::floor(q.x)), bz = int(std::floor(q.z)),
                  by = int(std::floor(q.y - 0.5));
        for (const int y : {by, by - 1})
            if (y + 0.5 <= q.y && q.y < y + 2.0 &&
                r.blockOf(world.getBlock({bx, y, bz})) == blocks::Hopper) {
                pickups[size_t(pickupCount++)] = {{bx, y, bz}, int(i)};
                break;
            }
    }
    world.forEachTickingChunk([&](Chunk& chunk) {
        for (auto& e : chunk.hoppers()) {
            HopperData& h = e.data;
            if (h.cooldown > 0 && --h.cooldown > 0)
                continue; // (moves on the tick it reaches 0: every 8)
            const BlockPos p{chunk.pos().x * 16 + e.x, e.y, chunk.pos().z * 16 + e.z};
            const BlockStateId s = chunk.get(e.x, e.y, e.z);
            if (r.blockOf(s) != blocks::Hopper || r.get(s, properties::enabled) != 0)
                continue; // (powered: off)
            static constexpr Direction kOut[5] = {Direction::Down, Direction::North,
                                                  Direction::South, Direction::West,
                                                  Direction::East}; // its facing values
            const Direction out = kOut[r.get(s, properties::hopperFacing)];
            bool moved = false;
            // Push: the first stack's item into the container it points into.
            const BlockPos target{p.x + kDirectionNormals[int(out)].x,
                                  p.y + kDirectionNormals[int(out)].y,
                                  p.z + kDirectionNormals[int(out)].z};
            for (ItemStack& st : h.items) // the leftmost stack that goes in
                if (!st.empty() && insertOne(world, target, oppositeOf(out), st)) {
                    if (--st.count == 0) st = {};
                    moved = true;
                    break;
                }
            // Pull: one item from the container above, or dropped items over it.
            const BlockPos above{p.x, p.y + 1, p.z};
            if (isContainer(world, above)) {
                ItemStack one;
                ItemStack* from = nullptr;
                if (extractOne(world, above, Direction::Down, one, &from)) {
                    if (putIn(h.items, one)) {
                        moved = true;
                    } else if (from) { // it doesn't fit here: back where it came from
                        if (from->empty())
                            *from = one;
                        else
                            ++from->count;
                    } else if (r.blockOf(world.getBlock(above)) ==
                               blocks::Composter) { // (the bone meal stays)
                        world.updateBlock(
                            above, r.set(world.getBlock(above), properties::composterLevel, 8));
                    } else if (r.blockOf(world.getBlock(above)) ==
                               blocks::Jukebox) { // (the disc stays)
                        if (Chunk* jc = world.chunk(above.chunk()))
                            if (JukeboxData* jd = jc->jukebox(blockToLocal(above.x), above.y,
                                                              blockToLocal(above.z))) {
                                jd->record = one;
                                world.updateBlock(
                                    above, r.set(world.getBlock(above), properties::hasRecord, 0));
                            }
                    }
                }
            } else if (!r.opaqueCube(world.getBlock(
                           above))) { // (only containers and full blocks stop pickup)
                for (int k = 0; k < pickupCount; ++k) {
                    if (!(pickups[size_t(k)].hopper == p)) continue;
                    ItemEntity& it = items.mutableItems()[size_t(pickups[size_t(k)].item)];
                    if (it.stack.empty() || !fits(h.items, it.stack)) continue;
                    while (it.stack.count > 0 &&
                           putIn(h.items, it.stack)) { // the whole stack, as it fits
                        --it.stack.count;
                        moved = true;
                    }
                    if (it.stack.count == 0) {
                        it.stack = {};
                        tookItems = true;
                    }
                    break;
                }
            }
            if (moved) {
                h.cooldown = 8; // (wiki: 8 game ticks between transfers)
                chunk.markDirty();
            }
        }
    });
    if (tookItems) items.sweepEmpty();
}

} // namespace mc
