// =============================================================================
//  Draw.h  -  Material description + hierarchical "Node" helper.
//
//  Every object in the scene is described as a tree of Nodes. A Node holds a
//  transformation matrix; child nodes are created with at()/rot()/scl() and
//  inherit the parent's matrix (hierarchical modelling). part() emits one
//  basic shape:  final = parent * T(pos) * R(rot) * S(size)
// =============================================================================
#pragma once
#include "../core/Math3D.h"
#include "Shapes.h"
#include "Textures.h"

// Material flags (some used by shaders, some by the CPU)
enum MaterialFlags {
    MF_MESH_UV   = 1,    // use mesh UVs instead of object-space box mapping
    MF_WIND      = 2,    // vertex sway (grass, leaves)
    MF_UNLIT     = 4,    // no lighting, only colour + emission
    MF_FOLIAGE   = 8,    // double sided + soft "wrap" lighting
    MF_CUTOUT    = 16,   // alpha-tested texture (leaves, grass)
    MF_TERRAIN   = 32,   // ground: blends grass / forest floor
    MF_NO_SHADOW = 64    // does not cast shadows (CPU side only)
};

struct Material {
    Vec3 color = Vec3(0.8f, 0.8f, 0.8f);
    float alpha = 1.0f;
    Vec3 emission = Vec3(0, 0, 0);
    int tex = TEX_NONE;
    float texScale = 1.0f;     // texture repeats per metre (object-space mapping)
    float spec = 0.15f;        // specular strength
    float shininess = 24.0f;
    int flags = 0;
    int lod = 0;               // 0 = always drawn, 1 = up to ~85 m, 2 = up to ~45 m

    Material() = default;
    Material(const Vec3& c, float sp = 0.15f, float sh = 24.0f) : color(c), spec(sp), shininess(sh) {}

    // fluent modifiers  e.g.  Material(red).tx(TEX_FABRIC, 2).em(glow)
    Material& tx(int layer, float scale = 1.0f) { tex = layer; texScale = scale; return *this; }
    Material& em(const Vec3& e) { emission = e; return *this; }
    Material& a(float al) { alpha = al; return *this; }
    Material& fl(int f) { flags |= f; return *this; }
    Material& sp(float s, float sh) { spec = s; shininess = sh; return *this; }
    Material& lv(int l) { lod = l; return *this; }
};

// Anything that can receive basic-shape instances (static world, per-frame list ...)
struct DrawSink {
    virtual void add(MeshId mesh, const Mat4& model, const Material& mat) = 0;
    virtual ~DrawSink() = default;
};

// A transform node in the object hierarchy
struct Node {
    DrawSink* sink;
    Mat4 M;

    Node(DrawSink& s, const Mat4& m = Mat4()) : sink(&s), M(m) {}

    // ---- create child nodes ----
    Node at(const Vec3& p) const { return Node(*sink, translate(M, p)); }
    Node at(const Vec3& p, const Vec3& rotDeg) const { return Node(*sink, rotateYXZ(translate(M, p), rotDeg)); }
    Node rot(float deg, const Vec3& axis) const { return Node(*sink, rotate(M, toRadians(deg), axis)); }
    Node rotY(float deg) const { return rot(deg, Vec3(0, 1, 0)); }
    Node rotX(float deg) const { return rot(deg, Vec3(1, 0, 0)); }
    Node rotZ(float deg) const { return rot(deg, Vec3(0, 0, 1)); }
    Node scl(const Vec3& s) const { return Node(*sink, scale(M, s)); }
    Vec3 worldPos(const Vec3& local = Vec3()) const { return M.transformPoint(local); }

    // ---- emit a basic shape: parent * T(pos) * R(rot) * S(size) ----
    void part(MeshId mesh, const Vec3& pos, const Vec3& size, const Material& m, const Vec3& rotDeg = Vec3()) const {
        sink->add(mesh, scale(rotateYXZ(translate(M, pos), rotDeg), size), m);
    }
    void box(const Vec3& p, const Vec3& s, const Material& m, const Vec3& r = Vec3()) const { part(M_CUBE, p, s, m, r); }
    void cyl(const Vec3& p, const Vec3& s, const Material& m, const Vec3& r = Vec3()) const { part(M_CYLINDER, p, s, m, r); }
    void sph(const Vec3& p, const Vec3& s, const Material& m, const Vec3& r = Vec3()) const { part(M_SPHERE, p, s, m, r); }
    void cone(const Vec3& p, const Vec3& s, const Material& m, const Vec3& r = Vec3()) const { part(M_CONE, p, s, m, r); }
    void tcone(const Vec3& p, const Vec3& s, const Material& m, const Vec3& r = Vec3()) const { part(M_TCONE, p, s, m, r); }
    void torus(const Vec3& p, const Vec3& s, const Material& m, const Vec3& r = Vec3()) const { part(M_TORUS, p, s, m, r); }
    void quad(const Vec3& p, const Vec3& s, const Material& m, const Vec3& r = Vec3()) const { part(M_QUAD, p, s, m, r); }

    // A thin parallelogram plate with corners a, b, b+d-a, d (e.g. a car window).
    // The unit cube is mapped by the columns (b-a), (d-a), normal*thick, so the
    // corners land exactly on the given points (no rounding to a rectangle).
    void panel(const Vec3& a, const Vec3& b, const Vec3& d, float thick, const Material& m) const {
        Vec3 X = b - a, Y = d - a, Z = normalize(cross(X, Y)) * thick;
        sink->add(M_CUBE, M * frame(X, Y, Z, a + (X + Y) * 0.5f), m);
    }

    // A thin triangular plate with corners p0, p1, p2 (a wedge mapped onto them).
    // Wedge corners: bottom-back -> p0, bottom-front -> p1, top-back -> p2.
    void tri(const Vec3& p0, const Vec3& p1, const Vec3& p2, float thick, const Material& m) const {
        Vec3 Zc = p1 - p0, Yc = p2 - p0, Xc = normalize(cross(Yc, Zc)) * thick;
        sink->add(M_WEDGE, M * frame(Xc, Yc, Zc, p0 + (Yc + Zc) * 0.5f), m);
    }

    // matrix whose columns are the three axes and the origin
    static Mat4 frame(const Vec3& x, const Vec3& y, const Vec3& z, const Vec3& o) {
        Mat4 L;
        L.m[0] = x.x; L.m[1] = x.y; L.m[2] = x.z;
        L.m[4] = y.x; L.m[5] = y.y; L.m[6] = y.z;
        L.m[8] = z.x; L.m[9] = z.y; L.m[10] = z.z;
        L.m[12] = o.x; L.m[13] = o.y; L.m[14] = o.z;
        return L;
    }

    // A shape stretched between two points a and b (its local Y axis is aligned to b-a)
    void beam(MeshId mesh, const Vec3& a, const Vec3& b, float thick, const Material& m) const {
        Vec3 d = b - a;
        float len = d.length();
        if (len < 1e-5f) return;
        Vec3 y = d / len;
        Vec3 axis = cross(Vec3(0, 1, 0), y);
        float s = axis.length(), c = y.y;
        Mat4 R;
        if (s > 1e-5f) R = rotationMatrix(std::atan2(s, c), axis / s);
        else if (c < 0) R = rotationMatrix(PI, Vec3(1, 0, 0));
        sink->add(mesh, scale(translate(M, (a + b) * 0.5f) * R, Vec3(thick, len, thick)), m);
    }
};

// Wraps another sink and only lets the first `limit` parts through.
// Used to render an object "being built" step by step for the report.
struct LimitSink : DrawSink {
    DrawSink* inner;
    int limit, count = 0;
    LimitSink(DrawSink& in, int lim) : inner(&in), limit(lim) {}
    void add(MeshId mesh, const Mat4& model, const Material& mat) override {
        if (count++ < limit) inner->add(mesh, model, mat);
    }
};

// Only counts parts (used to know how many shapes an object is made of)
struct CountSink : DrawSink {
    int count = 0;
    int perMesh[M_COUNT] = {};
    void add(MeshId mesh, const Mat4&, const Material&) override { ++count; ++perMesh[mesh]; }
};
