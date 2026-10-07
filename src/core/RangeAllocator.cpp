#include "core/RangeAllocator.h"

#include <algorithm>

namespace mc {

void RangeAllocator::reset(uint32_t capacity) {
    m_capacity = capacity;
    m_free.clear();
    if (capacity > 0) m_free.push_back({0, capacity});
}

void RangeAllocator::grow(uint32_t newCapacity) {
    if (newCapacity <= m_capacity) return;
    free({m_capacity, newCapacity - m_capacity});
    m_capacity = newCapacity;
}

std::optional<RangeAllocator::Range> RangeAllocator::allocate(uint32_t size) {
    if (size == 0) return Range{0, 0};
    for (auto it = m_free.begin(); it != m_free.end(); ++it) {
        if (it->size < size) continue;
        const Range out{it->offset, size};
        it->offset += size;
        it->size -= size;
        if (it->size == 0) m_free.erase(it);
        return out;
    }
    return std::nullopt;
}

void RangeAllocator::free(Range range) {
    if (range.size == 0) return;
    auto it = std::lower_bound(m_free.begin(), m_free.end(), range,
                               [](const Range& a, const Range& b) { return a.offset < b.offset; });
    it = m_free.insert(it, range);
    // Merge with the next range, then with the previous one.
    if (auto next = it + 1; next != m_free.end() && it->offset + it->size == next->offset) {
        it->size += next->size;
        m_free.erase(next);
    }
    if (it != m_free.begin()) {
        auto prev = it - 1;
        if (prev->offset + prev->size == it->offset) {
            prev->size += it->size;
            m_free.erase(it);
        }
    }
}

uint32_t RangeAllocator::freeTotal() const {
    uint32_t total = 0;
    for (const Range& r : m_free)
        total += r.size;
    return total;
}

} // namespace mc
