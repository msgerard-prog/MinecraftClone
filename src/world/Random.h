#pragma once

#include <cstdint>

// Xoroshiro128++ (Blackman & Vigna, public domain), the generator family vanilla uses
// for world generation since 1.18. Seeded through SplitMix64 so any 64-bit seed gives
// a well-mixed state. Deterministic and cheap; not thread-safe (one per user).
namespace mc::world {

constexpr uint64_t splitMix64(uint64_t& x) {
    uint64_t z = (x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Mixes several values into one 64-bit seed (e.g. world seed + position + purpose).
constexpr uint64_t mixSeed(uint64_t a, uint64_t b) {
    uint64_t x = a ^ (b * 0x9E3779B97F4A7C15ull);
    return splitMix64(x);
}

class Xoroshiro {
public:
    explicit Xoroshiro(uint64_t seed) {
        m_s0 = splitMix64(seed);
        m_s1 = splitMix64(seed);
        if ((m_s0 | m_s1) == 0) m_s1 = 1;
    }

    uint64_t nextLong() {
        const uint64_t s0 = m_s0;
        uint64_t s1 = m_s1;
        const uint64_t result = rotl(s0 + s1, 17) + s0;
        s1 ^= s0;
        m_s0 = rotl(s0, 49) ^ s1 ^ (s1 << 21);
        m_s1 = rotl(s1, 28);
        return result;
    }

    // Uniform in [0, bound). Lemire's multiply-shift (bias negligible for our bounds).
    uint32_t nextInt(uint32_t bound) {
        return static_cast<uint32_t>(((nextLong() >> 32) * bound) >> 32);
    }
    double nextDouble() { return static_cast<double>(nextLong() >> 11) * 0x1.0p-53; }
    float nextFloat() { return static_cast<float>(nextLong() >> 40) * 0x1.0p-24f; }

private:
    static constexpr uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    uint64_t m_s0;
    uint64_t m_s1;
};

} // namespace mc::world
