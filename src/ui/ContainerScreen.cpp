#include "ui/ContainerScreen.h"

#include "gameplay/Recipes.h"
#include "ui/Hud.h"

#include <algorithm>
#include <cmath>

namespace mc::ui {

namespace {

constexpr uint32_t kBody = gfx::rgba(198, 198, 198);
constexpr uint32_t kLight = gfx::rgba(255, 255, 255);
constexpr uint32_t kDark = gfx::rgba(85, 85, 85);
constexpr uint32_t kEdge = gfx::rgba(0, 0, 0);
constexpr uint32_t kSlotFill = gfx::rgba(139, 139, 139);
constexpr uint32_t kHover = gfx::rgba(255, 255, 255, 128);
constexpr uint32_t kLabel = gfx::argb(0xFF404040);

int maxStack(const world::ItemStack& s) { return world::itemRegistry().item(s.item).maxStack; }

void slotFrame(gfx::GuiBatch& b, float x, float y) {
    b.fill(x, y, 18, 18, kSlotFill);
    b.fill(x, y, 17, 1, kDark);
    b.fill(x, y, 1, 17, kDark);
    b.fill(x + 1, y + 17, 17, 1, kLight);
    b.fill(x + 17, y + 1, 1, 17, kLight);
}

} // namespace

void ContainerScreen::open(Type type, Furnace* furnace) {
    m_open = true;
    m_type = type;
    m_furnace = furnace;
    m_grid = {};
    m_result = {};
    m_carried = {};
}

void ContainerScreen::close(Inventory& inventory, std::vector<world::ItemStack>& drops) {
    for (auto& s : m_grid) {
        if (s.empty()) continue;
        if (const int left = inventory.add(s); left > 0) drops.push_back({s.item, uint8_t(left), s.damage, s.state});
        s = {};
    }
    if (!m_carried.empty()) {
        if (const int left = inventory.add(m_carried); left > 0)
            drops.push_back({m_carried.item, uint8_t(left), m_carried.damage, m_carried.state});
        m_carried = {};
    }
    m_result = {};
    m_furnace = nullptr;
    m_open = false;
}

std::span<const ContainerScreen::Slot> ContainerScreen::slots() const {
    auto build = [](Type type) {
        std::vector<Slot> out;
        using K = Slot::Kind;
        for (int i = 0; i < 27; ++i) // main inventory 9..35
            out.push_back({K::Inv, 9 + i, 8 + (i % 9) * 18, 84 + (i / 9) * 18});
        for (int i = 0; i < 9; ++i) // hotbar
            out.push_back({K::Inv, i, 8 + i * 18, 142});
        if (type == Type::Inventory) {
            for (int i = 0; i < 4; ++i)
                out.push_back({K::Grid, i, 98 + (i % 2) * 18, 18 + (i / 2) * 18});
            out.push_back({K::Result, 0, 154, 28});
        } else if (type == Type::Crafting) {
            for (int i = 0; i < 9; ++i)
                out.push_back({K::Grid, i, 30 + (i % 3) * 18, 17 + (i / 3) * 18});
            out.push_back({K::Result, 0, 124, 35});
        } else {
            out.push_back({K::FurnaceIn, 0, 56, 17});
            out.push_back({K::FurnaceFuel, 0, 56, 53});
            out.push_back({K::FurnaceOut, 0, 116, 35});
        }
        return out;
    };
    // Layouts never change: built on first use, then shared (no per-frame vectors).
    static const std::vector<Slot> inventory = build(Type::Inventory), crafting = build(Type::Crafting),
                                   furnace = build(Type::Furnace);
    return m_type == Type::Inventory ? inventory : m_type == Type::Crafting ? crafting : furnace;
}

world::ItemStack* ContainerScreen::stackAt(const Slot& s, Inventory& inventory) {
    switch (s.kind) {
    case Slot::Kind::Inv: return const_cast<world::ItemStack*>(&inventory.slot(s.index));
    case Slot::Kind::Grid: return &m_grid[size_t(s.index)];
    case Slot::Kind::Result: return &m_result;
    case Slot::Kind::FurnaceIn: return m_furnace ? &m_furnace->input : nullptr;
    case Slot::Kind::FurnaceFuel: return m_furnace ? &m_furnace->fuel : nullptr;
    case Slot::Kind::FurnaceOut: return m_furnace ? &m_furnace->output : nullptr;
    }
    return nullptr;
}

void ContainerScreen::updateResult() {
    if (m_type == Type::Furnace) return;
    const int n = gridSize();
    std::array<world::ItemStack, 9> g{};
    for (int i = 0; i < n * n; ++i)
        g[size_t(i)] = m_grid[size_t(i)];
    m_result = craft(std::span<const world::ItemStack>(g.data(), size_t(n * n)), n).value_or(world::ItemStack{});
}

void ContainerScreen::moveToInventory(world::ItemStack& s, Inventory& inventory, int from, int to) {
    // Merge into matching stacks in [from, to), then the first empty slot there.
    for (int pass = 0; pass < 2 && !s.empty(); ++pass)
        for (int i = from; i < to && !s.empty(); ++i) {
            world::ItemStack t = inventory.slot(i);
            if (pass == 0 && !t.empty() && t.sameKind(s) && t.count < maxStack(t)) {
                const int n = std::min<int>(s.count, maxStack(t) - t.count);
                t.count = uint8_t(t.count + n);
                s.count = uint8_t(s.count - n);
                inventory.setSlot(i, t);
            } else if (pass == 1 && t.empty()) {
                inventory.setSlot(i, s);
                s = {};
            }
        }
    if (s.count == 0) s = {};
}

void ContainerScreen::takeResult(Inventory& inventory, bool shift) {
    const int n = gridSize();
    for (int rounds = 0; rounds < 64 && !m_result.empty(); ++rounds) {
        world::ItemStack made = m_result;
        if (shift) {
            // Craft only if the whole result fits (no partial insert without using up
            // the ingredients: that would duplicate items).
            Inventory probe = inventory;
            world::ItemStack test = made;
            moveToInventory(test, probe, 0, Inventory::kSlots);
            if (!test.empty()) break;
            moveToInventory(made, inventory, 0, Inventory::kSlots);
        } else {
            if (!m_carried.empty() && (!m_carried.sameKind(made) || m_carried.count + made.count > maxStack(made)))
                return;
            if (m_carried.empty()) m_carried = made;
            else m_carried.count = uint8_t(m_carried.count + made.count);
        }
        for (int i = 0; i < n * n; ++i) // each ingredient is used once
            if (!m_grid[size_t(i)].empty() && --m_grid[size_t(i)].count == 0) m_grid[size_t(i)] = {};
        updateResult();
        if (!shift) return;
    }
}

void ContainerScreen::click(double mx, double my, Button button, bool shift, int guiWidth, int guiHeight,
                            Inventory& inventory, std::vector<world::ItemStack>& drops) {
    const double left = (guiWidth - kWidth) / 2, top = (guiHeight - kHeight) / 2;
    const double px = mx - left, py = my - top;
    if (px < 0 || py < 0 || px >= kWidth || py >= kHeight) { // outside: throw
        if (m_carried.empty()) return;
        world::ItemStack thrown = m_carried;
        if (button == Button::Right) {
            thrown.count = 1;
            if (--m_carried.count == 0) m_carried = {};
        } else {
            m_carried = {};
        }
        drops.push_back(thrown);
        return;
    }
    for (const Slot& slot : slots()) {
        if (px < slot.x - 1 || py < slot.y - 1 || px >= slot.x + 17 || py >= slot.y + 17) continue;
        world::ItemStack* s = stackAt(slot, inventory);
        if (!s) return;
        world::ItemStack v = *s; // edit a copy, write back (inventory slots via setSlot)
        auto store = [&] {
            if (v.count == 0) v = {};
            if (slot.kind == Slot::Kind::Inv) inventory.setSlot(slot.index, v);
            else *s = v;
            if (slot.kind == Slot::Kind::Grid) updateResult();
        };
        if (slot.kind == Slot::Kind::Result) {
            takeResult(inventory, shift);
            return;
        }
        if (shift && !v.empty()) {
            if (slot.kind == Slot::Kind::Inv) {
                if (m_type == Type::Furnace && m_furnace) { // into the furnace when it fits
                    if (smelt(v)) {
                        if (m_furnace->input.empty()) { m_furnace->input = v; v = {}; }
                    } else if (fuelTicks(v) > 0 && m_furnace->fuel.empty()) {
                        m_furnace->fuel = v;
                        v = {};
                    }
                }
                if (!v.empty()) {
                    if (slot.index < Inventory::kHotbar) moveToInventory(v, inventory, 9, 36);
                    else moveToInventory(v, inventory, 0, 9);
                }
            } else {
                moveToInventory(v, inventory, 0, Inventory::kSlots);
            }
            store();
            return;
        }
        const bool outputOnly = slot.kind == Slot::Kind::FurnaceOut;
        if (button == Button::Left) {
            if (m_carried.empty()) {
                m_carried = v;
                v = {};
            } else if (outputOnly) {
                if (!v.empty() && v.sameKind(m_carried) && m_carried.count + v.count <= maxStack(v)) {
                    m_carried.count = uint8_t(m_carried.count + v.count);
                    v = {};
                }
            } else if (v.empty()) {
                v = m_carried;
                m_carried = {};
            } else if (v.sameKind(m_carried)) {
                const int n = std::min<int>(m_carried.count, maxStack(v) - v.count);
                v.count = uint8_t(v.count + n);
                m_carried.count = uint8_t(m_carried.count - n);
                if (m_carried.count == 0) m_carried = {};
            } else {
                std::swap(v, m_carried);
            }
        } else { // right
            if (m_carried.empty()) {
                if (v.empty()) return;
                m_carried = v;
                m_carried.count = uint8_t((v.count + 1) / 2); // the larger half
                v.count = uint8_t(v.count - m_carried.count);
            } else if (!outputOnly && (v.empty() || (v.sameKind(m_carried) && v.count < maxStack(v)))) {
                if (v.empty()) {
                    v = m_carried;
                    v.count = 0;
                }
                ++v.count;
                if (--m_carried.count == 0) m_carried = {};
            } else if (!outputOnly && !v.sameKind(m_carried)) {
                std::swap(v, m_carried);
            }
        }
        store();
        return;
    }
}

void ContainerScreen::draw(gfx::GuiBatch& b, const gfx::ItemIcons& icons, const gfx::BlockModels& models,
                           const Inventory& inventory, int guiWidth, int guiHeight, double mx, double my) const {
    b.fill(0, 0, static_cast<float>(guiWidth), static_cast<float>(guiHeight), gfx::argb(0xC0101010));
    const float left = static_cast<float>((guiWidth - kWidth) / 2);
    const float top = static_cast<float>((guiHeight - kHeight) / 2);
    b.fill(left + 1, top, kWidth - 2, kHeight, kEdge);
    b.fill(left, top + 1, kWidth, kHeight - 2, kEdge);
    b.fill(left + 1, top + 1, kWidth - 2, kHeight - 2, kBody);
    b.fill(left + 1, top + 1, kWidth - 3, 2, kLight);
    b.fill(left + 1, top + 1, 2, kHeight - 3, kLight);
    b.fill(left + 2, top + kHeight - 3, kWidth - 3, 2, kDark);
    b.fill(left + kWidth - 3, top + 2, 2, kHeight - 3, kDark);
    const char* title = m_type == Type::Furnace ? "Furnace" : "Crafting";
    const float titleX = m_type == Type::Inventory ? 97.0f : m_type == Type::Crafting ? 28.0f : 70.0f;
    b.text(title, left + titleX, top + 6, kLabel, false);
    if (m_type != Type::Inventory) b.text("Inventory", left + 8, top + 72, kLabel, false);

    // Crafting arrow / furnace gauges.
    auto arrow = [&](float x, float y, float fill) {
        b.fill(left + x, top + y + 6, 16, 4, kDark);
        if (fill > 0) b.fill(left + x, top + y + 6, 16 * fill, 4, kLight);
    };
    if (m_type == Type::Inventory) arrow(134, 28, 0);
    if (m_type == Type::Crafting) arrow(90, 35, 0);
    if (m_type == Type::Furnace && m_furnace) {
        arrow(79, 34, float(m_furnace->cookTime) / float(kFurnaceCookTicks));
        // Flame gauge between input and fuel: burn time left.
        const float flame = m_furnace->burnDuration ? float(m_furnace->burnLeft) / float(m_furnace->burnDuration) : 0.0f;
        b.fill(left + 57, top + 37, 14, 14, kDark);
        if (flame > 0) b.fill(left + 57, top + 37 + 14 * (1 - flame), 14, 14 * flame, gfx::rgba(240, 140, 30));
    }

    Inventory& inv = const_cast<Inventory&>(inventory); // read-only use of stackAt
    const double px = mx - left, py = my - top;
    for (const Slot& slot : slots()) {
        const float x = left + slot.x, y = top + slot.y;
        const bool big = (slot.kind == Slot::Kind::Result && m_type == Type::Crafting) ||
                         slot.kind == Slot::Kind::FurnaceOut;
        if (big) { // result slots are 26x26 frames in vanilla
            b.fill(x - 5, y - 5, 26, 26, kSlotFill);
            b.fill(x - 5, y - 5, 25, 1, kDark);
            b.fill(x - 5, y - 5, 1, 25, kDark);
        } else {
            slotFrame(b, x - 1, y - 1);
        }
        const world::ItemStack* s = const_cast<ContainerScreen*>(this)->stackAt(slot, inv);
        if (s) icons.draw(b, models, *s, x, y, kIconGrassTint);
        if (px >= slot.x - 1 && py >= slot.y - 1 && px < slot.x + 17 && py < slot.y + 17)
            b.fill(x, y, 16, 16, kHover);
    }
    icons.draw(b, models, m_carried, static_cast<float>(mx) - 8, static_cast<float>(my) - 8, kIconGrassTint);
}

} // namespace mc::ui
