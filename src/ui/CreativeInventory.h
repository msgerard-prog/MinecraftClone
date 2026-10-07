#pragma once

#include "gameplay/Hotbar.h"
#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"

#include <string>
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

    // The item list: the default state of every block with a visible model,
    // except fluids (they come as buckets in vanilla). Load time.
    void build(const gfx::BlockModels& models);
    const std::vector<world::BlockStateId>& items() const { return m_items; }

    bool isOpen() const { return m_open; }
    void open();
    void close(); // drops the carried item (creative)

    // Input in GUI pixels.
    void click(double mx, double my, int guiWidth, int guiHeight, Hotbar& hotbar);
    void numberKey(int slot, double mx, double my, int guiWidth, int guiHeight, Hotbar& hotbar);
    void scroll(double steps); // + = up
    int scrollRow() const { return m_scrollRow; }
    int maxScrollRow() const;
    world::BlockStateId carried() const { return m_carried; }

    void draw(gfx::GuiBatch& batch, const gfx::BlockModels& models, const Hotbar& hotbar,
              int guiWidth, int guiHeight, double mx, double my);

private:
    struct Hit {
        enum Kind { None, Grid, HotbarSlot, Panel } kind = None;
        int index = -1; // grid item index or hotbar slot
    };
    Hit hitTest(double mx, double my, int guiWidth, int guiHeight) const;

    std::vector<world::BlockStateId> m_items;
    std::vector<std::string> m_names; // tooltip text per item (built at load)
    bool m_open = false;
    int m_scrollRow = 0;
    double m_scrollRemainder = 0.0;
    world::BlockStateId m_carried = 0;
};

} // namespace mc::ui
