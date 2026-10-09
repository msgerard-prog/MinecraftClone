#include "ui/RecipeBook.h"

#include "ui/Hud.h"

#include <algorithm>
#include <array>

namespace mc::ui {

namespace {

constexpr uint32_t kBody = gfx::rgba(198, 198, 198);
constexpr uint32_t kLight = gfx::rgba(255, 255, 255);
constexpr uint32_t kDark = gfx::rgba(85, 85, 85);
constexpr uint32_t kEdge = gfx::rgba(0, 0, 0);

bool inside(double mx, double my, float x, float y, float w, float h) {
    return mx >= x && my >= y && mx < x + w && my < y + h;
}

// The recipe's ingredients in order (empty cells skipped).
template <typename F> void forEachIngredient(const Recipe& r, F&& f) {
    for (const Ingredient& in : r.pattern)
        if (in.kind != Ingredient::Kind::Empty) f(in);
}

} // namespace

bool RecipeBook::canCraft(const Recipe& r, const Inventory& inventory, std::span<const world::ItemStack> grid) {
    // Greedy: each ingredient takes one matching item still free (the inventory's 36
    // slots, then the grid's).
    std::array<int, Inventory::kSlots + 9> left{};
    for (int i = 0; i < Inventory::kSlots; ++i) left[size_t(i)] = inventory.slot(i).count;
    for (size_t i = 0; i < grid.size() && i < 9; ++i) left[size_t(Inventory::kSlots) + i] = grid[i].count;
    auto stackAt = [&](int i) -> const world::ItemStack& {
        return i < Inventory::kSlots ? inventory.slot(i) : grid[size_t(i - Inventory::kSlots)];
    };
    bool ok = true;
    forEachIngredient(r, [&](const Ingredient& in) {
        if (!ok) return;
        for (int i = 0; i < Inventory::kSlots + int(std::min<size_t>(grid.size(), 9)); ++i)
            if (left[size_t(i)] > 0 && in.matches(stackAt(i))) {
                --left[size_t(i)];
                return;
            }
        ok = false;
    });
    return ok;
}

void RecipeBook::refresh(int gridSize, const Inventory& inventory, std::span<const world::ItemStack> grid) {
    const auto& all = craftingRecipes();
    if (m_list.capacity() < all.size()) m_list.reserve(all.size()); // (once)
    m_gridSize = gridSize;
    m_list.clear();
    for (int i = 0; i < int(all.size()); ++i) {
        const Recipe& r = all[size_t(i)];
        const bool fits = r.width > 0 ? r.width <= gridSize && r.height <= gridSize
                                      : int(r.pattern.size()) <= gridSize * gridSize;
        if (!fits) continue;
        const bool craftable = canCraft(r, inventory, grid);
        if (m_craftableOnly && !craftable) continue;
        m_list.push_back({i, craftable});
    }
    // Craftable recipes first (vanilla sorts them ahead); stable keeps the book's order.
    std::stable_partition(m_list.begin(), m_list.end(), [](const Entry& e) { return e.craftable; });
    m_page = std::clamp(m_page, 0, pages() - 1);
}

int RecipeBook::click(double mx, double my, float left, float top, glm::vec2 button, bool& used) {
    used = false;
    if (inside(mx, my, left + button.x, top + button.y, 20, 18)) {
        toggle();
        used = true;
        return -1;
    }
    if (!m_open) return -1;
    const float bx = left - float(kWidth) - 2.0f;
    if (!inside(mx, my, bx, top, float(kWidth), float(kHeight))) return -1;
    used = true;
    if (inside(mx, my, bx + 110, top + 12, 26, 16)) { // the craftable-only toggle
        m_craftableOnly = !m_craftableOnly;
        m_page = 0;
        return -1;
    }
    if (inside(mx, my, bx + 38, top + 137, 12, 17)) m_page = std::max(0, m_page - 1);
    if (inside(mx, my, bx + 93, top + 137, 12, 17)) m_page = std::min(pages() - 1, m_page + 1);
    for (int row = 0; row < kRows; ++row)
        for (int col = 0; col < kColumns; ++col) {
            const int i = m_page * kColumns * kRows + row * kColumns + col;
            if (i >= int(m_list.size())) return -1;
            if (inside(mx, my, bx + 11 + float(col * kCell), top + 31 + float(row * kCell), kCell, kCell))
                return m_list[size_t(i)].craftable ? m_list[size_t(i)].recipe : -1;
        }
    return -1;
}

void RecipeBook::draw(gfx::GuiBatch& b, const gfx::ItemIcons& icons, const gfx::BlockModels& models, float left,
                      float top, glm::vec2 button, double mx, double my) const {
    // The book button (a green book): lighter while hovered.
    const float kx = left + button.x, ky = top + button.y;
    const bool hot = inside(mx, my, kx, ky, 20, 18);
    b.fill(kx, ky, 20, 18, kEdge);
    b.fill(kx + 1, ky + 1, 18, 16, hot ? gfx::rgba(120, 200, 110) : gfx::rgba(70, 150, 60));
    b.fill(kx + 4, ky + 3, 12, 12, gfx::rgba(40, 90, 35));
    b.fill(kx + 5, ky + 4, 9, 10, gfx::rgba(230, 220, 190)); // pages
    if (!m_open) return;
    const float bx = left - float(kWidth) - 2.0f;
    b.fill(bx + 1, top, kWidth - 2, kHeight, kEdge);
    b.fill(bx, top + 1, kWidth, kHeight - 2, kEdge);
    b.fill(bx + 1, top + 1, kWidth - 2, kHeight - 2, kBody);
    b.fill(bx + 1, top + 1, kWidth - 3, 2, kLight);
    b.fill(bx + 2, top + kHeight - 3, kWidth - 3, 2, kDark);
    b.text("Recipe Book", bx + 10, top + 15, gfx::argb(0xFF404040), false);
    // The toggle: lit while showing only craftable recipes.
    b.fill(bx + 110, top + 12, 26, 16, kEdge);
    b.fill(bx + 111, top + 13, 24, 14, m_craftableOnly ? gfx::rgba(90, 170, 80) : gfx::rgba(140, 140, 140));
    b.text(m_craftableOnly ? "OK" : "All", bx + 115, top + 16, gfx::argb(0xFFFFFFFF), false);
    const auto& all = craftingRecipes();
    int hovered = -1;
    for (int row = 0; row < kRows; ++row)
        for (int col = 0; col < kColumns; ++col) {
            const int i = m_page * kColumns * kRows + row * kColumns + col;
            if (i >= int(m_list.size())) continue;
            const Entry& e = m_list[size_t(i)];
            const float x = bx + 11 + float(col * kCell), y = top + 31 + float(row * kCell);
            b.fill(x, y, kCell - 1, kCell - 1, kEdge);
            b.fill(x + 1, y + 1, kCell - 3, kCell - 3, e.craftable ? gfx::rgba(110, 160, 100) : gfx::rgba(170, 90, 90));
            icons.draw(b, models, all[size_t(e.recipe)].result, x + 4, y + 4, kIconGrassTint);
            if (inside(mx, my, x, y, kCell, kCell)) hovered = e.recipe;
        }
    // Pages: arrows and "n/m".
    char pagesText[16];
    std::snprintf(pagesText, sizeof(pagesText), "%d/%d", m_page + 1, pages());
    b.text(pagesText, bx + 60, top + 141, gfx::argb(0xFF404040), false);
    if (m_page > 0) b.text("<", bx + 40, top + 141, gfx::argb(0xFF404040), false);
    if (m_page + 1 < pages()) b.text(">", bx + 95, top + 141, gfx::argb(0xFF404040), false);
    if (hovered >= 0) { // the result's name
        std::string_view name = world::itemRegistry().item(all[size_t(hovered)].result.item).id;
        if (name.starts_with("minecraft:")) name.remove_prefix(10);
        const float tx = float(mx) + 12, ty = float(my) - 12;
        const int w = b.textWidth(name);
        b.fill(tx - 3, ty - 3, float(w + 6), 14, gfx::argb(0xF0100010));
        b.text(name, tx, ty, gfx::argb(0xFFFFFFFF));
    }
}

} // namespace mc::ui
