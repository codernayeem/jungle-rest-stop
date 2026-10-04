#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "Text.h"
#include <cstring>

namespace {
const char* TEXT_VS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
uniform mat4 uMVP;
out vec2 vUV;
out vec4 vColor;
void main() { vUV = aUV; vColor = aColor; gl_Position = uMVP * vec4(aPos, 1.0); }
)GLSL";
const char* TEXT_FS = R"GLSL(
#version 330 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uFont;
out vec4 FragColor;
void main() {
    if (vUV.x < 0.0) { FragColor = vColor; return; }
    float a = texture(uFont, vUV).r;
    FragColor = vec4(vColor.rgb, vColor.a * a);
}
)GLSL";
}  // namespace

bool TextRenderer::init() {
    // ---- rasterise ASCII 32..126 with GDI ----
    const int A = m_atlas;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi;
    std::memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = A;
    bi.bmiHeader.biHeight = -A;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    HFONT font = CreateFontA(-44, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    HGDIOBJ oldFont = SelectObject(dc, font);
    std::memset(bits, 0, A * A * 4);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    TEXTMETRICA tm;
    GetTextMetricsA(dc, &tm);
    m_adv = tm.tmAveCharWidth;
    m_lineH = tm.tmHeight;
    m_cellW = m_adv + 4;
    m_cellH = m_lineH + 4;
    m_cols = A / m_cellW;
    for (int c = 32; c < 127; ++c) {
        int i = c - 32;
        int x = (i % m_cols) * m_cellW + 2, y = (i / m_cols) * m_cellH + 2;
        char ch = (char)c;
        TextOutA(dc, x, y, &ch, 1);
    }
    GdiFlush();
    std::vector<unsigned char> px(A * A * 4);
    const unsigned char* src = (const unsigned char*)bits;
    for (int i = 0; i < A * A; ++i) {
        unsigned char v = src[i * 4 + 2];  // BGRA -> take red
        px[i * 4] = v; px[i * 4 + 1] = v; px[i * 4 + 2] = v; px[i * 4 + 3] = 255;
    }
    SelectObject(dc, oldFont);
    SelectObject(dc, oldBmp);
    DeleteObject(font);
    DeleteObject(bmp);
    DeleteDC(dc);

    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, A, A, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (!m_shader.build(TEXT_VS, TEXT_FS, "text")) return false;
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(V), (void*)(5 * sizeof(float)));
    glBindVertexArray(0);
    return true;
}

void TextRenderer::shutdown() {
    m_shader.destroy();
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_tex) glDeleteTextures(1, &m_tex);
    m_vbo = m_vao = m_tex = 0;
}

void TextRenderer::pushQuad(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3,
                            float u0, float v0, float u1, float v1, const Vec4& c) {
    // p0 = top-left, p1 = top-right, p2 = bottom-right, p3 = bottom-left
    V a{p0.x, p0.y, p0.z, u0, v0, c.x, c.y, c.z, c.w};
    V b{p1.x, p1.y, p1.z, u1, v0, c.x, c.y, c.z, c.w};
    V d{p2.x, p2.y, p2.z, u1, v1, c.x, c.y, c.z, c.w};
    V e{p3.x, p3.y, p3.z, u0, v1, c.x, c.y, c.z, c.w};
    m_verts.insert(m_verts.end(), {a, b, d, a, d, e});
}

void TextRenderer::flush(const Mat4& mvp) {
    if (m_verts.empty()) return;
    m_shader.use();
    m_shader.set("uMVP", mvp);
    m_shader.set("uFont", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_verts.size() * sizeof(V), m_verts.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_verts.size());
    glBindVertexArray(0);
    m_verts.clear();
}

float TextRenderer::width(const std::string& s, float size) const {
    return (float)s.size() * m_adv * (size / m_lineH);
}

// ---------------------------------------------------------------- 2D -------
void TextRenderer::begin2D(int w, int h) {
    m_mvp = ortho(0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
    m_verts.clear();
}

void TextRenderer::rect(float x, float y, float w, float h, const Vec4& c) {
    pushQuad({x, y, 0}, {x + w, y, 0}, {x + w, y + h, 0}, {x, y + h, 0}, -1, -1, -1, -1, c);
}

void TextRenderer::text(float x, float y, float size, const std::string& s, const Vec4& c, int align) {
    float sc = size / m_lineH;
    float adv = m_adv * sc;
    if (align == 1) x -= width(s, size) * 0.5f;
    else if (align == 2) x -= width(s, size);
    for (char ch : s) {
        if (ch >= 32 && ch < 127 && ch != ' ') {
            int i = ch - 32;
            float u0 = ((i % m_cols) * m_cellW + 2) / (float)m_atlas;
            float v0 = ((i / m_cols) * m_cellH + 2) / (float)m_atlas;
            float u1 = u0 + m_adv / (float)m_atlas, v1 = v0 + m_lineH / (float)m_atlas;
            pushQuad({x, y, 0}, {x + adv, y, 0}, {x + adv, y + size, 0}, {x, y + size, 0}, u0, v0, u1, v1, c);
        }
        x += adv;
    }
}

void TextRenderer::end2D() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    flush(m_mvp);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// ---------------------------------------------------------------- 3D -------
void TextRenderer::begin3D(const Mat4& vp) {
    m_mvp = vp;
    m_verts.clear();
}

static Vec3 canvasPoint(const Canvas& c, float x, float y) {
    return c.model.transformPoint(Vec3(x - c.w * 0.5f, y - c.h * 0.5f, 0.0f));
}

void TextRenderer::crect(const Canvas& c, float x, float y, float w, float h, const Vec4& col) {
    pushQuad(canvasPoint(c, x, y + h), canvasPoint(c, x + w, y + h), canvasPoint(c, x + w, y), canvasPoint(c, x, y),
             -1, -1, -1, -1, col);
}

void TextRenderer::ctext(const Canvas& c, float x, float y, float size, const std::string& s, const Vec4& col, int align) {
    // y = baseline-ish bottom of the text line, size = line height in canvas units
    float sc = size / m_lineH;
    float adv = m_adv * sc;
    if (align == 1) x -= s.size() * adv * 0.5f;
    else if (align == 2) x -= s.size() * adv;
    for (char ch : s) {
        if (ch >= 32 && ch < 127 && ch != ' ') {
            int i = ch - 32;
            float u0 = ((i % m_cols) * m_cellW + 2) / (float)m_atlas;
            float v0 = ((i / m_cols) * m_cellH + 2) / (float)m_atlas;
            float u1 = u0 + m_adv / (float)m_atlas, v1 = v0 + m_lineH / (float)m_atlas;
            pushQuad(canvasPoint(c, x, y + size), canvasPoint(c, x + adv, y + size), canvasPoint(c, x + adv, y),
                     canvasPoint(c, x, y), u0, v0, u1, v1, col);
        }
        x += adv;
    }
}

void TextRenderer::end3D() {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -4.0f);
    flush(m_mvp);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}
