#include "gameplay/BlockInteraction.h"

#include "gameplay/Mining.h"

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

    if (attack && m_destroyCooldown == 0) {
        world.setBlock(hit->block, 0);
        changed.push_back(hit->block);
        m_destroyCooldown = kDestroyDelay;
        return; // one action per tick
    }
    if (use && m_useCooldown == 0 && placeState != 0) {
        m_useCooldown = kUseDelay;
        bool placed = false;
        place(world, player, *hit, placeState, changed, placed);
    }
}

void BlockInteraction::place(world::World& world, const Player& player, const world::RayHit& hitRef,
                             world::BlockStateId placeState, std::vector<world::BlockPos>& changed,
                             bool& placed) {
    const auto& reg = world::blockRegistry();
    const world::RayHit* hit = &hitRef;
    {
        const world::BlockPos at = world::neighbour(hit->block, hit->face);
        if (!world::isInBuildHeight(at.y)) return;
        const world::BlockStateId existing = world.getBlock(at);
        // Air and fluids can be replaced (not by torches: they can't exist in water).
        const bool torch = reg.blockOf(placeState) == world::blocks::Torch;
        if (existing != 0 && (reg.blockOf(existing) != world::blocks::Water || torch)) return;
        const Aabb blockBox{{at.x, at.y, at.z}, {at.x + 1.0, at.y + 1.0, at.z + 1.0}};
        if (reg.collides(placeState) && player.box().intersects(blockBox))
            return; // not inside the player
        if (!world.chunk(world::ChunkPos{world::blockToChunk(at.x), world::blockToChunk(at.z)}))
            return;
        // Floor torches need a top face that supports its centre (wiki:
        // Opacity/Placement): any full-collision block, glass included. Wall
        // torches: not yet (game-design.md › Known deviations).
        if (torch &&
            (hit->face != world::Direction::Up || !reg.collides(world.getBlock(hit->block))))
            return;
        world.setBlock(at, orientedState(placeState, hit->face));
        changed.push_back(at);
        placed = true;
    }
}

void BlockInteraction::tickSurvival(world::World& world, const Player& player,
                                    const std::optional<world::RayHit>& hit, Inventory& inventory,
                                    Vitals& vitals, const InteractionInput& input, bool eyesInWater,
                                    world::Xoroshiro& rng, std::vector<world::BlockPos>& changed,
                                    std::vector<Drop>& drops) {
    changed.clear();
    drops.clear();
    const auto& items = world::itemRegistry();
    const bool attack = input.attack || input.attackClick;
    const bool use = input.use || input.useClick;
    const bool pausing = m_destroyCooldown > 0; // after a break: 5 idle ticks
    if (pausing) --m_destroyCooldown;
    m_useCooldown = input.useClick ? 0 : input.use ? std::max(0, m_useCooldown - 1) : 0;

    // Eating: hold use with food while hungry (wiki: Food).
    const world::ItemDef& held = items.item(inventory.selectedStack().item);
    if (use && held.food > 0 && vitals.food() < Vitals::kMaxFood) {
        if (++m_eatTicks >= kEatTicks) {
            vitals.eat(held.food, held.saturation);
            inventory.consumeSelected(1);
            m_eatTicks = 0;
        }
    } else {
        m_eatTicks = 0;
    }

    // Breaking.
    if (!attack || !hit) {
        m_breaking.reset();
        m_progress = 0.0f;
        m_heldTicks = 0;
    } else if (!pausing) {
        if (!m_breaking || !(*m_breaking == hit->block)) { // a new target starts over
            m_breaking = hit->block;
            m_heldTicks = 0;
        }
        const world::BlockStateId state = world.getBlock(hit->block);
        const int ticks = breakTicks(state, inventory.selectedStack(), player.onGround(), eyesInWater);
        if (ticks >= 0) {
            // Whole ticks held (the speed is re-evaluated each tick, like vanilla).
            ++m_heldTicks;
            m_progress = ticks == 0 ? 1.0f : std::min(1.0f, float(m_heldTicks) / float(ticks));
            if (m_heldTicks >= ticks) {
                for (const world::ItemStack& d : blockDrops(state, inventory.selectedStack(), rng))
                    drops.push_back({{hit->block.x + 0.5, hit->block.y + 0.25, hit->block.z + 0.5}, d});
                world.setBlock(hit->block, 0);
                changed.push_back(hit->block);
                vitals.exhaust(0.005f); // wiki: Hunger - breaking a block
                // Tools wear 1 per block broken (instant blocks don't count).
                const world::ItemStack& tool = inventory.selectedStack();
                const world::ItemDef& def = items.item(tool.item);
                if (def.durability > 0 && ticks > 0) {
                    world::ItemStack worn = tool;
                    ++worn.damage;
                    inventory.setSlot(inventory.selected(), worn.damage >= def.durability ? world::ItemStack{} : worn);
                }
                m_breaking.reset();
                m_progress = 0.0f;
                m_heldTicks = 0;
                m_destroyCooldown = kSurvivalBreakDelay;
                return; // one action per tick
            }
        }
    }

    // Placing uses up the held block.
    if (use && m_useCooldown == 0 && hit && held.block) {
        m_useCooldown = kUseDelay;
        bool placed = false;
        place(world, player, *hit, inventory.placeState(), changed, placed);
        if (placed) inventory.consumeSelected(1);
    }
}

} // namespace mc
