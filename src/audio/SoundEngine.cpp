#include "audio/SoundEngine.h"

#include "core/Log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xaudio2.h>
#endif

namespace mc::audio {

namespace {

constexpr uint32_t kRate = 22050;

struct Clip {
    std::vector<int16_t> samples;
};

// Minimal RIFF/WAVE reader: the "fmt " and "data" chunks of a 16-bit mono file.
bool parseWav(const std::vector<uint8_t>& b, Clip& out) {
    auto u32 = [&](size_t o) { return uint32_t(b[o]) | uint32_t(b[o + 1]) << 8 | uint32_t(b[o + 2]) << 16 | uint32_t(b[o + 3]) << 24; };
    auto u16 = [&](size_t o) { return uint16_t(b[o] | b[o + 1] << 8); };
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "WAVE", 4) != 0) return false;
    bool fmtOk = false;
    for (size_t o = 12; o + 8 <= b.size();) {
        const uint32_t size = u32(o + 4);
        if (o + 8 + size > b.size()) return false;
        if (std::memcmp(b.data() + o, "fmt ", 4) == 0 && size >= 16) {
            fmtOk = u16(o + 8) == 1 && u16(o + 10) == 1 && u32(o + 12) == kRate && u16(o + 22) == 16;
            if (!fmtOk) return false;
        } else if (std::memcmp(b.data() + o, "data", 4) == 0 && fmtOk) {
            out.samples.resize(size / 2);
            std::memcpy(out.samples.data(), b.data() + o + 8, out.samples.size() * 2);
            return !out.samples.empty();
        }
        o += 8 + size + (size & 1);
    }
    return false;
}

} // namespace

#ifdef _WIN32
struct SoundEngine::Impl {
    IXAudio2* xaudio = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    IXAudio2SourceVoice* voices[kVoices] = {};
    uint32_t channels = 2;
    int nextSteal = 0;
    std::vector<Clip> clips;
    glm::dvec3 eye{0.0};
    glm::dvec3 right{-1.0, 0.0, 0.0};
    bool comInit = false;

    ~Impl() {
        for (auto*& v : voices)
            if (v) {
                v->DestroyVoice();
                v = nullptr;
            }
        if (master) master->DestroyVoice();
        if (xaudio) xaudio->Release();
        if (comInit) CoUninitialize();
    }
};

SoundEngine::SoundEngine() = default;
SoundEngine::~SoundEngine() = default;

bool SoundEngine::init() {
    auto impl = std::make_unique<Impl>();
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    impl->comInit = SUCCEEDED(co); // (RPC_E_CHANGED_MODE: someone else set COM up; fine)
    if (FAILED(XAudio2Create(&impl->xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
        MC_LOG_WARN("Audio: XAudio2 unavailable - no sound");
        return false;
    }
    if (FAILED(impl->xaudio->CreateMasteringVoice(&impl->master))) {
        MC_LOG_WARN("Audio: no output device - no sound");
        return false;
    }
    XAUDIO2_VOICE_DETAILS details{};
    impl->master->GetVoiceDetails(&details);
    impl->channels = std::max<uint32_t>(1, details.InputChannels);
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = kRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = kRate * 2;
    for (auto*& v : impl->voices)
        if (FAILED(impl->xaudio->CreateSourceVoice(&v, &fmt, 0, 4.0f))) { // (vanilla pitches reach 3.4)
            MC_LOG_WARN("Audio: can't create voices - no sound");
            return false;
        }
    impl->clips.reserve(512);
    m = std::move(impl);
    MC_LOG_INFO("Audio: XAudio2, %u output channels, %d voices", m->channels, kVoices);
    return true;
}

bool SoundEngine::active() const { return m != nullptr; }

int SoundEngine::load(const std::vector<uint8_t>& wav) {
    if (!m) return -1;
    Clip c;
    if (!parseWav(wav, c)) return -1;
    m->clips.push_back(std::move(c));
    return int(m->clips.size()) - 1;
}

void SoundEngine::setListener(const glm::dvec3& eye, float yawDegrees) {
    if (!m) return;
    m->eye = eye;
    const double y = double(yawDegrees) * std::numbers::pi / 180.0;
    m->right = {-std::cos(y), 0.0, -std::sin(y)}; // (look = (-sin, 0, cos); right = look x up)
}

void SoundEngine::play(int handle, const glm::dvec3& pos, float volume, float pitch, bool positional) {
    if (!m || handle < 0 || handle >= int(m->clips.size())) return;
    float gain = std::min(volume, 1.0f), left = 1.0f, rightGain = 1.0f;
    if (positional) {
        // Vanilla: linear falloff to 0 at 16 blocks x max(volume, 1).
        const glm::dvec3 d = pos - m->eye;
        const double dist = glm::length(d);
        const double range = 16.0 * std::max(1.0f, volume);
        if (dist >= range) return;
        gain *= float(1.0 - dist / range);
        // Equal-power pan from the horizontal direction (a little on both ears always).
        const glm::dvec2 h(d.x, d.z);
        const double hl = glm::length(h);
        const double p = hl > 1e-6 ? (h.x * m->right.x + h.y * m->right.z) / hl * std::min(1.0, hl / 2.0) : 0.0;
        const double a = (p * 0.8 + 1.0) * std::numbers::pi / 4.0;
        left = float(std::cos(a) * std::numbers::sqrt2);
        rightGain = float(std::sin(a) * std::numbers::sqrt2);
    }
    if (gain <= 0.001f) return;
    // A free voice, else steal one in turn (the oldest-ish).
    IXAudio2SourceVoice* voice = nullptr;
    for (auto* v : m->voices) {
        XAUDIO2_VOICE_STATE st{};
        v->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (st.BuffersQueued == 0) {
            voice = v;
            break;
        }
    }
    if (!voice) {
        voice = m->voices[m->nextSteal];
        m->nextSteal = (m->nextSteal + 1) % kVoices;
        voice->Stop();
        voice->FlushSourceBuffers();
    }
    const Clip& c = m->clips[size_t(handle)];
    XAUDIO2_BUFFER buf{};
    buf.AudioBytes = UINT32(c.samples.size() * 2);
    buf.pAudioData = reinterpret_cast<const BYTE*>(c.samples.data());
    buf.Flags = XAUDIO2_END_OF_STREAM;
    float matrix[8] = {};
    if (m->channels == 1) {
        matrix[0] = gain;
    } else {
        matrix[0] = gain * left;
        matrix[1] = gain * rightGain;
    }
    voice->SetOutputMatrix(m->master, 1, std::min<uint32_t>(m->channels, 8), matrix);
    voice->SetFrequencyRatio(std::clamp(pitch, 0.25f, 4.0f));
    if (SUCCEEDED(voice->SubmitSourceBuffer(&buf))) voice->Start();
}

void SoundEngine::setMasterVolume(float volume) {
    if (m) m->master->SetVolume(std::clamp(volume, 0.0f, 1.0f));
}

void SoundEngine::stopAll() {
    if (!m) return;
    for (auto* v : m->voices) {
        v->Stop();
        v->FlushSourceBuffers();
    }
}
#else
struct SoundEngine::Impl {};
SoundEngine::SoundEngine() = default;
SoundEngine::~SoundEngine() = default;
bool SoundEngine::init() { return false; }
bool SoundEngine::active() const { return false; }
int SoundEngine::load(const std::vector<uint8_t>&) { return -1; }
void SoundEngine::setListener(const glm::dvec3&, float) {}
void SoundEngine::play(int, const glm::dvec3&, float, float, bool) {}
void SoundEngine::setMasterVolume(float) {}
void SoundEngine::stopAll() {}
#endif

} // namespace mc::audio
