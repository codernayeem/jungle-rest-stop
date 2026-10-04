#include "Furniture.h"
#include "Materials.h"
#include <cmath>

namespace Furniture {

// -----------------------------------------------------------------------------
// Sofa: base frame + seat cushions (box + rounded front cylinder) + leaning
// back cushions + two arm rests (box + rolled cylinder top) + 4 wooden legs
// -----------------------------------------------------------------------------
void sofa(const Node& n, int seats, const Vec3& col) {
    Material fab = Mat::fabric(col, 5.0f);
    Material fabDark = Mat::fabric(col * 0.78f, 5.0f);
    Material legM = Mat::wood(Vec3(0.30f, 0.20f, 0.12f));
    const float seatW = 0.72f, armW = 0.16f;
    float W = seats * seatW + 2 * armW;

    n.box({0, 0.20f, 0}, {W, 0.22f, 0.88f}, fabDark);                       // base frame
    n.box({0, 0.56f, -0.37f}, {W, 0.66f, 0.16f}, fabDark);                  // back frame
    for (int i = 0; i < seats; ++i) {
        float x = -seats * seatW * 0.5f + seatW * (i + 0.5f);
        n.box({x, 0.38f, 0.05f}, {seatW - 0.02f, 0.15f, 0.64f}, fab);       // seat cushion
        n.part(M_CYLINDER, {x, 0.38f, 0.37f}, {0.15f, seatW - 0.02f, 0.15f}, fab, {0, 0, 90});  // rounded front
        n.box({x, 0.68f, -0.24f}, {seatW - 0.04f, 0.44f, 0.17f}, fab, {-10, 0, 0});           // back cushion
    }
    for (int s = -1; s <= 1; s += 2) {
        float x = s * (W * 0.5f - armW * 0.5f);
        n.box({x, 0.42f, 0.0f}, {armW, 0.44f, 0.88f}, fabDark);              // arm
        n.part(M_CYLINDER, {x, 0.64f, 0.0f}, {armW + 0.03f, 0.88f, armW + 0.03f}, fabDark, {90, 0, 0});  // arm roll
        for (int f = -1; f <= 1; f += 2)
            n.cyl({x, 0.045f, f * 0.36f}, {0.05f, 0.09f, 0.05f}, legM);     // legs
    }
    if (seats >= 2) {  // throw pillows
        Material pil = Mat::fabric(Vec3(0.92f, 0.78f, 0.35f), 6.0f);
        float px = seats * seatW * 0.5f - 0.25f;
        n.box({-px, 0.62f, -0.12f}, {0.36f, 0.34f, 0.12f}, pil, {-14, 15, 6});
        n.box({px, 0.62f, -0.12f}, {0.36f, 0.34f, 0.12f}, pil, {-14, -15, -6});
    }
}

void armchair(const Node& n, const Vec3& col) { sofa(n, 1, col); }

void coffeeTable(const Node& n, float w, float d) {
    Material top = Mat::wood(Vec3(0.42f, 0.27f, 0.15f), 1.5f);
    Material leg = Mat::darkMetal();
    n.box({0, 0.42f, 0}, {w, 0.05f, d}, top);
    n.box({0, 0.13f, 0}, {w - 0.12f, 0.03f, d - 0.10f}, top);
    for (int i = -1; i <= 1; i += 2)
        for (int j = -1; j <= 1; j += 2) n.box({i * (w * 0.5f - 0.06f), 0.2f, j * (d * 0.5f - 0.06f)}, {0.04f, 0.40f, 0.04f}, leg);
    // a few magazines and a bowl as decoration
    n.box({-w * 0.22f, 0.452f, 0.02f}, {0.30f, 0.012f, 0.22f}, Mat::matte(Vec3(0.85f, 0.25f, 0.2f)), {0, 12, 0});
    n.box({-w * 0.22f, 0.462f, 0.0f}, {0.28f, 0.01f, 0.21f}, Mat::matte(Vec3(0.2f, 0.45f, 0.8f)), {0, -5, 0});
    n.part(M_HEMISPHERE, {w * 0.2f, 0.505f, 0}, {0.24f, -0.10f, 0.24f}, Mat::plastic(Vec3(0.9f, 0.9f, 0.88f)));
    for (int i = 0; i < 3; ++i)
        n.sph({w * 0.2f + 0.05f * (i - 1), 0.47f, 0.03f * (i % 2)}, {0.06f, 0.06f, 0.06f}, Mat::plastic(i == 1 ? Vec3(0.9f, 0.7f, 0.1f) : Vec3(0.75f, 0.1f, 0.1f)));
}

void sideTable(const Node& n) {
    Material top = Mat::wood(Vec3(0.45f, 0.29f, 0.16f));
    n.cyl({0, 0.55f, 0}, {0.5f, 0.04f, 0.5f}, top);
    n.cyl({0, 0.28f, 0}, {0.05f, 0.52f, 0.05f}, Mat::darkMetal());
    n.cyl({0, 0.015f, 0}, {0.34f, 0.03f, 0.34f}, Mat::darkMetal());
}

void tableLamp(const Node& n, bool on) {
    n.sph({0, 0.09f, 0}, {0.16f, 0.18f, 0.16f}, Mat::plastic(Vec3(0.75f, 0.68f, 0.55f)));
    n.cyl({0, 0.25f, 0}, {0.02f, 0.2f, 0.02f}, Mat::chrome());
    Material shade = Mat::fabric(Vec3(0.95f, 0.90f, 0.78f), 8.0f);
    if (on) shade.emission = Vec3(1.0f, 0.78f, 0.45f) * 0.9f;
    n.tcone({0, 0.42f, 0}, {0.34f, 0.24f, 0.34f}, shade);
}

void rug(const Node& n, float w, float d, const Vec3& c, const Vec3& b) {
    n.box({0, 0.004f, 0}, {w, 0.008f, d}, Mat::fabric(b, 6.0f).fl(MF_NO_SHADOW));
    n.box({0, 0.009f, 0}, {w - 0.24f, 0.01f, d - 0.24f}, Mat::fabric(c, 6.0f).tx(TEX_CARPET, 1.0f).fl(MF_NO_SHADOW));
}

// -----------------------------------------------------------------------------
// Potted plant: flipped truncated cone pot + soil disc + leaves
// -----------------------------------------------------------------------------
void pottedPlant(const Node& n, float s, int variant) {
    Material pot = Mat::plastic(variant == 1 ? Vec3(0.75f, 0.42f, 0.28f) : Vec3(0.92f, 0.92f, 0.9f));
    n.tcone({0, 0.22f * s, 0}, {0.42f * s, 0.44f * s, 0.42f * s}, pot, {180, 0, 0});
    n.cyl({0, 0.43f * s, 0}, {0.32f * s, 0.02f, 0.32f * s}, Mat::matte(Vec3(0.18f, 0.12f, 0.08f)));
    Material leaf = Mat::leafCard(Vec3(0.30f, 0.55f, 0.22f));
    leaf.flags &= ~MF_WIND;
    if (variant == 0) {           // leafy bush
        n.cyl({0, 0.6f * s, 0}, {0.04f * s, 0.4f * s, 0.04f * s}, Mat::bark());
        for (int i = 0; i < 5; ++i) {
            float a = i * 72.0f;
            n.part(M_LEAVES, {std::sin(toRadians(a)) * 0.12f * s, (0.85f + 0.12f * (i % 2)) * s, std::cos(toRadians(a)) * 0.12f * s},
                   Vec3(0.6f * s), leaf, {0, a, 0});
        }
        n.part(M_SPHERE_LOW, {0, 0.9f * s, 0}, Vec3(0.42f * s), Mat::matte(Vec3(0.12f, 0.30f, 0.10f)));
    } else if (variant == 1) {    // fern / palm in a pot
        Material fern = Mat::leafCard(Vec3(0.35f, 0.6f, 0.25f), TEX_FERN);
        fern.flags &= ~MF_WIND;
        for (int i = 0; i < 6; ++i)
            n.part(M_QUAD, {0, 0.75f * s, 0}, {0.5f * s, 0.8f * s, 1}, fern, {35.0f, i * 60.0f, 0});
        n.part(M_CROSS, {0, 0.44f * s, 0}, {0.7f * s, 0.9f * s, 0.7f * s}, fern);
    } else {                      // snake plant: tall pointed leaves
        Material sl = Mat::plastic(Vec3(0.22f, 0.42f, 0.18f));
        for (int i = 0; i < 7; ++i) {
            float a = i * 51.0f, r = 0.07f * s;
            n.part(M_CONE, {std::sin(toRadians(a)) * r, (0.75f + 0.05f * (i % 3)) * s, std::cos(toRadians(a)) * r},
                   {0.09f * s, (0.65f + 0.08f * (i % 3)) * s, 0.025f * s}, sl, {(i % 2 ? 6.0f : -6.0f), a, 0});
        }
    }
}

// -----------------------------------------------------------------------------
// Bookshelf: frame boards + 5 shelves + rows of books (textured boxes)
// -----------------------------------------------------------------------------
void bookshelf(const Node& n, float w, float h) {
    Material wd = Mat::wood(Vec3(0.40f, 0.25f, 0.14f), 1.4f);
    const float d = 0.38f;
    n.box({-w / 2 + 0.02f, h / 2, 0}, {0.04f, h, d}, wd);
    n.box({w / 2 - 0.02f, h / 2, 0}, {0.04f, h, d}, wd);
    n.box({0, h - 0.02f, 0}, {w, 0.04f, d}, wd);
    n.box({0, h / 2, -d / 2 + 0.01f}, {w, h, 0.02f}, wd);
    n.box({0, 0.05f, 0}, {w, 0.10f, d}, wd);
    int shelves = 5;
    float sh = (h - 0.12f) / shelves;
    Material books = Material(Vec3(0.95f), 0.12f, 16.0f).tx(TEX_BOOKS).fl(MF_MESH_UV);
    Material pages = Material(Vec3(0.86f, 0.82f, 0.72f), 0.05f, 8.0f);   // top / sides of a row of books
    for (int i = 0; i < shelves; ++i) {
        float y = 0.10f + i * sh;
        n.box({0, y, 0}, {w - 0.06f, 0.025f, d - 0.02f}, wd);
        // two blocks of books with different widths so the pattern does not repeat
        float bh = sh * 0.78f;
        float split = 0.35f + 0.1f * (i % 3);
        float w1 = (w - 0.1f) * split, w2 = (w - 0.1f) - w1 - 0.04f;
        // block of books = plain box (pages colour) + textured quad on the front (the spines)
        float x1 = -w / 2 + 0.05f + w1 / 2, x2 = w / 2 - 0.05f - w2 / 2;
        n.box({x1, y + 0.0125f + bh / 2, 0.02f}, {w1, bh, 0.24f}, pages);
        n.part(M_QUAD, {x1, y + 0.0125f + bh / 2, 0.141f}, {w1, bh, 1}, books, {0, 0, 0});
        n.box({x2, y + 0.0125f + bh * 0.46f, 0.02f}, {w2, bh * 0.92f, 0.24f}, pages);
        n.part(M_QUAD, {x2, y + 0.0125f + bh * 0.46f, 0.141f}, {(i % 2) ? -w2 : w2, bh * 0.92f, 1}, books);   // mirrored on odd shelves
        if (i == 2) n.box({-w / 2 + 0.085f + w1, y + 0.0125f + 0.085f, 0.02f}, {0.035f, 0.16f, 0.22f}, Mat::matte(Vec3(0.6f, 0.1f, 0.1f)), {0, 0, -12});
    }
}

void floorLamp(const Node& n, bool on) {
    n.cyl({0, 0.015f, 0}, {0.32f, 0.03f, 0.32f}, Mat::darkMetal());
    n.cyl({0, 0.8f, 0}, {0.03f, 1.6f, 0.03f}, Mat::darkMetal());
    Material shade = Mat::fabric(Vec3(0.95f, 0.88f, 0.72f), 8.0f);
    if (on) shade.emission = Vec3(1.0f, 0.75f, 0.42f) * 0.8f;
    n.tcone({0, 1.62f, 0}, {0.44f, 0.30f, 0.44f}, shade);
    n.sph({0, 1.55f, 0}, Vec3(0.08f), on ? Mat::glow(Vec3(1.0f, 0.85f, 0.6f), 2.5f) : Mat::plastic(Vec3(0.9f)));
}

// -----------------------------------------------------------------------------
// Wall clock: torus rim + white face + 12 tick marks + rotating hands
// -----------------------------------------------------------------------------
void wallClock(const Node& n, float hour) {
    n.torus({0, 0, 0.02f}, {0.42f, 0.25f, 0.42f}, Mat::wood(Vec3(0.25f, 0.15f, 0.08f)), {90, 0, 0});
    n.cyl({0, 0, 0.015f}, {0.36f, 0.02f, 0.36f}, Mat::matte(Vec3(0.96f, 0.95f, 0.92f)), {90, 0, 0});
    Material dark = Mat::matte(Vec3(0.08f));
    for (int i = 0; i < 12; ++i) {
        Node t = n.rotZ(-i * 30.0f);
        t.box({0, 0.145f, 0.03f}, {i % 3 == 0 ? 0.018f : 0.01f, i % 3 == 0 ? 0.04f : 0.025f, 0.006f}, dark);
    }
    float h12 = std::fmod(hour, 12.0f);
    float minutes = (hour - std::floor(hour)) * 60.0f;
    Node hh = n.rotZ(-h12 * 30.0f);
    hh.box({0, 0.05f, 0.036f}, {0.018f, 0.11f, 0.005f}, dark);
    Node mh = n.rotZ(-minutes * 6.0f);
    mh.box({0, 0.07f, 0.040f}, {0.012f, 0.15f, 0.005f}, dark);
    n.cyl({0, 0, 0.044f}, {0.025f, 0.01f, 0.025f}, Mat::matte(Vec3(0.7f, 0.1f, 0.1f)), {90, 0, 0});
}

void painting(const Node& n, float w, float h, int v) {
    n.box({0, 0, 0.015f}, {w + 0.08f, h + 0.08f, 0.03f}, Mat::wood(Vec3(0.20f, 0.13f, 0.07f)));
    Material base;
    if (v == 0) {          // landscape: sky, hills, sun
        n.box({0, h * 0.25f, 0.032f}, {w, h * 0.5f, 0.004f}, Mat::matte(Vec3(0.45f, 0.65f, 0.88f)));
        n.box({0, -h * 0.25f, 0.032f}, {w, h * 0.5f, 0.004f}, Mat::matte(Vec3(0.30f, 0.55f, 0.25f)));
        n.part(M_HEMISPHERE, {-w * 0.2f, -0.02f, 0.034f}, {w * 0.6f, h * 0.35f, 0.003f}, Mat::matte(Vec3(0.2f, 0.42f, 0.2f)), {90, 0, 0});
        n.cyl({w * 0.25f, h * 0.28f, 0.035f}, {h * 0.18f, 0.003f, h * 0.18f}, Mat::matte(Vec3(1.0f, 0.85f, 0.3f)), {90, 0, 0});
    } else if (v == 1) {   // abstract blocks
        n.box({0, 0, 0.032f}, {w, h, 0.004f}, Mat::matte(Vec3(0.95f, 0.93f, 0.88f)));
        n.box({-w * 0.2f, h * 0.15f, 0.035f}, {w * 0.4f, h * 0.5f, 0.003f}, Mat::matte(Vec3(0.85f, 0.25f, 0.18f)));
        n.box({w * 0.2f, -h * 0.2f, 0.035f}, {w * 0.35f, h * 0.35f, 0.003f}, Mat::matte(Vec3(0.15f, 0.3f, 0.6f)));
        n.box({w * 0.25f, h * 0.25f, 0.036f}, {w * 0.2f, h * 0.2f, 0.003f}, Mat::matte(Vec3(0.95f, 0.75f, 0.15f)));
    } else {               // jungle sunset
        n.box({0, 0, 0.032f}, {w, h, 0.004f}, Mat::matte(Vec3(0.95f, 0.55f, 0.30f)));
        n.cyl({0, -h * 0.05f, 0.034f}, {h * 0.35f, 0.003f, h * 0.35f}, Mat::matte(Vec3(1.0f, 0.85f, 0.4f)), {90, 0, 0});
        for (int i = 0; i < 5; ++i)
            n.part(M_CONE, {-w * 0.4f + i * w * 0.2f, -h * 0.22f, 0.036f}, {w * 0.18f, h * 0.55f, 0.003f}, Mat::matte(Vec3(0.1f, 0.15f, 0.08f)));
    }
}

void readingTable(const Node& n) {
    Material wd = Mat::wood(Vec3(0.48f, 0.31f, 0.17f));
    n.cyl({0, 0.74f, 0}, {1.0f, 0.04f, 1.0f}, wd);
    n.cyl({0, 0.37f, 0}, {0.08f, 0.72f, 0.08f}, Mat::darkMetal());
    n.cyl({0, 0.02f, 0}, {0.5f, 0.04f, 0.5f}, Mat::darkMetal());
    n.box({0.15f, 0.775f, 0.1f}, {0.22f, 0.03f, 0.16f}, Mat::matte(Vec3(0.2f, 0.35f, 0.6f)), {0, 20, 0});
}

void chair(const Node& n, const Vec3& c) {
    Material seat = Mat::fabric(c, 5.0f);
    Material leg = Mat::wood(Vec3(0.35f, 0.22f, 0.12f));
    n.box({0, 0.45f, 0}, {0.44f, 0.06f, 0.44f}, seat);
    n.box({0, 0.75f, -0.2f}, {0.42f, 0.5f, 0.04f}, seat, {-6, 0, 0});
    for (int i = -1; i <= 1; i += 2)
        for (int j = -1; j <= 1; j += 2) n.box({i * 0.19f, 0.21f, j * 0.19f}, {0.04f, 0.42f, 0.04f}, leg);
}

void bench(const Node& n, float len) {
    Material wd = Mat::wood(Vec3(0.50f, 0.33f, 0.18f), 2.0f);
    Material metal = Mat::darkMetal();
    for (int s = -1; s <= 1; s += 2) {
        n.box({s * (len / 2 - 0.15f), 0.22f, 0.0f}, {0.06f, 0.44f, 0.45f}, metal);
        n.box({s * (len / 2 - 0.15f), 0.65f, -0.22f}, {0.06f, 0.5f, 0.05f}, metal, {-10, 0, 0});
    }
    for (int i = 0; i < 4; ++i) n.box({0, 0.46f, -0.17f + i * 0.115f}, {len, 0.04f, 0.09f}, wd);
    for (int i = 0; i < 2; ++i) n.box({0, 0.62f + i * 0.17f, -0.25f}, {len, 0.10f, 0.035f}, wd, {-10, 0, 0});
}

void trashBin(const Node& n, const Vec3& c) {
    n.cyl({0, 0.40f, 0}, {0.42f, 0.80f, 0.42f}, Mat::paint(c));
    n.part(M_HEMISPHERE, {0, 0.80f, 0}, {0.44f, 0.22f, 0.44f}, Mat::steel());
    n.box({0, 0.86f, 0.18f}, {0.18f, 0.06f, 0.08f}, Mat::black());
    n.cyl({0, 0.02f, 0}, {0.44f, 0.04f, 0.44f}, Mat::darkMetal());
}

void counter(const Node& n, float w, float d, float h) {
    Material cab = Mat::wood(Vec3(0.30f, 0.20f, 0.13f), 1.5f);
    n.box({0, (h - 0.04f) / 2 + 0.06f, 0}, {w, h - 0.10f, d}, cab);
    n.box({0, 0.03f, 0.02f}, {w - 0.04f, 0.06f, d - 0.06f}, Mat::black());          // toe kick
    n.box({0, h - 0.02f, 0.015f}, {w + 0.04f, 0.04f, d + 0.03f}, Mat::marble(Vec3(0.88f, 0.87f, 0.85f)));
    int doors = std::max(2, (int)(w / 0.6f));
    float dw = w / doors;
    for (int i = 0; i < doors; ++i) {
        float x = -w / 2 + dw * (i + 0.5f);
        n.box({x, (h - 0.04f) / 2 + 0.06f, d / 2 + 0.006f}, {dw - 0.02f, h - 0.16f, 0.012f}, Mat::wood(Vec3(0.36f, 0.24f, 0.15f), 1.5f));
        n.box({x + (i % 2 ? -dw * 0.38f : dw * 0.38f), h - 0.22f, d / 2 + 0.02f}, {0.02f, 0.14f, 0.02f}, Mat::chrome());
    }
}

void wallCabinet(const Node& n, float w) {
    Material cab = Mat::wood(Vec3(0.36f, 0.24f, 0.15f), 1.5f);
    n.box({0, 0, 0}, {w, 0.6f, 0.34f}, cab);
    int doors = std::max(2, (int)(w / 0.6f));
    float dw = w / doors;
    for (int i = 0; i < doors; ++i) {
        float x = -w / 2 + dw * (i + 0.5f);
        n.box({x, 0, 0.175f}, {dw - 0.02f, 0.56f, 0.012f}, Mat::wood(Vec3(0.42f, 0.28f, 0.17f), 1.5f));
        n.box({x + (i % 2 ? -dw * 0.38f : dw * 0.38f), -0.2f, 0.19f}, {0.02f, 0.12f, 0.02f}, Mat::chrome());
    }
}

// -----------------------------------------------------------------------------
// Lights
// -----------------------------------------------------------------------------
void pendantLamp(const Node& n, float cordLen, bool on, const Vec3& shadeColor) {
    n.cyl({0, -0.015f, 0}, {0.14f, 0.03f, 0.14f}, Mat::darkMetal());
    n.cyl({0, -cordLen / 2, 0}, {0.012f, cordLen, 0.012f}, Mat::black());
    Material shade = Mat::paint(shadeColor);
    n.tcone({0, -cordLen - 0.13f, 0}, {0.46f, 0.26f, 0.46f}, shade);
    n.cyl({0, -cordLen - 0.265f, 0}, {0.42f, 0.01f, 0.42f}, on ? Mat::glow(Vec3(1.0f, 0.86f, 0.62f), 0.9f) : Mat::matte(Vec3(0.3f)));
    n.sph({0, -cordLen - 0.22f, 0}, Vec3(0.11f), on ? Mat::glow(Vec3(1.0f, 0.88f, 0.65f), 3.0f) : Mat::plastic(Vec3(0.85f)));
}

void spotFixture(const Node& n, bool on) {
    n.torus({0, -0.01f, 0}, {0.22f, 0.12f, 0.22f}, Mat::chrome());
    n.cyl({0, -0.012f, 0}, {0.13f, 0.01f, 0.13f}, on ? Mat::glow(Vec3(1.0f, 0.95f, 0.85f), 3.0f) : Mat::matte(Vec3(0.4f)));
}

void tubeLight(const Node& n, float len, bool on) {
    n.box({0, -0.03f, 0}, {len, 0.06f, 0.34f}, Mat::plastic(Vec3(0.93f)));
    for (int i = -1; i <= 1; i += 2)
        n.part(M_CYLINDER, {0, -0.07f, i * 0.08f}, {0.035f, len - 0.08f, 0.035f},
               on ? Mat::glow(Vec3(0.92f, 0.97f, 1.0f), 2.6f) : Mat::plastic(Vec3(0.9f)), {0, 0, 90});
}

void lightSwitch(const Node& n, bool on, bool hovered) {
    Material plate = Mat::plastic(Vec3(0.95f, 0.95f, 0.93f));
    if (hovered) plate.emission = Vec3(0.15f, 0.35f, 0.5f);
    n.box({0, 0, 0.006f}, {0.08f, 0.12f, 0.012f}, plate);
    n.box({0, 0, 0.016f}, {0.025f, 0.045f, 0.012f}, Mat::plastic(Vec3(0.85f)), {on ? -15.0f : 15.0f, 0, 0});
    n.sph({0, -0.042f, 0.013f}, Vec3(0.008f), on ? Mat::glow(Vec3(0.2f, 1.0f, 0.3f), 1.5f) : Mat::glow(Vec3(1.0f, 0.3f, 0.1f), 0.6f));
}

void streetLamp(const Node& n, float h, bool on) {
    Material pole = Mat::darkMetal();
    n.cyl({0, 0.15f, 0}, {0.34f, 0.3f, 0.34f}, Mat::concrete());
    n.tcone({0, h / 2, 0}, {0.18f, h, 0.18f}, pole);
    n.part(M_CYLINDER, {0, h - 0.05f, 0.55f}, {0.06f, 1.1f, 0.06f}, pole, {90, 0, 0});
    n.part(M_FRUSTUM_BOX, {0, h - 0.06f, 1.15f}, {0.38f, 0.14f, 0.62f}, pole, {180, 0, 0});
    n.box({0, h - 0.14f, 1.15f}, {0.30f, 0.02f, 0.5f}, on ? Mat::glow(Vec3(1.0f, 0.82f, 0.55f), 3.0f) : Mat::plastic(Vec3(0.75f)));
}

void bollard(const Node& n, bool on) {
    n.cyl({0, 0.45f, 0}, {0.16f, 0.9f, 0.16f}, Mat::darkMetal());
    n.cyl({0, 0.78f, 0}, {0.165f, 0.06f, 0.165f}, on ? Mat::glow(Vec3(1.0f, 0.85f, 0.6f), 2.0f) : Mat::plastic(Vec3(0.8f)));
    n.part(M_HEMISPHERE, {0, 0.9f, 0}, {0.16f, 0.08f, 0.16f}, Mat::darkMetal());
}

void stanchion(const Node& n) {
    n.cyl({0, 0.015f, 0}, {0.3f, 0.03f, 0.3f}, Mat::chrome());
    n.cyl({0, 0.48f, 0}, {0.05f, 0.95f, 0.05f}, Mat::chrome());
    n.sph({0, 0.97f, 0}, Vec3(0.08f), Mat::chrome());
}

void securityCamera(const Node& n, float t) {
    n.box({0, 0, 0.03f}, {0.1f, 0.12f, 0.06f}, Mat::plastic(Vec3(0.9f)));
    n.box({0, -0.05f, 0.14f}, {0.04f, 0.04f, 0.2f}, Mat::plastic(Vec3(0.9f)), {20, 0, 0});
    Node cam = n.at({0, -0.10f, 0.24f}, {25, std::sin(t * 0.4f) * 30.0f, 0});
    cam.box({0, 0, 0}, {0.12f, 0.10f, 0.24f}, Mat::plastic(Vec3(0.95f)));
    cam.part(M_CYLINDER, {0, 0, 0.125f}, {0.06f, 0.02f, 0.06f}, Mat::black(), {90, 0, 0});
    cam.sph({0.04f, 0.035f, 0.12f}, Vec3(0.015f), Mat::glow(Vec3(1, 0.1f, 0.1f), std::fmod(t, 1.0f) < 0.5f ? 2.0f : 0.2f));
}

void smokeDetector(const Node& n) {
    n.cyl({0, -0.02f, 0}, {0.13f, 0.04f, 0.13f}, Mat::plastic(Vec3(0.95f)));
    n.sph({0.03f, -0.04f, 0}, Vec3(0.012f), Mat::glow(Vec3(0.1f, 1.0f, 0.2f), 1.0f));
}

void acUnit(const Node& n) {
    n.box({0, 0.4f, 0}, {1.2f, 0.8f, 0.8f}, Mat::plastic(Vec3(0.82f, 0.83f, 0.82f)));
    n.cyl({0, 0.81f, 0}, {0.6f, 0.02f, 0.6f}, Mat::black());
    n.torus({0, 0.82f, 0}, {0.75f, 0.4f, 0.75f}, Mat::darkMetal());
    n.box({0, 0.4f, 0.41f}, {1.0f, 0.5f, 0.01f}, Mat::darkMetal());
}

void signBoard(const Node& n, float w, float h, const Vec3& c, bool lit) {
    n.box({0, 0, 0}, {w + 0.1f, h + 0.1f, 0.12f}, Mat::darkMetal());
    Material face(c, 0.3f, 32.0f);
    if (lit) face.emission = c * 0.6f;
    n.box({0, 0, 0.065f}, {w, h, 0.01f}, face);
}

}  // namespace Furniture
