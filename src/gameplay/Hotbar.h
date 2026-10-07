#pragma once

#include "world/BlockRegistry.h"

#include <array>

namespace mc {

// The 9 hotbar slots (blocks only until items exist in M9).
class Hotbar {
public:
    static constexpr int kSlots = 9;

    Hotbar(); // filled with the placeable blocks implemented so far

    void select(int slot) { m_selected = ((slot % kSlots) + kSlots) % kSlots; }
    // Mouse wheel: vanilla moves right (next slot) when scrolling down, wrapping.
    void scroll(double wheelSteps);
    int selected() const { return m_selected; }
    world::BlockStateId selectedBlock() const { return m_slots[static_cast<size_t>(m_selected)]; }
    world::BlockStateId slot(int i) const { return m_slots[static_cast<size_t>(i)]; }
    void setSlot(int i, world::BlockStateId state) { m_slots[static_cast<size_t>(i)] = state; }

private:
    std::array<world::BlockStateId, kSlots> m_slots{};
    int m_selected = 0;
    double m_scrollRemainder = 0.0;
};

} // namespace mc
