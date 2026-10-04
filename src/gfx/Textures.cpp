#include "Textures.h"
#include "../core/Math3D.h"
#include <cmath>

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

namespace {

const int N = TextureArray::SIZE;

// Tileable value noise: lattice coordinates wrap with the given period
float tnoise(float x, float y, int period, uint32_t seed) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    auto h = [&](int a, int b) {
        a = ((a % period) + period) % period;
        b = ((b % period) + period) % period;
        return hash2(a, b, seed);
    };
    return lerpf(lerpf(h(xi, yi), h(xi + 1, yi), ux), lerpf(h(xi, yi + 1), h(xi + 1, yi + 1), ux), uy);
}
// Tileable fractal noise; u,v in [0,1)
float tfbm(float u, float v, int period, int oct, uint32_t seed) {
    float s = 0, a = 0.5f, norm = 0;
    int p = period;
    for (int i = 0; i < oct; ++i) {
        s += a * tnoise(u * p, v * p, p, seed + i * 31u);
        norm += a;
        a *= 0.5f;
        p *= 2;
    }
    return s / norm;
}

struct Px { float r, g, b, a; };

using Gen = Px (*)(float u, float v);

uint8_t toByte(float f) { return (uint8_t)clampf(f * 255.0f + 0.5f, 0.0f, 255.0f); }

// helper: "detail" value d (1.0 = unchanged) -> stored value d*0.5
Px detail(float r, float g, float b, float a = 1.0f) { return {r * 0.5f, g * 0.5f, b * 0.5f, a}; }
Px gray(float d, float a = 1.0f) { return detail(d, d, d, a); }

// ---------------------------------------------------------------------------
Px genNone(float, float) { return gray(1.0f); }

Px genGrass(float u, float v) {
    float n = tfbm(u, v, 8, 5, 11);
    float blades = tnoise(u * 128, v * 32, 128, 7);
    float patch = tfbm(u, v, 3, 3, 99);
    float d = 0.70f + 0.45f * n + 0.18f * blades;
    // yellowish dry patches
    float dry = smoothstepf(0.55f, 0.75f, patch);
    return detail(d * (1.0f + 0.35f * dry), d * (1.0f + 0.05f * dry), d * (1.0f - 0.25f * dry));
}

Px genForestFloor(float u, float v) {
    float n = tfbm(u, v, 6, 5, 21);
    float leaves = tnoise(u * 48, v * 48, 48, 5);
    float d = 0.62f + 0.55f * n;
    if (leaves > 0.72f) d *= 1.25f;     // fallen leaves
    if (leaves < 0.18f) d *= 0.6f;      // dark soil
    float moss = smoothstepf(0.5f, 0.7f, tfbm(u, v, 4, 3, 77));
    return detail(d * (1.0f - 0.2f * moss), d * (1.0f + 0.25f * moss), d * (1.0f - 0.1f * moss));
}

Px genAsphalt(float u, float v) {
    float grain = hash2((int)(u * N), (int)(v * N), 3);
    float n = tfbm(u, v, 4, 4, 13);
    float d = 0.78f + 0.25f * grain + 0.25f * n;
    float crack = std::fabs(tfbm(u, v, 5, 4, 41) - 0.5f);
    if (crack < 0.012f) d *= 0.55f;
    return gray(d);
}

Px genConcrete(float u, float v) {
    float n = tfbm(u, v, 8, 5, 17);
    float speck = hash2((int)(u * N), (int)(v * N), 9);
    float d = 0.85f + 0.22f * n + 0.10f * speck;
    // expansion joints (2 x 2 slabs per tile)
    float fu = std::fmod(u * 2.0f, 1.0f), fv = std::fmod(v * 2.0f, 1.0f);
    if (fu < 0.012f || fv < 0.012f) d *= 0.62f;
    return gray(d);
}

Px genBrick(float u, float v) {
    const int rows = 8;
    float y = v * rows;
    int row = (int)y;
    float x = u * 4.0f + ((row & 1) ? 0.5f : 0.0f);
    int col = (int)std::floor(x);
    float fy = y - row, fx = x - col;
    bool mortar = fy < 0.10f || fx < 0.05f;
    if (mortar) {
        float n = tnoise(u * 64, v * 64, 64, 4);
        return detail(1.35f + 0.1f * n, 1.40f + 0.1f * n, 1.42f + 0.1f * n);
    }
    float b = 0.80f + 0.35f * hash2(col & 3, row, 88);
    float n = tfbm(u, v, 16, 3, 23);
    b *= 0.85f + 0.3f * n;
    return detail(b * 1.05f, b * 0.95f, b * 0.9f);
}

Px genWood(float u, float v) {
    const int planks = 4;
    float x = u * planks;
    int p = (int)x;
    float fx = x - p;
    float shift = hash2(p, 0, 5) * 3.0f;
    float grain = std::sin((v * 6.0f + shift) * 6.2831f * 2.0f + tfbm(u, v + shift, 4, 4, 61) * 9.0f);
    float d = 0.85f + 0.12f * grain + 0.18f * hash2(p, 1, 9) + 0.1f * tfbm(u, v, 16, 3, 3);
    // plank end joints
    float endPos = hash2(p, 2, 1);
    if (fx < 0.03f || fx > 0.97f || std::fabs(std::fmod(v + endPos, 1.0f) - 0.5f) < 0.006f) d *= 0.55f;
    return detail(d * 1.03f, d, d * 0.95f);
}

Px genTile(float u, float v) {
    float fu = std::fmod(u * 2.0f, 1.0f), fv = std::fmod(v * 2.0f, 1.0f);
    bool grout = fu < 0.02f || fv < 0.02f || fu > 0.98f || fv > 0.98f;
    if (grout) return gray(0.65f);
    float vein = std::fabs(std::sin((u + v * 0.6f) * 12.0f + tfbm(u, v, 4, 5, 33) * 8.0f));
    float d = 0.95f + 0.10f * tfbm(u, v, 8, 3, 2) - 0.12f * std::pow(1.0f - vein, 8.0f);
    return gray(d);
}

Px genFabric(float u, float v) {
    float wu = std::sin(u * N * 0.5f * PI), wv = std::sin(v * N * 0.5f * PI);
    float weave = 0.5f + 0.5f * (wu * wv);
    float d = 0.85f + 0.18f * weave + 0.10f * tfbm(u, v, 8, 3, 15);
    return gray(d);
}

Px genBark(float u, float v) {
    float ridges = tfbm(u * 1.0f, v * 0.25f, 8, 5, 51);
    float d = 0.55f + 0.85f * ridges;
    float crev = std::fabs(tnoise(u * 12, v * 3, 12, 7) - 0.5f);
    if (crev < 0.08f) d *= 0.5f;
    return detail(d, d * 0.95f, d * 0.88f);
}

// alpha cut-out: a round cluster of many small pointed leaves. Leaves drawn
// later lie on top; deeper (earlier) leaves are darker -> natural depth.
Px genLeaves(float u, float v) {
    Px out = gray(1.0f, 0.0f);
    Rng rng(4242);
    const int count = 120;
    for (int i = 0; i < count; ++i) {
        // place leaves inside a circle, denser near the middle
        float ang = rng.range(0, 2 * PI), rad = 0.43f * std::sqrt(rng.uniform());
        float cx = 0.5f + std::cos(ang) * rad, cy = 0.5f + std::sin(ang) * rad;
        float dir = rng.range(0, 2 * PI), len = rng.range(0.055f, 0.09f), wid = len * rng.range(0.30f, 0.42f);
        float hue = rng.range(-1.0f, 1.0f);
        float depth = (float)i / count;   // 0 = deep inside the cluster, 1 = outer leaves
        float dx = u - cx, dy = v - cy;
        float lx = dx * std::cos(dir) + dy * std::sin(dir);
        float ly = -dx * std::sin(dir) + dy * std::cos(dir);
        float t = lx / len;                       // -1 (stem end) .. 1 (tip)
        if (t < -1.0f || t > 1.0f) continue;
        float halfW = wid * std::sqrt(std::max(0.0f, 1.0f - t * t)) * (1.0f - 0.25f * t);   // pointed tip
        if (std::fabs(ly) > halfW) continue;
        float rib = std::fabs(ly) < wid * 0.07f ? 0.82f : 1.0f;              // midrib
        float edge = 1.0f - 0.25f * (std::fabs(ly) / std::max(halfW, 1e-4f));
        float shade = (0.55f + 0.6f * depth) * (0.85f + 0.25f * (t * 0.5f + 0.5f)) * rib * edge;
        out = detail(shade * (0.95f + 0.12f * hue), shade * (1.05f + 0.05f * hue), shade * (0.80f - 0.1f * hue), 1.0f);
    }
    return out;
}

Px genGrassBlades(float u, float v) {
    Px out = gray(1.0f, 0.0f);
    Rng rng(777);
    for (int i = 0; i < 38; ++i) {
        float x0 = rng.range(0.05f, 0.95f), h = rng.range(0.45f, 0.98f);
        float bend = rng.range(-0.18f, 0.18f), w = rng.range(0.012f, 0.026f);
        float tone = rng.range(0.75f, 1.25f);
        if (v > h) continue;
        float t = v / h;
        float cx = x0 + bend * t * t;
        float halfW = w * (1.0f - t);
        if (std::fabs(u - cx) < halfW) {
            float d = tone * (0.7f + 0.5f * t);
            out = detail(d * (0.95f + 0.2f * t), d * 1.05f, d * 0.8f, 1.0f);
        }
    }
    return out;
}

Px genFern(float u, float v) {
    Px out = gray(1.0f, 0.0f);
    // central stem along v from bottom (v=0) to top
    float stemX = 0.5f + 0.04f * std::sin(v * 3.0f);
    if (std::fabs(u - stemX) < 0.012f * (1.2f - v) && v < 0.97f) return detail(0.8f, 0.9f, 0.6f, 1.0f);
    // leaflets
    for (int i = 0; i < 16; ++i) {
        float vy = 0.06f + i * 0.057f;
        float len = 0.42f * (1.0f - vy * 0.85f);
        for (int side = -1; side <= 1; side += 2) {
            float dx = (u - stemX) * side;
            if (dx < 0 || dx > len) continue;
            float yOnLeaf = vy + dx * 0.55f;           // leaflets angle upward
            float halfW = 0.022f * (1.0f - dx / len) + 0.004f;
            if (std::fabs(v - yOnLeaf) < halfW) {
                float d = 0.85f + 0.3f * (dx / len) + 0.1f * hash2(i, side + 2, 3);
                return detail(d * 0.9f, d * 1.08f, d * 0.8f, 1.0f);
            }
        }
    }
    return out;
}

Px genPalm(float u, float v) {
    Px out = gray(1.0f, 0.0f);
    float stemX = 0.5f;
    if (std::fabs(u - stemX) < 0.014f) return detail(1.1f, 1.05f, 0.75f, 1.0f);
    for (int i = 0; i < 30; ++i) {
        float vy = 0.03f + i * 0.032f;
        float len = 0.48f * std::sin(PI * (0.15f + 0.85f * vy));
        for (int side = -1; side <= 1; side += 2) {
            float dx = (u - stemX) * side;
            if (dx < 0 || dx > len) continue;
            float yOnLeaf = vy + dx * 0.30f;
            float halfW = 0.011f * (1.0f - dx / len) + 0.002f;
            if (std::fabs(v - yOnLeaf) < halfW) {
                float d = 0.85f + 0.25f * hash2(i, side + 2, 13);
                return detail(d * 0.92f, d * 1.05f, d * 0.75f, 1.0f);
            }
        }
    }
    return out;
}

Px genMetal(float u, float v) {
    float streak = tnoise(u * 4, v * 256, 4 * 64, 71);   // horizontal brushing
    float d = 0.92f + 0.16f * streak + 0.05f * tfbm(u, v, 4, 3, 9);
    return gray(d);
}

Px genPlaster(float u, float v) {
    float d = 0.94f + 0.12f * tfbm(u, v, 16, 4, 81);
    return gray(d);
}

Px genCarpet(float u, float v) {
    float speck = hash2((int)(u * N), (int)(v * N), 19);
    float pattern = 0.5f + 0.5f * std::sin(u * PI * 16) * std::sin(v * PI * 16);
    float d = 0.80f + 0.25f * speck + 0.10f * pattern;
    return gray(d);
}

Px genRoof(float u, float v) {
    float g = hash2((int)(u * N), (int)(v * N), 29);
    float d = 0.75f + 0.35f * g + 0.15f * tfbm(u, v, 8, 3, 1);
    return gray(d);
}

Px genRock(float u, float v) {
    float n = tfbm(u, v, 4, 6, 91);
    float d = 0.55f + 0.95f * n;
    float crack = std::fabs(tfbm(u, v, 3, 4, 92) - 0.5f);
    if (crack < 0.015f) d *= 0.45f;
    float moss = smoothstepf(0.62f, 0.75f, tfbm(u, v, 5, 3, 93));
    return detail(d * (1.0f - 0.3f * moss), d * (1.0f + 0.2f * moss), d * (1.0f - 0.35f * moss));
}

Px genBooks(float u, float v) {
    // spines of different widths and colours along u, book height along v
    float x = u;
    Rng rng(555);
    float pos = 0;
    int idx = 0;
    while (pos < 1.0f) {
        float w = rng.range(0.025f, 0.06f);
        if (x >= pos && x < pos + w) break;
        pos += w;
        idx++;
    }
    float r = hash2(idx, 0, 1), g = hash2(idx, 1, 1), b = hash2(idx, 2, 1);
    float h = 0.72f + 0.26f * hash2(idx, 3, 1);
    if (v > h) return detail(0.25f, 0.22f, 0.20f, 1.0f);  // shadow behind short books
    float band = (std::fabs(v - h * 0.82f) < 0.015f || std::fabs(v - h * 0.18f) < 0.015f) ? 1.6f : 1.0f;
    float edge = (x - pos) < 0.003f ? 0.5f : 1.0f;
    return detail((0.25f + 1.1f * r) * band * edge, (0.25f + 0.9f * g) * band * edge, (0.25f + 0.9f * b) * band * edge, 1.0f);
}

Px genFlowers(float u, float v) {
    Px out = gray(1.0f, 0.0f);
    Rng rng(909);
    for (int i = 0; i < 9; ++i) {
        float x0 = rng.range(0.15f, 0.85f), h = rng.range(0.45f, 0.9f);
        float hue = rng.uniform();
        // stem
        if (v < h && std::fabs(u - x0) < 0.008f) out = detail(0.7f, 1.0f, 0.6f, 1.0f);
        // flower head
        float dx = u - x0, dy = v - h;
        float r = std::sqrt(dx * dx + dy * dy);
        if (r < 0.055f) {
            float petals = 0.5f + 0.5f * std::cos(std::atan2(dy, dx) * 5.0f);
            if (r < 0.018f) return detail(1.9f, 1.6f, 0.3f, 1.0f);
            if (r < 0.03f + 0.025f * petals) {
                if (hue < 0.33f) return detail(1.9f, 0.5f, 0.6f, 1.0f);
                if (hue < 0.66f) return detail(1.9f, 1.9f, 1.9f, 1.0f);
                return detail(1.3f, 0.6f, 1.8f, 1.0f);
            }
        }
    }
    return out;
}

Px genCeiling(float u, float v) {
    float fu = std::fmod(u * 2.0f, 1.0f), fv = std::fmod(v * 2.0f, 1.0f);
    if (fu < 0.015f || fv < 0.015f) return gray(0.7f);
    float speck = hash2((int)(u * N), (int)(v * N), 37) > 0.93f ? 0.8f : 1.0f;
    return gray((0.95f + 0.06f * tfbm(u, v, 16, 2, 5)) * speck);
}

Px genMarble(float u, float v) {
    float t = tfbm(u, v, 3, 6, 101);
    float vein = std::fabs(std::sin((u * 2.0f + t * 6.0f) * PI));
    float d = 1.02f - 0.35f * std::pow(1.0f - vein, 10.0f) + 0.05f * tfbm(u, v, 16, 2, 3);
    return gray(d);
}

Px genLeather(float u, float v) {
    float cell = tnoise(u * 48, v * 48, 48, 61);
    float d = 0.88f + 0.18f * cell + 0.06f * tfbm(u, v, 8, 3, 62);
    return gray(d);
}

Px genSand(float u, float v) {
    float g = hash2((int)(u * N), (int)(v * N), 71);
    float ripple = 0.5f + 0.5f * std::sin((v + tfbm(u, v, 2, 3, 72) * 0.3f) * PI * 12);
    return gray(0.85f + 0.2f * g + 0.08f * ripple);
}

Px genVine(float u, float v) {
    Px out = gray(1.0f, 0.0f);
    float cx = 0.5f + 0.12f * std::sin(v * 14.0f);
    if (std::fabs(u - cx) < 0.018f) return detail(0.8f, 0.9f, 0.6f, 1.0f);
    for (int i = 0; i < 12; ++i) {
        float vy = (i + 0.5f) / 12.0f;
        float side = (i & 1) ? 1.0f : -1.0f;
        float lx = 0.5f + 0.12f * std::sin(vy * 14.0f) + side * 0.12f;
        float dx = (u - lx) / 0.11f, dy = (v - vy) / 0.05f;
        if (dx * dx + dy * dy < 1.0f) {
            float d = 0.85f + 0.3f * hash2(i, 0, 3);
            return detail(d * 0.9f, d * 1.1f, d * 0.8f, 1.0f);
        }
    }
    return out;
}

const Gen GENERATORS[TEX_COUNT] = {
    genNone, genGrass, genForestFloor, genAsphalt, genConcrete, genBrick, genWood, genTile, genFabric, genBark,
    genLeaves, genGrassBlades, genFern, genPalm, genMetal, genPlaster, genCarpet, genRoof, genRock, genBooks,
    genFlowers, genCeiling, genMarble, genLeather, genSand, genVine};

}  // namespace

bool TextureArray::create() {
    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, N, N, TEX_COUNT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    for (int layer = 0; layer < TEX_COUNT; ++layer) {
        std::vector<uint8_t>& px = m_pixels[layer];
        px.resize(N * N * 4);
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                Px p = GENERATORS[layer]((x + 0.5f) / N, (y + 0.5f) / N);
                uint8_t* o = &px[(y * N + x) * 4];
                o[0] = toByte(p.r); o[1] = toByte(p.g); o[2] = toByte(p.b); o[3] = toByte(p.a);
            }
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, N, N, 1, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    }
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLfloat maxAniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    if (glGetError() == GL_NO_ERROR && maxAniso > 1.0f)
        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(4.0f, maxAniso));
    while (glGetError() != GL_NO_ERROR) {}
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return true;
}

void TextureArray::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_tex);
}

void TextureArray::destroy() {
    if (m_tex) glDeleteTextures(1, &m_tex);
    m_tex = 0;
}

const char* TextureArray::layerName(int layer) {
    static const char* names[TEX_COUNT] = {"None", "Grass", "Forest floor", "Asphalt", "Concrete", "Brick", "Wood planks",
                                           "Floor tile", "Fabric", "Tree bark", "Leaf cluster", "Grass blades", "Fern",
                                           "Palm frond", "Brushed metal", "Plaster", "Carpet", "Roof", "Rock", "Books",
                                           "Flowers", "Ceiling tile", "Marble", "Leather", "Sand", "Vine"};
    return layer >= 0 && layer < TEX_COUNT ? names[layer] : "?";
}
