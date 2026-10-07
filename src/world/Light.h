#pragma once

#include <cstdint>
#include <memory>

namespace mc::world {

// One section's worth (4096) of 4-bit light values, like vanilla's DataLayer:
// either a single uniform value (no array: fully lit sky, pitch-dark rock) or a
// packed nibble array of 2048 bytes. Index order matches Section::index.
class LightLayer {
public:
    uint8_t get(int i) const {
        return m_data ? static_cast<uint8_t>((m_data[i >> 1] >> ((i & 1) * 4)) & 15) : m_uniform;
    }
    void set(int i, uint8_t v);
    void fill(uint8_t v) {
        m_data.reset();
        m_uniform = v;
    }
    // Drops the array if every value is the same.
    void compact();
    bool isUniform() const { return !m_data; }
    uint8_t uniformValue() const { return m_uniform; }
    bool operator==(const LightLayer& o) const;

private:
    uint8_t m_uniform = 0;
    std::unique_ptr<uint8_t[]> m_data;
};

struct SectionLight {
    LightLayer sky;
    LightLayer block;
    bool operator==(const SectionLight& o) const { return sky == o.sky && block == o.block; }
};

} // namespace mc::world
