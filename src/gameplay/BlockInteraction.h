#pragma once

#include "gameplay/Inventory.h"
#include "gameplay/Player.h"
#include "gameplay/Vitals.h"
#include "world/Random.h"
#include "world/Raycast.h"
#include "world/World.h"

#include <optional>
#include <vector>

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
class BlockInteraction {
public:
    // Vanilla repeat delays while a button is held (ticks).
    static constexpr int kDestroyDelay = 6; // creative (wiki: Creative, 0.3 s)
    static constexpr int kUseDelay = 4;     // public write-ups; unverified on the wiki

    // Breaks/places for this tick at `hit` - the block the outline showed on the last
    // rendered frame (vanilla acts on the highlighted block). `changed` gets the
    // edited positions (cleared first).
    void tick(world::World& world, const Player& player, const std::optional<world::RayHit>& hit,
              world::BlockStateId placeState, const InteractionInput& input,
              std::vector<world::BlockPos>& changed);

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
    struct Drop {
        glm::dvec3 pos;
        world::ItemStack stack;
    };
    void tickSurvival(world::World& world, const Player& player,
                      const std::optional<world::RayHit>& hit, Inventory& inventory,
                      Vitals& vitals, const InteractionInput& input, bool eyesInWater,
                      world::Xoroshiro& rng, std::vector<world::BlockPos>& changed,
                      std::vector<Drop>& drops);
    // Crack overlay: the block being broken and progress 0..1 (no block: nullopt).
    std::optional<world::BlockPos> breakingBlock() const { return m_breaking; }
    float breakProgress() const { return m_progress; }
    int eatingTicks() const { return m_eatTicks; }
    static constexpr int kEatTicks = 32; // wiki: Food (1.61 s)
    static constexpr int kSurvivalBreakDelay = 5;

private:
    void place(world::World& world, const Player& player, const world::RayHit& hit,
               world::BlockStateId state, std::vector<world::BlockPos>& changed, bool& placed);

    int m_destroyCooldown = 0;
    int m_useCooldown = 0;
    std::optional<world::BlockPos> m_breaking;
    float m_progress = 0.0f;
    int m_heldTicks = 0;
    int m_eatTicks = 0;
    std::vector<world::ItemStack> m_dropScratch = std::vector<world::ItemStack>(8); // reused

public:
    BlockInteraction() { m_dropScratch.clear(); }
};

} // namespace mc
