// =============================================================================
//  Shader.h  -  GLSL program wrapper with cached uniform locations
// =============================================================================
#pragma once
#include <glad/gl.h>
#include <string>
#include <unordered_map>
#include "../core/Math3D.h"

class Shader {
public:
    bool build(const char* vsSrc, const char* fsSrc, const char* debugName);
    void use() const { glUseProgram(m_prog); }
    void destroy();
    GLuint id() const { return m_prog; }

    GLint loc(const char* name);
    void set(const char* n, int v) { glUniform1i(loc(n), v); }
    void set(const char* n, float v) { glUniform1f(loc(n), v); }
    void set(const char* n, const Vec2& v) { glUniform2f(loc(n), v.x, v.y); }
    void set(const char* n, const Vec3& v) { glUniform3f(loc(n), v.x, v.y, v.z); }
    void set(const char* n, const Vec4& v) { glUniform4f(loc(n), v.x, v.y, v.z, v.w); }
    void set(const char* n, const Mat4& m) { glUniformMatrix4fv(loc(n), 1, GL_FALSE, m.m); }

private:
    GLuint m_prog = 0;
    std::unordered_map<std::string, GLint> m_cache;
};
