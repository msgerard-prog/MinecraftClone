// Tripwire and tripwire hooks (M29.5; wiki: Tripwire, Tripwire Hook). Part of BlockUpdates.
//
// Two hooks facing each other on a straight line of tripwire (up to 40 pieces) are
// attached; an entity in the wire powers it, and while any piece of an attached line is
// powered the hooks give power 15 (strongly into the block they hang on).
#include "world/BlockUpdates.h"

#include "world/Blocks.h"

namespace mc::world {

namespace {

using namespace properties;
namespace B = blocks;

const BlockRegistry& R() { return blockRegistry(); }
BlockId blockOf(BlockStateId s) { return R().blockOf(s); }
BlockPos rel(const BlockPos& p, Direction d, int n = 1) {
    const glm::ivec3 v = normal(d);
    return {p.x + v.x * n, p.y + v.y * n, p.z + v.z * n};
}
Direction opposite(Direction d) { return static_cast<Direction>(static_cast<int>(d) ^ 1); }
Direction hookFacing(BlockStateId s) { return static_cast<Direction>(R().get(s, facing) + 2); }
bool flag(BlockStateId s, const Property& p) { return R().get(s, p) == 0; } // [true, false]
BlockStateId withFlag(BlockStateId s, const Property& p, bool v) { return R().set(s, p, v ? 0 : 1); }
constexpr int kMaxLine = 41; // the partner hook at most 41 blocks along (40 pieces of wire)

const Property& sideProp(Direction d) {
    return d == Direction::North ? fireNorth : d == Direction::South ? fireSouth : d == Direction::West ? fireWest : fireEast;
}

} // namespace

BlockStateId BlockUpdates::tripwireConnected(const World& world, const BlockPos& p, BlockStateId wire) {
    // A piece joins wire beside it and hooks facing it (vanilla: the model's east..west).
    for (const Direction d : {Direction::North, Direction::South, Direction::West, Direction::East}) {
        const BlockStateId n = world.getBlock(rel(p, d));
        const bool joins = blockOf(n) == B::Tripwire || (blockOf(n) == B::TripwireHook && hookFacing(n) == opposite(d));
        wire = withFlag(wire, sideProp(d), joins);
    }
    return wire;
}

void BlockUpdates::tripwireHookUpdate(const BlockPos& p) {
    const BlockStateId s = at(p);
    if (blockOf(s) != B::TripwireHook) return;
    const Direction f = hookFacing(s);
    int length = 0;
    bool pressed = false;
    for (int i = 1; i <= kMaxLine; ++i) {
        const BlockStateId q = at(rel(p, f, i));
        if (blockOf(q) == B::TripwireHook) {
            if (hookFacing(q) == opposite(f)) length = i;
            break;
        }
        if (blockOf(q) != B::Tripwire) break;
        pressed = pressed || (flag(q, powered) && !flag(q, disarmed));
    }
    const bool attachedNow = length > 1;
    const bool poweredNow = attachedNow && pressed;
    // The wire between knows whether it is part of a working line (its model sits lower).
    for (int i = 1; i <= (attachedNow ? length - 1 : kMaxLine); ++i) {
        const BlockPos q = rel(p, f, i);
        if (blockOf(at(q)) != B::Tripwire) break;
        setRaw(q, withFlag(at(q), attached, attachedNow));
    }
    auto apply = [&](const BlockPos& hook) {
        const BlockStateId hs = at(hook);
        const BlockStateId now = withFlag(withFlag(hs, attached, attachedNow), powered, poweredNow);
        if (now == hs) return;
        if (flag(hs, powered) != poweredNow) // (vanilla: click on, click off)
            m_world.playSound(Sound::Click, hook.x + 0.5, hook.y + 0.5, hook.z + 0.5, 0.4f, poweredNow ? 1.0f : 0.8f);
        set(hook, now);
    };
    apply(p);
    if (attachedNow) apply(rel(p, f, length));
}

void BlockUpdates::tripwireChanged(const BlockPos& wire) {
    // Tell the hooks at either end of this piece's lines.
    for (const Direction d : {Direction::North, Direction::South, Direction::West, Direction::East})
        for (int i = 1; i <= kMaxLine; ++i) {
            const BlockPos q = rel(wire, d, i);
            const BlockId b = blockOf(at(q));
            if (b == B::TripwireHook) {
                if (hookFacing(at(q)) == opposite(d)) tripwireHookUpdate(q);
                break;
            }
            if (b != B::Tripwire) break;
        }
}

} // namespace mc::world
