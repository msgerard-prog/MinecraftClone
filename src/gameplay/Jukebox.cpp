// Jukeboxes and our music discs (M23.6; wiki: Jukebox, Music Disc).
#include "gameplay/Jukebox.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace mc {

namespace {

// wiki: Music Disc - comparator output and duration (seconds) of each disc.
constexpr DiscInfo kDiscs[] = {
    {"music_disc_13", 1, 178 * 20},       {"music_disc_cat", 2, 185 * 20},
    {"music_disc_blocks", 3, 345 * 20},   {"music_disc_chirp", 4, 185 * 20},
    {"music_disc_far", 5, 174 * 20},      {"music_disc_mall", 6, 197 * 20},
    {"music_disc_mellohi", 7, 96 * 20},   {"music_disc_stal", 8, 150 * 20},
    {"music_disc_strad", 9, 188 * 20},    {"music_disc_ward", 10, 251 * 20},
    {"music_disc_11", 11, 71 * 20},       {"music_disc_wait", 12, 237 * 20},
    {"music_disc_pigstep", 13, 148 * 20}, {"music_disc_otherside", 14, 195 * 20},
    {"music_disc_5", 15, 178 * 20}, // (M28.5b; wiki: Music Disc - "5" gives 15, 2:58)
    // (M29.3a; wiki: Music Disc) Relic 14, 3:38; Precipice 13, 4:59; Creator 12, 2:56; Creator
    // (Music Box) 11, 1:13; Tears 10, 2:55; Lava Chicken 9, 2:15.
    {"music_disc_relic", 14, 218 * 20},   {"music_disc_precipice", 13, 299 * 20},
    {"music_disc_creator", 12, 176 * 20}, {"music_disc_creator_music_box", 11, 73 * 20},
    {"music_disc_tears", 10, 175 * 20},   {"music_disc_lava_chicken", 9, 135 * 20},
    {"music_disc_bounce", 8, 235 * 20}}; // (M33.2d; wiki: Music Disc - 3:55)

uint32_t hash(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h =
        a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}
float pitchOf(int note) { return std::pow(2.0f, float(note - 12) / 12.0f); } // note 0..24, F#3..F#5

} // namespace

int discCount() { return int(std::size(kDiscs)); }
const DiscInfo& discInfo(int index) { return kDiscs[index]; }

int discIndex(world::ItemId item) {
    static const std::array<world::ItemId, std::size(kDiscs)> ids = [] {
        std::array<world::ItemId, std::size(kDiscs)> a{};
        for (size_t i = 0; i < a.size(); ++i)
            a[i] = world::itemRegistry().find(kDiscs[i].id).value_or(world::kNoItem);
        return a;
    }();
    for (size_t i = 0; i < ids.size(); ++i)
        if (ids[i] != world::kNoItem && ids[i] == item) return int(i);
    return -1;
}

int jukeboxNotes(int disc, int tick, std::array<JukeboxNote, 3>& out) {
    using world::Sound;
    // Per disc: a step length (tempo), a lead and a bass instrument, sparse or busy.
    static constexpr Sound kLead[] = {
        Sound::NoteFlute,     Sound::NoteHarp,   Sound::NoteBell,
        Sound::NoteChime,     Sound::NoteGuitar, Sound::NotePling,
        Sound::NoteXylophone, Sound::NoteBanjo,  Sound::NoteIronXylophone};
    const uint32_t d = uint32_t(disc);
    const int step = 3 + int(hash(d, 1, 0) % 4); // 3..6 ticks per step
    if (tick % step != 0) return 0;
    const bool eerie = disc == 0 || disc == 10; // ("13" and "11": sparse, low)
    const int s = tick / step;
    const int bar = s / 16, pos = s % 16;
    const Sound lead = eerie ? Sound::NoteDidgeridoo : kLead[hash(d, 2, 0) % std::size(kLead)];
    // Four chords per phrase (roots in semitones over F#), a scale for the melody.
    static constexpr int kProgressions[4][4] = {
        {0, 5, 7, 5}, {0, 9, 5, 7}, {0, 7, 9, 5}, {0, 3, 5, 7}};
    const int* prog = kProgressions[hash(d, 3, 0) % 4];
    static constexpr int kMajor[7] = {0, 2, 4, 5, 7, 9, 11}, kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
    const int* scale = (hash(d, 4, 0) & 1) ? kMajor : kMinor;
    const int root = prog[bar % 4];
    int n = 0;
    // The melody repeats in phrases of 2 bars (A A B A ...), so the tune is recognisable.
    const int phrase = (bar / 2) % 4 == 2 ? 1 : 0;
    const uint32_t r = hash(d, uint32_t(phrase * 32 + (bar % 2) * 16 + pos), 7);
    if ((r % 100) < (eerie ? 25u : 70u)) {
        const int degree = int(r / 100 % 7);
        int note = 6 + scale[degree] + root % 12; // around the middle of the range
        while (note > 24)
            note -= 12;
        out[size_t(n++)] = {lead, pitchOf(note)};
    }
    if (pos % 4 == 0) out[size_t(n++)] = {Sound::NoteBass, pitchOf(std::min(24, 6 + root))};
    if (!eerie && (hash(d, 5, 0) & 1) && pos % 8 == 0)
        out[size_t(n++)] = {Sound::NoteBasedrum, 1.0f};
    return n;
}

} // namespace mc
