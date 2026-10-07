#pragma once

struct GLFWwindow;

namespace mc {

// Owns the GLFW window and its OpenGL 4.6 core context.
class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(int width, int height, const char* title, bool visible);

    bool shouldClose() const;
    void pollEvents();
    void swapBuffers();
    void framebufferSize(int& width, int& height) const;

    GLFWwindow* handle() const { return m_window; }

private:
    GLFWwindow* m_window = nullptr;
    bool m_glfwInitialized = false;
};

// Seconds since GLFW init (monotonic).
double timeSeconds();

} // namespace mc
