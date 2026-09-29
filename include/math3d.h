#ifndef MATH3D_H
#define MATH3D_H

#include <cmath>
#include <cstring>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

inline float toRadians(float degrees) {
    return degrees * (static_cast<float>(M_PI) / 180.0f);
}

struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}
};

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float length() const { return std::sqrt(x * x + y * y + z * z); }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

inline Vec3 normalize(const Vec3& v) {
    float len = v.length();
    if (len > 1e-6f) {
        return v / len;
    }
    return Vec3(0, 0, 0);
}

struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(1) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}
};

// Column-major 4x4 matrix
struct Mat4 {
    float m[16];

    Mat4() {
        identity();
    }

    void identity() {
        std::memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static Mat4 Identity() {
        Mat4 res;
        return res;
    }

    float& operator()(int col, int row) {
        return m[col * 4 + row];
    }

    const float& operator()(int col, int row) const {
        return m[col * 4 + row];
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 res;
        std::memset(res.m, 0, sizeof(res.m));
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                for (int k = 0; k < 4; ++k) {
                    res.m[c * 4 + r] += m[k * 4 + r] * o.m[c * 4 + k];
                }
            }
        }
        return res;
    }

    Vec4 operator*(const Vec4& v) const {
        return Vec4(
            m[0]*v.x + m[4]*v.y + m[8]*v.z  + m[12]*v.w,
            m[1]*v.x + m[5]*v.y + m[9]*v.z  + m[13]*v.w,
            m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
            m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w
        );
    }
};

inline Mat4 translate(const Mat4& mat, const Vec3& v) {
    Mat4 t;
    t.m[12] = v.x;
    t.m[13] = v.y;
    t.m[14] = v.z;
    return mat * t;
}

inline Mat4 scale(const Mat4& mat, const Vec3& s) {
    Mat4 sc;
    sc.m[0]  = s.x;
    sc.m[5]  = s.y;
    sc.m[10] = s.z;
    return mat * sc;
}

inline Mat4 rotate(const Mat4& mat, float angleRad, const Vec3& axis) {
    Vec3 a = normalize(axis);
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    float nc = 1.0f - c;

    Mat4 rot;
    rot.m[0] = c + a.x * a.x * nc;
    rot.m[1] = a.y * a.x * nc + a.z * s;
    rot.m[2] = a.z * a.x * nc - a.y * s;
    rot.m[3] = 0.0f;

    rot.m[4] = a.x * a.y * nc - a.z * s;
    rot.m[5] = c + a.y * a.y * nc;
    rot.m[6] = a.z * a.y * nc + a.x * s;
    rot.m[7] = 0.0f;

    rot.m[8]  = a.x * a.z * nc + a.y * s;
    rot.m[9]  = a.y * a.z * nc - a.x * s;
    rot.m[10] = c + a.z * a.z * nc;
    rot.m[11] = 0.0f;

    rot.m[12] = 0.0f;
    rot.m[13] = 0.0f;
    rot.m[14] = 0.0f;
    rot.m[15] = 1.0f;

    return mat * rot;
}

inline Mat4 perspective(float fovRad, float aspect, float zNear, float zFar) {
    Mat4 res;
    std::memset(res.m, 0, sizeof(res.m));
    float tanHalfFov = std::tan(fovRad / 2.0f);

    res.m[0]  = 1.0f / (aspect * tanHalfFov);
    res.m[5]  = 1.0f / tanHalfFov;
    res.m[10] = -(zFar + zNear) / (zFar - zNear);
    res.m[11] = -1.0f;
    res.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
    res.m[15] = 0.0f;

    return res;
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 res;
    res.m[0] = s.x;
    res.m[4] = s.y;
    res.m[8] = s.z;
    res.m[12] = -dot(s, eye);

    res.m[1] = u.x;
    res.m[5] = u.y;
    res.m[9] = u.z;
    res.m[13] = -dot(u, eye);

    res.m[2] = -f.x;
    res.m[6] = -f.y;
    res.m[10] = -f.z;
    res.m[14] = dot(f, eye);

    res.m[3] = 0.0f;
    res.m[7] = 0.0f;
    res.m[11] = 0.0f;
    res.m[15] = 1.0f;

    return res;
}

#endif // MATH3D_H
