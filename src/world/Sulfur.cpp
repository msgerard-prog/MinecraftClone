// Potent sulfur (M33.2b; wiki: Potent Sulfur, 26.2). Part of BlockUpdates.
#include "world/BlockUpdates.h"

#include "world/Blocks.h"

namespace mc::world {

namespace {
const BlockRegistry& R() { return blockRegistry(); }
} // namespace

BlockId BlockUpdates::potentSulfur() {
    static const BlockId id = R().findBlock("potent_sulfur").value_or(0);
    return id;
}

// Every second while under water (and its cycle as a geyser): the water column above it - 1
// to 4 source blocks, air over the top - makes its gas; with a magma block under it, it rests
// about 10 x (depth - 1) + 15-30 s, then erupts for (depth - 1) + 1-2 s, throwing what is in
// the column 5 blocks a block of water; over lava it erupts without rest.
void BlockUpdates::tickPotentSulfur(const BlockPos& p, BlockStateId s) {
    const BlockId self = potentSulfur();
    const BlockStateId water = R().defaultState(blocks::Water);
    int depth = 0;
    while (depth < 5 && at({p.x, p.y + 1 + depth, p.z}) == water) ++depth;
    const bool open = depth >= 1 && depth <= 4 && at({p.x, p.y + 1 + depth, p.z}) == 0;
    const bool erupting = R().get(s, properties::triggered) == 0; // [true, false]
    auto setErupting = [&](bool on) {
        const BlockStateId now = R().set(s, properties::triggered, on ? 0 : 1);
        if (now != s) m_world.setBlock(p, now); // (no updates: only its phase)
    };
    if (!open) {
        setErupting(false);
        return; // (asleep until the water around it changes or a random tick)
    }
    if (m_sulfurGas.size() < 64) m_sulfurGas.push_back({{p.x, p.y + depth, p.z}});
    m_world.levelEvent(LevelEvent::Type::SulfurGas, p.x, p.y, p.z, uint32_t(depth));
    const BlockId below = R().blockOf(at({p.x, p.y - 1, p.z}));
    const int top = p.y + depth + 5 * depth;
    if (below == blocks::Lava) { // continuous
        setErupting(true);
        if (m_geysers.size() < 64) m_geysers.push_back({p, top, 20});
        schedule(p, self, 20, 0);
        return;
    }
    if (below != blocks::MagmaBlock) {
        setErupting(false);
        schedule(p, self, 20, 0);
        return;
    }
    // Resting, it is checked every second (for its gas) and erupts with 1 chance in the
    // rest's average length in seconds - 10 x (depth - 1) + 22.5 on average, as the wiki's
    // 15-30 s base (ours memoryless: a rest has no fixed length).
    if (erupting || int(m_random.nextInt(uint32_t(20 * (depth - 1) + 45))) >= 2) {
        setErupting(false);
        schedule(p, self, 20, 0);
    } else {
        const int ticks = 20 * ((depth - 1) + 1 + int(m_random.nextInt(2)));
        setErupting(true);
        if (m_geysers.size() < 64) m_geysers.push_back({p, top, ticks});
        schedule(p, self, ticks, 0);
    }
}

} // namespace mc::world
