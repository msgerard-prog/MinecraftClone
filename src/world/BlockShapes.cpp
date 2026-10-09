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

} // namespace

BlockShape stairShapeOf(BlockStateId s) {
    const auto& r = blockRegistry();
    const bool top = r.get(s, slabHalf) == 0;
    const int f = r.get(s, facing); // 0 north, 1 south, 2 west, 3 east
    const int shape = r.get(s, stairShape);
    // Quadrants of the raised half (x0, z0): 0 NW, 1 NE, 2 SW, 3 SE.
    auto quadrant = [](int q, int& x0, int& z0) {
        x0 = (q & 1) ? 8 : 0;
        z0 = (q & 2) ? 8 : 0;
    };
    // The two quadrants on the facing side, ordered (left, right) as seen looking
    // along the facing - "left" is counterclockwise from the facing (north -> west).
    static constexpr int kFront[4][2] = {{0, 1}, {3, 2}, {2, 0}, {1, 3}};
    // The back quadrants (behind the front ones), same order.
    static constexpr int kBack[4][2] = {{2, 3}, {1, 0}, {3, 1}, {0, 2}};
    const int y0 = top ? 0 : 8, y1 = top ? 8 : 16; // the raised part
    BlockShape sh = top ? box(0, 8, 0, 16, 16, 16) : box(0, 0, 0, 16, 8, 16);
    auto addQuad = [&](int q) {
        int x0, z0;
        quadrant(q, x0, z0);
        add(sh, x0, y0, z0, x0 + 8, y1, z0 + 8);
    };
    switch (shape) {
    case 3: addQuad(kFront[f][0]); break; // outer_left: only the left front quarter
    case 4: addQuad(kFront[f][1]); break; // outer_right
    default:
        addQuad(kFront[f][0]);
        addQuad(kFront[f][1]);
        if (shape == 1) addQuad(kBack[f][0]); // inner_left: and the left back quarter
        if (shape == 2) addQuad(kBack[f][1]); // inner_right
        break;
    }
    return sh;
}

namespace {

BlockShape compute(BlockStateId s) {
    const auto& r = blockRegistry();
    const BlockId b = r.likeOf(r.blockOf(s)); // (M23.3: wood sets take the oak shapes)
    if (!r.collides(s)) return {};
    switch (r.kind(b)) {
    case BlockKind::Slab: { // wiki: Slab - bottom, top or a full double slab
        const int t = r.get(s, slabType);
        return t == 0 ? box(0, 8, 0, 16, 16, 16) : t == 1 ? box(0, 0, 0, 16, 8, 16) : box(0, 0, 0, 16, 16, 16);
    }
    case BlockKind::Stairs: return stairShapeOf(s);
    case BlockKind::Wall: {
        // A post where it isn't a straight run, arms to its connections; 1.5 tall
        // for collision like fences (wiki: Wall).
        BlockShape sh;
        if (r.get(s, fireUp) == 0) add(sh, 4, 0, 4, 12, 24, 12);
        if (r.get(s, wallNorth) != 0) add(sh, 5, 0, 0, 11, 24, 8);
        if (r.get(s, wallSouth) != 0) add(sh, 5, 0, 8, 11, 24, 16);
        if (r.get(s, wallWest) != 0) add(sh, 0, 0, 5, 8, 24, 11);
        if (r.get(s, wallEast) != 0) add(sh, 8, 0, 5, 16, 24, 11);
        if (sh.count == 0) add(sh, 4, 0, 4, 12, 24, 12);
        return sh;
    }
    case BlockKind::Pane: { // wiki: Glass Pane - a 2-wide post and arms, like iron bars
        BlockShape sh = box(7, 0, 7, 9, 16, 9);
        if (r.get(s, fireNorth) == 0) add(sh, 7, 0, 0, 9, 16, 7);
        if (r.get(s, fireSouth) == 0) add(sh, 7, 0, 9, 9, 16, 16);
        if (r.get(s, fireWest) == 0) add(sh, 0, 0, 7, 7, 16, 9);
        if (r.get(s, fireEast) == 0) add(sh, 9, 0, 7, 16, 16, 9);
        return sh;
    }
    case BlockKind::Carpet: return box(0, 0, 0, 16, 1, 16); // wiki: Carpet - 1/16 tall
    case BlockKind::Plain: break;
    }
    switch (b) {
    case B::Mud: return box(0, 0, 0, 16, 14, 16); // (M27.1; wiki: Mud - 14 pixels, so things sink a little)
    case B::DecoratedPot: return box(1, 0, 1, 15, 16, 15); // (M27.5)
    // (M28.4d-M28.5a; wiki) the heavy core 8 wide, a cake 14 wide and 8 tall less its bites,
    // a candle cake with its candle, candles a small post (ours: grows with the count)
    case B::HeavyCore: return box(4, 0, 4, 12, 8, 12);
    case B::LilyPad: return box(0, 0, 0, 16, 1, 16); // (M29.4a; wiki: 1.5/16 - ours 1/16)
    case B::Cake: return box(1 + 2 * r.get(s, bites), 0, 1, 15, 8, 15);
    case B::CandleCake: {
        BlockShape sh = box(1, 0, 1, 15, 8, 15);
        add(sh, 7, 8, 7, 9, 14, 9);
        return sh;
    }
    case B::Candle: return r.get(s, candles) == 0 ? box(7, 0, 7, 9, 6, 9) : box(5, 0, 5, 11, 6, 11);
    case B::SculkSensor: // (M27.3; wiki: half a block)
    case B::SculkShrieker: return box(0, 0, 0, 16, 8, 16);
    case B::PointedDripstone: { // (M27.2b; ours: thinner toward the tip)
        const int t = r.get(s, thickness);
        return t >= 3 ? box(4, 0, 4, 12, 16, 12) : t == 2 ? box(5, 0, 5, 11, 16, 11) : box(6, 0, 6, 10, 16, 10);
    }
    case B::BigDripleaf: { // (M27.2; wiki: Big Dripleaf - its leaf holds you until fully tipped)
        const int t = r.get(s, tilt);
        if (t == 3) return {};
        return box(0, 11, 0, 16, t == 2 ? 13 : 15, 16);
    }
    case B::Lantern: // wiki: Lantern - 6x7x6 (with the handle 6x9), hanging one pixel lower
    case B::SoulLantern: {
        const int y0 = r.get(s, hanging) == 0 ? 1 : 0;
        BlockShape sh = box(5, y0, 5, 11, y0 + 7, 11);
        add(sh, 6, y0 + 7, 6, 10, y0 + 9, 10);
        return sh;
    }
    case B::Chain: { // a 3-thick bar along its axis (wiki: Chain)
        const int a = r.get(s, axis);
        return a == 0 ? box(0, 6, 6, 16, 10, 10) : a == 1 ? box(6, 0, 6, 10, 16, 10) : box(6, 6, 0, 10, 10, 16);
    }
    case B::Bamboo: return box(6, 0, 6, 9, 16, 9); // (vanilla: 3x3, offset per position)
    case B::Campfire: // (wiki: Campfire - 7 pixels tall)
    case B::SoulCampfire: return box(0, 0, 0, 16, 7, 16);
    case B::MangroveRoots: return box(0, 0, 0, 16, 16, 16); // (a full block to stand on)
    case B::Ladder: // against the block behind it: facing north hangs on the south side
        return panel(oppositeH(r.get(s, facing)));
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
    case B::Comparator: return box(0, 0, 0, 16, 2, 16);
    case B::Farmland:
    case B::DirtPath: return box(0, 0, 0, 16, 15, 16);
    case B::SoulSand: return box(0, 0, 0, 16, 14, 16);
    case B::Chest: return box(1, 0, 1, 15, 14, 15);
    case B::Stonecutter: return box(0, 0, 0, 16, 9, 16); // (wiki: 9 pixels tall)
    case B::Grindstone: { // its wheel and legs' bounds, turned with the face
        const int f = r.get(s, face), dir = r.get(s, facing); // facing: north, south, west, east
        if (f != 1) return box(2, 0, 2, 14, 16, 14);
        return dir < 2 ? box(2, 2, 0, 14, 14, 16) : box(0, 2, 2, 16, 14, 14);
    }
    case B::Composter: { // a 2-thick open box with a 2-high floor (wiki: Composter)
        BlockShape sh = box(0, 0, 0, 16, 2, 16);
        add(sh, 0, 0, 0, 16, 16, 2);
        add(sh, 0, 0, 14, 16, 16, 16);
        add(sh, 0, 0, 0, 2, 16, 16);
        add(sh, 14, 0, 0, 16, 16, 16);
        return sh;
    }
    case B::Cauldron:
    case B::WaterCauldron:
    case B::LavaCauldron:
    case B::PowderSnowCauldron: { // the bowl's floor at 4 pixels, walls 2 thick (wiki: Cauldron)
        BlockShape sh = box(0, 0, 0, 16, 4, 16);
        add(sh, 0, 0, 0, 16, 16, 2);
        add(sh, 0, 0, 14, 16, 16, 16);
        add(sh, 0, 0, 0, 2, 16, 16);
        add(sh, 14, 0, 0, 16, 16, 16);
        return sh;
    }
    case B::RedBed: return box(0, 0, 0, 16, 9, 16);
    case B::FlowerPot: return box(5, 0, 5, 11, 6, 11); // (M29.4b; potted ones too)
    // (M25 review; our sizes: the wiki lists no boxes) sea pickles 4-8 wide by count, 6 tall;
    // turtle eggs one small egg or a 14-wide clutch, 7 tall.
    case B::SeaPickle: return r.get(s, pickles) == 0 ? box(6, 0, 6, 10, 6, 10) : box(2, 0, 2, 14, 6, 14);
    case B::TurtleEgg: return r.get(s, eggs) == 0 ? box(3, 0, 3, 12, 7, 12) : box(1, 0, 1, 15, 7, 15);
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
