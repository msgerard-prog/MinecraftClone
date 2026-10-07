# core — local rules
Lowest layer: platform (GLFW window/input), `GameClock` (fixed 20 TPS), logging,
command line, files, later math/allocators/job system. Depends on nothing of ours.

- No game concepts here (no blocks, chunks, players). If it mentions Minecraft, it
  belongs in `world` or `gameplay`.
- No OpenGL calls; the window only creates the context. GLFW is included with
  `GLFW_INCLUDE_NONE` (set by CMake) — glad provides GL.
- New launch option → `CommandLine.h/.cpp` + a case in `tests/core_commandline.cpp`
  + the list in root CLAUDE.md › Commands.
- `MC_LOG_*` is for startup/errors. Don't log per frame; add a counter to the F3
  screen instead (M6).
- Allocators/pools for hot paths live here once needed (hard rule 1).
