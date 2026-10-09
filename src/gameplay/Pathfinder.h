#pragma once

#include "world/World.h"

#include <glm/glm.hpp>

#include <climits>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mc {

// Ground pathfinding for mobs (M16.2; wiki: Mob AI › Pathfinding): A* over block cells
// where a mob can stand - its body (`height` blocks, `footprint` cells square from the
// cell's corner) fits and the block below is solid, or it swims in water. From a cell a
// mob may walk to the 8 neighbours (diagonals only on the level and without cutting a
// corner - M30.5), step up 1 block, or drop up to 3 (vanilla's default max fall). Lava and
// fire cells are avoided; cells next to them cost +8 and water +8 (vanilla's path
// "maluses"). M30.5 (vanilla WalkNodeEvaluator's node types): open doors, gates and
// trapdoors and low blocks (carpets, thin snow) are passable, closed wooden doors too for
// mobs that open doors; fences, walls and closed gates (1.5 high) can't be stepped onto.
// When the goal can't be reached the path leads to the visited cell nearest to it (a
// partial path, as vanilla). All buffers are reserved once (hard rule 1).
struct PathOptions {
    int height = 2;         // blocks the body needs
    int footprint = 1;      // cells square (ceil of the mob's width)
    bool openDoors = false; // villagers, traders, piglins open wooden doors (vanilla)
};

class Pathfinder {
public:
    static constexpr int kMaxNodes = 1024;

    Pathfinder();
    // Writes up to `maxOut` cells from `start` (excluded) toward `goal` into `out`;
    // returns how many. `maxNodes` bounds the search (vanilla: follow range x 16).
    int find(const world::World& world, const glm::ivec3& start, const glm::ivec3& goal, const PathOptions& opts,
             int maxNodes, glm::ivec3* out, int maxOut);
    int find(const world::World& world, const glm::ivec3& start, const glm::ivec3& goal, int height,
             int maxNodes, glm::ivec3* out, int maxOut) {
        return find(world, start, goal, PathOptions{height, 1, false}, maxNodes, out, maxOut);
    }

    // Whether a mob can stand in this cell (public for tests).
    bool standable(const world::World& world, const glm::ivec3& c, int height) {
        m_opts = PathOptions{height, 1, false};
        return standable(world, c);
    }
    // The cell a mob of `footprint` cells stands in, from its position (the footprint's
    // min corner: wide mobs are centred on footprint / 2 from it).
    static glm::ivec3 cellOf(const glm::dvec3& pos, int footprint) {
        const double off = double(footprint) * 0.5 - 0.5;
        return {int(std::floor(pos.x - off)), int(std::floor(pos.y + 0.01)), int(std::floor(pos.z - off))};
    }

private:
    struct Node {
        glm::ivec3 pos;
        int g;      // cost so far (x10)
        int f;      // g + heuristic
        int parent; // node index, -1 for the start
        bool closed;
    };
    world::BlockStateId at(const world::World& world, int x, int y, int z);
    bool cellFree(world::BlockStateId s, bool feet) const; // the body may be in this block
    bool passable(const world::World& world, const glm::ivec3& c); // body fits, no lava/fire
    bool standable(const world::World& world, const glm::ivec3& c);
    bool tallAt(const world::World& world, const glm::ivec3& c); // a fence, wall or closed gate
    int danger(const world::World& world, const glm::ivec3& c); // extra cost (x10)
    PathOptions m_opts;
    int slot(const glm::ivec3& p) const;
    void push(int node);
    int pop();

    std::vector<Node> m_nodes;
    std::vector<int> m_heap;       // open set: node indices, min-heap on f
    std::vector<uint64_t> m_keys;  // hash slots: packed position (0 = empty) ...
    std::vector<int> m_index;      // ... and the node there
    std::vector<uint32_t> m_stamp; // slot used in search number ...
    uint32_t m_search = 0;
    const world::Chunk* m_chunk = nullptr; // last chunk read
    world::ChunkPos m_chunkPos{INT32_MIN, INT32_MIN};
    uint64_t m_epoch = ~0ull;
};

} // namespace mc
