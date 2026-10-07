#include "ui/CreativeInventory.h"

#include "ui/Hud.h"
#include "world/Blocks.h"

#include <algorithm>
#include <cmath>

namespace mc::ui {

namespace {

// Panel colours in the game's GUI style (our palette): light grey body, white
// top-left highlight, dark bottom-right shadow; slots are recessed.
constexpr uint32_t kBody = gfx::rgba(198, 198, 198);
constexpr uint32_t kLight = gfx::rgba(255, 255, 255);
constexpr uint32_t kDark = gfx::rgba(85, 85, 85);
constexpr uint32_t kEdge = gfx::rgba(0, 0, 0);
constexpr uint32_t kSlotFill = gfx::rgba(139, 139, 139);
constexpr uint32_t kHover = gfx::rgba(255, 255, 255, 128);

void slotFrame(gfx::GuiBatch& b, float x, float y) {
    b.fill(x, y, 18, 18, kSlotFill);
    b.fill(x, y, 17, 1, kDark);  // top
    b.fill(x, y, 1, 17, kDark);  // left
    b.fill(x + 1, y + 17, 17, 1, kLight); // bottom
    b.fill(x + 17, y + 1, 1, 17, kLight); // right
}

} // namespace

void CreativeInventory::build(const gfx::BlockModels& models) {
    const auto& reg = world::blockRegistry();
    const auto& items = world::itemRegistry();
    m_items.clear();
    m_names.clear();
    for (size_t i = 1; i < items.count(); ++i) {
        const world::ItemDef& def = items.item(static_cast<world::ItemId>(i));
        if (def.block) {
            const world::BlockStateId s = reg.defaultState(def.block);
            if (s >= models.size() || !models[s].visible || models[s].fluid) continue;
        }
        m_items.push_back({static_cast<world::ItemId>(i), 1});
        std::string name = def.id;
        if (name.starts_with("minecraft:")) name.erase(0, 10);
        m_names.push_back(std::move(name));
    }
}

void CreativeInventory::open() {
    m_open = true;
    m_carried = {};
}

void CreativeInventory::close() {
    m_open = false;
    m_carried = {};
}

int CreativeInventory::maxScrollRow() const {
    const int rows = (static_cast<int>(m_items.size()) + kColumns - 1) / kColumns;
    return std::max(0, rows - kRows);
}

void CreativeInventory::scroll(double steps) {
    m_scrollRemainder += steps;
    const double whole = std::trunc(m_scrollRemainder);
    m_scrollRemainder -= whole;
    m_scrollRow = std::clamp(m_scrollRow - static_cast<int>(whole), 0, maxScrollRow());
}

CreativeInventory::Hit CreativeInventory::hitTest(double mx, double my, int guiWidth,
                                                  int guiHeight) const {
    const double left = (guiWidth - kWidth) / 2, top = (guiHeight - kHeight) / 2;
    const double x = mx - left, y = my - top;
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return {};
    auto cell = [&](double ox, double oy, int cols, int rows, int& col, int& row) {
        col = static_cast<int>(std::floor((x - ox) / kSlot));
        row = static_cast<int>(std::floor((y - oy) / kSlot));
        return x >= ox && y >= oy && col < cols && row < rows;
    };
    int col = 0, row = 0;
    if (cell(9, 18, kColumns, kRows, col, row)) {
        const int i = (m_scrollRow + row) * kColumns + col;
        if (i < static_cast<int>(m_items.size())) return {Hit::Grid, i};
        return {Hit::Panel, -1};
    }
    if (cell(9, 112, kColumns, 1, col, row)) return {Hit::HotbarSlot, col};
    return {Hit::Panel, -1};
}

void CreativeInventory::click(double mx, double my, int guiWidth, int guiHeight, Inventory& inventory) {
    const Hit h = hitTest(mx, my, guiWidth, guiHeight);
    switch (h.kind) {
    case Hit::Grid:
        // Creative: the grid is an infinite source; clicking it with something
        // carried deletes that and takes the clicked item.
        m_carried = m_items[size_t(h.index)];
        m_carried.count = static_cast<uint8_t>(world::itemRegistry().item(m_carried.item).maxStack);
        break;
    case Hit::HotbarSlot: {
        const world::ItemStack old = inventory.slot(h.index);
        inventory.setSlot(h.index, m_carried);
        m_carried = old;
        break;
    }
    case Hit::None: m_carried = {}; break; // outside the panel: drop it
    case Hit::Panel: break;
    }
}

void CreativeInventory::numberKey(int slot, double mx, double my, int guiWidth, int guiHeight,
                                  Inventory& inventory) {
    const Hit h = hitTest(mx, my, guiWidth, guiHeight);
    if (h.kind == Hit::Grid) { // a full stack, as vanilla's creative number keys
        world::ItemStack s = m_items[size_t(h.index)];
        s.count = static_cast<uint8_t>(world::itemRegistry().item(s.item).maxStack);
        inventory.setSlot(slot, s);
    }
    if (h.kind == Hit::HotbarSlot) { // swap two hotbar slots
        const world::ItemStack a = inventory.slot(h.index);
        inventory.setSlot(h.index, inventory.slot(slot));
        inventory.setSlot(slot, a);
    }
}

void CreativeInventory::draw(gfx::GuiBatch& b, const gfx::ItemIcons& icons,
                             const gfx::BlockModels& models, const Inventory& inventory,
                             int guiWidth, int guiHeight, double mx, double my) {
    // Dim the world behind (vanilla: gradient #C0101010 -> #D0101010).
    b.fill(0, 0, static_cast<float>(guiWidth), static_cast<float>(guiHeight),
           gfx::argb(0xC8101010));
    const float left = static_cast<float>((guiWidth - kWidth) / 2);
    const float top = static_cast<float>((guiHeight - kHeight) / 2);
    // Panel: black outline, bevel, body.
    b.fill(left + 1, top, kWidth - 2, kHeight, kEdge);
    b.fill(left, top + 1, kWidth, kHeight - 2, kEdge);
    b.fill(left + 1, top + 1, kWidth - 2, kHeight - 2, kBody);
    b.fill(left + 1, top + 1, kWidth - 3, 2, kLight);
    b.fill(left + 1, top + 1, 2, kHeight - 3, kLight);
    b.fill(left + 2, top + kHeight - 3, kWidth - 3, 2, kDark);
    b.fill(left + kWidth - 3, top + 2, 2, kHeight - 3, kDark);
    b.text("Blocks", left + 8, top + 6, gfx::argb(0xFF404040), /*shadow=*/false);

    const Hit hover = hitTest(mx, my, guiWidth, guiHeight);
    for (int row = 0; row < kRows; ++row)
        for (int col = 0; col < kColumns; ++col) {
            const float x = left + 9 + col * kSlot - 1, y = top + 18 + row * kSlot - 1;
            slotFrame(b, x, y);
            const int i = (m_scrollRow + row) * kColumns + col;
            if (i < static_cast<int>(m_items.size()))
                icons.draw(b, models, m_items[size_t(i)], x + 1, y + 1, kIconGrassTint);
            if (hover.kind == Hit::Grid && hover.index == i) b.fill(x + 1, y + 1, 16, 16, kHover);
        }
    for (int col = 0; col < Inventory::kHotbar; ++col) {
        const float x = left + 9 + col * kSlot - 1, y = top + 112 - 1;
        slotFrame(b, x, y);
        icons.draw(b, models, inventory.slot(col), x + 1, y + 1, kIconGrassTint);
        if (hover.kind == Hit::HotbarSlot && hover.index == col)
            b.fill(x + 1, y + 1, 16, 16, kHover);
    }
    // Scrollbar track and thumb (12x15).
    const float sx = left + 175, sy = top + 18;
    b.fill(sx - 1, sy - 1, 14, 112 - 18 + 2, kDark);
    b.fill(sx, sy, 12, 112 - 18, kSlotFill);
    const int maxRow = maxScrollRow();
    const float thumbY = sy + (maxRow > 0 ? (112.0f - 18 - 15) * m_scrollRow / maxRow : 0.0f);
    b.fill(sx, thumbY, 12, 15, maxRow > 0 ? kLight : kBody);

    // Tooltip for the hovered grid item, then the carried item on the cursor.
    if (hover.kind == Hit::Grid && m_carried.empty()) {
        const std::string& name = m_names[size_t(hover.index)];
        const float tx = static_cast<float>(mx) + 12, ty = static_cast<float>(my) - 12;
        const int w = b.textWidth(name);
        b.fill(tx - 3, ty - 3, static_cast<float>(w + 6), 14, gfx::argb(0xF0100010));
        b.fill(tx - 2, ty - 2, static_cast<float>(w + 4), 12, gfx::argb(0xFF2A0A5A));
        b.fill(tx - 1, ty - 1, static_cast<float>(w + 2), 10, gfx::argb(0xF0100010));
        b.text(name, tx, ty, gfx::argb(0xFFFFFFFF));
    }
    icons.draw(b, models, m_carried, static_cast<float>(mx) - 8, static_cast<float>(my) - 8,
               kIconGrassTint);
}

} // namespace mc::ui
