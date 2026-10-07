# 0002. Reference version: Java Edition 1.21.x
- Status: Accepted (2026-10-06)
- Context: Vanilla behaviour differs a lot between versions (world height, block
  states, worldgen).
- Decision: Replicate Java Edition 1.21.x. Fundamentals (Y −64..319, block states,
  paletted sections, 20 TPS) are 1.21 from day one; complex systems (multi-noise
  worldgen) are phased in, with temporary deviations listed in game-design.md.
- Alternatives: Beta 1.7.3 (much smaller), 1.12.2 (pre-flattening).
- Consequences: largest target; the wiki documents it best; worldgen is the hardest
  part and gets its own milestone (M8).

## Update 2026-10-07: pinned patch 1.21.11 (user decision)
- The exact target is **1.21.11** (released 2025-12-09, data version 4671), the last
  1.21 release (wiki: Java Edition 1.21.11; 26.1 follows with a new version scheme).
- Consequences (migration tracked in ROADMAP): saves write DataVersion 4671 and the
  1.21.5+ field names (entity `fall_distance`, `equipment`, cow variants; furnace
  1.21.4+ names); 1.21.11 defaults (graphics presets, mipmap levels, Nether fog fixed
  10-96 blocks, sneak-sprint, copper tools 1.21.9, spears/mounts 1.21.11 as content
  milestones). Patch-dependent deviations in game-design.md now resolve to 1.21.11.
