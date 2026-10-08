# ADR 0008 — Audio: Windows XAudio2 and our own synthesized sounds

Status: **Accepted** (2026-10-07; part of the M22 plan the user approved: "Windows
XAudio2 (system API, no new dependency) playing our own synthesized sounds").

## Context
M22.4 adds sound. The audio folder's early notes planned miniaudio (a new FetchContent
dependency, which needs the user's approval). Sounds must be our own (ADR 0004): we
can't ship or read vanilla's sound files, and vanilla's sounds aren't in the client jar
anyway (they live in the launcher's asset index).

## Decision
- Playback through **XAudio2 2.9**, part of Windows 10+ (`xaudio2.lib` from the Windows
  SDK): no new dependency, nothing to download. The game only builds for Windows
  (ADR 0001); `audio/SoundEngine` is the only code that touches it, and a build
  without it (or a machine without an audio device) runs silent.
- A fixed pool of source voices (one PCM format: 16-bit mono 22,050 Hz), no allocation
  when playing (hard rule 1). Positional sound is ours: vanilla's linear falloff over
  16 blocks × volume and an equal-power stereo pan from the listener's yaw.
- Sounds are **synthesized by `tools/sounds/gen_sounds.py`** (deterministic, original)
  into `assets/minecraft/sounds/` as WAV, in vanilla's folder layout (`dig/stone1`,
  `step/grass2`, `mob/cow/say1`...), so a resource pack can replace any file with its
  own WAV of the same name.
- Sound events are named after vanilla's (`block.stone.break`, `entity.cow.ambient`):
  `world/Sounds` lists them with their files, volume and pitch (vanilla randomises
  between variants and pitches).

## Alternatives
- miniaudio (single header, cross-platform, decodes OGG): a new dependency for a
  Windows-only game; would let packs use OGG. Revisit if we ever port.
- Decoding vanilla OGGs from resource packs with stb_vorbis: possible later; the
  sounds still couldn't be in the repo.

## Consequences
Screenshot and hidden runs stay silent (`--hidden`/`--screenshot` imply `--mute`).
Our sounds are simple synthesis - recognisable, not vanilla quality; the user decides
later whether they need a pass.
