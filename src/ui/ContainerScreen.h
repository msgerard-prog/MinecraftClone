#pragma once

#include "gameplay/Furnace.h"
#include "gameplay/Inventory.h"
#include "rendering/BlockModels.h"
#include "rendering/GuiBatch.h"
#include "rendering/ItemIcons.h"
#include "world/BlockEntity.h"
#include "world/Mob.h"
#include "world/Random.h"

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
    enum class Type {
        Inventory,
        Crafting,
        Furnace,
        Chest,
        Enchanting,
        Anvil,
        Brewing,
        Hopper,
        Dispenser,
        Stonecutter,
        Grindstone,
        Smithing,
        Loom,
        Cartography,
        Beacon,
        Trading,
        Mount // (M26.2) a horse's, donkey's, llama's or camel's gear and chest; a chest boat's chest
    };
    static constexpr int kWidth = 176, kHeight = 166;
    // Panel height: 166, or a chest's 114 + 18 per row (3 rows single, 6 double).
    int height() const {
        return m_type == Type::Chest     ? 114 + chestRows() * 18
               : m_type == Type::Hopper  ? 133
               : m_type == Type::Trading ? 222
                                         : kHeight;
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
    // The grindstone's removed enchantments' cost since the last call (main turns it
    // into experience orbs with grindExperience, M23.5).
    int takeGrindCost() { return std::exchange(m_grindCost, 0); }
    // The stonecutter's chosen recipe (index into stonecutterRecipes of the input), -1 none.
    int stonecutterChoice() const { return m_stoneChoice; }
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
    // The beacon (M23.6), owned by the world; re-pointed every frame like brewing stands.
    // Its powers are chosen with the buttons and set by paying one ingot or gem.
    void openBeacon(world::BeaconData* beacon) {
        open(Type::Beacon);
        m_beacon = beacon;
        m_beaconPrimary = beacon ? beacon->primary : 0;
        m_beaconSecondary = beacon ? beacon->secondary : 0;
    }
    void setBeacon(world::BeaconData* beacon) { m_beacon = beacon; }
    // Trading with a villager (M24.2), owned by its chunk; re-pointed every frame (mobs
    // move in memory). Experience points the player earned since the last call (main
    // drops orbs), and whether a trade happened (the villager's sound).
    void openTrading(world::MobData* villager) {
        open(Type::Trading);
        m_trader = villager;
        m_tradeChoice = -1;
    }
    void setTrader(world::MobData* villager) { m_trader = villager; }
    // Hero of the Village level of the player trading (M24.5: lower prices).
    void setHeroLevel(int level) {
        if (level == m_heroLevel) return;
        m_heroLevel = level;
        if (m_type == Type::Trading) updateResult(); // (the prices changed)
    }
    int takeTradeExperience() { return std::exchange(m_tradeXp, 0); }
    // Statistics (M28.1d): what was crafted (crafting grid, smithing, furnace outputs
    // taken) and trades made since the last call.
    std::span<const world::ItemStack> crafted() const { return {m_crafted.data(), size_t(m_craftedCount)}; }
    void clearCrafted() { m_craftedCount = 0; }
    int takeTrades() { return std::exchange(m_trades, 0); }
    int tradeChoice() const { return m_tradeChoice; }
    // Hoppers (5 slots) and dispensers/droppers (3x3) (M21.3): their slots, owned by
    // the world; re-pointed every frame like chests.
    void openStore(Type type, std::span<world::ItemStack> slots, bool dropper = false) {
        open(type);
        m_store = slots;
        m_dropper = dropper;
    }
    void setStore(std::span<world::ItemStack> slots) { m_store = slots; }
    // A mount's screen (M26.2; vanilla's horse screen): its saddle and body slots (horse
    // armor or a llama's carpet) where it has them, and its chest (`chest`: 0-15 slots,
    // 27 for a chest boat), both owned by the world. The gear slots edit the mob's fields;
    // re-pointed every frame (mobs move in memory).
    void openMount(world::MobData* mob, std::span<world::ItemStack> chest);
    void setMount(world::MobData* mob, std::span<world::ItemStack> chest) {
        m_mount = mob;
        m_store = chest;
    }
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
            Store,
            MountGear // (index 0 saddle, 1 body)
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
    world::BeaconData* m_beacon = nullptr;
    world::MobData* m_trader = nullptr;
    world::MobData* m_mount = nullptr;
    world::ItemStack m_gearScratch; // (what a gear slot shows: built from the mob's fields)
    bool gearFits(int slot, const world::ItemStack& s) const;
    void setGear(int slot, const world::ItemStack& s);
    int m_heroLevel = 0;
    int m_tradeChoice = -1, m_tradeXp = 0;
    std::array<world::ItemStack, 8> m_crafted{};
    int m_craftedCount = 0, m_trades = 0;
    void noteCrafted(const world::ItemStack& s);
    world::Xoroshiro m_tradeRng{0x7a4d'e5u}; // (trade rewards and new trades)
    uint8_t m_beaconPrimary = 0, m_beaconSecondary = 0; // (the choice before paying)
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
    int m_grindCost = 0;        // (taken by main)
    int m_stoneChoice = -1;     // the stonecutter's selected recipe
    world::ItemId m_stoneInput = 0; // the input the choice belongs to (a new kind clears it)
    int m_stoneScroll = 0;      // first shown row of recipes (4 per row, 3 rows shown)
    int m_loomChoice = -1;      // (M28.3d) the loom's chosen pattern (world::kBannerPatterns index)
    int m_loomScroll = 0;       // first shown row of patterns (4 per row, 4 rows shown)
};

} // namespace mc::ui
