#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace mc {

// Fixed-size record of recent frame times (no allocation per frame).
class FrameStats {
public:
    static constexpr size_t kCapacity = 4096;

    void add(double ms) {
        m_samples[m_next] = ms;
        m_next = (m_next + 1) % kCapacity;
        if (m_count < kCapacity) ++m_count;
    }

    struct Summary {
        size_t frames = 0;
        double avgMs = 0, p99Ms = 0, maxMs = 0;
    };

    // Not for per-frame use (sorts a copy).
    Summary summarize() const {
        Summary s;
        s.frames = m_count;
        if (m_count == 0) return s;
        std::array<double, kCapacity> sorted{};
        std::copy_n(m_samples.begin(), m_count, sorted.begin());
        std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(m_count));
        double total = 0;
        for (size_t i = 0; i < m_count; ++i)
            total += sorted[i];
        s.avgMs = total / static_cast<double>(m_count);
        s.p99Ms = sorted[std::min(m_count - 1, m_count * 99 / 100)];
        s.maxMs = sorted[m_count - 1];
        return s;
    }

private:
    std::array<double, kCapacity> m_samples{};
    size_t m_next = 0;
    size_t m_count = 0;
};

} // namespace mc
