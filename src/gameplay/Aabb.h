#pragma once

#include <glm/glm.hpp>

#include <algorithm>

namespace mc {

// Axis-aligned bounding box in world space (doubles, like vanilla's AABB).
struct Aabb {
    glm::dvec3 min{0.0};
    glm::dvec3 max{0.0};

    static Aabb fromFeet(const glm::dvec3& feet, double width, double height) {
        const double h = width / 2.0;
        return {{feet.x - h, feet.y, feet.z - h}, {feet.x + h, feet.y + height, feet.z + h}};
    }
    Aabb moved(const glm::dvec3& d) const { return {min + d, max + d}; }
    // Grows the box toward a movement so it covers the swept volume.
    Aabb expandedTowards(const glm::dvec3& d) const {
        Aabb b = *this;
        for (int a = 0; a < 3; ++a)
            (d[a] < 0 ? b.min[a] : b.max[a]) += d[a];
        return b;
    }
    bool intersects(const Aabb& o) const {
        return min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y &&
               min.z < o.max.z && max.z > o.min.z;
    }

    // How far this box may move along `axis` by `d` before hitting `wall` (vanilla
    // VoxelShape.collide, per axis): returns d clipped to the gap, or d if the boxes
    // don't overlap on the other two axes.
    double clip(const Aabb& wall, int axis, double d) const {
        const int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
        if (max[a1] <= wall.min[a1] || min[a1] >= wall.max[a1]) return d;
        if (max[a2] <= wall.min[a2] || min[a2] >= wall.max[a2]) return d;
        if (d > 0 && max[axis] <= wall.min[axis]) return std::min(d, wall.min[axis] - max[axis]);
        if (d < 0 && min[axis] >= wall.max[axis]) return std::max(d, wall.max[axis] - min[axis]);
        return d;
    }
};

} // namespace mc
