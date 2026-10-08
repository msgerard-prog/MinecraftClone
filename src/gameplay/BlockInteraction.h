#pragma once

#include "gameplay/Inventory.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Random.h"
#include "world/Raycast.h"
#include "world/World.h"

#include <optional>
#include <utility>
#include <vector>

namespace mc::world {
class BlockUpdates;
}

namespace mc {

// Mouse buttons for one tick.
struct InteractionInput {
    bool attack = false;      // left button held
    bool use = false;         // right button held
    bool attackClick = false; // a left press happened since the last tick (act now)
    bool useClick = false;    // a right press happened since the last tick
};

// Creative-mode breaking and placing (survival mining times come with items/tools in
// M9). Runs in the game tick; records every block it changed so the renderer can
// re-mesh them.
// Where eating a chorus fruit takes the player from `feet` (nullopt: nowhere found).
std::optional<glm::dvec3> chorusTeleport(const world::World& world, const glm::dvec3& feet, world::Xoroshiro& rng);

class BlockInteraction {
public:
    // An item a broken block (or its container contents) drops.
    struct Drop {
        glm::dvec3 pos;
        world::ItemStack stack;
    };

    // Vanilla repeat delays while a button is held (ticks).
    static constexpr int kDestroyDelay = 6; // creative (wiki: Creative, 0.3 s)
    static constexpr int kUseDelay = 4;     // public write-ups; unverified on the wiki

    // Breaks/places for this tick at `hit` - the block the outline showed on the last
    // rendered frame (vanilla acts on the highlighted block). `changed` gets the
    // edited positions (cleared first).
    void tick(world::World& world, const Player& player, const std::optional<world::RayHit>& hit,
              world::BlockStateId placeState, const InteractionInput& input,
              std::vector<world::BlockPos>& changed, std::vector<Drop>* drops = nullptr,
              bool holdingSword = false); // swords can't break blocks in creative (wiki)

    // What the player is looking at from its current tick position (tests, scripts).
    static std::optional<world::RayHit> target(const world::World& world, const Player& player);

    // The state to place against `face`: pillars (logs, deepslate) take the axis of
    // the clicked face, as in vanilla.
    static world::BlockStateId orientedState(world::BlockStateId state, world::Direction face);

    // Survival (wiki: Breaking, Placing, Food): holding attack builds break progress
    // at the speed `breakTicks` gives for the held item; the block breaks at 100%
    // and drops its items (`drops`, cleared first), then a 5-tick pause. Tools wear
    // by 1 per block. Placing uses up the held stack; holding use with food eats it
    // after 32 ticks when hungry.
    void tickSurvival(world::World& world, const Player& player,
                      const std::optional<world::RayHit>& hit, Inventory& inventory,
                      Vitals& vitals, const InteractionInput& input, bool eyesInWater,
                      world::Xoroshiro& rng, std::vector<world::BlockPos>& changed,
                      std::vector<Drop>& drops);
    // Drinking (M19.4; wiki: Potion, Milk Bucket): 32 ticks of holding use, in any
    // mode; survival swaps the potion for a glass bottle (milk: the bucket), creative
    // keeps it. Returns true while the held item is a drink being used.
    bool tickDrinking(Inventory& inventory, Vitals& vitals, bool use, bool survival);
    // Crack overlay: the block being broken and progress 0..1 (no block: nullopt).
    std::optional<world::BlockPos> breakingBlock() const { return m_breaking; }
    float breakProgress() const { return m_progress; }
    int eatingTicks() const { return m_eatTicks; }
    // A chorus fruit was eaten this tick (once; main teleports the player).
    bool takeChorusTeleport() { return std::exchange(m_ateChorus, false); }
    static constexpr int kEatTicks = 32; // wiki: Food (1.61 s)
    static constexpr int kSurvivalBreakDelay = 6; // wiki: Breaking - 6 ticks before the next block

    // Levers, buttons, repeaters and dust react to right-clicks through this.
    void setBlockUpdates(world::BlockUpdates* updates) { m_updates = updates; }
    // The held item's carried slots (a shulker box, M23.6), set before each tick: a
    // placed box gets them.
    void setPlaceContents(uint32_t contents) { m_placeContents = contents; }

    // Experience from blocks mined this tick (ores), and where (orbs spawn there).
    int takeExperience() { return std::exchange(m_experience, 0); }
    world::BlockPos experienceAt() const { return m_xpAt; }

    // Ticks spent eating or drinking the held item so far (sounds, M22.4).
    int eatTicks() const { return m_eatTicks; }

    // Game rule block_drops (M28.1): off, broken blocks give no items or experience.
    void setBlockDrops(bool on) { m_blockDrops = on; }
    // Adventure mode (M28.1c; wiki: Adventure): no breaking or placing blocks; using
    // them (doors, buttons, containers) still works.
    void setMayBuild(bool on) { m_mayBuild = on; }
    // Statistics (M28.1d): the block broken, the item used (a placed block, a mining
    // tool) and a tool worn out this tick (0: none).
    world::BlockId takeBroken() { return std::exchange(m_broken, world::BlockId(0)); }
    world::ItemId takeUsed() { return std::exchange(m_used, world::ItemId(0)); }
    world::ItemId takeBrokenTool() { return std::exchange(m_brokenTool, world::ItemId(0)); }

private:
    bool m_blockDrops = true;
    bool m_mayBuild = true;
    world::BlockId m_broken = 0;
    world::ItemId m_used = 0, m_brokenTool = 0;
    int m_experience = 0;
    world::BlockPos m_xpAt{};
    bool useBlock(const Player& player, const world::RayHit& hit, bool holding);
    void place(world::World& world, const Player& player, const world::RayHit& hit,
               world::BlockStateId state, std::vector<world::BlockPos>& changed, bool& placed);

    world::BlockUpdates* m_updates = nullptr;
    uint32_t m_placeContents = 0;
    int m_destroyCooldown = 0;
    int m_useCooldown = 0;
    std::optional<world::BlockPos> m_breaking;
    float m_progress = 0.0f;
    double m_progressExact = 0.0;
    int m_eatTicks = 0;
    int m_chorusCooldown = 0; // chorus fruit: 20 ticks between teleports
    bool m_ateChorus = false;
    std::vector<world::ItemStack> m_dropScratch = std::vector<world::ItemStack>(8); // reused

public:
    BlockInteraction() { m_dropScratch.clear(); }
};

} // namespace mc
