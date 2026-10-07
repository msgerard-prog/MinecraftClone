#include "world/Light.h"

#include <algorithm>
#include <cstring>

namespace mc::world {

void LightLayer::set(int i, uint8_t v) {
    if (!m_data) {
        if (v == m_uniform) return;
        m_data = std::make_unique<uint8_t[]>(2048);
        std::memset(m_data.get(), m_uniform | (m_uniform << 4), 2048);
    }
    uint8_t& b = m_data[i >> 1];
    const int shift = (i & 1) * 4;
    b = static_cast<uint8_t>((b & ~(15 << shift)) | ((v & 15) << shift));
}

void LightLayer::compact() {
    if (!m_data) return;
    const uint8_t first = m_data[0] & 15;
    const uint8_t pair = static_cast<uint8_t>(first | (first << 4));
    for (int i = 0; i < 2048; ++i)
        if (m_data[i] != pair) return;
    fill(first);
}

bool LightLayer::operator==(const LightLayer& o) const {
    if (!m_data && !o.m_data) return m_uniform == o.m_uniform;
    if (m_data && o.m_data) return std::memcmp(m_data.get(), o.m_data.get(), 2048) == 0;
    // One uniform, one array: equal only if the array holds one value throughout
    // (arrays are normally compacted, so this is rare).
    const LightLayer& arr = m_data ? *this : o;
    const uint8_t v = m_data ? o.m_uniform : m_uniform;
    const auto pair = static_cast<uint8_t>(v | (v << 4));
    for (int i = 0; i < 2048; ++i)
        if (arr.m_data[i] != pair) return false;
    return true;
}

} // namespace mc::world
