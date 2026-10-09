#pragma once

#include "gameplay/Inventory.h"
#include "rendering/ItemIcons.h"
#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace mc::ui {

// The creative inventory screen (wiki: Creative inventory), vanilla layout: a
// 195x136 panel centred on screen, 9x5 item grid at (9, 18) with 18 px slots, a
// scrollbar at (175, 18), the hotbar row at (9, 112). Left click takes an item
// onto the cursor; clicking a hotbar slot puts it there (swapping); clicking
// outside the panel drops it. Number keys 1-9 over a grid item copy it into that
// hotbar slot. Blocks only until items exist (M9). GL-free.
class CreativeInventory {
public:
    static constexpr int kWidth = 195, kHeight = 136;
    static constexpr int kColumns = 9, kRows = 5, kSlot = 18;
    // Tabs (M30.6; wiki: Creative inventory): seven along the top, five below, 28 px apart.
    enum class Tab : uint8_t {
        Building, Colored, Natural, Functional, Redstone, Operator, Search, // top
        Tools, Combat, Food, Ingredients, SpawnEggs,                        // bottom
        Count
    };
    static constexpr int kTabWidth = 26, kTabHeight = 28, kTopTabs = 7;
    // Which tab an item is listed under (vanilla's grouping, by the item's kind and id).
    static Tab tabOf(const world::ItemDef& def);
    static const char* tabName(Tab t);
    Tab tab() const { return m_tab; }
    void selectTab(Tab t);
    // The search box (Search tab): typed text filters the list by name.
    void type(std::string_view text);
    void backspace();
    const std::string& query() const { return m_query; }
    // The items the grid shows now (indices into items()).
    const std::vector<int>& shown() const { return m_shown; }

    // The item list: every item (block items whose block has a visible model, then
    // tools, materials, food). Load time.
    void build(const gfx::BlockModels& models);
    const std::vector<world::ItemStack>& items() const { return m_items; }

    bool isOpen() const { return m_open; }
    void open();
    void close(); // drops the carried item (creative)

    // Input in GUI pixels.
    void click(double mx, double my, int guiWidth, int guiHeight, Inventory& inventory);
    void numberKey(int slot, double mx, double my, int guiWidth, int guiHeight, Inventory& inventory);
    void scroll(double steps); // + = up
    int scrollRow() const { return m_scrollRow; }
    int maxScrollRow() const;
    const world::ItemStack& carried() const { return m_carried; }

    void draw(gfx::GuiBatch& batch, const gfx::ItemIcons& icons, const gfx::BlockModels& models,
              const Inventory& inventory,
              int guiWidth, int guiHeight, double mx, double my);

private:
    struct Hit {
        enum Kind { None, Grid, HotbarSlot, Panel, TabButton } kind = None;
        int index = -1; // grid item index, hotbar slot or tab
    };
    Hit hitTest(double mx, double my, int guiWidth, int guiHeight) const;
    void refilter();
    static void tabRect(Tab t, float left, float top, float& x, float& y);

    std::vector<world::ItemStack> m_items;
    std::vector<std::string> m_names; // tooltip text per item (built at load)
    std::vector<std::string> m_searchNames; // lower case, '_' as ' ' (search)
    std::array<std::vector<int>, size_t(Tab::Count)> m_tabItems;
    std::array<world::ItemStack, size_t(Tab::Count)> m_tabIcons{};
    std::vector<int> m_shown;
    Tab m_tab = Tab::Building;
    std::string m_query;
    bool m_open = false;
    int m_scrollRow = 0;
    double m_scrollRemainder = 0.0;
    world::ItemStack m_carried;
};

} // namespace mc::ui
