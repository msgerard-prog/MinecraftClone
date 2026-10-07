#pragma once

#include "gameplay/Player.h"
#include "world/Raycast.h"
#include "world/World.h"

#include <optional>
#include <vector>

namespace mc {

// Mouse buttons for one tick.
struct InteractionInput {
    bool attack = false; // left button held
    bool use = false;    // right button held
};

// Creative-mode breaking and placing (survival mining times come with items/tools in
// M9). Runs in the game tick; records every block it changed so the renderer can
// re-mesh them.
class BlockInteraction {
public:
    // Vanilla repeat delays while a button is held (ticks).
    static constexpr int kDestroyDelay = 5; // creative
    static constexpr int kUseDelay = 4;

    // Breaks/places for this tick. `changed` gets the edited positions (cleared first).
    void tick(world::World& world, const Player& player, world::BlockStateId placeState,
              const InteractionInput& input, std::vector<world::BlockPos>& changed);

    // What the player is looking at (from the current tick position).
    static std::optional<world::RayHit> target(const world::World& world, const Player& player);

    // The state to place against `face`: pillars (logs, deepslate) take the axis of
    // the clicked face, as in vanilla.
    static world::BlockStateId orientedState(world::BlockStateId state, world::Direction face);

private:
    int m_destroyCooldown = 0;
    int m_useCooldown = 0;
};

} // namespace mc
