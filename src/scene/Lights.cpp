#include "Lights.h"
#include <algorithm>
#include <cmath>

void LightManager::updateEffective(bool nightLamps) {
    for (int g = 0; g < LG_COUNT; ++g) m_effective[g] = groupOn[g];
    if (outdoorAuto) m_effective[LG_OUTDOOR] = groupOn[LG_OUTDOOR] && nightLamps;
}

int LightManager::regionOf(const Vec3& p) const {
    bool inside = p.x > indoorBox.x && p.x < indoorBox.z && p.z > indoorBox.y && p.z < indoorBox.w && p.y < 3.7f;
    if (!inside) return 3;
    return p.x < splitX0 ? 0 : (p.x < splitX1 ? 1 : 2);
}

void LightManager::collect(FrameInfo& f, const Vec3& cam) {
    struct Cand { const LightDef* l; float score; };
    std::vector<Cand> pts, spots;
    Frustum fr;
    fr.fromMatrix(f.proj * f.view);
    auto consider = [&](const LightDef& l) {
        if (!isOn(l.group)) return;
        float d = (l.pos - cam).length();
        float score = d - l.range * 0.6f;   // big lights win a little
        if (d > l.range + 45.0f) return;    // too far away to matter (fog)
        // skip lights whose area of effect is completely outside the view
        Vec3 r(l.range);
        if (!fr.boxVisible(l.pos - r, l.pos + r)) return;
        (l.kind == LIGHT_POINT ? pts : spots).push_back({&l, score});
    };
    for (const auto& l : m_static) consider(l);
    for (const auto& l : m_frame) consider(l);
    auto byScore = [](const Cand& a, const Cand& b) { return a.score < b.score; };
    std::sort(pts.begin(), pts.end(), byScore);
    std::sort(spots.begin(), spots.end(), byScore);

    if (pts.size() > (size_t)Renderer::MAX_POINT) pts.resize(Renderer::MAX_POINT);
    if (spots.size() > (size_t)Renderer::MAX_SPOT) spots.resize(Renderer::MAX_SPOT);
    // sort the chosen lights by region so every pixel loops only over its own room
    auto byRegion = [&](const Cand& a, const Cand& b) { return regionOf(a.l->pos) < regionOf(b.l->pos); };
    std::stable_sort(pts.begin(), pts.end(), byRegion);
    std::stable_sort(spots.begin(), spots.end(), byRegion);
    f.points.clear();
    f.spots.clear();
    for (int r = 0; r <= FrameInfo::REGIONS; ++r) f.pointStart[r] = f.spotStart[r] = 0;
    for (const auto& c : pts) {
        f.points.push_back({c.l->pos, c.l->color * c.l->intensity, c.l->range});
        f.pointStart[regionOf(c.l->pos) + 1]++;
    }
    for (const auto& c : spots) {
        const LightDef* l = c.l;
        f.spots.push_back({l->pos, l->dir, l->color * l->intensity, l->range,
                           std::cos(toRadians(l->innerDeg)), std::cos(toRadians(l->outerDeg))});
        f.spotStart[regionOf(l->pos) + 1]++;
    }
    for (int r = 1; r <= FrameInfo::REGIONS; ++r) { f.pointStart[r] += f.pointStart[r - 1]; f.spotStart[r] += f.spotStart[r - 1]; }
    f.regionSplit = Vec4(splitX0, splitX1, 0, 0);
}
