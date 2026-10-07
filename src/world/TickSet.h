#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mc::world {

// A set of 64-bit keys (open addressing, linear probing, backward-shift deletion) for
// "is a tick already pending here?" in O(1) - vanilla keeps a hash set next to its
// tick queue for the same reason. Grows by doubling (capacity is kept: no allocation
// once a chunk has seen its largest number of pending ticks).
class TickSet {
public:
    bool contains(uint64_t key) const {
        if (m_slots.empty()) return false;
        const uint64_t k = key + 1;
        for (size_t i = slot(k);; i = (i + 1) & mask()) {
            if (m_slots[i] == k) return true;
            if (m_slots[i] == 0) return false;
        }
    }
    void insert(uint64_t key) {
        if ((m_count + 1) * 2 > m_slots.size()) grow();
        const uint64_t k = key + 1;
        size_t i = slot(k);
        for (; m_slots[i] != 0; i = (i + 1) & mask())
            if (m_slots[i] == k) return;
        m_slots[i] = k;
        ++m_count;
    }
    void erase(uint64_t key) {
        if (m_slots.empty()) return;
        const uint64_t k = key + 1;
        size_t i = slot(k);
        for (; m_slots[i] != k; i = (i + 1) & mask())
            if (m_slots[i] == 0) return;
        // Backward shift: move later entries of the probe run into the hole.
        for (size_t j = (i + 1) & mask(); m_slots[j] != 0; j = (j + 1) & mask()) {
            const size_t home = slot(m_slots[j]);
            if (((j - home) & mask()) >= ((j - i) & mask())) {
                m_slots[i] = m_slots[j];
                i = j;
            }
        }
        m_slots[i] = 0;
        --m_count;
    }
    void clear() {
        std::fill(m_slots.begin(), m_slots.end(), 0);
        m_count = 0;
    }
    size_t size() const { return m_count; }

private:
    size_t mask() const { return m_slots.size() - 1; }
    size_t slot(uint64_t k) const {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdull;
        k ^= k >> 33;
        return static_cast<size_t>(k) & mask();
    }
    void grow() {
        std::vector<uint64_t> old;
        old.swap(m_slots);
        m_slots.assign(old.empty() ? 16 : old.size() * 2, 0);
        m_count = 0;
        for (const uint64_t k : old)
            if (k != 0) insert(k - 1);
    }

    std::vector<uint64_t> m_slots; // 0 = empty; keys stored + 1
    size_t m_count = 0;
};

} // namespace mc::world
