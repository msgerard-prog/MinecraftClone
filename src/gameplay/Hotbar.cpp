#include "gameplay/Hotbar.h"

#include "world/Blocks.h"

#include <cmath>

namespace mc {

Hotbar::Hotbar() {
    using namespace world;
    const auto& r = blockRegistry();
    const BlockId blocks[kSlots] = {blocks::Stone,      blocks::Cobblestone, blocks::Dirt,
                                    blocks::GrassBlock, blocks::OakPlanks,   blocks::OakLog,
                                    blocks::Glass,      blocks::Torch,       blocks::Glowstone};
    for (int i = 0; i < kSlots; ++i)
        m_slots[static_cast<size_t>(i)] = r.defaultState(blocks[i]);
}

void Hotbar::scroll(double wheelSteps) {
    // Accumulate fractions (touchpads/smooth wheels send small offsets per frame).
    m_scrollRemainder += wheelSteps;
    const double whole = std::trunc(m_scrollRemainder);
    m_scrollRemainder -= whole;
    if (whole != 0.0) select(m_selected - static_cast<int>(whole)); // up (+) = previous slot
}

} // namespace mc
