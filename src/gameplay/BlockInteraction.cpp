#include "gameplay/BlockInteraction.h"

#include "world/Blocks.h"
#include "world/Rotation.h"

namespace mc {

std::optional<world::RayHit> BlockInteraction::target(const world::World& world,
                                                      const Player& player) {
    const glm::dvec3 eye = player.position() + glm::dvec3(0.0, player.eyeHeight(), 0.0);
    return world::raycastBlocks(world, eye,
                                glm::dvec3(world::lookVector(player.yaw(), player.pitch())),
                                world::kCreativeReach);
}

world::BlockStateId BlockInteraction::orientedState(world::BlockStateId state,
                                                    world::Direction face) {
    const auto& r = world::blockRegistry();
    if (!r.value(state, "axis")) return state;
    const char* axis = (face == world::Direction::East || face == world::Direction::West) ? "x"
                       : (face == world::Direction::Up || face == world::Direction::Down) ? "y"
                                                                                          : "z";
    return r.with(state, "axis", axis).value_or(state);
}

void BlockInteraction::tick(world::World& world, const Player& player,
                            const std::optional<world::RayHit>& hit, world::BlockStateId placeState,
                            const InteractionInput& input, std::vector<world::BlockPos>& changed) {
    changed.clear();
    // Cooldowns only run while the button is held; releasing allows an instant click.
    // A fresh click always acts (even a press shorter than a tick); holding repeats.
    m_destroyCooldown = input.attackClick ? 0
                        : input.attack    ? std::max(0, m_destroyCooldown - 1)
                                          : 0;
    m_useCooldown = input.useClick ? 0 : input.use ? std::max(0, m_useCooldown - 1) : 0;
    const bool attack = input.attack || input.attackClick;
    const bool use = input.use || input.useClick;

    if (!hit) return;
    const auto& reg = world::blockRegistry();

    if (attack && m_destroyCooldown == 0) {
        world.setBlock(hit->block, 0);
        changed.push_back(hit->block);
        m_destroyCooldown = kDestroyDelay;
        return; // one action per tick
    }
    if (use && m_useCooldown == 0) {
        m_useCooldown = kUseDelay;
        const world::BlockPos at = world::neighbour(hit->block, hit->face);
        if (!world::isInBuildHeight(at.y)) return;
        const world::BlockStateId existing = world.getBlock(at);
        // Air and fluids can be replaced.
        if (existing != 0 && reg.blockOf(existing) != world::blocks::Water) return;
        const Aabb blockBox{{at.x, at.y, at.z}, {at.x + 1.0, at.y + 1.0, at.z + 1.0}};
        if (reg.collides(placeState) && player.box().intersects(blockBox))
            return; // not inside the player
        if (!world.chunk(world::ChunkPos{world::blockToChunk(at.x), world::blockToChunk(at.z)}))
            return;
        world.setBlock(at, orientedState(placeState, hit->face));
        changed.push_back(at);
    }
}

} // namespace mc
