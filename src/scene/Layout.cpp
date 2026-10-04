#include "Layout.h"
#include <cmath>

float roadCenterZ(float x) {
    float ax = std::fabs(x);
    if (ax < 30.0f) return Layout::AISLE_Z;
    float t = ax - 30.0f;
    float side = x > 0 ? 1.0f : -1.0f;
    return Layout::AISLE_Z + std::sin(t * 0.035f) * 7.0f * side;
}

bool isClearedArea(float x, float z, float m) {
    using namespace Layout;
    // building + sidewalk + lot
    if (x > LOT_X0 - 4.0f - m && x < LOT_X1 + 4.0f + m && z > BLD_Z0 - 4.0f - m && z < LOT_Z1 + 3.0f + m) return true;
    // road corridor
    if (std::fabs(z - roadCenterZ(x)) < 5.5f + m) return true;
    return false;
}

float terrainHeight(float x, float z) {
    // distance outside the cleared core area -> hills grow smoothly
    float dx = std::max(0.0f, std::fabs(x - 1.5f) - 34.0f);
    float dzFront = std::max(0.0f, z - 32.0f);
    float dzBack = std::max(0.0f, -12.0f - z);
    float d = std::sqrt(dx * dx + (dzFront + dzBack) * (dzFront + dzBack));
    float hills = (fbm(x * 0.018f, z * 0.018f, 4, 7) - 0.45f) * 14.0f + fbm(x * 0.07f, z * 0.07f, 2, 9) * 1.2f;
    float k = smoothstepf(0.0f, 30.0f, d);
    // keep the road flat
    float road = std::fabs(z - roadCenterZ(x));
    float roadFlat = smoothstepf(5.0f, 14.0f, road);
    float h = hills * k * roadFlat;
    return std::max(h, -1.5f);
}
