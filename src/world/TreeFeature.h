#pragma once

#include "world/Random.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>

namespace mc::world {

// Tree shapes (wiki: Tree), shared by world generation and saplings (M15) so a grown
// sapling looks like a generated tree of its kind.
// M18.2 adds jungle (small; mega = a 2x2 trunk from four saplings), dark oak (always
// 2x2) and cherry. 2x2 trunks occupy (x..x+1, z..z+1) from the given corner.
// MegaSpruce (M27.1): the giant 2x2 spruce of old growth taigas (also from four spruce
// saplings in vanilla).
enum class TreeKind : uint8_t { Oak, Birch, Spruce, Acacia, Jungle, MegaJungle, DarkOak, Cherry, PaleOak, Mangrove, MegaSpruce };
inline bool twoByTwo(TreeKind k) {
    return k == TreeKind::MegaJungle || k == TreeKind::DarkOak || k == TreeKind::PaleOak || k == TreeKind::MegaSpruce;
}

// Random trunk height per kind (wiki: Tree - oak 4-6, birch 5-7, spruce 6-9, acacia 5-6;
// jungle 4-12, large jungle 10-29, dark oak 6-8, cherry 5-7: the M18.2 ones are our
// reading of the wiki's pictures and descriptions).
inline int treeHeight(TreeKind kind, Xoroshiro& rng) {
    switch (kind) {
    case TreeKind::Oak: return 4 + static_cast<int>(rng.nextInt(3));
    case TreeKind::Birch: return 5 + static_cast<int>(rng.nextInt(3));
    case TreeKind::Spruce: return 6 + static_cast<int>(rng.nextInt(4));
    case TreeKind::Acacia: return 5 + static_cast<int>(rng.nextInt(2));
    case TreeKind::Jungle: return 4 + static_cast<int>(rng.nextInt(9));
    case TreeKind::MegaJungle: return 10 + static_cast<int>(rng.nextInt(20));
    case TreeKind::DarkOak: return 6 + static_cast<int>(rng.nextInt(3));
    case TreeKind::Cherry: return 5 + static_cast<int>(rng.nextInt(3));
    case TreeKind::PaleOak: return 6 + static_cast<int>(rng.nextInt(3)); // (M23.3b: like dark oak)
    case TreeKind::Mangrove: return 5 + static_cast<int>(rng.nextInt(4));
    case TreeKind::MegaSpruce: return 13 + static_cast<int>(rng.nextInt(15)); // (wiki: Spruce - giant ones 13-30ish)
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
    std::array<std::array<int32_t, 3>, 160> trunk{};
    int trunkCount = 0;
    auto log = [&](int32_t x, int32_t y, int32_t z) {
        if (trunkCount < int(trunk.size())) trunk[size_t(trunkCount++)] = {x, y, z};
        put(x, y, z, 0);
    };
    auto leaf = [&](int32_t x, int32_t y, int32_t z) {
        int best = 7;
        for (int t = 0; t < trunkCount; ++t) {
            const auto& tb = trunk[size_t(t)];
            best = std::min(best, std::abs(x - tb[0]) + std::abs(y - tb[1]) + std::abs(z - tb[2]));
        }
        if (best > 6) return; // would decay at once: big canopies stop where their logs reach
        put(x, y, z, std::max(best, 1));
    };
    // A round leaf layer of radius r around (cx + 0.5, cz + 0.5) - or around the
    // middle of a 2x2 trunk (`wide`); `holes`: chance per edge leaf to be left out.
    auto disc = [&](int32_t cx, int32_t y, int32_t cz, int r, bool wide, float holes) {
        const double mx = wide ? cx + 1.0 : cx + 0.5, mz = wide ? cz + 1.0 : cz + 0.5;
        for (int32_t x = cx - r - 1; x <= cx + r + 1; ++x)
            for (int32_t z = cz - r - 1; z <= cz + r + 1; ++z) {
                const double dx = x + 0.5 - mx, dz = z + 0.5 - mz, d2 = dx * dx + dz * dz;
                if (d2 > (r + 0.5) * (r + 0.5)) continue;
                if (holes > 0.0f && d2 > (r - 0.5) * (r - 0.5) && shape.nextFloat() < holes) continue;
                leaf(x, y, z);
            }
    };
    if (kind == TreeKind::MegaJungle || kind == TreeKind::DarkOak || kind == TreeKind::PaleOak) {
        const bool jungle = kind == TreeKind::MegaJungle;
        for (int i = 0; i < height; ++i)
            for (int k = 0; k < 4; ++k)
                log(wx + (k & 1), y0 + i, wz + (k >> 1));
        const int32_t top = y0 + height - 1;
        if (jungle) {
            // Side branches with small leaf clumps down the trunk, then a wide crown.
            for (int32_t y = top - 4; y > y0 + 3; y -= 2 + static_cast<int32_t>(shape.nextInt(3))) {
                const int dir = static_cast<int>(shape.nextInt(4));
                const int dx[4] = {1, -1, 0, 0}, dz[4] = {0, 0, 1, -1};
                int32_t bx = wx + (dx[dir] > 0 ? 1 : 0), bz = wz + (dz[dir] > 0 ? 1 : 0);
                const int len = 2 + static_cast<int>(shape.nextInt(2));
                for (int s = 1; s <= len; ++s)
                    log(bx + dx[dir] * s, y + s / 2, bz + dz[dir] * s);
                bx += dx[dir] * len;
                bz += dz[dir] * len;
                disc(bx, y + len / 2, bz, 2, false, 0.4f);
                disc(bx, y + len / 2 + 1, bz, 1, false, 0.0f);
            }
            disc(wx, top - 1, wz, 4, true, 0.3f);
            disc(wx, top, wz, 4, true, 0.2f);
            disc(wx, top + 1, wz, 3, true, 0.0f);
        } else {
            // Dark oak: a low, flat, very wide crown.
            disc(wx, top - 1, wz, 3, true, 0.3f);
            disc(wx, top, wz, 4, true, 0.25f);
            disc(wx, top + 1, wz, 2, true, 0.0f);
        }
    } else if (kind == TreeKind::MegaSpruce) {
        // A 2x2 trunk with a cone of leaves over its upper part (vanilla's giant spruce;
        // the pine form of old growth pine taigas is the same with a shorter cone - ours
        // has one form): radius grows by a block every 2-3 layers down from the tip.
        for (int i = 0; i < height; ++i)
            for (int k = 0; k < 4; ++k)
                log(wx + (k & 1), y0 + i, wz + (k >> 1));
        // The tip: 2 layers over the trunk, then rings widening by one every 2 layers,
        // every third layer pulled in by one (the spruce's tiers), down to radius 4.
        const int32_t tip = y0 + height + 1;
        const int cone = std::max(6, height / 2);
        for (int i = 0; i <= cone; ++i) {
            if (i < 2) {
                for (int k = 0; k < 4; ++k)
                    leaf(wx + (k & 1), tip - i, wz + (k >> 1));
                continue;
            }
            // (a wide disc needs radius 2 to reach past the 2x2 trunk)
            const int r = std::min(5, 2 + i / 3) - (i % 3 == 2 && i > 3 ? 1 : 0);
            disc(wx, tip - i, wz, std::max(2, r), true, r >= 4 ? 0.2f : 0.0f);
        }
    } else if (kind == TreeKind::Mangrove) {
        // A tall trunk under a rounded crown (vanilla's mangroves stand on arching
        // roots and hang propagules: ours don't yet).
        for (int i = 0; i < height; ++i)
            log(wx, y0 + i, wz);
        const int32_t top = y0 + height - 1;
        disc(wx, top - 2, wz, 2, false, 0.3f);
        disc(wx, top - 1, wz, 3, false, 0.3f);
        disc(wx, top, wz, 3, false, 0.2f);
        disc(wx, top + 1, wz, 2, false, 0.1f);
    } else if (kind == TreeKind::Cherry) {
        // A straight trunk under a broad, rounded pink crown (vanilla's cherry trees
        // branch; ours keeps one trunk).
        for (int i = 0; i < height; ++i)
            log(wx, y0 + i, wz);
        const int32_t top = y0 + height - 1;
        disc(wx, top - 1, wz, 3, false, 0.35f);
        disc(wx, top, wz, 4, false, 0.25f);
        disc(wx, top + 1, wz, 3, false, 0.2f);
        disc(wx, top + 2, wz, 1, false, 0.0f);
    } else if (kind == TreeKind::Acacia) {
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
        // Oak/birch/small jungle: vanilla's blob canopy - two wide layers, two narrow ones.
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
