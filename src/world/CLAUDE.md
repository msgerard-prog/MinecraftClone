# world — local rules
Block registry & states, chunks/sections, worldgen, lighting, saves. **No OpenGL, no
GLFW** — everything here must be unit-testable from `tests/`.

- Coordinates: always via `Coords.h` helpers (`blockToChunk`, `blockToLocal`,
  `HeightRange::sectionIndex` of the chunk's/world's height - heights differ per
  dimension). Never `/ 16` or `% 16` on block coordinates (wrong for negatives).
- Worldgen must be deterministic per seed: no `rand()`, no global RNG, no dependence
  on thread scheduling or unordered-container iteration order. Use a seeded RNG
  derived from (world seed, chunk pos, feature id) the way vanilla does.
- Changing generated output for an existing seed breaks pinned hashes → ask the user.
- Sections store palette + packed indices (see docs/architecture.md › World model).
- Block/neighbour access in loops goes through section pointers, not per-block
  hash lookups.
- Cite the wiki page for every mechanic constant (light falloff, hardness, ...).
- "Air" is currently `state == 0` (`isAir`, `Section` non-air count). When `cave_air`
  or `void_air` are registered they must count as air everywhere, as in vanilla.
