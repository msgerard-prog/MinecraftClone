# 0003. Dependencies via pinned FetchContent
- Status: Accepted (2026-10-06)
- Context: Need GLFW, glm, stb, doctest; reproducible builds; minimal setup.
- Decision: CMake FetchContent with release URLs + SHA-256 (`cmake/Dependencies.cmake`),
  downloaded into `out/deps`, marked SYSTEM so `/W4 /WX` applies only to our code.
  glad is generated once and committed in `third_party/glad`.
- Alternatives: vcpkg manifest (extra bootstrap and triplet setup); git submodules
  (heavier history, easy to forget to update).
- Consequences: first configure needs internet (~1 min). Version bumps are a one-line
  change plus hash, and need the user's OK.
