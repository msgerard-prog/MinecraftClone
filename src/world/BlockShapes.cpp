#include "world/BlockShapes.h"

#include "world/Blocks.h"

#include <vector>

namespace mc::world {

using namespace properties;
namespace B = blocks;

namespace {

BlockShape box(int x0, int y0, int z0, int x1, int y1, int z1) {
    BlockShape s;
    s.count = 1;
    s.boxes[0] = {{uint8_t(x0), uint8_t(y0), uint8_t(z0)}, {uint8_t(x1), uint8_t(y1), uint8_t(z1)}};
    return s;
}
void add(BlockShape& s, int x0, int y0, int z0, int x1, int y1, int z1) {
    if (s.count < s.boxes.size())
        s.boxes[s.count++] = {{uint8_t(x0), uint8_t(y0), uint8_t(z0)}, {uint8_t(x1), uint8_t(y1), uint8_t(z1)}};
}

// A 3-thick panel against the side `d` of the cell (north = -z ...).
BlockShape panel(int facingIndex /* facing: north south west east */) {
    switch (facingIndex) {
    case 0: return box(0, 0, 0, 16, 16, 3);   // against the north side
    case 1: return box(0, 0, 13, 16, 16, 16); // south
    case 2: return box(0, 0, 0, 3, 16, 16);   // west
    default: return box(13, 0, 0, 16, 16, 16); // east
    }
}
int oppositeH(int f) { return f ^ 1; } // north<->south, west<->east (facing order)
// Left of a horizontal facing, as seen facing it: north->west, south->east,
// west->south, east->north (facing indices 0 north, 1 south, 2 west, 3 east).
int leftOf(int f) {
    static constexpr int kLeft[4] = {2, 3, 1, 0};
    return kLeft[f];
}

BlockShape compute(BlockStateId s) {
    const auto& r = blockRegistry();
    const BlockId b = r.blockOf(s);
    if (!r.collides(s)) return {};
    switch (b) {
    case B::OakDoor:
    case B::IronDoor: {
        // Closed: a panel on the side the player placed it from (opposite its
        // facing); open: swung to its hinge side (wiki: Door).
        const int f = r.get(s, facing);
        if (r.get(s, open) != 0) return panel(oppositeH(f));
        const int side = r.get(s, hinge) == 0 ? leftOf(f) : oppositeH(leftOf(f));
        return panel(side);
    }
    case B::OakTrapdoor:
    case B::IronTrapdoor: {
        if (r.get(s, open) == 0) return panel(oppositeH(r.get(s, facing)));
        return r.get(s, slabHalf) == 0 ? box(0, 13, 0, 16, 16, 16) : box(0, 0, 0, 16, 3, 16);
    }
    case B::OakFenceGate: {
        if (r.get(s, open) == 0) return {}; // open gates let you through
        const int f = r.get(s, facing);
        return f < 2 ? box(0, 0, 6, 16, 24, 10) : box(6, 0, 0, 10, 24, 16);
    }
    case B::OakFence:
    case B::NetherBrickFence: {
        BlockShape sh = box(6, 0, 6, 10, 24, 10); // the post, 1.5 tall (wiki: Fence)
        if (b == B::OakFence) {
            if (r.get(s, fireNorth) == 0) add(sh, 6, 0, 0, 10, 24, 6);
            if (r.get(s, fireSouth) == 0) add(sh, 6, 0, 10, 10, 24, 16);
            if (r.get(s, fireWest) == 0) add(sh, 0, 0, 6, 6, 24, 10);
            if (r.get(s, fireEast) == 0) add(sh, 10, 0, 6, 16, 24, 10);
        }
        return sh;
    }
    case B::IronBars: {
        BlockShape sh = box(7, 0, 7, 9, 16, 9);
        if (r.get(s, fireNorth) == 0) add(sh, 7, 0, 0, 9, 16, 7);
        if (r.get(s, fireSouth) == 0) add(sh, 7, 0, 9, 9, 16, 16);
        if (r.get(s, fireWest) == 0) add(sh, 0, 0, 7, 7, 16, 9);
        if (r.get(s, fireEast) == 0) add(sh, 9, 0, 7, 16, 16, 9);
        return sh;
    }
    case B::Cactus: return box(1, 0, 1, 15, 15, 15);
    case B::Farmland:
    case B::DirtPath: return box(0, 0, 0, 16, 15, 16);
    case B::SoulSand: return box(0, 0, 0, 16, 14, 16);
    case B::Chest: return box(1, 0, 1, 15, 14, 15);
    case B::RedBed: return box(0, 0, 0, 16, 9, 16);
    case B::EnchantingTable: return box(0, 0, 0, 16, 12, 16);
    case B::EndPortalFrame: return box(0, 0, 0, 16, 13, 16);
    default: return box(0, 0, 0, 16, 16, 16);
    }
}

} // namespace

const BlockShape& collisionShape(BlockStateId state) {
    // Built once, after the registry is frozen (C++ magic statics make it thread-safe).
    static const std::vector<BlockShape> table = [] {
        std::vector<BlockShape> t(blockRegistry().stateCount());
        for (size_t i = 0; i < t.size(); ++i)
            t[i] = compute(static_cast<BlockStateId>(i));
        return t;
    }();
    return table[state];
}

} // namespace mc::world
