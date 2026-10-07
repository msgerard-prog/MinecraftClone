#include "gameplay/PrimedTnt.h"

#include "gameplay/BlockCollision.h"

#include <cmath>
#include <numbers>

namespace mc {

void PrimedTnt::prime(const world::BlockPos& b, int fuse, world::Xoroshiro& rng) {
    if (int(m_items.size()) >= kMax) return;
    PrimedTntEntity e;
    e.pos = e.prevPos = glm::dvec3(b.x + 0.5, b.y, b.z + 0.5);
    const double a = rng.nextDouble() * 2.0 * std::numbers::pi; // (wiki: a 0.02 push in a random direction)
    e.vel = glm::dvec3(-std::sin(a) * 0.02, 0.2, -std::cos(a) * 0.02);
    e.fuse = fuse;
    m_items.push_back(e);
}

void PrimedTnt::tick(const world::World& world) {
    m_explode.clear();
    for (size_t i = 0; i < m_items.size();) {
        PrimedTntEntity& e = m_items[i];
        e.prevPos = e.pos;
        e.vel.y -= 0.04;
        // Move with collision, axis by axis (y first), against block shapes.
        const Aabb box = Aabb::fromFeet(e.pos, 0.98, 0.98);
        gatherBlockBoxes(world, box.expandedTowards(e.vel), m_boxes);
        glm::dvec3 d = e.vel;
        Aabb b = box;
        bool onGround = false;
        for (int axis : {1, 0, 2}) {
            const double wanted = d[axis];
            for (const Aabb& w : m_boxes)
                d[axis] = b.clip(w, axis, d[axis]);
            if (axis == 1 && wanted < 0.0 && d[1] > wanted) onGround = true;
            if (d[axis] != wanted) e.vel[axis] = 0.0;
            glm::dvec3 step(0.0);
            step[axis] = d[axis];
            b = b.moved(step);
        }
        e.pos += d;
        e.vel *= 0.98;
        if (onGround) e.vel *= glm::dvec3(0.7, -0.5, 0.7);
        if (--e.fuse <= 0) {
            if (m_explode.size() < m_explode.capacity()) m_explode.push_back(e.pos + glm::dvec3(0.0, 0.0625, 0.0));
            m_items[i] = m_items.back();
            m_items.pop_back();
        } else {
            ++i;
        }
    }
}

void PrimedTnt::push(const glm::dvec3& centre, float power) {
    const double reach = 2.0 * power;
    for (PrimedTntEntity& e : m_items) {
        const glm::dvec3 mid = e.pos + glm::dvec3(0.0, 0.49, 0.0);
        const glm::dvec3 d = mid - centre;
        const double dist = glm::length(d);
        if (dist >= reach || dist < 1e-6) continue;
        e.vel += d / dist * (1.0 - dist / reach);
    }
}

} // namespace mc
