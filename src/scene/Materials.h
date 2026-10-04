// =============================================================================
//  Materials.h  -  Shared material palette (colour + texture + shininess)
// =============================================================================
#pragma once
#include "../gfx/Draw.h"

namespace Mat {
inline Material plastic(const Vec3& c) { return Material(c, 0.35f, 48.0f); }
inline Material matte(const Vec3& c) { return Material(c, 0.05f, 8.0f); }
inline Material paint(const Vec3& c) { return Material(c, 0.55f, 64.0f).tx(TEX_METAL, 1.0f); }
inline Material carPaint(const Vec3& c) { return Material(c, 0.9f, 96.0f); }
inline Material chrome() { return Material(Vec3(0.82f, 0.84f, 0.88f), 0.95f, 128.0f).tx(TEX_METAL, 2.0f); }
inline Material steel() { return Material(Vec3(0.62f, 0.64f, 0.68f), 0.6f, 64.0f).tx(TEX_METAL, 1.5f); }
inline Material darkMetal() { return Material(Vec3(0.16f, 0.17f, 0.19f), 0.5f, 48.0f).tx(TEX_METAL, 1.5f); }
inline Material rubber() { return Material(Vec3(0.05f, 0.05f, 0.055f), 0.08f, 8.0f); }
inline Material black() { return Material(Vec3(0.03f, 0.03f, 0.035f), 0.4f, 64.0f); }
inline Material wood(const Vec3& c = Vec3(0.55f, 0.36f, 0.20f), float s = 1.2f) { return Material(c, 0.2f, 24.0f).tx(TEX_WOOD, s); }
inline Material fabric(const Vec3& c, float s = 4.0f) { return Material(c, 0.04f, 6.0f).tx(TEX_FABRIC, s); }
inline Material leather(const Vec3& c) { return Material(c, 0.3f, 24.0f).tx(TEX_LEATHER, 2.0f); }
inline Material plaster(const Vec3& c) { return Material(c, 0.04f, 8.0f).tx(TEX_PLASTER, 0.5f); }
inline Material brick(const Vec3& c = Vec3(0.62f, 0.32f, 0.24f)) { return Material(c, 0.05f, 8.0f).tx(TEX_BRICK, 0.6f); }
inline Material concrete(const Vec3& c = Vec3(0.66f, 0.66f, 0.64f), float s = 0.25f) { return Material(c, 0.05f, 8.0f).tx(TEX_CONCRETE, s); }
inline Material marble(const Vec3& c = Vec3(0.92f, 0.92f, 0.90f)) { return Material(c, 0.6f, 96.0f).tx(TEX_MARBLE, 0.6f); }
inline Material glass(float alpha = 0.22f, const Vec3& tint = Vec3(0.75f, 0.88f, 0.95f)) {
    return Material(tint, 1.0f, 160.0f).a(alpha).fl(MF_NO_SHADOW);
}
// Emissive surface (screens, bulbs, LEDs): visible at night, stays bright
inline Material glow(const Vec3& c, float strength = 1.0f) {
    Material m(c, 0.0f, 1.0f);
    m.emission = c * strength;
    m.flags |= MF_NO_SHADOW;
    return m;
}
inline Material bark(const Vec3& c = Vec3(0.33f, 0.24f, 0.17f)) { return Material(c, 0.03f, 6.0f).tx(TEX_BARK, 1.2f); }
inline Material leafCard(const Vec3& c, int layer = TEX_LEAVES) {
    return Material(c, 0.12f, 16.0f).tx(layer).fl(MF_MESH_UV | MF_CUTOUT | MF_FOLIAGE | MF_WIND);
}
}  // namespace Mat
