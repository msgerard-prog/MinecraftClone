mc_add_subsystem(core DEPS glfw glm::glm stb)
# Every GLFW include must leave OpenGL headers to glad.
target_compile_definitions(mc_core PUBLIC GLFW_INCLUDE_NONE)
# Dev builds read assets straight from the source tree (no copy step).
target_compile_definitions(mc_core PUBLIC MC_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets")
# The user's own resource packs (git-ignored): folders, .zip packs or a client .jar.
target_compile_definitions(mc_core PUBLIC
    MC_RESOURCEPACKS_DIR="${PROJECT_SOURCE_DIR}/resourcepacks")
