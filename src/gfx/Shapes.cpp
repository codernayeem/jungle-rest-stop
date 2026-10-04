#include "Shapes.h"
#include <cmath>

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------
static void addQuad(std::vector<Vertex>& v, std::vector<GLuint>& idx,
                    Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n,
                    Vec2 ua = {0, 0}, Vec2 ub = {1, 0}, Vec2 uc = {1, 1}, Vec2 ud = {0, 1}) {
    GLuint base = (GLuint)v.size();
    v.push_back({a, n, ua});
    v.push_back({b, n, ub});
    v.push_back({c, n, uc});
    v.push_back({d, n, ud});
    idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

static void addTri(std::vector<Vertex>& v, std::vector<GLuint>& idx, Vec3 a, Vec3 b, Vec3 c, Vec3 n) {
    GLuint base = (GLuint)v.size();
    v.push_back({a, n, {0, 0}});
    v.push_back({b, n, {1, 0}});
    v.push_back({c, n, {0, 1}});
    idx.insert(idx.end(), {base, base + 1, base + 2});
}

// -----------------------------------------------------------------------------
// Cube: 6 faces x 4 vertices so every face has its own flat normal
// -----------------------------------------------------------------------------
void genCube(std::vector<Vertex>& v, std::vector<GLuint>& i) {
    const float h = 0.5f;
    addQuad(v, i, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}, {0, 0, 1});      // front
    addQuad(v, i, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}, {0, 0, -1}); // back
    addQuad(v, i, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}, {0, 1, 0});      // top
    addQuad(v, i, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}, {0, -1, 0}); // bottom
    addQuad(v, i, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}, {1, 0, 0});      // right
    addQuad(v, i, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}, {-1, 0, 0}); // left
}

// -----------------------------------------------------------------------------
// Cylinder / cone / truncated cone (all share one generator)
//   bottom radius 0.5 at y=-0.5, top radius = topRadius at y=+0.5
// -----------------------------------------------------------------------------
void genCylinder(std::vector<Vertex>& v, std::vector<GLuint>& idx, int seg, float topR, bool caps) {
    const float botR = 0.5f, h = 0.5f;
    // side normal tilts outward/upward when radii differ
    float slope = (botR - topR);  // over height 1
    GLuint base = (GLuint)v.size();
    for (int s = 0; s <= seg; ++s) {
        float t = 2.0f * PI * s / seg;
        float cx = std::cos(t), cz = std::sin(t);
        Vec3 n = normalize(Vec3(cx, slope, cz));
        float u = (float)s / seg;
        v.push_back({{cx * topR, h, cz * topR}, n, {u, 1}});
        v.push_back({{cx * botR, -h, cz * botR}, n, {u, 0}});
    }
    for (int s = 0; s < seg; ++s) {
        GLuint tl = base + s * 2, bl = tl + 1, tr = tl + 2, br = tl + 3;
        idx.insert(idx.end(), {bl, br, tr, bl, tr, tl});
    }
    if (!caps) return;
    auto cap = [&](float y, float r, float ny) {
        if (r <= 0.0001f) return;
        GLuint c = (GLuint)v.size();
        v.push_back({{0, y, 0}, {0, ny, 0}, {0.5f, 0.5f}});
        for (int s = 0; s <= seg; ++s) {
            float t = 2.0f * PI * s / seg;
            float cx = std::cos(t), cz = std::sin(t);
            v.push_back({{cx * r, y, cz * r}, {0, ny, 0}, {0.5f + 0.5f * cx, 0.5f + 0.5f * cz}});
        }
        for (int s = 0; s < seg; ++s) {
            if (ny > 0) idx.insert(idx.end(), {c, c + 2 + s, c + 1 + s});
            else idx.insert(idx.end(), {c, c + 1 + s, c + 2 + s});
        }
    };
    cap(h, topR, 1.0f);
    cap(-h, botR, -1.0f);
}

// -----------------------------------------------------------------------------
// UV sphere (rings = latitude bands, sectors = longitude slices)
// -----------------------------------------------------------------------------
void genSphere(std::vector<Vertex>& v, std::vector<GLuint>& idx, int rings, int sectors, bool hemi) {
    const float r = 0.5f;
    GLuint base = (GLuint)v.size();
    float phiMax = hemi ? PI * 0.5f : PI;
    for (int i = 0; i <= rings; ++i) {
        float phi = phiMax * i / rings;
        float y = r * std::cos(phi), sp = std::sin(phi);
        for (int s = 0; s <= sectors; ++s) {
            float th = 2.0f * PI * s / sectors;
            Vec3 p(r * sp * std::cos(th), y, r * sp * std::sin(th));
            v.push_back({p, normalize(p), {(float)s / sectors, (float)i / rings}});
        }
    }
    for (int i = 0; i < rings; ++i)
        for (int s = 0; s < sectors; ++s) {
            GLuint a = base + i * (sectors + 1) + s, b = a + sectors + 1;
            idx.insert(idx.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    if (hemi) {  // flat disc closing the bottom
        GLuint c = (GLuint)v.size();
        v.push_back({{0, 0, 0}, {0, -1, 0}, {0.5f, 0.5f}});
        for (int s = 0; s <= sectors; ++s) {
            float th = 2.0f * PI * s / sectors;
            v.push_back({{r * std::cos(th), 0, r * std::sin(th)}, {0, -1, 0}, {0.5f + 0.5f * std::cos(th), 0.5f + 0.5f * std::sin(th)}});
        }
        for (int s = 0; s < sectors; ++s) idx.insert(idx.end(), {c, c + 1 + s, c + 2 + s});
    }
}

// -----------------------------------------------------------------------------
// Torus lying flat (in XZ plane)
// -----------------------------------------------------------------------------
static void genTorus(std::vector<Vertex>& v, std::vector<GLuint>& idx, float R, float r, int seg, int sides) {
    GLuint base = (GLuint)v.size();
    for (int i = 0; i <= seg; ++i) {
        float u = 2.0f * PI * i / seg;
        Vec3 c(std::cos(u) * R, 0, std::sin(u) * R);
        Vec3 out(std::cos(u), 0, std::sin(u));
        for (int j = 0; j <= sides; ++j) {
            float w = 2.0f * PI * j / sides;
            Vec3 n = out * std::cos(w) + Vec3(0, 1, 0) * std::sin(w);
            v.push_back({c + n * r, n, {(float)i / seg, (float)j / sides}});
        }
    }
    for (int i = 0; i < seg; ++i)
        for (int j = 0; j < sides; ++j) {
            GLuint a = base + i * (sides + 1) + j, b = a + sides + 1;
            idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
}

// -----------------------------------------------------------------------------
// Wedge: triangle (bottom-front, bottom-back, top-back) extruded along X.
// Slope faces +Z/+Y.  Bounds: x,y,z in [-0.5, 0.5]
// -----------------------------------------------------------------------------
static void genWedge(std::vector<Vertex>& v, std::vector<GLuint>& idx) {
    const float h = 0.5f;
    Vec3 lbf(-h, -h, h), lbb(-h, -h, -h), ltb(-h, h, -h);
    Vec3 rbf(h, -h, h), rbb(h, -h, -h), rtb(h, h, -h);
    addTri(v, idx, lbf, ltb, lbb, {-1, 0, 0});
    addTri(v, idx, rbf, rbb, rtb, {1, 0, 0});
    addQuad(v, idx, lbb, rbb, rbf, lbf, {0, -1, 0});                       // bottom
    addQuad(v, idx, rbb, lbb, ltb, rtb, {0, 0, -1});                       // back
    addQuad(v, idx, lbf, rbf, rtb, ltb, normalize(Vec3(0, 1, 1)));         // slope
}

// -----------------------------------------------------------------------------
// Tapered box (frustum): bottom 1 x 1 at y=-0.5, top 0.7(x) x 0.6(z) at y=+0.5
// -----------------------------------------------------------------------------
static void genFrustumBox(std::vector<Vertex>& v, std::vector<GLuint>& idx) {
    const float b = 0.5f, tx = 0.35f, tz = 0.30f, h = 0.5f;
    Vec3 b0(-b, -h, b), b1(b, -h, b), b2(b, -h, -b), b3(-b, -h, -b);
    Vec3 t0(-tx, h, tz), t1(tx, h, tz), t2(tx, h, -tz), t3(-tx, h, -tz);
    auto face = [&](Vec3 a, Vec3 bb, Vec3 c, Vec3 d) {
        Vec3 n = normalize(cross(bb - a, d - a));
        addQuad(v, idx, a, bb, c, d, n);
    };
    face(b0, b1, t1, t0);  // front (+Z)
    face(b2, b3, t3, t2);  // back
    face(b1, b2, t2, t1);  // right
    face(b3, b0, t0, t3);  // left
    addQuad(v, idx, t0, t1, t2, t3, {0, 1, 0});
    addQuad(v, idx, b3, b2, b1, b0, {0, -1, 0});
}

// -----------------------------------------------------------------------------
// Cards for vegetation (alpha-tested textures)
// -----------------------------------------------------------------------------
static void genCross(std::vector<Vertex>& v, std::vector<GLuint>& idx) {
    // normals point mostly up so grass is lit like the ground under it
    Vec3 n = normalize(Vec3(0, 1, 0.25f));
    addQuad(v, idx, {-0.5f, 0, 0}, {0.5f, 0, 0}, {0.5f, 1, 0}, {-0.5f, 1, 0}, n);
    Vec3 n2 = normalize(Vec3(0.25f, 1, 0));
    addQuad(v, idx, {0, 0, -0.5f}, {0, 0, 0.5f}, {0, 1, 0.5f}, {0, 1, -0.5f}, n2);
}

static void genLeaves(std::vector<Vertex>& v, std::vector<GLuint>& idx) {
    const float h = 0.5f;
    addQuad(v, idx, {-h, -h, 0}, {h, -h, 0}, {h, h, 0}, {-h, h, 0}, normalize(Vec3(0, 0.6f, 1)));
    addQuad(v, idx, {0, -h, -h}, {0, -h, h}, {0, h, h}, {0, h, -h}, normalize(Vec3(1, 0.6f, 0)));
    addQuad(v, idx, {-h, 0, h}, {h, 0, h}, {h, 0, -h}, {-h, 0, -h}, {0, 1, 0});
}

// -----------------------------------------------------------------------------
// Helical coil: a thin tube swept along a helix around the Z axis
// -----------------------------------------------------------------------------
static void genCoil(std::vector<Vertex>& v, std::vector<GLuint>& idx, int turns, int ptsPerTurn,
                    float coilR, float wireR, float length) {
    int steps = turns * ptsPerTurn, tube = 6;
    GLuint base = (GLuint)v.size();
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / steps, a = t * turns * 2.0f * PI;
        Vec3 c(coilR * std::cos(a), coilR * std::sin(a), -length * 0.5f + t * length);
        Vec3 tan = normalize(Vec3(-coilR * std::sin(a) * turns * 2 * PI, coilR * std::cos(a) * turns * 2 * PI, length));
        Vec3 nrm = normalize(Vec3(std::cos(a), std::sin(a), 0));
        Vec3 bin = normalize(cross(tan, nrm));
        for (int j = 0; j < tube; ++j) {
            float p = 2.0f * PI * j / tube;
            Vec3 off = (nrm * std::cos(p) + bin * std::sin(p));
            v.push_back({c + off * wireR, off, {t, (float)j / tube}});
        }
    }
    for (int i = 0; i < steps; ++i)
        for (int j = 0; j < tube; ++j) {
            int nj = (j + 1) % tube;
            GLuint a = base + i * tube + j, b = base + (i + 1) * tube + j;
            GLuint c = base + i * tube + nj, d = base + (i + 1) * tube + nj;
            idx.insert(idx.end(), {a, b, c, c, b, d});
        }
}

// -----------------------------------------------------------------------------
// Terrain: a 1 x 1 grid in XZ (scaled later to world size), height from heightFn
// -----------------------------------------------------------------------------
static const float TERRAIN_SIZE = 300.0f;
static const int TERRAIN_RES = 150;

static void genTerrain(std::vector<Vertex>& v, std::vector<GLuint>& idx, float (*heightFn)(float, float)) {
    int n = TERRAIN_RES;
    float step = TERRAIN_SIZE / n;
    auto H = [&](float x, float z) { return heightFn ? heightFn(x, z) : 0.0f; };
    for (int iz = 0; iz <= n; ++iz)
        for (int ix = 0; ix <= n; ++ix) {
            float x = -TERRAIN_SIZE * 0.5f + ix * step, z = -TERRAIN_SIZE * 0.5f + iz * step;
            float y = H(x, z);
            Vec3 nrm = normalize(Vec3(H(x - step, z) - H(x + step, z), 2.0f * step, H(x, z - step) - H(x, z + step)));
            v.push_back({{x, y, z}, nrm, {x, z}});
        }
    for (int iz = 0; iz < n; ++iz)
        for (int ix = 0; ix < n; ++ix) {
            GLuint a = iz * (n + 1) + ix, b = a + 1, c = a + n + 1, d = c + 1;
            idx.insert(idx.end(), {a, c, b, b, c, d});
        }
}

// -----------------------------------------------------------------------------
static Mesh upload(const std::vector<Vertex>& v, const std::vector<GLuint>& i) {
    Mesh m;
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, i.size() * sizeof(GLuint), i.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    m.indexCount = (GLsizei)i.size();
    // bounding sphere from vertices
    Vec3 mn(1e9f), mx(-1e9f);
    for (auto& vv : v) {
        mn = Vec3(std::min(mn.x, vv.position.x), std::min(mn.y, vv.position.y), std::min(mn.z, vv.position.z));
        mx = Vec3(std::max(mx.x, vv.position.x), std::max(mx.y, vv.position.y), std::max(mx.z, vv.position.z));
    }
    m.boundCenter = (mn + mx) * 0.5f;
    m.boundRadius = (mx - mn).length() * 0.5f;
    return m;
}

void createAllShapes(Mesh meshes[M_COUNT], float (*heightFn)(float, float)) {
    for (int id = 0; id < M_COUNT; ++id) {
        std::vector<Vertex> v;
        std::vector<GLuint> i;
        switch (id) {
            case M_CUBE: genCube(v, i); break;
            case M_CYLINDER: genCylinder(v, i, 24); break;
            case M_CYLINDER_LOW: genCylinder(v, i, 8); break;
            case M_SPHERE: genSphere(v, i, 14, 20); break;
            case M_SPHERE_LOW: genSphere(v, i, 5, 8); break;
            case M_CONE: genCylinder(v, i, 20, 0.0f); break;
            case M_TCONE: genCylinder(v, i, 20, 0.35f); break;
            case M_TORUS: genTorus(v, i, 0.4f, 0.1f, 28, 10); break;
            case M_HEMISPHERE: genSphere(v, i, 8, 20, true); break;
            case M_WEDGE: genWedge(v, i); break;
            case M_FRUSTUM_BOX: genFrustumBox(v, i); break;
            case M_QUAD: addQuad(v, i, {-0.5f, -0.5f, 0}, {0.5f, -0.5f, 0}, {0.5f, 0.5f, 0}, {-0.5f, 0.5f, 0}, {0, 0, 1}); break;
            case M_CROSS: genCross(v, i); break;
            case M_LEAVES: genLeaves(v, i); break;
            case M_COIL: genCoil(v, i, 5, 20, 0.055f, 0.0045f, 0.30f); break;
            case M_TERRAIN: genTerrain(v, i, heightFn); break;
        }
        meshes[id] = upload(v, i);
    }
}

void destroyAllShapes(Mesh meshes[M_COUNT]) {
    for (int id = 0; id < M_COUNT; ++id) {
        if (meshes[id].vbo) glDeleteBuffers(1, &meshes[id].vbo);
        if (meshes[id].ebo) glDeleteBuffers(1, &meshes[id].ebo);
        meshes[id] = Mesh();
    }
}

const char* meshName(MeshId id) {
    static const char* names[M_COUNT] = {"Cube", "Cylinder", "Cylinder (low)", "Sphere", "Sphere (low)", "Cone",
                                         "Truncated cone", "Torus", "Hemisphere", "Wedge", "Tapered box", "Quad",
                                         "Cross card", "Leaf card", "Coil", "Terrain grid"};
    return names[id];
}
