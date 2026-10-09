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

## Update 2026-10-09: saves follow 26.3 (user decision)
- After M33 added the 26.1-26.3 content, the user chose (2026-10-09) to write 26.3's save
  format: DataVersion **5023** and 26.1's world layout (dimensions/, players/, data/minecraft/
  files split out of level.dat), 26.3's block state fields (`id`/`properties`) and
  `version_history`. Gameplay parity outside the 26.x additions stays 1.21.11.
- Alternatives: keep 4671 (vanilla 1.21.11 could still open our worlds, but would not know
  the 26.x blocks); write 26.x content under 4671 (inconsistent for any reader).
- Consequences: older worlds are moved into the new layout when opened (one way, like
  vanilla); vanilla 1.21.11 can no longer open our worlds. The insides of world_clocks.dat,
  game_rules.dat and last_id.dat aren't documented on the wiki: ours keep the earlier field
  names in vanilla's saved-data wrapper (data-formats.md).
