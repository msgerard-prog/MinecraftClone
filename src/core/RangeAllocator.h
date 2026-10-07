#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace mc {

// First-fit allocator over an abstract range [0, capacity) in arbitrary units.
// Used to sub-allocate GPU buffers (e.g. section meshes in one vertex arena).
// Not thread-safe. Allocation cost is O(free ranges); fine for thousands of ranges.
class RangeAllocator {
public:
    struct Range {
        uint32_t offset = 0;
        uint32_t size = 0;
    };

    explicit RangeAllocator(uint32_t capacity = 0) { reset(capacity); }

    void reset(uint32_t capacity);
    // Adds space at the end (after the backing buffer has grown).
    void grow(uint32_t newCapacity);

    std::optional<Range> allocate(uint32_t size);
    void free(Range range); // merges with adjacent free ranges

    uint32_t capacity() const { return m_capacity; }
    uint32_t freeTotal() const;
    size_t freeRangeCount() const { return m_free.size(); }

private:
    uint32_t m_capacity = 0;
    std::vector<Range> m_free; // sorted by offset, never adjacent
};

} // namespace mc
