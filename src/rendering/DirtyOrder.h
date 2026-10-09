#pragma once

#include "world/SectionSnapshot.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace mc::gfx {

// The renderer's queue of sections to mesh, kept far -> near from `camera` (the nearest
// at the back, popped first). `sorted` entries at the front are already in order; the
// rest were appended since. Sorts the tail and merges it in through `scratch` (reused:
// no allocation once it has grown). Both runs must be ordered for the same `camera` - a
// prefix sorted for an older camera position isn't in order for a new one (the caller
// re-sorts all with sorted = 0 when its camera moves on). GL-free, so it is testable.
inline void sortFarToNear(std::vector<world::SectionPos>& list, size_t sorted, const glm::dvec3& camera,
                          std::vector<world::SectionPos>& scratch) {
    auto distance2 = [&camera](const world::SectionPos& p) {
        const glm::dvec3 d = glm::dvec3(p.x * 16.0 + 8.0, p.y * 16.0 + 8.0, p.z * 16.0 + 8.0) - camera;
        return glm::dot(d, d);
    };
    auto farFirst = [&](const world::SectionPos& a, const world::SectionPos& b) { return distance2(a) > distance2(b); };
    const auto mid = list.begin() + static_cast<std::ptrdiff_t>(std::min(sorted, list.size()));
    std::sort(mid, list.end(), farFirst);
    scratch.resize(list.size());
    std::merge(list.begin(), mid, mid, list.end(), scratch.begin(), farFirst);
    list.swap(scratch);
}

} // namespace mc::gfx
