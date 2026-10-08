#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace mc::audio {

// Sound playback (M22.4, ADR 0008): XAudio2 with a fixed pool of voices playing
// 16-bit mono 22,050 Hz WAV buffers loaded at startup. Positional sounds fade linearly
// to silence at 16 blocks x volume (vanilla's attenuation) and pan by the listener's
// yaw. Without an audio device (or when muted) everything is a no-op.
class SoundEngine {
public:
    static constexpr int kVoices = 32;

    SoundEngine();
    ~SoundEngine();
    SoundEngine(const SoundEngine&) = delete;
    SoundEngine& operator=(const SoundEngine&) = delete;

    bool init();   // false: no audio (stays silent)
    bool active() const;
    // Parses a WAV (PCM 16-bit mono 22,050 Hz) and keeps it; -1 if unusable.
    int load(const std::vector<uint8_t>& wav);
    // The listener: eye position and vanilla yaw (degrees, 0 = facing +Z).
    void setListener(const glm::dvec3& eye, float yawDegrees);
    // Plays a loaded sound: volume (above 1 only extends the range), pitch 0.5..2.
    // Non-positional sounds (UI, the player's own) play centred at `volume`.
    void play(int handle, const glm::dvec3& pos, float volume, float pitch, bool positional = true);
    void setMasterVolume(float volume);
    void stopAll();

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace mc::audio
