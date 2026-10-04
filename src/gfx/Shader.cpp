#include "Shader.h"
#include <iostream>
#include <vector>

static GLuint compileStage(GLenum type, const char* src, const char* name) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len + 1);
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::cerr << "[Shader] " << name << (type == GL_VERTEX_SHADER ? " (vertex)" : " (fragment)")
                  << " compile error:\n" << log.data() << std::endl;
    }
    return s;
}

bool Shader::build(const char* vsSrc, const char* fsSrc, const char* name) {
    GLuint vs = compileStage(GL_VERTEX_SHADER, vsSrc, name);
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fsSrc, name);
    m_prog = glCreateProgram();
    glAttachShader(m_prog, vs);
    glAttachShader(m_prog, fs);
    glLinkProgram(m_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(m_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(m_prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len + 1);
        glGetProgramInfoLog(m_prog, len, nullptr, log.data());
        std::cerr << "[Shader] " << name << " link error:\n" << log.data() << std::endl;
        return false;
    }
    m_cache.clear();
    return true;
}

GLint Shader::loc(const char* name) {
    auto it = m_cache.find(name);
    if (it != m_cache.end()) return it->second;
    GLint l = glGetUniformLocation(m_prog, name);
    m_cache.emplace(name, l);
    return l;
}

void Shader::destroy() {
    if (m_prog) glDeleteProgram(m_prog);
    m_prog = 0;
    m_cache.clear();
}
