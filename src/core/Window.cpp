#include "core/Window.h"

#include "core/Log.h"

#include <GLFW/glfw3.h>

namespace mc {

namespace {

void onGlfwError(int code, const char* description) {
    MC_LOG_ERROR("GLFW error %d: %s", code, description);
}

} // namespace

Window::~Window() {
    if (m_window) glfwDestroyWindow(m_window);
    if (m_glfwInitialized) glfwTerminate();
}

bool Window::create(int width, int height, const char* title, bool visible) {
    glfwSetErrorCallback(onGlfwError);
    if (!glfwInit()) return false;
    m_glfwInitialized = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) return false;
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    return true;
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }

void Window::pollEvents() { glfwPollEvents(); }

void Window::swapBuffers() { glfwSwapBuffers(m_window); }

void Window::framebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(m_window, &width, &height);
}

double timeSeconds() { return glfwGetTime(); }

} // namespace mc
