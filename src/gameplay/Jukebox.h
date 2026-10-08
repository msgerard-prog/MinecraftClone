#pragma once

#include "world/Items.h"
#include "world/Sounds.h"

#include <array>

namespace mc {

// Jukeboxes and music discs (M23.6; wiki: Jukebox, Music Disc). A disc plays for its
// vanilla length and gives comparators its number (13: 1 ... otherside: 14). Our discs
// carry no recordings: each is a tune made from note-block sounds, composed from the
// disc's index (fixed phrases, a chord progression, a tempo and instruments per disc),
// played note by note in the game tick.
struct DiscInfo {
    const char* id;  // "music_disc_cat"
    int comparator;  // 1..15
    int lengthTicks; // vanilla's song length
};
// The disc an item is (-1: not a disc).
int discIndex(world::ItemId item);
const DiscInfo& discInfo(int index);
int discCount();

struct JukeboxNote {
    world::Sound sound = world::Sound::NoteHarp;
    float pitch = 1.0f;
};
// The notes a disc plays on this tick of its song (0-3).
int jukeboxNotes(int disc, int tick, std::array<JukeboxNote, 3>& out);

} // namespace mc
