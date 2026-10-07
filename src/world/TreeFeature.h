#pragma once

#include "world/Random.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>

namespace mc::world {

// Tree shapes (wiki: Tree), shared by world generation and saplings (M15) so a grown
// sapling looks like a generated tree of its kind.
enum class TreeKind : uint8_t { Oak, Birch, Spruce, Acacia };

// Random trunk height per kind (wiki: Tree - oak 4-6, birch 5-7, spruce 6-9, acacia 5-6).
inline int treeHeight(TreeKind kind, Xoroshiro& rng) {
    switch (kind) {
    case TreeKind::Oak: return 4 + static_cast<int>(rng.nextInt(3));
    case TreeKind::Birch: return 5 + static_cast<int>(rng.nextInt(3));
    case TreeKind::Spruce: return 6 + static_cast<int>(rng.nextInt(4));
    case TreeKind::Acacia: return 5 + static_cast<int>(rng.nextInt(2));
    }
    return 4;
}

// Calls put(x, y, z, distance) for every block of a tree whose trunk starts at
// (wx, y0, wz): distance 0 for logs, 1..7 for leaves - taxicab steps to the nearest
// trunk block of this tree (vanilla leaves' `distance`, so generated leaves never
// decay). `shape` decides the random leaf corners and the acacia's lean. Logs come
// before the leaves around them.
template <typename Put>
void treeShape(TreeKind kind, int32_t wx, int32_t y0, int32_t wz, int height, Xoroshiro& shape, Put&& put) {
    std::array<std::array<int32_t, 3>, 16> trunk{};
    int trunkCount = 0;
    auto log = [&](int32_t x, int32_t y, int32_t z) {
        if (trunkCount < 16) trunk[size_t(trunkCount++)] = {x, y, z};
        put(x, y, z, 0);
    };
    auto leaf = [&](int32_t x, int32_t y, int32_t z) {
        int best = 7;
        for (int t = 0; t < trunkCount; ++t) {
            const auto& tb = trunk[size_t(t)];
            best = std::min(best, std::abs(x - tb[0]) + std::abs(y - tb[1]) + std::abs(z - tb[2]));
        }
        put(x, y, z, std::clamp(best, 1, 7));
    };
    if (kind == TreeKind::Acacia) {
        // A trunk that leans 2 blocks in a random direction, flat wide canopy.
        int32_t topX = wx, topZ = wz;
        const int dir = static_cast<int>(shape.nextInt(4));
        const int dx[4] = {1, -1, 0, 0}, dz[4] = {0, 0, 1, -1};
        for (int i = 0; i < height; ++i) {
            if (i >= height - 2) {
                topX += dx[dir];
                topZ += dz[dir];
            }
            log(topX, y0 + i, topZ);
        }
        const int32_t ty = y0 + height - 1;
        for (int ox = -3; ox <= 3; ++ox)
            for (int oz = -3; oz <= 3; ++oz)
                if (std::abs(ox) + std::abs(oz) <= 4) leaf(topX + ox, ty + 1, topZ + oz);
        for (int ox = -1; ox <= 1; ++ox)
            for (int oz = -1; oz <= 1; ++oz)
                leaf(topX + ox, ty + 2, topZ + oz);
    } else if (kind == TreeKind::Spruce) {
        for (int i = 0; i < height; ++i)
            log(wx, y0 + i, wz);
        // Cone: radius alternates 1, 2 going down from the tip, starting 2 above the
        // ground layer.
        const int32_t tip = y0 + height;
        leaf(wx, tip, wz);
        int r = 0;
        for (int32_t y = tip - 1; y >= y0 + 2; --y) {
            r = (r >= 2 || (tip - y) % 2 == 1) ? 1 : 2;
            if (tip - y <= 1) r = 1;
            for (int ox = -r; ox <= r; ++ox)
                for (int oz = -r; oz <= r; ++oz)
                    if (!(std::abs(ox) == r && std::abs(oz) == r && r > 1)) leaf(wx + ox, y, wz + oz);
        }
    } else {
        // Oak/birch: vanilla's blob canopy - two wide layers, two narrow ones.
        for (int i = 0; i < height; ++i)
            log(wx, y0 + i, wz);
        const int32_t top = y0 + height - 1;
        for (int32_t y = top - 2; y <= top + 1; ++y) {
            const int r = y >= top ? 1 : 2;
            for (int ox = -r; ox <= r; ++ox)
                for (int oz = -r; oz <= r; ++oz) {
                    const bool corner = std::abs(ox) == r && std::abs(oz) == r;
                    if (corner && (y == top + 1 || shape.nextInt(2) == 0)) continue;
                    if (y == top + 1 && std::abs(ox) + std::abs(oz) > 1) continue;
                    leaf(wx + ox, y, wz + oz);
                }
        }
    }
}

} // namespace mc::world
