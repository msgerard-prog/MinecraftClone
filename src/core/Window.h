#pragma once

struct GLFWwindow;

namespace mc {

class Window;
void onScroll(GLFWwindow* handle, double xoffset, double yoffset);

// Keys the game reads. Mapped to GLFW in Window.cpp so no other code includes GLFW.
enum class Key {
    W,
    A,
    S,
    D,
    Space,
    LeftShift,
    LeftControl,
    Escape,
    Num1,
    Num2,
    Num3,
    Num4,
    Num5,
    Num6,
    Num7,
    Num8,
    Num9,
    Count
};

// Owns the GLFW window, its OpenGL 4.6 core context, and raw input state.
class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(int width, int height, const char* title, bool visible, bool vsync = true);

    bool shouldClose() const;
    // Polls events and updates the mouse delta for this frame.
    void pollEvents();
    void swapBuffers();
    // Sleeps until an event arrives or `timeoutSeconds` passes (e.g. while minimised).
    void waitEvents(double timeoutSeconds);
    void framebufferSize(int& width, int& height) const;

    bool keyDown(Key key) const;
    bool leftMousePressed() const;
    bool rightMousePressed() const;
    // Mouse wheel steps since the previous pollEvents (+ = up / away from you).
    double scrollDelta() const { return m_scrollDelta; }
    void setTitle(const char* title);

    // Captured = cursor hidden and locked, mouse movement turns the camera.
    void setCursorCaptured(bool captured);
    bool cursorCaptured() const { return m_captured; }
    // Mouse movement (pixels) since the previous pollEvents; zero when not captured.
    double mouseDx() const { return m_mouseDx; }
    double mouseDy() const { return m_mouseDy; }

    GLFWwindow* handle() const { return m_window; }

private:
    GLFWwindow* m_window = nullptr;
    bool m_glfwInitialized = false;
    bool m_captured = false;
    bool m_skipNextDelta = false;
    double m_lastX = 0.0;
    double m_lastY = 0.0;
    double m_mouseDx = 0.0;
    double m_mouseDy = 0.0;
    double m_scrollDelta = 0.0;
    double m_scrollAccum = 0.0; // filled by the GLFW callback
    friend void onScroll(GLFWwindow*, double, double);
};

// Seconds since GLFW init (monotonic).
double timeSeconds();

} // namespace mc
