#include "gameplay/Hoppers.h"

#include "gameplay/Brewing.h"
#include "gameplay/Recipes.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"

namespace mc {

using namespace world;

namespace {

Direction oppositeOf(Direction d) { return static_cast<Direction>(static_cast<int>(d) ^ 1); }
int maxStackOf(const ItemStack& s) { return std::max(1, int(itemRegistry().item(s.item).maxStack)); }

// Puts one item into the first slot of `slots` it merges with, else the first empty one.
template <size_t N>
bool putIn(std::array<ItemStack, N>& slots, const ItemStack& one) {
    for (ItemStack& t : slots)
        if (!t.empty() && t.sameKind(one) && t.count < maxStackOf(t)) {
            ++t.count;
            return true;
        }
    for (ItemStack& t : slots)
        if (t.empty()) {
            t = one;
            t.count = 1;
            return true;
        }
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
bool takeFrom(ItemStack& s, ItemStack& out) {
    if (s.empty()) return false;
    out = s;
    out.count = 1;
    if (--s.count == 0) s = {};
    return true;
}
template <size_t N>
bool takeFirst(std::array<ItemStack, N>& slots, ItemStack& out) {
    for (ItemStack& s : slots)
        if (takeFrom(s, out)) return true;
    return false;
}

} // namespace

bool isContainer(const World& world, const BlockPos& p) {
    const BlockId b = blockRegistry().blockOf(world.getBlock(p));
    return b == blocks::Chest || b == blocks::Hopper || b == blocks::Dispenser || b == blocks::Dropper ||
           b == blocks::Furnace || b == blocks::BrewingStand;
}

bool insertOne(World& world, const BlockPos& p, Direction from, const ItemStack& one) {
    Chunk* c = world.chunk(p.chunk());
    if (!c || one.empty()) return false;
    const int x = blockToLocal(p.x), z = blockToLocal(p.z);
    const BlockId b = blockRegistry().blockOf(world.getBlock(p));
    bool ok = false;
    switch (b) {
    case blocks::Chest:
        if (ChestData* d = c->chest(x, p.y, z)) ok = putIn(d->items, one);
        if (!ok)
            if (const auto partner = BlockUpdates::chestPartner(world, p))
                if (Chunk* pc = world.chunk(partner->chunk()))
                    if (ChestData* d = pc->chest(blockToLocal(partner->x), partner->y, blockToLocal(partner->z))) {
                        ok = putIn(d->items, one);
                        if (ok) pc->markDirty();
                    }
        break;
    case blocks::Hopper:
        if (HopperData* d = c->hopper(x, p.y, z)) ok = putIn(d->items, one);
        break;
    case blocks::Dispenser:
    case blocks::Dropper:
        if (DispenserData* d = c->dispenser(x, p.y, z)) ok = putIn(d->items, one);
        break;
    case blocks::Furnace:
        if (FurnaceData* d = c->furnace(x, p.y, z)) {
            if (from == Direction::Up) ok = putInto(d->input, one);
            else if (fuelTicks(one) > 0) ok = putInto(d->fuel, one);
        }
        break;
    case blocks::BrewingStand:
        if (BrewingData* d = c->brewing(x, p.y, z)) {
            static const ItemId powder = *itemRegistry().find("blaze_powder");
            static const ItemId potion = *itemRegistry().find("potion"), splash = *itemRegistry().find("splash_potion");
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

bool extractOne(World& world, const BlockPos& p, Direction from, ItemStack& out) {
    Chunk* c = world.chunk(p.chunk());
    if (!c) return false;
    const int x = blockToLocal(p.x), z = blockToLocal(p.z);
    const BlockId b = blockRegistry().blockOf(world.getBlock(p));
    bool ok = false;
    switch (b) {
    case blocks::Chest:
        if (ChestData* d = c->chest(x, p.y, z)) ok = takeFirst(d->items, out);
        if (!ok)
            if (const auto partner = BlockUpdates::chestPartner(world, p))
                if (Chunk* pc = world.chunk(partner->chunk()))
                    if (ChestData* d = pc->chest(blockToLocal(partner->x), partner->y, blockToLocal(partner->z))) {
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
        if (FurnaceData* d = c->furnace(x, p.y, z); d && from == Direction::Down) ok = takeFrom(d->output, out);
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
    return ok;
}

void tickHoppers(World& world, ItemEntities& items) {
    const auto& r = blockRegistry();
    bool tookItems = false;
    world.forEachTickingChunk([&](Chunk& chunk) {
        for (auto& e : chunk.hoppers()) {
            HopperData& h = e.data;
            if (h.cooldown > 0) {
                --h.cooldown;
                continue;
            }
            const BlockPos p{chunk.pos().x * 16 + e.x, e.y, chunk.pos().z * 16 + e.z};
            const BlockStateId s = chunk.get(e.x, e.y, e.z);
            if (r.blockOf(s) != blocks::Hopper || r.get(s, properties::enabled) != 0) continue; // (powered: off)
            static constexpr Direction kOut[5] = {Direction::Down, Direction::North, Direction::South, Direction::West,
                                                  Direction::East}; // its facing values
            const Direction out = kOut[r.get(s, properties::hopperFacing)];
            bool moved = false;
            // Push: the first stack's item into the container it points into.
            const BlockPos target{p.x + kDirectionNormals[int(out)].x, p.y + kDirectionNormals[int(out)].y,
                                  p.z + kDirectionNormals[int(out)].z};
            for (ItemStack& st : h.items)
                if (!st.empty()) {
                    if (insertOne(world, target, oppositeOf(out), st)) {
                        if (--st.count == 0) st = {};
                        moved = true;
                    }
                    break;
                }
            // Pull: one item from the container above, or dropped items over it.
            const BlockPos above{p.x, p.y + 1, p.z};
            if (isContainer(world, above)) {
                ItemStack one;
                bool room = false;
                for (const ItemStack& st : h.items)
                    room = room || st.empty() || st.count < maxStackOf(st);
                if (room && extractOne(world, above, Direction::Down, one)) {
                    if (!putIn(h.items, one)) insertOne(world, above, Direction::Down, one); // (no room after all)
                    else moved = true;
                }
            } else if (!r.collides(world.getBlock(above))) {
                for (ItemEntity& it : items.mutableItems()) {
                    if (it.stack.empty() || it.pos.x < p.x || it.pos.x >= p.x + 1 || it.pos.z < p.z ||
                        it.pos.z >= p.z + 1 || it.pos.y < p.y + 0.5 || it.pos.y >= p.y + 2.0)
                        continue;
                    while (it.stack.count > 0 && putIn(h.items, it.stack)) { // the whole stack, as it fits
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
