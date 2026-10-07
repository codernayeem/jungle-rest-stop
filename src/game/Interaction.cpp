#include "Interaction.h"
#include "Audio.h"
#include <cmath>

void InteractionSystem::add(int id, const Mat4& model, const Vec3& half, const std::string& prompt,
                            std::function<void()> onUse) {
    m_items.push_back({id, model, affineInverse(model), half, prompt, std::move(onUse)});
}

// Ray against box [-half, +half] in the box's own coordinate system
static bool rayBox(const Vec3& o, const Vec3& d, const Vec3& h, float& tHit) {
    float t0 = 0.0f, t1 = 1e9f;
    const float* po = &o.x; const float* pd = &d.x; const float* ph = &h.x;
    for (int a = 0; a < 3; ++a) {
        if (std::fabs(pd[a]) < 1e-7f) {
            if (po[a] < -ph[a] || po[a] > ph[a]) return false;
        } else {
            float ta = (-ph[a] - po[a]) / pd[a], tb = (ph[a] - po[a]) / pd[a];
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) return false;
        }
    }
    tHit = t0;
    return true;
}

void InteractionSystem::update(const Vec3& origin, const Vec3& dir, float maxDist, bool enabled) {
    m_hoveredId = -1;
    m_prompt.clear();
    m_hoveredAction = nullptr;
    if (!enabled) return;
    float best = maxDist;
    for (auto& it : m_items) {
        Vec3 lo = it.inv.transformPoint(origin);
        Vec3 ld = it.inv.transformDir(dir);   // not normalised: t stays in world units
        float t;
        if (rayBox(lo, ld, it.half, t) && t < best) {
            best = t;
            m_hoveredId = it.id;
            m_prompt = it.prompt;
            m_hoveredAction = it.onUse;
        }
    }
}

void InteractionSystem::useHovered() {
    if (m_hoveredAction) {
        Audio::play(SND_CLICK, 0.8f);
        m_hoveredAction();
    }
}
