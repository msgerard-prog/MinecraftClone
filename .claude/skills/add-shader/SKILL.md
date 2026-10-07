---
name: add-shader
description: Add or change a GLSL shader program and hook it into the renderer. Use when adding a render pass, material, post effect, or editing any .vert/.frag file.
---
# Add a shader

1. Create `assets/shaders/<name>.vert` and `<name>.frag`. First line
   `#version 460 core`. Comment what pass uses it and its inputs.
2. Vertex inputs use explicit `layout(location = N)`; uniforms used by several
   shaders go in a UBO with `layout(std140, binding = N)` — record binding numbers in
   `docs/architecture.md` › Rendering so they never collide.
3. Load once at startup: `gfx::Shader s; s.load("<name>")` (fails loudly with the GLSL
   log and file name). Never load or look up uniforms per frame: cache locations or use
   UBOs.
4. Shader files are read from the source tree, so a shader-only change needs a
   restart, not a rebuild.
5. Verify: `tools/screenshot.sh <name> ...` and Read the PNG. Check the run log has no
   `[error] GL` lines. Describe what you saw.
6. If it's a new pass, add it to the pass list in architecture.md. Commit
   `rendering: <what the shader does>`.
