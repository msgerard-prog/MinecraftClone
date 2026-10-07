#pragma once

#include "world/BlockRegistry.h"

#include <cstdint>
#include <vector>

namespace mc::world {

// A 16x16x16 cube of block states, stored like vanilla's PalettedContainer:
//   bits 0     single value (whole section is one state; no index array)
//   bits 4..8  local palette (list of states) + packed indices into it
//   direct     packed global state ids (palette would need more than 8 bits)
// Indices are packed into uint64s without spanning two longs (vanilla since 1.16):
// 64 / bits entries per long. Index order is vanilla's: (y * 16 + z) * 16 + x.
class Section {
public:
    static constexpr int kVolume = 16 * 16 * 16;
    static constexpr int kMinBits = 4; // vanilla's minimum for block palettes
    static constexpr int kMaxLocalBits = 8;
    // Vanilla uses ceil(log2(state count)) (15 bits in 1.21); 16 covers every uint16 id.
    static constexpr int kDirectBits = 16;

    static constexpr int index(int x, int y, int z) { return (y * 16 + z) * 16 + x; }

    BlockStateId get(int x, int y, int z) const { return getIndex(index(x, y, z)); }
    BlockStateId getIndex(int i) const;
    void set(int x, int y, int z, BlockStateId state);
    void fill(BlockStateId state);

    // Decodes all 4096 states into `out` (vanilla index order). Fast path for meshing.
    void copyTo(BlockStateId* out) const;

    bool isEmpty() const { return m_nonAir == 0; } // all air: skip in meshing
    int nonAirCount() const { return m_nonAir; }
    int bitsPerEntry() const { return m_bits; }
    bool isDirect() const { return m_direct; }
    size_t paletteSize() const { return m_palette.size(); }
    size_t memoryBytes() const;

private:
    uint32_t readRaw(int i) const;
    void writeRaw(int i, uint32_t value);
    // Returns the palette index for `state`, growing the palette/bits if needed.
    uint32_t paletteIndexFor(BlockStateId state);
    void resize(int newBits, bool direct);

    uint8_t m_bits = 0;
    bool m_direct = false;
    uint16_t m_nonAir = 0;
    std::vector<BlockStateId> m_palette{0}; // starts as all air
    std::vector<uint64_t> m_data;
};

} // namespace mc::world
