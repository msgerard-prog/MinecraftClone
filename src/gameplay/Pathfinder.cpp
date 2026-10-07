#include "gameplay/Pathfinder.h"

#include "world/Blocks.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

namespace {

constexpr int kHashSlots = 4096; // > 2 x kMaxNodes, power of two
constexpr int kMaxDrop = 3;      // vanilla: mobs path down at most 3 blocks

uint64_t pack(const glm::ivec3& p) {
    return (uint64_t(uint32_t(p.x) & 0x3FFFFFFu) << 38) | (uint64_t(uint32_t(p.z) & 0x3FFFFFFu) << 12) |
           (uint64_t(uint32_t(p.y) & 0xFFFu)); // y: 12 bits cover any world height
}

int heuristic(const glm::ivec3& a, const glm::ivec3& b) {
    const glm::ivec3 d = glm::abs(a - b);
    return (d.x + d.z) * 10 + d.y * 5; // never more than the real cost (admissible)
}

} // namespace

Pathfinder::Pathfinder() {
    m_nodes.reserve(kMaxNodes + 8);
    m_heap.reserve(kMaxNodes * 5 + 8); // + re-pushes of improved nodes
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
    return m_chunk ? m_chunk->get(blockToLocal(x), y, blockToLocal(z)) : blockRegistry().defaultState(blocks::Stone);
}

bool Pathfinder::passable(const World& world, const glm::ivec3& c, int height) {
    const auto& reg = blockRegistry();
    for (int i = 0; i < height; ++i) {
        const BlockStateId s = at(world, c.x, c.y + i, c.z);
        const BlockId b = reg.blockOf(s);
        if (reg.collides(s) || b == blocks::Lava || b == blocks::Fire) return false;
    }
    return true;
}

bool Pathfinder::standable(const World& world, const glm::ivec3& c, int height) {
    if (!world.isInHeight(c.y) || !passable(world, c, height)) return false;
    const auto& reg = blockRegistry();
    if (reg.blockOf(at(world, c.x, c.y, c.z)) == blocks::Water) return true; // swimming
    return reg.collides(at(world, c.x, c.y - 1, c.z));
}

int Pathfinder::danger(const World& world, const glm::ivec3& c) {
    const auto& reg = blockRegistry();
    int cost = reg.blockOf(at(world, c.x, c.y, c.z)) == blocks::Water ? 80 : 0;
    static constexpr int kSides[5][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}};
    for (const auto& d : kSides) {
        const BlockId b = reg.blockOf(at(world, c.x + d[0], c.y + d[1], c.z + d[2]));
        if (b == blocks::Lava || b == blocks::Fire) return cost + 80;
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
    std::push_heap(m_heap.begin(), m_heap.end(), [&](int a, int b) { return m_nodes[size_t(a)].f > m_nodes[size_t(b)].f; });
}

int Pathfinder::pop() {
    std::pop_heap(m_heap.begin(), m_heap.end(), [&](int a, int b) { return m_nodes[size_t(a)].f > m_nodes[size_t(b)].f; });
    const int n = m_heap.back();
    m_heap.pop_back();
    return n;
}

int Pathfinder::find(const World& world, const glm::ivec3& start, const glm::ivec3& goal, int height, int maxNodes,
                     glm::ivec3* out, int maxOut) {
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
        for (const auto& d : kDirs) {
            glm::ivec3 q{n.pos.x + d[0], n.pos.y, n.pos.z + d[1]};
            if (passable(world, q, height)) {
                // Walk on, or drop down to the first floor within kMaxDrop.
                int drop = 0;
                while (drop <= kMaxDrop && !standable(world, q, height)) {
                    --q.y;
                    ++drop;
                    if (!passable(world, q, height)) {
                        drop = kMaxDrop + 1;
                        break;
                    }
                }
                if (drop > kMaxDrop || closedAt(q)) continue;
                visit(q, n.g + 10 + drop * 2 + danger(world, q), ni);
            } else {
                // Step up 1: the cell above the wall, with headroom over the mob.
                const glm::ivec3 up{q.x, q.y + 1, q.z};
                if (!closedAt(up) && standable(world, up, height) &&
                    passable(world, {n.pos.x, n.pos.y + height, n.pos.z}, 1))
                    visit(up, n.g + 15 + danger(world, up), ni);
            }
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
