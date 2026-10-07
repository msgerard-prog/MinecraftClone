#include "rendering/Shader.h"

#include "core/Files.h"
#include "core/Log.h"

#include <glad/gl.h>

#include <string>

namespace mc::gfx {

namespace {

GLuint compileStage(GLenum stage, const std::string& path) {
    const auto source = readTextFile(path);
    if (!source) {
        MC_LOG_ERROR("Shader file not found: %s", path.c_str());
        return 0;
    }
    const GLuint shader = glCreateShader(stage);
    const char* text = source->c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        MC_LOG_ERROR("%s:\n%s", path.c_str(), log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

Shader::~Shader() {
    if (m_program) glDeleteProgram(m_program);
}

bool Shader::load(const char* name) {
    const std::string base = assetPath("shaders/") + name;
    const GLuint vert = compileStage(GL_VERTEX_SHADER, base + ".vert");
    const GLuint frag = compileStage(GL_FRAGMENT_SHADER, base + ".frag");
    if (!vert || !frag) {
        glDeleteShader(vert);
        glDeleteShader(frag);
        return false;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    glDeleteShader(vert);
    glDeleteShader(frag);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        MC_LOG_ERROR("Link failed for shader '%s':\n%s", name, log);
        glDeleteProgram(program);
        return false;
    }
    if (m_program) glDeleteProgram(m_program);
    m_program = program;
    return true;
}

void Shader::bind() const { glUseProgram(m_program); }

} // namespace mc::gfx
