#pragma once

#include "gameplay/Furnace.h"
#include "gameplay/Inventory.h"
#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"
#include "rendering/ItemIcons.h"

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
    enum class Type { Inventory, Crafting, Furnace };
    static constexpr int kWidth = 176, kHeight = 166;

    bool isOpen() const { return m_open; }
    Type type() const { return m_type; }
    // `furnace`: the furnace being used (Type::Furnace), owned by the world.
    void open(Type type, Furnace* furnace = nullptr);
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
        enum class Kind { Inv, Grid, Result, FurnaceIn, FurnaceFuel, FurnaceOut } kind;
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
    std::array<world::ItemStack, 9> m_grid{};
    world::ItemStack m_result;
    world::ItemStack m_carried;
};

} // namespace mc::ui
