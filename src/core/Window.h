#pragma once

struct GLFWwindow;

namespace mc {

class Window;
void onScroll(GLFWwindow* handle, double xoffset, double yoffset);
void onMouseButton(GLFWwindow* handle, int button, int action, int mods);
void onKey(GLFWwindow* handle, int key, int scancode, int action, int mods);
void onChar(GLFWwindow* handle, unsigned int codepoint);

// Presses counted by callbacks, so a press shorter than a frame or tick isn't lost.
// Text-editing keys (Backspace, arrows) also count key repeats.
enum class Press {
    LeftMouse,
    RightMouse,
    Jump,
    Escape,
    F3,
    Chat,      // T
    Command,   // /
    Inventory, // E
    Drop,      // Q
    Enter,
    Backspace,
    Up,
    Down,
    Perspective, // F5 (M30.1)
    SwapHands,   // F (M32.4): the held item and the offhand trade places
    Count
};

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
    void setVsync(bool on); // (the window's context must be current: the main thread)
    // Presses since the last call (and resets the count).
    void addPress(Press p) { ++m_presses[static_cast<int>(p)]; } // re-queue a press
    int takePresses(Press p) {
        const int n = m_presses[static_cast<int>(p)];
        m_presses[static_cast<int>(p)] = 0;
        return n;
    }

    // Printable characters typed since the last call (ASCII; others dropped), with
    // Backspace as '\b' in typing order.
    // Returns the count written to `out` (at most `max`) and clears the buffer.
    int takeText(char* out, int max);
    // Cursor position in framebuffer pixels (for GUI screens).
    void cursorPos(double& x, double& y) const;

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
    friend void onMouseButton(GLFWwindow*, int, int, int);
    friend void onKey(GLFWwindow*, int, int, int, int);
    friend void onChar(GLFWwindow*, unsigned int);
    char m_text[64] = {};
    int m_textLength = 0;
    int m_presses[static_cast<int>(Press::Count)] = {};
};

// Seconds since GLFW init (monotonic).
double timeSeconds();

} // namespace mc
