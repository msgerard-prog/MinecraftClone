#include "world/Section.h"

#include "world/Blocks.h"

#include <algorithm>
#include <cassert>

namespace mc::world {

namespace {

constexpr int entriesPerLong(int bits) { return 64 / bits; }

size_t longsFor(int bits) {
    const int per = entriesPerLong(bits);
    return static_cast<size_t>((Section::kVolume + per - 1) / per);
}

} // namespace

uint32_t Section::readRaw(int i) const {
    const int per = entriesPerLong(m_bits);
    const uint64_t word = m_data[static_cast<size_t>(i / per)];
    const int shift = (i % per) * m_bits;
    return static_cast<uint32_t>((word >> shift) & ((uint64_t{1} << m_bits) - 1));
}

void Section::writeRaw(int i, uint32_t value) {
    const int per = entriesPerLong(m_bits);
    uint64_t& word = m_data[static_cast<size_t>(i / per)];
    const int shift = (i % per) * m_bits;
    const uint64_t mask = ((uint64_t{1} << m_bits) - 1) << shift;
    word = (word & ~mask) | (static_cast<uint64_t>(value) << shift);
}

BlockStateId Section::getIndex(int i) const {
    if (m_bits == 0) return m_palette[0];
    const uint32_t raw = readRaw(i);
    return m_direct ? static_cast<BlockStateId>(raw) : m_palette[raw];
}

void Section::copyTo(BlockStateId* out) const {
    if (m_bits == 0) {
        std::fill(out, out + kVolume, m_palette[0]);
        return;
    }
    const int per = entriesPerLong(m_bits);
    const uint64_t mask = (uint64_t{1} << m_bits) - 1;
    int i = 0;
    for (const uint64_t word : m_data) {
        uint64_t w = word;
        for (int k = 0; k < per && i < kVolume; ++k, ++i, w >>= m_bits) {
            const auto raw = static_cast<uint32_t>(w & mask);
            out[i] = m_direct ? static_cast<BlockStateId>(raw) : m_palette[raw];
        }
    }
}

void Section::resize(int newBits, bool direct) {
    // Decode with the old encoding, re-encode with the new one.
    std::vector<BlockStateId> states(kVolume);
    copyTo(states.data());
    m_bits = static_cast<uint8_t>(newBits);
    m_direct = direct;
    m_data.assign(longsFor(newBits), 0);
    if (direct) m_palette.clear();
    for (int i = 0; i < kVolume; ++i) {
        uint32_t value = states[static_cast<size_t>(i)];
        if (!direct) {
            const auto it = std::find(m_palette.begin(), m_palette.end(), states[i]);
            value = static_cast<uint32_t>(it - m_palette.begin());
        }
        writeRaw(i, value);
    }
}

uint32_t Section::paletteIndexFor(BlockStateId state) {
    if (m_direct) return state;
    const auto it = std::find(m_palette.begin(), m_palette.end(), state);
    if (it != m_palette.end()) return static_cast<uint32_t>(it - m_palette.begin());

    m_palette.push_back(state);
    const size_t capacity = m_bits == 0 ? 1 : size_t{1} << m_bits;
    if (m_palette.size() > capacity) {
        // Grow: 0 -> 4 bits, then +1 bit, past 8 bits switch to direct global ids.
        const int wanted = m_bits == 0 ? kMinBits : m_bits + 1;
        if (wanted > kMaxLocalBits) {
            m_palette.pop_back(); // resize() re-reads states through the old palette
            resize(kDirectBits, true);
            return state;
        }
        m_palette.pop_back();
        resize(wanted, false);
        m_palette.push_back(state);
    }
    return static_cast<uint32_t>(m_palette.size() - 1);
}

void Section::set(int x, int y, int z, BlockStateId state) {
    const int i = index(x, y, z);
    const BlockStateId old = getIndex(i);
    if (old == state) return;
    const uint32_t value = paletteIndexFor(state);
    writeRaw(i, value);
    if (old == 0) ++m_nonAir;
    if (state == 0) --m_nonAir;
    const auto& reg = blockRegistry();
    m_randomTicking = static_cast<uint16_t>(m_randomTicking - reg.randomTicks(old) + reg.randomTicks(state));
}

void Section::fill(BlockStateId state) {
    m_bits = 0;
    m_direct = false;
    m_palette.assign(1, state);
    m_data.clear();
    m_nonAir = state == 0 ? 0 : kVolume;
    m_randomTicking = blockRegistry().randomTicks(state) ? kVolume : 0;
}

void Section::assign(const BlockStateId* states) {
    // State -> palette index via a per-thread lookup table (reset after use).
    static thread_local std::vector<int16_t> index(65536, -1);
    const auto& reg = blockRegistry();
    m_palette.clear();
    m_nonAir = 0;
    m_randomTicking = 0;
    for (int i = 0; i < kVolume; ++i) {
        const BlockStateId s = states[i];
        if (index[s] < 0) {
            index[s] = static_cast<int16_t>(m_palette.size());
            m_palette.push_back(s);
        }
        if (s != 0) ++m_nonAir;
        m_randomTicking = static_cast<uint16_t>(m_randomTicking + reg.randomTicks(s));
    }
    if (m_palette.size() == 1) {
        index[m_palette[0]] = -1;
        m_bits = 0;
        m_direct = false;
        m_data.clear();
        return;
    }
    int bits = kMinBits;
    while ((size_t{1} << bits) < m_palette.size())
        ++bits;
    m_direct = bits > kMaxLocalBits;
    m_bits = static_cast<uint8_t>(m_direct ? kDirectBits : bits);
    m_data.assign(longsFor(m_bits), 0);
    for (int i = 0; i < kVolume; ++i) {
        writeRaw(i, m_direct ? states[i] : static_cast<uint32_t>(index[states[i]]));
    }
    for (BlockStateId s : m_palette)
        index[s] = -1;
    if (m_direct) m_palette.clear();
}

size_t Section::memoryBytes() const {
    return sizeof(*this) + m_palette.capacity() * sizeof(BlockStateId) +
           m_data.capacity() * sizeof(uint64_t);
}

} // namespace mc::world
