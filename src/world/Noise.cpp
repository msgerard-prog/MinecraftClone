#include "world/Noise.h"

#include <cmath>

namespace mc::world {

namespace {

double fade(double t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
double lerp(double t, double a, double b) { return a + t * (b - a); }

// The 12 cube-edge gradient directions (Perlin 2002), picked by the hash's low bits.
double grad(int hash, double x, double y, double z) {
    const int h = hash & 15;
    const double u = h < 8 ? x : y;
    const double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

} // namespace

ImprovedNoise::ImprovedNoise(Xoroshiro& rng) {
    m_xo = rng.nextDouble() * 256.0;
    m_yo = rng.nextDouble() * 256.0;
    m_zo = rng.nextDouble() * 256.0;
    for (int i = 0; i < 256; ++i)
        m_perm[i] = static_cast<uint8_t>(i);
    for (int i = 255; i > 0; --i) { // Fisher-Yates shuffle
        const int j = static_cast<int>(rng.nextInt(static_cast<uint32_t>(i + 1)));
        std::swap(m_perm[i], m_perm[j]);
    }
    for (int i = 0; i < 256; ++i)
        m_perm[256 + i] = m_perm[i];
}

double ImprovedNoise::noise(double x, double y, double z) const {
    x += m_xo;
    y += m_yo;
    z += m_zo;
    const double fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
    const int X = static_cast<int>(fx) & 255, Y = static_cast<int>(fy) & 255,
              Z = static_cast<int>(fz) & 255;
    x -= fx;
    y -= fy;
    z -= fz;
    const double u = fade(x), v = fade(y), w = fade(z);
    const auto& p = m_perm;
    const int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
    const int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;
    return lerp(
        w,
        lerp(v, lerp(u, grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z)),
             lerp(u, grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z))),
        lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1)),
             lerp(u, grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1))));
}

OctaveNoise::OctaveNoise(uint64_t seed, int firstOctave, int octaves)
    : m_lowestFrequency(std::pow(2.0, firstOctave)) {
    double amplitudeSum = 0.0;
    double amplitude = 1.0;
    for (int i = 0; i < octaves; ++i) {
        Xoroshiro rng(mixSeed(seed, static_cast<uint64_t>(i) + 1));
        m_octaves.emplace_back(rng);
        amplitudeSum += amplitude;
        amplitude *= 0.5;
    }
    m_norm = 1.0 / amplitudeSum;
}

double OctaveNoise::noise(double x, double y, double z) const {
    double sum = 0.0;
    double frequency = m_lowestFrequency;
    double amplitude = 1.0;
    for (const ImprovedNoise& octave : m_octaves) {
        sum += octave.noise(x * frequency, y * frequency, z * frequency) * amplitude;
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return sum * m_norm;
}

} // namespace mc::world
