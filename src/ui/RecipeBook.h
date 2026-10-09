#pragma once

#include "gameplay/Inventory.h"
#include "gameplay/Recipes.h"
#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"
#include "rendering/ItemIcons.h"

#include <span>
#include <vector>

namespace mc::ui {

// The recipe book (M30.6; wiki: Recipe book) on the inventory and crafting table screens:
// a green book button opens a 147-wide panel to the left of the screen (which moves 77
// px right, as vanilla). The panel lists, 20 to a page, the recipes that fit the grid -
// green when the player has what they need (in the inventory and the grid), red when
// not - and a toggle shows only the craftable ones. Clicking a craftable recipe places
// its ingredients in the grid. Every recipe is known (vanilla unlocks them as items are
// picked up). GL-free.
class RecipeBook {
public:
    static constexpr int kWidth = 147, kHeight = 166;
    static constexpr int kColumns = 5, kRows = 4, kCell = 25;
    static constexpr int kShift = 77; // how far the container moves right while open

    bool isOpen() const { return m_open; }
    void toggle() { m_open = !m_open; }
    void setOpen(bool open) { m_open = open; }
    bool craftableOnly() const { return m_craftableOnly; }
    int page() const { return m_page; }
    int pages() const { return std::max(1, (int(m_list.size()) + kColumns * kRows - 1) / (kColumns * kRows)); }

    // Lists the recipes fitting a `gridSize` grid (2 or 3), marking which the items in
    // `inventory` and `grid` can make. No allocation once reserved (called each frame).
    void refresh(int gridSize, const Inventory& inventory, std::span<const world::ItemStack> grid);
    struct Entry {
        int recipe; // index in craftingRecipes()
        bool craftable;
    };
    const std::vector<Entry>& list() const { return m_list; }
    static bool canCraft(const Recipe& r, const Inventory& inventory, std::span<const world::ItemStack> grid);

    // A click at GUI (mx, my) with the container panel at (left, top) - already moved
    // right while the book is open - and its book button at `button` (panel coordinates).
    // Returns the recipe to place (craftable) or -1; `used`: the click was the book's.
    int click(double mx, double my, float left, float top, glm::vec2 button, bool& used);
    void draw(gfx::GuiBatch& batch, const gfx::ItemIcons& icons, const gfx::BlockModels& models, float left, float top,
              glm::vec2 button, double mx, double my) const;

private:
    bool m_open = false;
    bool m_craftableOnly = false;
    int m_page = 0;
    int m_gridSize = 2;
    std::vector<Entry> m_list;
};

} // namespace mc::ui
