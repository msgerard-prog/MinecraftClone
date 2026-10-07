#pragma once

#include "gameplay/Aabb.h"
#include "gameplay/Inventory.h"
#include "world/Random.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <vector>

namespace mc {

// Dropped item stacks (wiki: Item (entity)): fall with gravity 0.04 and drag 0.98,
// slide with ground friction, can be picked up after 10 ticks by walking within a
// block of them, despawn after 6000 ticks (5 minutes). Stored in a reserved pool
// (no allocation per spawn; hard rule 1). Box 0.25 x 0.25.
struct ItemEntity {
    glm::dvec3 pos{0.0}, prevPos{0.0}, vel{0.0};
    world::ItemStack stack;
    int age = 0;
    int pickupDelay = 10;
    float spinOffset = 0.0f; // random start angle for the render spin
    bool onGround = false;
    uint8_t skyLight = 15, blockLight = 0; // at its position, updated each tick (rendering)
};

class ItemEntities {
public:
    static constexpr int kMax = 2048;  // oldest are removed beyond this
    static constexpr int kDespawnTicks = 6000;
    static constexpr double kSize = 0.25;

    ItemEntities() { m_items.reserve(kMax); }

    // A dropped stack at `pos` (block drops: random small throw, vanilla-like).
    // Returns the new item (nullptr for an empty stack).
    ItemEntity* spawn(const glm::dvec3& pos, const world::ItemStack& stack, world::Xoroshiro& rng,
                      int pickupDelay = 10);
    // Thrown by the player along `look` (Q / death): faster, longer pickup delay.
    void throwFrom(const glm::dvec3& eye, const glm::dvec3& look, const world::ItemStack& stack,
                   world::Xoroshiro& rng);

    // One tick: physics, pickup into `inventory` if `player` (box) is near and
    // `canPickUp`, despawn. Returns how many stacks were picked up (for a sound later).
    int tick(const world::World& world, const Aabb& player, bool canPickUp, Inventory& inventory);

    const std::vector<ItemEntity>& items() const { return m_items; }
    // The nearest stack of `item` within `radius` of `pos` that may be picked up
    // (piglins and gold, M19.2); takeOne removes one item of it.
    const ItemEntity* nearest(const glm::dvec3& pos, world::ItemId item, double radius) const {
        const ItemEntity* best = nullptr;
        double bestD = radius * radius;
        for (const ItemEntity& e : m_items) {
            const glm::dvec3 d = e.pos - pos;
            const double d2 = glm::dot(d, d);
            if (e.stack.item == item && e.stack.count > 0 && e.pickupDelay == 0 && d2 < bestD) {
                best = &e;
                bestD = d2;
            }
        }
        return best;
    }
    void takeOne(const ItemEntity* e) {
        for (size_t i = 0; i < m_items.size(); ++i)
            if (&m_items[i] == e) {
                if (--m_items[i].stack.count == 0) {
                    m_items[i] = m_items.back();
                    m_items.pop_back();
                }
                return;
            }
    }
    void clear() { m_items.clear(); }

private:
    void move(const world::World& world, ItemEntity& e);
    std::vector<ItemEntity> m_items;
    std::vector<Aabb> m_boxes; // reused
};

} // namespace mc
