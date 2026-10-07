#include "gameplay/Inventory.h"

#include "world/Blocks.h"

#include <algorithm>
#include <cmath>

namespace mc {

Inventory::Inventory() {
    using namespace world;
    const BlockId blocks[kHotbar] = {blocks::Stone,      blocks::Cobblestone, blocks::Dirt,
                                     blocks::GrassBlock, blocks::OakPlanks,   blocks::OakLog,
                                     blocks::Glass,      blocks::Torch,       blocks::Glowstone};
    for (int i = 0; i < kHotbar; ++i)
        m_slots[static_cast<size_t>(i)] = blockStack(blockRegistry().defaultState(blocks[i]));
}

world::ItemStack Inventory::blockStack(world::BlockStateId state, int count) {
    const auto& reg = world::blockRegistry();
    const world::BlockId block = reg.blockOf(state);
    world::ItemStack s{world::itemRegistry().blockItem(block), static_cast<uint8_t>(count)};
    if (s.item && state != reg.defaultState(block)) s.state = state;
    if (!s.item) s = {};
    return s;
}

void Inventory::setSlot(int i, world::ItemStack s) {
    if (s.count == 0 || s.item == world::kNoItem) s = {};
    m_slots[static_cast<size_t>(i)] = s;
}

void Inventory::scroll(double wheelSteps) {
    // Accumulate fractions (touchpads/smooth wheels send small offsets per frame).
    m_scrollRemainder += wheelSteps;
    const double whole = std::trunc(m_scrollRemainder);
    m_scrollRemainder -= whole;
    if (whole != 0.0) select(m_selected - static_cast<int>(whole)); // up (+) = previous slot
}

world::BlockStateId Inventory::placeState() const {
    const world::ItemStack& s = selectedStack();
    if (s.empty()) return 0;
    const world::ItemDef& def = world::itemRegistry().item(s.item);
    if (!def.block) return 0;
    return s.state ? s.state : world::blockRegistry().defaultState(def.block);
}

int Inventory::add(world::ItemStack stack) {
    if (stack.empty()) return 0;
    const int max = world::itemRegistry().item(stack.item).maxStack;
    int left = stack.count;
    auto fill = [&](int i) {
        world::ItemStack& s = m_slots[static_cast<size_t>(i)];
        if (s.empty() || !s.sameKind(stack) || s.count >= max) return;
        const int n = std::min(left, max - int(s.count));
        s.count = static_cast<uint8_t>(s.count + n);
        left -= n;
    };
    fill(m_selected);
    for (int i = 0; i < kSlots && left > 0; ++i)
        fill(i);
    for (int i = 0; i < kSlots && left > 0; ++i) {
        world::ItemStack& s = m_slots[static_cast<size_t>(i)];
        if (!s.empty()) continue;
        s = stack;
        s.count = static_cast<uint8_t>(std::min(left, max));
        left -= s.count;
    }
    return left;
}

int Inventory::armorPoints() const {
    int points = 0;
    for (const auto& a : m_armor)
        if (!a.empty()) points += world::itemRegistry().item(a.item).armor;
    return points;
}

float Inventory::armorToughness() const {
    float t = 0.0f;
    for (const auto& a : m_armor)
        if (!a.empty()) t += world::itemRegistry().item(a.item).toughness;
    return t;
}

void Inventory::wearArmor(int amount) {
    for (auto& a : m_armor) {
        if (a.empty() || amount <= 0) continue;
        a.damage = static_cast<uint16_t>(a.damage + amount);
        if (a.damage >= world::itemRegistry().item(a.item).durability) a = {}; // broke
    }
}

bool Inventory::equipSelected() {
    const world::ItemStack held = selectedStack();
    if (held.empty()) return false;
    const int slot = world::itemRegistry().item(held.item).armorSlot;
    if (slot == 0) return false;
    setSlot(m_selected, m_armor[size_t(slot - 1)]);
    m_armor[size_t(slot - 1)] = held;
    return true;
}

bool Inventory::has(world::ItemId item) const {
    for (const auto& s : m_slots)
        if (!s.empty() && s.item == item) return true;
    return false;
}

bool Inventory::takeOne(world::ItemId item) {
    for (auto& s : m_slots) // slots 0..8 (hotbar) come first, then the main grid
        if (!s.empty() && s.item == item) {
            if (--s.count == 0) s = {};
            return true;
        }
    return false;
}

void Inventory::consumeSelected(int n) {
    world::ItemStack& s = m_slots[static_cast<size_t>(m_selected)];
    if (s.empty()) return;
    s.count = static_cast<uint8_t>(std::max(0, int(s.count) - n));
    if (s.count == 0) s = {};
}

} // namespace mc
