---
name: new-system
description: Create a new engine/game system or subsystem folder with the right layer, CMake target, local CLAUDE.md, tests and docs. Use when starting a new major system (e.g. lighting, fluids, saves, job system).
---
# New system

1. **Place it.** Decide its layer from docs/architecture.md › Layers. Most systems are
   a folder inside an existing subsystem (`src/world/lighting/`) — no CMake change
   needed (sources are globbed). Only a genuinely new layer gets a new `src/<name>/`.
2. **New layer only:** `src/<name>/CMakeLists.txt` with
   `mc_add_subsystem(<name> DEPS <lower layers>)`, add it to the root `CMakeLists.txt`
   in dependency order and to `MinecraftClone`'s links, add a row to the Layers table,
   and write `src/<name>/CLAUDE.md` (≤ 25 lines: owns, depends on, local rules).
3. **Design first** (3–10 lines) in `docs/architecture.md`: data layout, who calls it,
   which thread, per-tick vs per-frame cost, how vanilla does it. If there were real
   alternatives, add an ADR (`docs/decisions/`, next number) and an index row.
4. **Interface + tests** before the implementation fills out:
   `tests/<subsystem>_<system>.cpp`.
5. Implement. Respect the hard rules (no hot-path allocation, tick-only simulation,
   determinism).
6. `tools/test.sh`, visual check if visible, `perf-reviewer` if it runs every tick or
   frame. Update ROADMAP.md. Commit.
