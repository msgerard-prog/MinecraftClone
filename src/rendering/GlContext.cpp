#include "rendering/GlContext.h"

#include "core/Log.h"

#include <GLFW/glfw3.h>
#include <glad/gl.h>

namespace mc::gfx {

namespace {

void APIENTRY onGlDebug(GLenum, GLenum type, GLuint id, GLenum severity, GLsizei,
                        const GLchar* message, const void*) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    if (id == 131154) return; // NVIDIA: synchronous glReadPixels (expected for screenshots)
    if (type == GL_DEBUG_TYPE_ERROR || severity == GL_DEBUG_SEVERITY_HIGH) {
        MC_LOG_ERROR("GL %u: %s", id, message);
    } else {
        MC_LOG_WARN("GL %u: %s", id, message);
    }
}

} // namespace

bool initOpenGl() {
    const int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        MC_LOG_ERROR("Failed to load OpenGL");
        return false;
    }
    MC_LOG_INFO("OpenGL %d.%d - %s", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

#ifndef NDEBUG
    // Synchronous output reports errors at the offending call, but serialises the
    // driver, so release builds leave it off.
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
    glDebugMessageCallback(onGlDebug, nullptr);
    return true;
}

} // namespace mc::gfx
