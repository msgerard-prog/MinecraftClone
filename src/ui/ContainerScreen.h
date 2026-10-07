#pragma once

#include "gameplay/Furnace.h"
#include "gameplay/Inventory.h"
#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"
#include "rendering/ItemIcons.h"
#include "world/BlockEntity.h"

#include <array>
#include <span>
#include <vector>

namespace mc::ui {

// Slot screens over the player's inventory (wiki: Inventory, Crafting Table,
// Furnace), vanilla's 176x166 layouts: the survival inventory with its 2x2 crafting
// grid, the crafting table (3x3) and the furnace (input, fuel, output).
// Vanilla clicks: left picks up / places / merges / swaps; right takes half or places
// one; shift-left moves a stack between hotbar and main inventory (or crafts as many
// as fit); clicking outside drops the carried stack. GL-free.
class ContainerScreen {
public:
    enum class Type { Inventory, Crafting, Furnace, Chest };
    static constexpr int kWidth = 176, kHeight = 166;
    // Panel height: 166, or a chest's 114 + 18 per row (3 rows single, 6 double).
    int height() const { return m_type == Type::Chest ? 114 + chestRows() * 18 : kHeight; }
    int chestRows() const { return m_chests[1] ? 6 : 3; }

    bool isOpen() const { return m_open; }
    Type type() const { return m_type; }
    // `furnace`: the furnace being used (Type::Furnace), owned by the world.
    void open(Type type, Furnace* furnace = nullptr);
    // A chest (and the other half of a double chest: rows 4-6), owned by the world.
    void openChest(world::ChestData* first, world::ChestData* second);
    void setChests(world::ChestData* first, world::ChestData* second) {
        m_chests[0] = first;
        m_chests[1] = second;
    }
    // Returns the grid and the carried stack to `inventory`; what doesn't fit is
    // appended to `drops` (thrown by the caller).
    void close(Inventory& inventory, std::vector<world::ItemStack>& drops);

    enum class Button { Left, Right };
    void click(double mx, double my, Button button, bool shift, int guiWidth, int guiHeight,
               Inventory& inventory, std::vector<world::ItemStack>& drops);

    void setFurnace(Furnace* furnace) { m_furnace = furnace; }
    const world::ItemStack& carried() const { return m_carried; }
    const world::ItemStack& result() const { return m_result; }
    const world::ItemStack& grid(int i) const { return m_grid[size_t(i)]; }

    void draw(gfx::GuiBatch& batch, const gfx::ItemIcons& icons, const gfx::BlockModels& models,
              const Inventory& inventory, int guiWidth, int guiHeight, double mx, double my) const;

private:
    struct Slot {
        enum class Kind { Inv, Grid, Result, FurnaceIn, FurnaceFuel, FurnaceOut, Chest } kind;
        int index;
        int x, y; // panel coordinates of the 16x16 item area
    };
    std::span<const Slot> slots() const; // fixed per screen type (built once)
    int gridSize() const { return m_type == Type::Crafting ? 3 : 2; }
    world::ItemStack* stackAt(const Slot& s, Inventory& inventory);
    void updateResult();
    void takeResult(Inventory& inventory, bool shift);
    void moveToInventory(world::ItemStack& s, Inventory& inventory, int from, int to);

    bool m_open = false;
    Type m_type = Type::Inventory;
    Furnace* m_furnace = nullptr;
    std::array<world::ChestData*, 2> m_chests{};
    std::array<world::ItemStack, 9> m_grid{};
    world::ItemStack m_result;
    world::ItemStack m_carried;
};

} // namespace mc::ui
