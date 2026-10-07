#pragma once

#include "gameplay/Furnace.h"
#include "gameplay/Inventory.h"
#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"
#include "rendering/ItemIcons.h"
#include "world/BlockEntity.h"

#include <array>
#include <span>
#include <utility>
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
    enum class Type { Inventory, Crafting, Furnace, Chest, Enchanting, Anvil, Brewing, Hopper, Dispenser };
    static constexpr int kWidth = 176, kHeight = 166;
    // Panel height: 166, or a chest's 114 + 18 per row (3 rows single, 6 double).
    int height() const {
        return m_type == Type::Chest ? 114 + chestRows() * 18 : m_type == Type::Hopper ? 133 : kHeight;
    }
    int chestRows() const { return m_chests[1] ? 6 : 3; }

    bool isOpen() const { return m_open; }
    Type type() const { return m_type; }
    // `furnace`: the furnace being used (Type::Furnace), owned by the world.
    void open(Type type, Furnace* furnace = nullptr);
    // A chest (and the other half of a double chest: rows 4-6), owned by the world.
    void openChest(world::ChestData* first, world::ChestData* second);
    // The enchanting table (M17.5): bookshelves around it and the player's seed.
    void openEnchanting(int bookshelves, uint64_t seed);
    void openAnvil();
    // The player's levels and mode, set each frame (offers and anvil costs need them).
    void setPlayer(int levels, bool creative, uint64_t enchantSeed) {
        m_levels = levels;
        m_creative = creative;
        m_seed = enchantSeed;
    }
    // Levels spent by enchanting or the anvil since the last call (main spends them);
    // whether an item was enchanted (new seed) / the anvil was used (it may wear).
    int takeLevelsSpent() { return std::exchange(m_levelsSpent, 0); }
    bool takeEnchanted() { return std::exchange(m_enchanted, false); }
    bool takeAnvilUsed() { return std::exchange(m_anvilUsed, false); }
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
    // The brewing stand (M19.4), owned by the world; re-pointed every frame like furnaces.
    void openBrewing(world::BrewingData* stand) {
        open(Type::Brewing);
        m_brewing = stand;
    }
    void setBrewing(world::BrewingData* stand) { m_brewing = stand; }
    // Hoppers (5 slots) and dispensers/droppers (3x3) (M21.3): their slots, owned by
    // the world; re-pointed every frame like chests.
    void openStore(Type type, std::span<world::ItemStack> slots, bool dropper = false) {
        open(type);
        m_store = slots;
        m_dropper = dropper;
    }
    void setStore(std::span<world::ItemStack> slots) { m_store = slots; }
    const world::ItemStack& carried() const { return m_carried; }
    // Recipe uses whose experience was earned by taking smelted items out of a
    // furnace since the last call (main pays them with recipesExperience).
    std::array<world::FurnaceData::RecipeUse, 16> takeRecipes() {
        return std::exchange(m_takenRecipes.recipesUsed, {});
    }
    const world::ItemStack& result() const { return m_result; }
    const world::ItemStack& grid(int i) const { return m_grid[size_t(i)]; }

    void draw(gfx::GuiBatch& batch, const gfx::ItemIcons& icons, const gfx::BlockModels& models,
              const Inventory& inventory, int guiWidth, int guiHeight, double mx, double my) const;

private:
    struct Slot {
        enum class Kind {
            Inv,
            Grid,
            Result,
            FurnaceIn,
            FurnaceFuel,
            FurnaceOut,
            Chest,
            Armor,
            Offhand,
            BrewBottle,
            BrewIngredient,
            BrewFuel,
            Store
        } kind;
        int index;
        int x, y; // panel coordinates of the 16x16 item area
    };
    std::span<const Slot> slots() const; // fixed per screen type (built once)
    void clickSlots(double mx, double my, Button button, bool shift, int guiWidth, int guiHeight, Inventory& inventory,
                    std::vector<world::ItemStack>& drops);
    int gridSize() const { return m_type == Type::Crafting ? 3 : 2; }
    world::ItemStack* stackAt(const Slot& s, Inventory& inventory);
    void updateResult();
    void takeResult(Inventory& inventory, bool shift);
    void moveToInventory(world::ItemStack& s, Inventory& inventory, int from, int to);

    bool m_open = false;
    Type m_type = Type::Inventory;
    Furnace* m_furnace = nullptr;
    world::BrewingData* m_brewing = nullptr;
    std::span<world::ItemStack> m_store;
    bool m_dropper = false;
    std::array<world::ChestData*, 2> m_chests{};
    std::array<world::ItemStack, 9> m_grid{};
    world::ItemStack m_result;
    world::ItemStack m_carried;
    world::FurnaceData m_takenRecipes; // (only its recipesUsed)
    int m_bookshelves = 0, m_levels = 0, m_levelsSpent = 0, m_anvilCost = 0;
    uint64_t m_seed = 0;
    bool m_creative = false, m_enchanted = false, m_anvilUsed = false, m_anvilTooExpensive = false;
    int m_anvilMaterial = 1;
};

} // namespace mc::ui
