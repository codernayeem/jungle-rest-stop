// =============================================================================
//  Text.h  -  Text + flat rectangle drawing for the HUD and for in-world
//  screens (ATM, vending display, TV ...). The font atlas is rasterised once
//  at start-up with Windows GDI (Consolas) into an OpenGL texture.
// =============================================================================
#pragma once
#include <glad/gl.h>
#include <string>
#include <vector>
#include "Shader.h"

// A flat 2D drawing surface placed somewhere in the 3D world.
// Local coordinates: x in [0,w], y in [0,h], origin at the bottom-left corner.
struct Canvas {
    Mat4 model;      // maps canvas centre to world; canvas lies in local XY plane facing +Z
    float w = 1, h = 1;
};

class TextRenderer {
public:
    bool init();
    void shutdown();

    // ---------- 2D overlay (pixels, origin top-left) ----------
    void begin2D(int screenW, int screenH);
    void rect(float x, float y, float w, float h, const Vec4& color);
    void text(float x, float y, float sizePx, const std::string& s, const Vec4& color, int align = 0);
    float width(const std::string& s, float sizePx) const;
    void end2D();

    // ---------- 3D canvases ----------
    void begin3D(const Mat4& viewProj);
    void crect(const Canvas& c, float x, float y, float w, float h, const Vec4& color);
    void ctext(const Canvas& c, float x, float y, float size, const std::string& s, const Vec4& color, int align = 0);
    void end3D();

private:
    struct V { float x, y, z, u, v, r, g, b, a; };
    void pushQuad(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3,
                  float u0, float v0, float u1, float v1, const Vec4& c);
    void flush(const Mat4& mvp);

    Shader m_shader;
    GLuint m_vao = 0, m_vbo = 0, m_tex = 0;
    std::vector<V> m_verts;
    Mat4 m_mvp;
    // glyph metrics (pixels in atlas)
    int m_atlas = 512, m_cellW = 0, m_cellH = 0, m_cols = 0, m_adv = 0, m_lineH = 0;
};
