#include "core/Window.h"

#include "core/Log.h"

#include <GLFW/glfw3.h>

namespace mc {

namespace {

void onGlfwError(int code, const char* description) {
    MC_LOG_ERROR("GLFW error %d: %s", code, description);
}

constexpr int kGlfwKeys[] = {
    GLFW_KEY_W,
    GLFW_KEY_A,
    GLFW_KEY_S,
    GLFW_KEY_D,
    GLFW_KEY_SPACE,
    GLFW_KEY_LEFT_SHIFT,
    GLFW_KEY_LEFT_CONTROL,
    GLFW_KEY_ESCAPE,
    GLFW_KEY_1,
    GLFW_KEY_2,
    GLFW_KEY_3,
    GLFW_KEY_4,
    GLFW_KEY_5,
    GLFW_KEY_6,
    GLFW_KEY_7,
    GLFW_KEY_8,
    GLFW_KEY_9,
};
static_assert(sizeof(kGlfwKeys) / sizeof(kGlfwKeys[0]) == static_cast<int>(Key::Count));

} // namespace

void onScroll(GLFWwindow* handle, double, double yoffset) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(handle));
    if (self) self->m_scrollAccum += yoffset;
}

Window::~Window() {
    if (m_window) glfwDestroyWindow(m_window);
    if (m_glfwInitialized) glfwTerminate();
}

bool Window::create(int width, int height, const char* title, bool visible, bool vsync) {
    glfwSetErrorCallback(onGlfwError);
    if (!glfwInit()) return false;
    m_glfwInitialized = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
    // Debug builds only: debug contexts disable some driver optimisations.
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) return false;
    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetScrollCallback(m_window, onScroll);
    glfwSwapInterval(vsync ? 1 : 0);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    return true;
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }

void Window::pollEvents() {
    glfwPollEvents();
    m_scrollDelta = m_scrollAccum;
    m_scrollAccum = 0.0;
    m_mouseDx = 0.0;
    m_mouseDy = 0.0;
    if (!m_captured) return;
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(m_window, &x, &y);
    if (!m_skipNextDelta) {
        m_mouseDx = x - m_lastX;
        m_mouseDy = y - m_lastY;
    }
    m_skipNextDelta = false;
    m_lastX = x;
    m_lastY = y;
}

void Window::swapBuffers() { glfwSwapBuffers(m_window); }

void Window::waitEvents(double timeoutSeconds) { glfwWaitEventsTimeout(timeoutSeconds); }

void Window::framebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(m_window, &width, &height);
}

bool Window::keyDown(Key key) const {
    return glfwGetKey(m_window, kGlfwKeys[static_cast<int>(key)]) == GLFW_PRESS;
}

bool Window::leftMousePressed() const {
    return glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
}

bool Window::rightMousePressed() const {
    return glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
}

void Window::setTitle(const char* title) { glfwSetWindowTitle(m_window, title); }

void Window::setCursorCaptured(bool captured) {
    if (captured == m_captured) return;
    m_captured = captured;
    glfwSetInputMode(m_window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    // The cursor jumps when the mode changes; don't turn that jump into a camera spin.
    m_skipNextDelta = true;
}

double timeSeconds() { return glfwGetTime(); }

} // namespace mc
