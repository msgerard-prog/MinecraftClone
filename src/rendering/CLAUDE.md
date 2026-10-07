# rendering — local rules
The only place OpenGL is called. Owns context setup, shaders, textures/atlas, camera,
chunk meshes, render passes and screenshots.

- GL 4.6 core with DSA (`glCreate*`, `glNamed*`) — no `glBind*` + edit patterns for
  new code. No legacy/fixed-function GL.
- Shaders live in `assets/shaders/<name>.vert|.frag`, loaded with `gfx::Shader::load`.
  Use the `add-shader` skill. `#version 460 core` at the top.
- No GL object creation, buffer resize, or `glGet*` queries per frame. Create at load,
  update with persistent-mapped or sub-data uploads. Exception: GPU timer queries in
  `WorldRenderer::drawFrame`, read only when `GL_QUERY_RESULT_AVAILABLE` (never stalls).
- Meshing reads world data but never modifies it. Meshing runs on workers (M2+); GL
  uploads happen on the main thread only.
- Any visual change → `visual-check` skill (screenshot + Read it) before "done".
- The KHR_debug callback logs GL errors; a new GL error in a run is a bug.
