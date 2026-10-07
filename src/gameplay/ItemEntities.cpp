#include "gameplay/ItemEntities.h"

#include "world/Blocks.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mc {

void ItemEntities::spawn(const glm::dvec3& pos, const world::ItemStack& stack,
                         world::Xoroshiro& rng, int pickupDelay) {
    if (stack.empty()) return;
    if (m_items.size() >= size_t(kMax)) m_items.erase(m_items.begin()); // rare: drop the oldest
    ItemEntity e;
    e.pos = e.prevPos = pos;
    // wiki: Item (entity) - dropped blocks pop out with a small random velocity.
    e.vel = {rng.nextDouble() * 0.2 - 0.1, 0.2, rng.nextDouble() * 0.2 - 0.1};
    e.stack = stack;
    e.pickupDelay = pickupDelay;
    e.spinOffset = rng.nextFloat() * 2.0f * std::numbers::pi_v<float>;
    m_items.push_back(e);
}

void ItemEntities::throwFrom(const glm::dvec3& eye, const glm::dvec3& look,
                             const world::ItemStack& stack, world::Xoroshiro& rng) {
    spawn(eye - glm::dvec3(0, 0.3, 0), stack, rng, 40);
    m_items.back().vel = look * 0.3 + glm::dvec3(0, 0.1, 0);
}

void ItemEntities::move(const world::World& world, ItemEntity& e) {
    const auto& reg = world::blockRegistry();
    const Aabb box = Aabb::fromFeet(e.pos, kSize, kSize);
    const Aabb region = box.expandedTowards(e.vel);
    m_boxes.clear();
    for (int x = int(std::floor(region.min.x)); x <= int(std::floor(region.max.x)); ++x)
        for (int y = int(std::floor(region.min.y)); y <= int(std::floor(region.max.y)); ++y)
            for (int z = int(std::floor(region.min.z)); z <= int(std::floor(region.max.z)); ++z)
                if (reg.collides(world.getBlock({x, y, z})))
                    m_boxes.push_back({{double(x), double(y), double(z)}, {x + 1.0, y + 1.0, z + 1.0}});
    glm::dvec3 d = e.vel;
    Aabb b = box;
    for (int axis : {1, 0, 2}) { // y first, like vanilla
        for (const Aabb& w : m_boxes)
            d[axis] = b.clip(w, axis, d[axis]);
        glm::dvec3 step(0.0);
        step[axis] = d[axis];
        b = b.moved(step);
    }
    e.onGround = e.vel.y < 0.0 && d.y != e.vel.y;
    for (int a = 0; a < 3; ++a)
        if (d[a] != e.vel[a]) e.vel[a] = 0.0;
    e.pos += d;
}

int ItemEntities::tick(const world::World& world, const Aabb& player, bool canPickUp,
                       Inventory& inventory) {
    int picked = 0;
    const Aabb reach{player.min - glm::dvec3(1.0, 0.5, 1.0), player.max + glm::dvec3(1.0, 0.5, 1.0)};
    for (size_t i = 0; i < m_items.size();) {
        ItemEntity& e = m_items[i];
        e.prevPos = e.pos;
        ++e.age;
        if (e.pickupDelay > 0) --e.pickupDelay;
        // Unloaded chunk: keep still (vanilla doesn't tick entities there).
        const world::BlockPos at{int(std::floor(e.pos.x)), int(std::floor(e.pos.y)), int(std::floor(e.pos.z))};
        if (world.chunk(at.chunk())) {
            const auto& reg = world::blockRegistry();
            const bool inWater = reg.blockOf(world.getBlock(at)) == world::blocks::Water;
            if (inWater) {
                e.vel.y += 5.0e-4; // items float up slowly in water (wiki)
                e.vel *= 0.99;
            } else {
                e.vel.y -= 0.04; // gravity
            }
            move(world, e);
            const double friction = e.onGround ? 0.6 * 0.98 : 0.98;
            e.vel.x *= friction;
            e.vel.z *= friction;
            e.vel.y *= 0.98;
            if (e.onGround) e.vel.y *= -0.5;
        }
        bool remove = e.age >= kDespawnTicks || e.pos.y < world::kMinY - 64;
        if (!remove && canPickUp && e.pickupDelay == 0 &&
            reach.intersects(Aabb::fromFeet(e.pos, kSize, kSize))) {
            const int left = inventory.add(e.stack);
            if (left < e.stack.count) ++picked;
            e.stack.count = static_cast<uint8_t>(left);
            remove = left == 0;
        }
        if (remove) {
            m_items[i] = m_items.back(); // order doesn't matter: swap-remove
            m_items.pop_back();
        } else {
            ++i;
        }
    }
    return picked;
}

} // namespace mc
