#include "Collision.h"
#include <cmath>

void CollisionWorld::addBox(float x0, float z0, float x1, float z1, float y0, float y1, const bool* enabled) {
    Obstacle o;
    o.cx = (x0 + x1) * 0.5f;
    o.cz = (z0 + z1) * 0.5f;
    o.hx = std::fabs(x1 - x0) * 0.5f;
    o.hz = std::fabs(z1 - z0) * 0.5f;
    o.yaw = 0.0f;
    o.y0 = y0;
    o.y1 = y1;
    o.enabled = enabled;
    m_static.push_back(o);
}

void CollisionWorld::addOriented(const Vec3& c, float hx, float hz, float yawDeg, float y0, float y1, bool dynamic) {
    Obstacle o{c.x, c.z, hx, hz, toRadians(yawDeg), y0, y1, nullptr};
    (dynamic ? m_dynamic : m_static).push_back(o);
}

void CollisionWorld::addCircle(float x, float z, float r, bool dynamic) {
    (dynamic ? m_dynCircles : m_circles).push_back({x, z, r});
}

// Circle vs oriented box: transform the circle centre into the box frame,
// find the closest point on the box and push the circle away from it.
bool CollisionWorld::pushOut(const Obstacle& o, float& x, float& z, float r) const {
    if (o.enabled && !*o.enabled) return false;
    float c = std::cos(o.yaw), s = std::sin(o.yaw);
    float dx = x - o.cx, dz = z - o.cz;
    // world -> local (rotation by -yaw about Y). Our yaw rotates local +Z towards +X.
    float lx = dx * c - dz * s;
    float lz = dx * s + dz * c;
    float qx = clampf(lx, -o.hx, o.hx), qz = clampf(lz, -o.hz, o.hz);
    float ex = lx - qx, ez = lz - qz;
    float d2 = ex * ex + ez * ez;
    if (d2 >= r * r) return false;
    float nx, nz, pen;
    if (d2 > 1e-8f) {
        float d = std::sqrt(d2);
        nx = ex / d; nz = ez / d;
        pen = r - d;
    } else {
        // centre inside the box: push out along the shallowest axis
        float px = o.hx - std::fabs(lx), pz = o.hz - std::fabs(lz);
        if (px < pz) { nx = lx >= 0 ? 1.0f : -1.0f; nz = 0; pen = px + r; }
        else { nx = 0; nz = lz >= 0 ? 1.0f : -1.0f; pen = pz + r; }
    }
    lx += nx * pen;
    lz += nz * pen;
    // local -> world
    x = o.cx + lx * c + lz * s;
    z = o.cz - lx * s + lz * c;
    return true;
}

Vec3 CollisionWorld::resolve(const Vec3& p, float r, float feetY, float headY, const void*) const {
    float x = p.x, z = p.z;
    for (int iter = 0; iter < 3; ++iter) {
        bool any = false;
        for (const auto& o : m_static)
            if (o.y1 > feetY + 0.25f && o.y0 < headY) any |= pushOut(o, x, z, r);
        for (const auto& o : m_dynamic)
            if (o.y1 > feetY + 0.25f && o.y0 < headY) any |= pushOut(o, x, z, r);
        auto circ = [&](const CircleObstacle& ci) {
            float dx = x - ci.x, dz = z - ci.z, d = std::sqrt(dx * dx + dz * dz), m = r + ci.r;
            if (d < m && d > 1e-5f) { x = ci.x + dx / d * m; z = ci.z + dz / d * m; any = true; }
        };
        for (const auto& ci : m_circles) circ(ci);
        for (const auto& ci : m_dynCircles) circ(ci);
        if (!any) break;
    }
    return Vec3(x, p.y, z);
}

bool CollisionWorld::isFree(const Vec3& p, float r) const {
    Vec3 q = resolve(p, r);
    return std::fabs(q.x - p.x) < 1e-4f && std::fabs(q.z - p.z) < 1e-4f;
}

float CollisionWorld::raycast(const Vec3& o, const Vec3& d, float maxT) const {
    float best = maxT;
    auto test = [&](const Obstacle& ob) {
        if (ob.enabled && !*ob.enabled) return;
        float c = std::cos(ob.yaw), s = std::sin(ob.yaw);
        float ox = o.x - ob.cx, oz = o.z - ob.cz;
        Vec3 lo(ox * c - oz * s, o.y, ox * s + oz * c);
        Vec3 ld(d.x * c - d.z * s, d.y, d.x * s + d.z * c);
        Vec3 mn(-ob.hx, ob.y0, -ob.hz), mx(ob.hx, ob.y1, ob.hz);
        float t0 = 0.0f, t1 = best;
        const float* po = &lo.x; const float* pd = &ld.x;
        const float* pmn = &mn.x; const float* pmx = &mx.x;
        for (int a = 0; a < 3; ++a) {
            if (std::fabs(pd[a]) < 1e-6f) {
                if (po[a] < pmn[a] || po[a] > pmx[a]) return;
            } else {
                float ta = (pmn[a] - po[a]) / pd[a], tb = (pmx[a] - po[a]) / pd[a];
                if (ta > tb) std::swap(ta, tb);
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb);
                if (t0 > t1) return;
            }
        }
        if (t0 < best) best = t0;
    };
    for (const auto& ob : m_static) test(ob);
    return best;
}
