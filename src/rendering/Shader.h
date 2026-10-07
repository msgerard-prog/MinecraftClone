#pragma once

#include <cstdint>

namespace mc::gfx {

// A linked GLSL program built from assets/shaders/<name>.vert + <name>.frag.
class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Load-time only. Logs compile/link errors with the file name.
    bool load(const char* name);
    void bind() const;
    uint32_t id() const { return m_program; }

private:
    uint32_t m_program = 0;
};

} // namespace mc::gfx
