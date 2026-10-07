# 0001. C++20 + OpenGL 4.6, MSVC driven from WSL
- Status: Accepted (2026-10-06)
- Context: Learning replica of Minecraft; wants to see how things work under the hood.
  Dev machine is Windows 11 + RTX 5080; Claude Code runs in WSL.
- Decision: C++20, custom engine, OpenGL 4.6 core (glad 2), GLFW, glm, stb, doctest.
  CMake presets + Ninja + MSVC from Visual Studio 2026. WSL scripts in `tools/` call
  `tools/win/*.cmd`, which load `vcvars64` and run CMake on the Windows side.
- Alternatives: Java + LWJGL (same language as vanilla, but user chose C++); Vulkan
  (far more code before the first cube); Linux build in WSL (weaker GPU path via WSLg).
- Consequences: native GPU performance and screenshots that match what the user sees.
  clangd must use the Windows `compile_commands.json`; Linux tooling can't build it.
