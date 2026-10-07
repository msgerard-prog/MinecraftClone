#pragma once

#include "world/Random.h"

#include <array>
#include <cstdint>
#include <vector>

namespace mc::world {

// Ken Perlin's "improved noise" (2002): gradient noise on a lattice with a shuffled
// permutation table and a quintic fade. Vanilla's ImprovedNoise is this algorithm
// with a random origin offset; so is ours. Output roughly in [-1, 1].
class ImprovedNoise {
public:
    explicit ImprovedNoise(Xoroshiro& rng);
    double noise(double x, double y, double z) const;

private:
    std::array<uint8_t, 512> m_perm{};
    double m_xo, m_yo, m_zo; // random origin, so octaves don't line up at 0
};

// Sum of octaves: each octave doubles the frequency and halves the amplitude.
// `firstOctave` < 0 means the first octave is lower frequency than 1 (e.g. -7 -> 1/128).
class OctaveNoise {
public:
    OctaveNoise(uint64_t seed, int firstOctave, int octaves);
    // Normalised to roughly [-1, 1].
    double noise(double x, double y, double z) const;
    double noise2d(double x, double z) const { return noise(x, 0.0, z); }

private:
    std::vector<ImprovedNoise> m_octaves;
    double m_lowestFrequency;
    double m_norm;
};

} // namespace mc::world
