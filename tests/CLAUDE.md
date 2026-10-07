# tests — local rules
doctest, one executable `mc_tests` (all `tests/*.cpp` globbed). Run `tools/test.sh`;
filter with `tools/test.sh debug -tc="*name*"`.

- File per subsystem area: `<subsystem>_<topic>.cpp` (`world_coords.cpp`).
- Test names describe the vanilla rule: `"blockToChunk floors negative coordinates"`.
- Test `core`, `world`, `gameplay` directly, and GL-free parts of `rendering`
  (meshing, e.g. `rendering_cubemesher.cpp`). Never create GL contexts in tests;
  GPU output is checked with screenshots instead.
- Worldgen: pin a hash of generated chunks for fixed seeds; a changed hash means
  generator output changed → ask the user before updating it.
- Every bug fix adds a failing-first regression test where possible.
- Keep the suite fast (< 5 s); mark slow tests with `* doctest::skip()` + a tag, and
  document how to run them.
