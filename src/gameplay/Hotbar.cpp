#include "gameplay/Hotbar.h"

#include "world/Blocks.h"

#include <cmath>

namespace mc {

Hotbar::Hotbar() {
    using namespace world;
    const auto& r = blockRegistry();
    const BlockId blocks[kSlots] = {blocks::Stone,      blocks::Cobblestone, blocks::Dirt,
                                    blocks::GrassBlock, blocks::OakPlanks,   blocks::OakLog,
                                    blocks::Sand,       blocks::Gravel,      blocks::Deepslate};
    for (int i = 0; i < kSlots; ++i)
        m_slots[static_cast<size_t>(i)] = r.defaultState(blocks[i]);
}

void Hotbar::scroll(double wheelSteps) {
    const int steps = static_cast<int>(std::round(wheelSteps));
    if (steps != 0) select(m_selected - steps); // wheel up (+) = previous slot
}

} // namespace mc
