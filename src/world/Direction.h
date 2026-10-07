#pragma once

#include <glm/glm.hpp>

namespace mc::world {

// The six block faces, in vanilla's Direction order (DOWN, UP, NORTH, SOUTH, WEST, EAST).
// North = -Z, south = +Z, west = -X, east = +X.
enum class Direction { Down, Up, North, South, West, East };
inline constexpr int kDirectionCount = 6;

inline constexpr glm::ivec3 kDirectionNormals[kDirectionCount] = {
    {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0},
};

constexpr glm::ivec3 normal(Direction d) { return kDirectionNormals[static_cast<int>(d)]; }

} // namespace mc::world
