// =============================================================================
//  Collision.h  -  Simple 2.5D collision world.
//  Obstacles are oriented boxes on the ground (with a height range) and
//  circles (tree trunks). People are circles that get pushed out of obstacles.
// =============================================================================
#pragma once
#include <vector>
#include "../core/Math3D.h"

struct Obstacle {
    float cx, cz;        // centre
    float hx, hz;        // half extents (local)
    float yaw;           // rotation (radians)
    float y0, y1;        // vertical range
    const bool* enabled; // optional switch (e.g. a door that can open)
};

struct CircleObstacle { float x, z, r; };

class CollisionWorld {
public:
    void clearStatic() { m_static.clear(); m_circles.clear(); }
    void clearDynamic() { m_dynamic.clear(); m_dynCircles.clear(); }

    // axis-aligned box from min/max corners
    void addBox(float x0, float z0, float x1, float z1, float y0 = 0.0f, float y1 = 3.0f, const bool* enabled = nullptr);
    // oriented box (centre, half sizes, yaw in degrees)
    void addOriented(const Vec3& c, float hx, float hz, float yawDeg, float y0 = 0.0f, float y1 = 2.0f, bool dynamic = false);
    void addCircle(float x, float z, float r, bool dynamic = false);

    // Push a circle (x,z,radius) out of all obstacles. Returns corrected position.
    Vec3 resolve(const Vec3& p, float radius, float feetY = 0.0f, float headY = 1.8f, const void* ignore = nullptr) const;
    // Is this circle free of obstacles?
    bool isFree(const Vec3& p, float radius) const;
    // Ray vs obstacles (for the third-person camera). Returns hit distance or maxT.
    float raycast(const Vec3& o, const Vec3& d, float maxT) const;

    size_t staticCount() const { return m_static.size() + m_circles.size(); }

private:
    bool pushOut(const Obstacle& o, float& x, float& z, float r) const;
    std::vector<Obstacle> m_static, m_dynamic;
    std::vector<CircleObstacle> m_circles, m_dynCircles;
};
