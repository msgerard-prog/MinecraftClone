# MinecraftClone — working notes for Claude

A from-scratch replica of **vanilla Minecraft Java Edition 1.21.x** in C++20 / OpenGL 4.6.
Non-commercial and for learning: the goal is that the user understands *how Minecraft
works*, so code should explain the vanilla mechanic it implements.

@ROADMAP.md
@docs/architecture.md

Read on demand (not every session): `docs/game-design.md` (what "done" means for a
feature, parity rules), `docs/data-formats.md` (registries, JSON, save format),
`docs/decisions/` (why things are the way they are). Each subsystem folder has its own
`CLAUDE.md` with local rules — read it before editing that folder.

## Commands (run from WSL; they drive MSVC on Windows)

| Task | Command |
|---|---|
| Build (debug / release) | `tools/build.sh` / `tools/build.sh release` |
| All tests | `tools/test.sh` |
| Some tests | `tools/test.sh debug -tc="*chunk*"` (doctest filters) |
| Run the game | `tools/run.sh [debug] [game args]` (release by default; debug is ~10x slower) |
| Screenshot for a visual check | `tools/screenshot.sh <name> [game args]` → `out/screenshots/<name>.png`, then Read it (hidden runs are uncapped: add `--max-fps 30` when game ticks must pass) |
| Format | `tools/format.sh [files]` (the hook does edited files automatically) |

Game args: `--screenshot <png>`, `--frames N` (frames before capture, default 60),
`--hidden`, `--menu title|worlds|create|options|pause|statistics|advancements|commandblock` (screenshot a menu), `--mute` (hidden/screenshot runs are silent; `--sound` forces audio on), `--no-vsync`, `--max-fps N`, `--auto-fly`, `--demo-edit`, `--resourcepacks DIR`, `--render-distance N`,
`--flat`, `--survival`, `--difficulty peaceful|easy|normal|hard` (new worlds), `--size WxH`, `--seed N`, `--time T` (day ticks: 0 sunrise, 6000 noon, 18000 midnight), `--f3`, `--command CMD`
(repeatable chat line, e.g. "/time set night"), `--world NAME` (saves/NAME; interactive
runs default to "New World"), `--no-save`, `--generator overworld8|overworld7|overworld6|overworld5|overworld4|overworld3|overworld2|overworld|terrain` (new worlds; overworld8 default), `--dimension overworld|nether|end` (start there), `--version` (print the build, exit), `--perspective 0|1|2` (F5's view), `--recipe-book` (open it on the inventory screens), `--pos x,y,z`, `--look yaw,pitch` (vanilla
degrees: yaw 0 = south/+Z, 90 = west; pitch +90 = straight down).
Each run logs "World meshed in …" and frame-time stats at exit; measure performance
with `tools/run.sh release --hidden --no-vsync --screenshot out/p.png --frames 3000`
(streaming stress: add `--auto-fly --max-fps 240 --frames 2400`; logs CPU work and GPU time).
Add new args in `core/CommandLine.*` + its test and list them here.
Controls: title screen first (interactive runs without --world); click to capture mouse, Esc = Game Menu (pauses); WASD walk, space jump (double-tap: fly),
shift sneak / fly down, ctrl sprint, left click break (or hit the mob in front), right click place, 1-9 / wheel
select block, F3 debug screen, F5 view (first/third person), T chat, / command (/tp /time /give /gamemode /kill /setblock /fill /summon (with {Color:14b,Age:-24000,...}; lightning_bolt) /weather /data merge block (sign text) /effect give|clear /enchant /xp /gamerule /difficulty /item replace /seed /help),
E inventory (creative: item list; survival: 2x2 crafting), Q drop item (Ctrl+Q: stack), F swap hands; in screens drag to split, double-click to gather, 1-9/F/Q over a slot, right-click animals
with their food (breeding) or shears (sheep), right-click crafting
table/furnace to use them (`--inventory` opens the inventory screen, `--open-block x,y,z` a chest's or a sign's editor, `--trade` the nearest employed villager's trades, `--book TEXT` the held book with TEXT typed, `--use N` N scripted right-clicks, `--mount` rides the nearest mount (+`--inventory`: its screen), for screenshots). `--demo-edit` scripts a few clicks. `--pos` and `--auto-fly` start flying.
New keys go in `Key` / `kGlfwKeys` (`core/Window.*`); list them here and in README.

The first build downloads dependencies into `out/deps` (≈1 min). Build output is
`out/build/<preset>/`; `compile_commands.json` is there for clangd.

## Hooks (`.claude/settings.json`)
- **PostToolUse** formats every edited `.cpp/.h` with `.clang-format`. Don't hand-format.
- **Stop** runs an incremental debug build; if it fails you will be sent the errors and
  must fix them. It allows stopping after 3 failed attempts — then tell the user plainly.

## Session routine
1. **Start:** read ROADMAP.md › Status and Next; check `git log --oneline -5` matches.
2. **Work** one roadmap step at a time. For each step: code → tests → `tools/test.sh` →
   visual check if anything renders differently → commit.
3. **End:** run the `session-end` skill (ROADMAP update, docs, commit; push at milestone end).

## Versions
`project(... VERSION x.y.z)` in CMakeLists.txt: before 1.0 the minor number is the last
finished milestone (0.12 = M12); after 1.0 it counts milestones since (1.1 = M30, 1.2 = M31).
At each milestone end bump it, commit, then tag
`vX.Y.Z` (annotated) and push the tag. Each build embeds `git describe` (BuildInfo.h):
the log, F3, `--version` and the exe's Windows version resource show it.

## Workflow agreed with the user
- Work through a milestone autonomously; **commit after each verified step**; push to
  `origin main` at the end of each milestone; then report.
- Every visual change is verified with a screenshot that you actually look at before
  calling it done. Say what you saw.
- Basic placeholder graphics first; polish later.
- Explain vanilla mechanics in comments and in your reports — the user is learning.

## Coding conventions
- C++20, namespace `mc` (+ `mc::world`, `mc::gfx`, ...). One class per `.h/.cpp` pair.
- Names: `PascalCase` types, `camelCase` functions/variables, `m_` members,
  `kName` constants, `MC_` macros. Files named after their main type.
- Includes: `"subsystem/File.h"` from `src/`; own header first, then project, then
  third-party, then std.
- `#pragma once`. No `using namespace` in headers.
- Errors: return `bool`/`std::optional` and log; no exceptions across subsystem APIs.
- Sources are globbed per subsystem — adding a `.cpp` needs no CMake edit.
- `/W4 /WX`: warnings are errors in our code. Fix them, don't silence them.
- Comments say *why* and name the vanilla behaviour (e.g. "vanilla: leaves decay
  when no log within 6 blocks").

## Hard rules
1. **No heap allocation in per-frame or per-tick hot paths** (`new`, growing a
   `std::vector`, `std::string` building, `std::function`, `shared_ptr` copies).
   Preallocate, reuse buffers, use pools. Load time and chunk-build workers may allocate.
2. **Simulation runs only in the fixed 20 TPS tick** (`core/GameClock.h`). Rendering
   interpolates with `alpha`. Game logic never reads frame time.
3. **World generation is deterministic** for a seed: same seed → same blocks, on any
   thread order. Tests pin hashes of generated chunks.
4. **Coordinates** follow vanilla (`world/Coords.h`): Y up, chunk 16×16, section 16³,
   heights per dimension as vanilla (`HeightRange`: Overworld −64..319, Nether/End 0..255;
   never assume one), block coords `int32`, floor division via `>> 4` / `& 15`.
5. **No Mojang code or assets in the repo.** Implement from the Minecraft Wiki and
   observed behaviour, never paste decompiled source. Textures in `assets/` are our own.
   Vanilla textures may only be loaded at runtime from the git-ignored `resourcepacks/`
   (never read them yourself; test with `tests/data/testpack` from tools/make_test_pack.py).
   Our textures are original art in vanilla's style (docs/art-style.md, ADR 0006).
6. Subsystem dependencies only point "down" the layer list in docs/architecture.md.
7. OpenGL calls only in `rendering/`. `main.cpp` drives `gfx::WorldRenderer`.
8. Every bug fix gets a regression test where testable.

## Never change without asking the user
- Save/region file format and anything that would break existing worlds.
- Worldgen output for an existing seed (pinned hashes), or the coordinate conventions.
- Dependency versions/hashes (`cmake/Dependencies.cmake`), `third_party/`.
- The hard rules above, the hooks, or the agreed workflow.
- Deliberate deviations from vanilla behaviour (propose them; record an ADR).

## Keeping docs honest
A change isn't done until the doc that describes it is updated: ROADMAP.md (status),
`docs/architecture.md` (structure), `docs/data-formats.md` (formats/registries),
subsystem `CLAUDE.md` (local rules), an ADR in `docs/decisions/` for any decision with
alternatives. Keep this file under 150 lines — move detail into those docs.

## Project skills and agents
Skills (`.claude/skills/`): `add-block`, `add-item`, `add-recipe`, `add-entity`,
`add-asset`, `add-shader`, `new-system`, `visual-check`, `session-end`.
Agents (`.claude/agents/`): `code-reviewer` (correctness + rules), `perf-reviewer`
(hot paths, GPU), `design-keeper` (vanilla parity vs game-design.md). Run
`code-reviewer` before each milestone push; `perf-reviewer` on renderer/world/tick
changes; `design-keeper` when a feature is finished.
