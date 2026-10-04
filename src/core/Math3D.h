// =============================================================================
//  Math3D.h  -  Small self-contained vector / matrix library
//  Column-major 4x4 matrices (same memory layout OpenGL expects).
//  All object transformations in the project (translate, rotate, scale) are
//  built from the functions in this file.
// =============================================================================
#pragma once

#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>

constexpr float PI = 3.14159265358979323846f;

inline float toRadians(float deg) { return deg * (PI / 180.0f); }
inline float toDegrees(float rad) { return rad * (180.0f / PI); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
// Move value towards target by at most maxStep
inline float approach(float v, float target, float maxStep) {
    if (v < target) return std::min(v + maxStep, target);
    return std::max(v - maxStep, target);
}
// Wrap angle (radians) into [-PI, PI]
inline float wrapAngle(float a) {
    while (a > PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}

// -----------------------------------------------------------------------------
struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    float length() const { return std::sqrt(x * x + y * y); }
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit Vec3(float s) : x(s), y(s), z(s) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 normalize(const Vec3& v) {
    float l = v.length();
    return l > 1e-6f ? v / l : Vec3(0, 0, 0);
}
inline Vec3 mix(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
inline float distXZ(const Vec3& a, const Vec3& b) { return std::hypot(a.x - b.x, a.z - b.z); }

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 1;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    Vec3 xyz() const { return {x, y, z}; }
};

// -----------------------------------------------------------------------------
// Column-major 4x4 matrix: element (col, row) is m[col*4 + row]
// -----------------------------------------------------------------------------
struct Mat4 {
    float m[16];
    Mat4() { identity(); }
    void identity() {
        std::memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
    static Mat4 Identity() { return Mat4(); }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + row] * o.m[c * 4 + k];
                r.m[c * 4 + row] = s;
            }
        return r;
    }
    Vec4 operator*(const Vec4& v) const {
        return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
                m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
                m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
                m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w};
    }
    Vec3 transformPoint(const Vec3& p) const {
        return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
    }
    Vec3 transformDir(const Vec3& d) const {
        return {m[0] * d.x + m[4] * d.y + m[8] * d.z,
                m[1] * d.x + m[5] * d.y + m[9] * d.z,
                m[2] * d.x + m[6] * d.y + m[10] * d.z};
    }
    Vec3 translation() const { return {m[12], m[13], m[14]}; }
    float maxAxisScale() const {
        float sx = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
        float sy = std::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
        float sz = std::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
        return std::max(sx, std::max(sy, sz));
    }
};

// --- Basic transformation matrices -------------------------------------------
inline Mat4 translationMatrix(const Vec3& v) {
    Mat4 t;
    t.m[12] = v.x; t.m[13] = v.y; t.m[14] = v.z;
    return t;
}
inline Mat4 scaleMatrix(const Vec3& s) {
    Mat4 r;
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
    return r;
}
// Rodrigues rotation about an arbitrary axis (angle in radians)
inline Mat4 rotationMatrix(float angle, const Vec3& axis) {
    Vec3 a = normalize(axis);
    float c = std::cos(angle), s = std::sin(angle), nc = 1.0f - c;
    Mat4 r;
    r.m[0] = c + a.x * a.x * nc;       r.m[1] = a.y * a.x * nc + a.z * s; r.m[2] = a.z * a.x * nc - a.y * s;
    r.m[4] = a.x * a.y * nc - a.z * s; r.m[5] = c + a.y * a.y * nc;       r.m[6] = a.z * a.y * nc + a.x * s;
    r.m[8] = a.x * a.z * nc + a.y * s; r.m[9] = a.y * a.z * nc - a.x * s; r.m[10] = c + a.z * a.z * nc;
    return r;
}
// Post-multiplying helpers: M' = M * T  (child transform applied in parent space)
inline Mat4 translate(const Mat4& m, const Vec3& v) { return m * translationMatrix(v); }
inline Mat4 scale(const Mat4& m, const Vec3& s) { return m * scaleMatrix(s); }
inline Mat4 rotate(const Mat4& m, float angleRad, const Vec3& axis) { return m * rotationMatrix(angleRad, axis); }
// Euler rotation in degrees, applied Y (yaw) then X (pitch) then Z (roll)
inline Mat4 rotateYXZ(const Mat4& m, const Vec3& deg) {
    Mat4 r = m;
    if (deg.y != 0.0f) r = rotate(r, toRadians(deg.y), Vec3(0, 1, 0));
    if (deg.x != 0.0f) r = rotate(r, toRadians(deg.x), Vec3(1, 0, 0));
    if (deg.z != 0.0f) r = rotate(r, toRadians(deg.z), Vec3(0, 0, 1));
    return r;
}

// --- Camera / projection matrices --------------------------------------------
inline Mat4 perspective(float fovRad, float aspect, float zNear, float zFar) {
    Mat4 r;
    std::memset(r.m, 0, sizeof(r.m));
    float t = std::tan(fovRad * 0.5f);
    r.m[0] = 1.0f / (aspect * t);
    r.m[5] = 1.0f / t;
    r.m[10] = -(zFar + zNear) / (zFar - zNear);
    r.m[11] = -1.0f;
    r.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
    return r;
}
inline Mat4 ortho(float l, float r, float b, float t, float n, float f) {
    Mat4 o;
    o.m[0] = 2.0f / (r - l);
    o.m[5] = 2.0f / (t - b);
    o.m[10] = -2.0f / (f - n);
    o.m[12] = -(r + l) / (r - l);
    o.m[13] = -(t + b) / (t - b);
    o.m[14] = -(f + n) / (f - n);
    return o;
}
inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);
    Mat4 r;
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;  r.m[12] = -dot(s, eye);
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;  r.m[13] = -dot(u, eye);
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = dot(f, eye);
    return r;
}

// Inverse of an affine matrix (rotation/scale/translation, no projection)
inline Mat4 affineInverse(const Mat4& a) {
    // Upper-left 3x3 inverse via cofactors
    const float* m = a.m;
    float a00 = m[0], a01 = m[4], a02 = m[8];
    float a10 = m[1], a11 = m[5], a12 = m[9];
    float a20 = m[2], a21 = m[6], a22 = m[10];
    float c00 = a11 * a22 - a12 * a21, c01 = a12 * a20 - a10 * a22, c02 = a10 * a21 - a11 * a20;
    float det = a00 * c00 + a01 * c01 + a02 * c02;
    float id = std::fabs(det) > 1e-12f ? 1.0f / det : 0.0f;
    float i00 = c00 * id, i01 = (a02 * a21 - a01 * a22) * id, i02 = (a01 * a12 - a02 * a11) * id;
    float i10 = c01 * id, i11 = (a00 * a22 - a02 * a20) * id, i12 = (a02 * a10 - a00 * a12) * id;
    float i20 = c02 * id, i21 = (a01 * a20 - a00 * a21) * id, i22 = (a00 * a11 - a01 * a10) * id;
    Mat4 r;
    r.m[0] = i00; r.m[4] = i01; r.m[8] = i02;
    r.m[1] = i10; r.m[5] = i11; r.m[9] = i12;
    r.m[2] = i20; r.m[6] = i21; r.m[10] = i22;
    float tx = m[12], ty = m[13], tz = m[14];
    r.m[12] = -(i00 * tx + i01 * ty + i02 * tz);
    r.m[13] = -(i10 * tx + i11 * ty + i12 * tz);
    r.m[14] = -(i20 * tx + i21 * ty + i22 * tz);
    return r;
}

// Normal matrix = transpose(inverse(upper 3x3)), written column-major into out[9]
inline void normalMatrix(const Mat4& a, float out[9]) {
    Mat4 inv = affineInverse(a);
    // transpose of inverse: column c of result = row c of inverse
    out[0] = inv.m[0]; out[1] = inv.m[4]; out[2] = inv.m[8];
    out[3] = inv.m[1]; out[4] = inv.m[5]; out[5] = inv.m[9];
    out[6] = inv.m[2]; out[7] = inv.m[6]; out[8] = inv.m[10];
}

// General 4x4 inverse (used for un-projecting the sky)
inline Mat4 inverse(const Mat4& mat) {
    const float* m = mat.m;
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    Mat4 r;
    if (std::fabs(det) < 1e-12f) return r;
    det = 1.0f / det;
    for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * det;
    return r;
}

// --- View frustum (for culling chunks of the world) --------------------------
struct Frustum {
    Vec4 planes[6];
    void fromMatrix(const Mat4& vp) {
        const float* m = vp.m;
        auto row = [&](int r) { return Vec4(m[r], m[4 + r], m[8 + r], m[12 + r]); };
        Vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
        planes[0] = Vec4(r3.x + r0.x, r3.y + r0.y, r3.z + r0.z, r3.w + r0.w);
        planes[1] = Vec4(r3.x - r0.x, r3.y - r0.y, r3.z - r0.z, r3.w - r0.w);
        planes[2] = Vec4(r3.x + r1.x, r3.y + r1.y, r3.z + r1.z, r3.w + r1.w);
        planes[3] = Vec4(r3.x - r1.x, r3.y - r1.y, r3.z - r1.z, r3.w - r1.w);
        planes[4] = Vec4(r3.x + r2.x, r3.y + r2.y, r3.z + r2.z, r3.w + r2.w);
        planes[5] = Vec4(r3.x - r2.x, r3.y - r2.y, r3.z - r2.z, r3.w - r2.w);
    }
    bool boxVisible(const Vec3& mn, const Vec3& mx) const {
        for (const auto& p : planes) {
            Vec3 v(p.x >= 0 ? mx.x : mn.x, p.y >= 0 ? mx.y : mn.y, p.z >= 0 ? mx.z : mn.z);
            if (p.x * v.x + p.y * v.y + p.z * v.z + p.w < 0.0f) return false;
        }
        return true;
    }
};

// --- Deterministic random numbers (xorshift) ---------------------------------
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 12345u) : s(seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uniform() { return (next() & 0xFFFFFF) / float(0x1000000); }      // [0,1)
    float range(float a, float b) { return a + (b - a) * uniform(); }
    int irange(int a, int b) { return a + int(next() % uint32_t(b - a + 1)); } // [a,b]
    bool chance(float p) { return uniform() < p; }
};

// Cheap hash-based value noise (used for terrain and textures)
inline float hash2(int x, int y, uint32_t seed = 0) {
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFFF) / float(0xFFFFFF);
}
inline float valueNoise(float x, float y, uint32_t seed = 0) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    float a = hash2(xi, yi, seed), b = hash2(xi + 1, yi, seed);
    float c = hash2(xi, yi + 1, seed), d = hash2(xi + 1, yi + 1, seed);
    return lerpf(lerpf(a, b, ux), lerpf(c, d, ux), uy);
}
inline float fbm(float x, float y, int octaves, uint32_t seed = 0) {
    float sum = 0, amp = 0.5f, f = 1.0f, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise(x * f, y * f, seed + i * 17u);
        norm += amp;
        amp *= 0.5f;
        f *= 2.0f;
    }
    return sum / norm;
}
