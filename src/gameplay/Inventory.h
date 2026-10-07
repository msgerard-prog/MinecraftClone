#pragma once

#include "world/Items.h"

#include <array>

namespace mc {

// The player's inventory (wiki: Inventory): 36 slots, 0..8 the hotbar (one selected),
// 9..35 the main grid. Stacks hold items (blocks are block items).
class Inventory {
public:
    static constexpr int kSlots = 36;
    static constexpr int kHotbar = 9;

    Inventory(); // creative starter hotbar: the blocks implemented first

    const world::ItemStack& slot(int i) const { return m_slots[static_cast<size_t>(i)]; }
    void setSlot(int i, world::ItemStack s);

    void select(int slot) { m_selected = ((slot % kHotbar) + kHotbar) % kHotbar; }
    // Mouse wheel: vanilla moves right (next slot) when scrolling down, wrapping.
    void scroll(double wheelSteps);
    int selected() const { return m_selected; }
    const world::ItemStack& selectedStack() const { return slot(m_selected); }
    // The block state the selected stack places (0 if it isn't a block item).
    world::BlockStateId placeState() const;

    // Adds as much of `stack` as fits (vanilla order: matching stacks first, the
    // selected slot first, then empty slots from 0). Returns how many didn't fit.
    int add(world::ItemStack stack);
    // Removes `n` from the selected stack (survival: placing, eating).
    void consumeSelected(int n = 1);
    // Finds and removes one `item` (arrows for a bow: the selected hotbar slot... any
    // slot, vanilla prefers the offhand, then the hotbar, then the inventory). False
    // if there is none.
    bool takeOne(world::ItemId item);
    bool has(world::ItemId item) const;

    // A single block item stack for a block state (keeps non-default states).
    static world::ItemStack blockStack(world::BlockStateId state, int count = 1);

private:
    std::array<world::ItemStack, kSlots> m_slots{};
    int m_selected = 0;
    double m_scrollRemainder = 0.0;
};

} // namespace mc
