#include "gameplay/Pathfinder.h"

#include "world/BlockShapes.h"
#include "world/Blocks.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

namespace {

constexpr int kHashSlots = 4096; // > 2 x kMaxNodes, power of two
constexpr int kMaxDrop = 3;      // vanilla: mobs path down at most 3 blocks

uint64_t pack(const glm::ivec3& p) {
    return (uint64_t(uint32_t(p.x) & 0x3FFFFFFu) << 38) |
           (uint64_t(uint32_t(p.z) & 0x3FFFFFFu) << 12) |
           (uint64_t(uint32_t(p.y) & 0xFFFu)); // y: 12 bits cover any world height
}

int heuristic(const glm::ivec3& a, const glm::ivec3& b) {
    const glm::ivec3 d = glm::abs(a - b);
    // Octile distance (diagonals cost 14): never more than the real cost (admissible).
    return std::max(d.x, d.z) * 10 + std::min(d.x, d.z) * 4 + d.y * 5;
}

} // namespace

Pathfinder::Pathfinder() {
    m_nodes.reserve(kMaxNodes + 8);
    m_heap.reserve(kMaxNodes * 9 + 8); // 8 neighbours a node + re-pushes (M30.5: diagonals)
    m_keys.assign(kHashSlots, 0);
    m_index.assign(kHashSlots, -1);
    m_stamp.assign(kHashSlots, 0);
}

BlockStateId Pathfinder::at(const World& world, int x, int y, int z) {
    if (!world.isInHeight(y)) return 0;
    const ChunkPos cp{blockToChunk(x), blockToChunk(z)};
    if (m_epoch != world.chunkEpoch() || !(m_chunkPos == cp)) {
        m_chunk = world.chunk(cp);
        m_chunkPos = cp;
        m_epoch = world.chunkEpoch();
    }
    return m_chunk ? m_chunk->get(blockToLocal(x), y, blockToLocal(z))
                   : blockRegistry().defaultState(blocks::Stone);
}

bool Pathfinder::cellFree(BlockStateId s, bool feet) const {
    const auto& reg = blockRegistry();
    const BlockId b = reg.blockOf(s);
    if (b == blocks::Lava || isFire(b)) return false;
    if (!reg.collides(s)) return true;
    const BlockId like = reg.likeOf(b);
    // Doors (vanilla DOOR_OPEN / DOOR_WOOD_CLOSED / DOOR_IRON_CLOSED), gates and trapdoors.
    if (like == blocks::OakDoor || b == blocks::IronDoor)
        return reg.get(s, properties::open) == 0 || (like == blocks::OakDoor && m_opts.openDoors); // [true, false]
    if ((like == blocks::OakFenceGate || like == blocks::OakTrapdoor || b == blocks::IronTrapdoor) &&
        reg.get(s, properties::open) == 0)
        return true;
    // Low blocks a mob simply walks over (carpets, thin snow, lily pads...).
    if (feet) {
        const BlockShape& shape = collisionShape(s);
        int top = 0;
        for (int i = 0; i < shape.count; ++i) top = std::max<int>(top, shape.boxes[size_t(i)].to[1]);
        return top <= 3; // (3/16 block)
    }
    return false;
}

bool Pathfinder::passable(const World& world, const glm::ivec3& c) {
    for (int dz = 0; dz < m_opts.footprint; ++dz)
        for (int dx = 0; dx < m_opts.footprint; ++dx)
            for (int i = 0; i < m_opts.height; ++i)
                if (!cellFree(at(world, c.x + dx, c.y + i, c.z + dz), i == 0)) return false;
    return true;
}

bool Pathfinder::tallAt(const World& world, const glm::ivec3& c) {
    const BlockStateId s = at(world, c.x, c.y, c.z);
    if (!blockRegistry().collides(s)) return false;
    const BlockShape& shape = collisionShape(s);
    for (int i = 0; i < shape.count; ++i)
        if (shape.boxes[size_t(i)].to[1] > 16) return true; // (fences, walls, closed gates: 24/16)
    return false;
}

bool Pathfinder::standable(const World& world, const glm::ivec3& c) {
    if (!world.isInHeight(c.y) || !passable(world, c)) return false;
    const auto& reg = blockRegistry();
    if (reg.blockOf(at(world, c.x, c.y, c.z)) == blocks::Water) return true; // swimming
    // Something to stand on under the footprint, and no fence top to balance on.
    bool floor = false;
    for (int dz = 0; dz < m_opts.footprint; ++dz)
        for (int dx = 0; dx < m_opts.footprint; ++dx) {
            const glm::ivec3 below{c.x + dx, c.y - 1, c.z + dz};
            if (tallAt(world, below)) return false;
            floor = floor || reg.collides(at(world, below.x, below.y, below.z));
        }
    return floor;
}

int Pathfinder::danger(const World& world, const glm::ivec3& c) {
    const auto& reg = blockRegistry();
    int cost = reg.blockOf(at(world, c.x, c.y, c.z)) == blocks::Water ? 80 : 0;
    static constexpr int kSides[5][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}};
    for (const auto& d : kSides) {
        const BlockId b = reg.blockOf(at(world, c.x + d[0], c.y + d[1], c.z + d[2]));
        if (b == blocks::Lava || isFire(b)) return cost + 80;
    }
    return cost;
}

int Pathfinder::slot(const glm::ivec3& p) const {
    const uint64_t k = pack(p) | 1; // never 0
    uint64_t h = k * 0x9E3779B97F4A7C15ull;
    size_t i = size_t(h >> 52) & (kHashSlots - 1);
    while (m_stamp[i] == m_search && m_keys[i] != k)
        i = (i + 1) & (kHashSlots - 1);
    return int(i);
}

void Pathfinder::push(int node) {
    m_heap.push_back(node);
    std::push_heap(m_heap.begin(), m_heap.end(),
                   [&](int a, int b) { return m_nodes[size_t(a)].f > m_nodes[size_t(b)].f; });
}

int Pathfinder::pop() {
    std::pop_heap(m_heap.begin(), m_heap.end(),
                  [&](int a, int b) { return m_nodes[size_t(a)].f > m_nodes[size_t(b)].f; });
    const int n = m_heap.back();
    m_heap.pop_back();
    return n;
}

int Pathfinder::find(const World& world, const glm::ivec3& start, const glm::ivec3& goal,
                     const PathOptions& opts, int maxNodes, glm::ivec3* out, int maxOut) {
    m_opts = opts;
    const int height = opts.height;
    maxNodes = std::min(maxNodes, kMaxNodes);
    ++m_search;
    m_nodes.clear();
    m_heap.clear();
    auto visit = [&](const glm::ivec3& p, int g, int parent) {
        const int s = slot(p);
        if (m_stamp[size_t(s)] == m_search) { // seen: keep the cheaper way there
            Node& n = m_nodes[size_t(m_index[size_t(s)])];
            if (n.closed || g >= n.g) return;
            n.g = g;
            n.f = g + heuristic(p, goal);
            n.parent = parent;
            push(m_index[size_t(s)]); // (duplicate heap entries are skipped by `closed`)
            return;
        }
        if (int(m_nodes.size()) >= maxNodes) return;
        m_stamp[size_t(s)] = m_search;
        m_keys[size_t(s)] = pack(p) | 1;
        m_index[size_t(s)] = int(m_nodes.size());
        m_nodes.push_back({p, g, g + heuristic(p, goal), parent, false});
        push(int(m_nodes.size()) - 1);
    };
    // A cell already finished needs no danger/standability work (about half the
    // neighbours on open ground).
    auto closedAt = [&](const glm::ivec3& p) {
        const int s = slot(p);
        return m_stamp[size_t(s)] == m_search && m_nodes[size_t(m_index[size_t(s)])].closed;
    };
    visit(start, 0, -1);
    int best = 0; // the node nearest to the goal (partial path)
    int bestH = heuristic(start, goal);
    static constexpr int kDirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    static constexpr int kDiagonals[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    while (!m_heap.empty()) {
        const int ni = pop();
        if (m_nodes[size_t(ni)].closed) continue; // (an older, costlier heap entry)
        m_nodes[size_t(ni)].closed = true;
        const Node n = m_nodes[size_t(ni)];
        const int h = heuristic(n.pos, goal);
        if (h < bestH) {
            best = ni;
            bestH = h;
        }
        if (n.pos == goal) break;
        bool flat[4] = {false, false, false, false}; // (each side walkable on the level: diagonals)
        for (int k = 0; k < 4; ++k) {
            const auto& d = kDirs[k];
            glm::ivec3 q{n.pos.x + d[0], n.pos.y, n.pos.z + d[1]};
            if (passable(world, q)) {
                // Walk on, or drop down to the first floor within kMaxDrop.
                int drop = 0;
                while (drop <= kMaxDrop && !standable(world, q)) {
                    --q.y;
                    ++drop;
                    if (!passable(world, q)) {
                        drop = kMaxDrop + 1;
                        break;
                    }
                }
                flat[k] = drop == 0;
                if (drop > kMaxDrop || closedAt(q)) continue;
                visit(q, n.g + 10 + drop * 2 + danger(world, q), ni);
            } else if (!tallAt(world, q)) {
                // Step up 1: the cell above the wall (not a fence or wall), with headroom
                // over the mob.
                const glm::ivec3 up{q.x, q.y + 1, q.z};
                const int saved = m_opts.height;
                m_opts.height = 1;
                const bool headroom = passable(world, {n.pos.x, n.pos.y + height, n.pos.z});
                m_opts.height = saved;
                if (!closedAt(up) && standable(world, up) && headroom)
                    visit(up, n.g + 15 + danger(world, up), ni);
            }
        }
        // Diagonals (M30.5; vanilla): on the level, both sides walkable - no corner cutting.
        for (const auto& d : kDiagonals) {
            const int sx = d[0] < 0 ? 2 : 3, sz = d[1] < 0 ? 0 : 1; // kDirs indices of the two sides
            if (!flat[sx] || !flat[sz]) continue;
            const glm::ivec3 q{n.pos.x + d[0], n.pos.y, n.pos.z + d[1]};
            if (closedAt(q) || !standable(world, q)) continue;
            visit(q, n.g + 14 + danger(world, q), ni);
        }
    }
    // Walk back from the best node; write the first maxOut steps (start excluded).
    int count = 0;
    for (int i = best; m_nodes[size_t(i)].parent >= 0; i = m_nodes[size_t(i)].parent)
        ++count;
    int idx = count - 1;
    for (int i = best; m_nodes[size_t(i)].parent >= 0; i = m_nodes[size_t(i)].parent, --idx)
        if (idx < maxOut) out[idx] = m_nodes[size_t(i)].pos;
    return std::min(count, maxOut);
}

} // namespace mc
