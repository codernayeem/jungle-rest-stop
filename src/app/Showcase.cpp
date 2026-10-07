// =============================================================================
//  Showcase.cpp  -  Automatic screenshots for the report (main.exe --shots DIR)
//    * scene views (day / night / interiors / machines / basic vs enhanced)
//    * "studio" renders of single objects, also part-by-part build stages
//    * texture swatches and a CSV with how many basic shapes each object uses
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "App.h"
#include "../game/Interaction.h"
#include "../game/Inventory.h"
#include "../game/Messages.h"
#include "../gfx/Screenshot.h"
#include "../gfx/Profiler.h"
#include "../scene/Furniture.h"
#include "../scene/Layout.h"
#include "../scene/Lights.h"
#include "../scene/Materials.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <algorithm>

namespace {
// ---- debug: find boxes whose faces lie in the same plane (z-fighting / flicker) ----
struct BoxRecorder : DrawSink {
    std::vector<std::pair<Mat4, Material>> boxes;
    void add(MeshId m, const Mat4& M, const Material& mat) override {
        if (m == M_CUBE && mat.alpha > 0.99f) boxes.push_back({M, mat});
    }
};
void findCoplanarFaces(const std::vector<std::pair<Mat4, Material>>& boxes, std::ostream& out) {
    struct Face { Vec3 n, p, a, b; float d, ha, hb; int box; };
    std::map<long long, std::vector<Face>> buckets;
    for (int i = 0; i < (int)boxes.size(); ++i) {
        const float* m = boxes[i].first.m;
        Vec3 C[3] = {Vec3(m[0], m[1], m[2]), Vec3(m[4], m[5], m[6]), Vec3(m[8], m[9], m[10])};
        Vec3 o(m[12], m[13], m[14]);
        for (int k = 0; k < 3; ++k) {
            float L = C[k].length();
            if (L < 1e-5f) continue;
            const Vec3& A = C[(k + 1) % 3];
            const Vec3& B = C[(k + 2) % 3];
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                Face f;
                f.n = C[k] * (sgn / L);
                f.p = o + C[k] * (0.5f * sgn);
                f.a = normalize(A); f.ha = A.length() * 0.5f;
                f.b = normalize(B); f.hb = B.length() * 0.5f;
                f.d = dot(f.n, f.p);
                f.box = i;
                long long key = ((long long)std::lround(f.n.x * 50) + 64) * 1000003LL * 131 +
                                ((long long)std::lround(f.n.y * 50) + 64) * 1000003LL + ((long long)std::lround(f.n.z * 50) + 64) * 7919LL +
                                (long long)std::floor(f.d / 0.004f);
                buckets[key].push_back(f);
            }
        }
    }
    struct Hit { float area; Vec3 p; int i, j; Vec3 n; };
    std::vector<Hit> hits;
    for (auto& kv : buckets) {
        for (int dk = 0; dk <= 1; ++dk) {
            auto it = buckets.find(kv.first + dk);
            if (it == buckets.end()) continue;
            const auto& A = kv.second;
            const auto& B = it->second;
            for (size_t x = 0; x < A.size(); ++x)
                for (size_t y = (dk == 0 ? x + 1 : 0); y < B.size(); ++y) {
                    const Face& f = A[x];
                    const Face& g = B[y];
                    if (f.box == g.box || dot(f.n, g.n) < 0.9995f || std::fabs(f.d - g.d) > 0.0015f) continue;
                    if (f.n.y < -0.9f && f.p.y < 0.05f) continue;                          // bottoms lying on the ground
                    if (f.n.y > 0.9f && std::fabs(f.p.y - 3.6f) < 0.02f) continue;        // wall tops hidden under the roof
                    Vec3 r = g.p - f.p;
                    float u = dot(r, f.a), v = dot(r, f.b);
                    float eu = std::fabs(dot(g.a, f.a)) * g.ha + std::fabs(dot(g.b, f.a)) * g.hb;
                    float ev = std::fabs(dot(g.a, f.b)) * g.ha + std::fabs(dot(g.b, f.b)) * g.hb;
                    float ou = std::min(f.ha, u + eu) - std::max(-f.ha, u - eu);
                    float ovv = std::min(f.hb, v + ev) - std::max(-f.hb, v - ev);
                    if (ou < 0.004f || ovv < 0.004f) continue;
                    const Material& m1 = boxes[f.box].second;
                    const Material& m2 = boxes[g.box].second;
                    bool differ = (m1.color - m2.color).length() > 0.02f || m1.tex != m2.tex || (m1.emission - m2.emission).length() > 0.01f;
                    if (!differ) continue;
                    hits.push_back({ou * ovv, f.p + f.a * ((std::min(f.ha, u + eu) + std::max(-f.ha, u - eu)) * 0.5f) +
                                                   f.b * ((std::min(f.hb, v + ev) + std::max(-f.hb, v - ev)) * 0.5f), f.box, g.box, f.n});
                }
        }
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.area > b.area; });
    out << "coplanar face pairs with different materials: " << hits.size() << "\n";
    for (size_t k = 0; k < hits.size() && k < 80; ++k) {
        const auto& h = hits[k];
        const Material& m1 = boxes[h.i].second;
        const Material& m2 = boxes[h.j].second;
        out << "  n(" << std::lround(h.n.x) << "," << std::lround(h.n.y) << "," << std::lround(h.n.z) << ") area " << h.area << " at (" << h.p.x << ", " << h.p.y << ", " << h.p.z << ")  col1 (" << m1.color.x << "," << m1.color.y << ","
            << m1.color.z << ") tex " << m1.tex << "  col2 (" << m2.color.x << "," << m2.color.y << "," << m2.color.z << ") tex " << m2.tex << "\n";
    }
}

enum StudioObject {
    SO_VENDING, SO_ATM, SO_COFFEE, SO_TV, SO_SOFA, SO_ARMCHAIR, SO_BOOKSHELF, SO_HUMAN, SO_HUMAN_WALK, SO_HUMAN_SIT,
    SO_SEDAN, SO_SUV, SO_HATCH, SO_TREE, SO_PALM, SO_BAMBOO, SO_BANANA, SO_STREETLAMP, SO_PENDANT, SO_AQUARIUM,
    SO_PRODUCTS, SO_SHAPES, SO_TABLESET, SO_CLOCK, SO_UNDERGROWTH, SO_COUNT
};
const char* SO_NAMES[SO_COUNT] = {"vending", "atm", "coffee", "tv", "sofa", "armchair", "bookshelf", "human", "human_walk", "human_sit",
                                  "car_red", "car_blue", "car_white", "tree", "palm", "bamboo", "banana", "streetlamp", "pendant", "aquarium",
                                  "products", "shapes", "tableset", "clock", "undergrowth"};

// machines used only for studio renders (kept here so the text pass can reach them)
VendingMachine g_sVend;
ATM g_sAtm;
CoffeeMachine g_sCoffee;
Television g_sTv;
Aquarium g_sAqua;
bool g_sInit = false;
bool g_catalogMode = false;   // parts catalogue of the car: leave the driver out

// Collects every part of an object (shape, matrix, material) without drawing it
struct PartCollector : DrawSink {
    struct P { MeshId mesh; Mat4 M; Material m; };
    std::vector<P> parts;
    void add(MeshId mesh, const Mat4& M, const Material& m) override { parts.push_back({mesh, M, m}); }
};

// world-space box around one part: the 8 corners of its unit shape, transformed
void partBounds(MeshId mesh, const Mat4& M, Vec3& lo, Vec3& hi) {
    float y0 = (mesh == M_CROSS) ? 0.0f : -0.5f, y1 = (mesh == M_CROSS) ? 1.0f : 0.5f;
    float hx = 0.5f, hz = 0.5f;
    if (mesh == M_COIL) { hx = 0.065f; y0 = -0.065f; y1 = 0.065f; hz = 0.16f; }   // the spring is much smaller than its unit box
    lo = Vec3(1e9f);
    hi = Vec3(-1e9f);
    for (int i = 0; i < 8; ++i) {
        Vec3 c((i & 1) ? hx : -hx, (i & 2) ? y1 : y0, (i & 4) ? hz : -hz);
        Vec3 w = M.transformPoint(c);
        lo = Vec3(std::min(lo.x, w.x), std::min(lo.y, w.y), std::min(lo.z, w.z));
        hi = Vec3(std::max(hi.x, w.x), std::max(hi.y, w.y), std::max(hi.z, w.z));
    }
}

// Parts catalogue: identical parts (same shape, size and material) become one entry with a count
struct CatalogEntry { MeshId mesh; Mat4 M; Material m; int count; Vec3 size; };
std::vector<CatalogEntry> buildCatalog(const PartCollector& pc) {
    std::vector<CatalogEntry> out;
    auto q = [](float v, float st) { return (long)std::lround(v / st); };
    auto qs = [](float v) { return (long)std::lround(std::log(std::max(v, 0.002f)) / std::log(1.15f)); };   // sizes within ~15% match
    std::map<std::string, size_t> seen;
    for (const auto& p : pc.parts) {
        const float* m = p.M.m;
        float d[3] = {Vec3(m[0], m[1], m[2]).length(), Vec3(m[4], m[5], m[6]).length(), Vec3(m[8], m[9], m[10]).length()};
        float sd[3] = {d[0], d[1], d[2]};
        std::sort(sd, sd + 3);
        char key[256];
        std::snprintf(key, sizeof(key), "%d|%d|%ld|%ld|%ld|%d|%d|%ld|%ld|%ld", (int)p.mesh, p.m.tex, q(p.m.color.x, 0.12f), q(p.m.color.y, 0.12f),
                      q(p.m.color.z, 0.12f), p.m.alpha < 0.99f ? 1 : 0, p.m.emission.length() > 0.01f ? 1 : 0, qs(sd[0]), qs(sd[1]), qs(sd[2]));
        auto it = seen.find(key);
        if (it != seen.end()) { out[it->second].count++; continue; }
        seen[key] = out.size();
        out.push_back({p.mesh, p.M, p.m, 1, Vec3(d[0], d[1], d[2])});
    }
    // biggest parts first
    auto vol = [](const Vec3& v) { return std::max(v.x, 0.01f) * std::max(v.y, 0.01f) * std::max(v.z, 0.01f); };
    std::stable_sort(out.begin(), out.end(), [&](const CatalogEntry& a, const CatalogEntry& b) { return vol(a.size) > vol(b.size); });
    return out;
}

// Part list for the report: identical parts (same shape + material) are grouped
struct PartListSink : DrawSink {
    struct G { MeshId mesh; Material m; int count; Vec3 size; };
    std::vector<G> groups;
    void add(MeshId mesh, const Mat4& M, const Material& m) override {
        Vec3 sz(Vec3(M.m[0], M.m[1], M.m[2]).length(), Vec3(M.m[4], M.m[5], M.m[6]).length(), Vec3(M.m[8], M.m[9], M.m[10]).length());
        for (auto& g : groups)
            if (g.mesh == mesh && g.m.tex == m.tex && (g.m.color - m.color).length() < 0.03f && std::fabs(g.m.alpha - m.alpha) < 0.05f &&
                (g.m.emission.length() > 0.01f) == (m.emission.length() > 0.01f)) {
                g.count++;
                return;
            }
        groups.push_back({mesh, m, 1, sz});
    }
};

struct StudioCam { Vec3 pos, target; float fov; };
StudioCam studioCam(int o) {
    switch (o) {
        case SO_VENDING: return {Vec3(1.6f, 1.5f, 2.6f), Vec3(0, 1.0f, 0), 40};
        case SO_ATM: return {Vec3(1.5f, 1.6f, 2.4f), Vec3(0, 1.1f, 0), 40};
        case SO_COFFEE: return {Vec3(0.55f, 0.55f, 0.85f), Vec3(0, 0.38f, 0), 40};
        case SO_TV: return {Vec3(1.0f, 1.2f, 2.4f), Vec3(0, 1.0f, 0), 40};
        case SO_SOFA: return {Vec3(1.6f, 1.4f, 2.6f), Vec3(0, 0.4f, 0), 40};
        case SO_ARMCHAIR: return {Vec3(1.1f, 1.1f, 1.7f), Vec3(0, 0.4f, 0), 40};
        case SO_BOOKSHELF: return {Vec3(1.2f, 1.5f, 2.8f), Vec3(0, 1.1f, 0), 40};
        case SO_HUMAN: case SO_HUMAN_WALK: case SO_HUMAN_SIT: return {Vec3(1.3f, 1.3f, 2.6f), Vec3(0, 0.85f, 0), 40};
        case SO_SEDAN: case SO_SUV: case SO_HATCH: return {Vec3(-4.2f, 2.2f, 4.8f), Vec3(0, 0.7f, 0), 40};
        case SO_TREE: return {Vec3(9.0f, 6.5f, 14.0f), Vec3(0, 5.5f, 0), 45};
        case SO_PALM: return {Vec3(8.0f, 6.0f, 13.0f), Vec3(0, 5.6f, 0), 50};
        case SO_BAMBOO: return {Vec3(6.0f, 4.0f, 9.0f), Vec3(0, 3.8f, 0), 45};
        case SO_BANANA: return {Vec3(2.6f, 2.0f, 4.0f), Vec3(0, 1.3f, 0), 45};
        case SO_STREETLAMP: return {Vec3(5.0f, 3.6f, 8.0f), Vec3(0, 3.0f, 0.6f), 45};
        case SO_PENDANT: return {Vec3(0.9f, -1.3f, 1.3f), Vec3(0, -0.55f, 0), 40};
        case SO_AQUARIUM: return {Vec3(1.3f, 1.5f, 2.6f), Vec3(0, 0.85f, 0), 40};
        case SO_PRODUCTS: return {Vec3(0.0f, 0.35f, 0.75f), Vec3(0, 0.06f, 0), 40};
        case SO_SHAPES: return {Vec3(0.0f, 3.2f, 6.2f), Vec3(0, 0.3f, 0), 45};
        case SO_TABLESET: return {Vec3(1.6f, 1.6f, 2.6f), Vec3(0, 0.3f, 0), 40};
        case SO_CLOCK: return {Vec3(0.0f, 0.0f, 0.9f), Vec3(0, 0, 0), 40};
        case SO_UNDERGROWTH: return {Vec3(3.0f, 2.0f, 4.5f), Vec3(0, 0.5f, 0), 45};
    }
    return {Vec3(2, 2, 2), Vec3(0, 0, 0), 45};
}
}  // namespace

// Draws one object at the origin (used for studio renders and part counting)
void App::studioObject(DrawSink& s, int o, float time) {
    Node n(s);
    Rng rng(42);
    if (!g_sInit) {
        g_sVend.init(0, VEND_DRINKS, Vec3(0, 0, 0), 0.0f);
        g_sAtm.init(Vec3(0, 0, 0), 0.0f);
        g_sAtm.m_state = ATM::MENU;
        g_sCoffee.init(Vec3(0, 0, 0), 0.0f);
        g_sCoffee.start(1, true);
        g_sCoffee.m_state = CoffeeMachine::BREWING;
        g_sCoffee.m_t = 2.5f;
        g_sTv.init(Vec3(0, 1.0f, 0), 0.0f);
        g_sAqua.init(Vec3(0, 0, 0), 0.0f);
        g_sInit = true;
    }
    switch (o) {
        case SO_VENDING: g_sVend.drawStatic(s); g_sVend.drawDynamic(s, Vec3(0, 1, 2)); break;
        case SO_ATM: g_sAtm.drawStatic(s); g_sAtm.drawDynamic(s); break;
        case SO_COFFEE: g_sCoffee.drawStatic(s); g_sCoffee.drawDynamic(s); break;
        case SO_TV: g_sTv.drawStatic(s); g_sTv.drawDynamic(s); break;
        case SO_SOFA: Furniture::sofa(n, 3, Vec3(0.28f, 0.36f, 0.48f)); break;
        case SO_ARMCHAIR: Furniture::armchair(n, Vec3(0.55f, 0.30f, 0.22f)); break;
        case SO_BOOKSHELF: Furniture::bookshelf(n, 1.8f, 2.3f); break;
        case SO_HUMAN: Human::draw(n, Human::look(1), Human::idle(0.0f), HELD_CUP); break;
        case SO_HUMAN_WALK: Human::draw(n, Human::look(0), Human::walk(1.2f, 1.0f), HELD_NONE); break;
        case SO_HUMAN_SIT: {
            Furniture::chair(n.at({0, 0, -0.05f}), Vec3(0.45f, 0.2f, 0.15f));
            Human::draw(n, Human::look(0), Human::sit(0.0f), HELD_PHONE);
            break;
        }
        case SO_SEDAN: case SO_SUV: case SO_HATCH: {
            CarLook l = Car::randomLook((unsigned)(o - SO_SEDAN));   // red, blue, white
            CarState st;
            st.doorAngle = 55.0f;
            st.steer = 18.0f;
            st.headlights = true;
            st.time = time;
            Car::draw(n, l, st);
            CarDims d = Car::dims();
            if (!g_catalogMode) Human::draw(n.at(d.driverSeat), Human::look(0), Human::drive(0.0f), HELD_NONE);
            break;
        }
        case SO_TREE: Nature::rainforestTree(n, 1.0f, rng); break;
        case SO_PALM: Nature::palmTree(n, 1.0f, rng); break;
        case SO_BAMBOO: Nature::bambooClump(n, 0.8f, rng); break;
        case SO_BANANA: Nature::bananaPlant(n, 1.0f, rng); break;
        case SO_STREETLAMP: Furniture::streetLamp(n, 5.8f, true); break;
        case SO_PENDANT: Furniture::pendantLamp(n, 0.6f, true, Vec3(0.12f, 0.12f, 0.13f)); break;
        case SO_AQUARIUM: g_sAqua.update(0.016f); g_sAqua.drawStatic(s); g_sAqua.drawDynamic(s); break;
        case SO_PRODUCTS: {
            static VendingMachine a, b, c;
            static bool init = false;
            if (!init) { a.init(0, VEND_DRINKS, Vec3(), 0); b.init(1, VEND_SNACKS, Vec3(), 0); c.init(2, VEND_SWEETS, Vec3(), 0); init = true; }
            const Product* ps[6] = {&a.m_slots[0].p, &a.m_slots[6].p, &a.m_slots[5].p, &b.m_slots[0].p, &c.m_slots[0].p, &c.m_slots[6].p};
            for (int i = 0; i < 6; ++i) drawProduct(n.at({-0.45f + i * 0.18f, 0, 0}), *ps[i]);
            break;
        }
        case SO_SHAPES: {
            Material m(Vec3(0.85f, 0.55f, 0.25f), 0.4f, 48.0f);
            const MeshId ids[12] = {M_CUBE, M_CYLINDER, M_SPHERE, M_CONE, M_TCONE, M_TORUS, M_HEMISPHERE, M_WEDGE, M_FRUSTUM_BOX, M_QUAD, M_CROSS, M_COIL};
            for (int i = 0; i < 12; ++i) {
                float x = -3.0f + (i % 6) * 1.2f, z = i < 6 ? -0.8f : 0.8f;
                Material mm = m;
                mm.color = Vec3(0.25f + 0.06f * i, 0.45f + 0.03f * (i % 4), 0.85f - 0.05f * i);
                Vec3 size(0.8f);
                Vec3 pos(x, 0.4f, z);
                if (ids[i] == M_HEMISPHERE || ids[i] == M_CROSS) pos.y = 0.0f;
                if (ids[i] == M_TORUS) size = Vec3(1.0f, 1.6f, 1.0f);
                if (ids[i] == M_QUAD) mm.flags |= MF_FOLIAGE;
                if (ids[i] == M_COIL) size = Vec3(5.0f, 5.0f, 2.5f);
                if (ids[i] == M_CROSS) { mm = Mat::leafCard(Vec3(0.35f, 0.6f, 0.2f), TEX_GRASS_BLADES); size = Vec3(0.9f); }
                n.part(ids[i], pos, size, mm);
            }
            break;
        }
        case SO_TABLESET:
            Furniture::rug(n, 2.4f, 1.8f, Vec3(0.65f, 0.55f, 0.42f), Vec3(0.35f, 0.25f, 0.18f));
            Furniture::coffeeTable(n, 1.2f, 0.6f);
            Furniture::pottedPlant(n.at({0.9f, 0, -0.5f}), 1.0f, 0);
            break;
        case SO_CLOCK: Furniture::wallClock(n, 10.1f); break;
        case SO_UNDERGROWTH:
            Nature::bush(n.at({-1.0f, 0, 0}), 1.0f, rng);
            Nature::fern(n.at({0.6f, 0, 0.4f}), 1.3f);
            Nature::rock(n.at({1.2f, 0, -0.8f}), 0.8f, rng);
            Nature::fallenLog(n.at({0, 0, -1.4f}), 3.0f, rng);
            break;
    }
}

int App::studioPartCount(int o) {
    CountSink c;
    studioObject(c, o, 0.0f);
    return c.count;
}

void App::renderStudio(int w, int h, const Vec3& camPos0, const Vec3& target0, float fov, int partLimit, int object, float time, float white) {
    // white mode: keep the studio viewing direction, but move the camera so the whole object fits
    Vec3 target = target0, camPos = camPos0;
    if (white >= 0.0f) {
        PartCollector pc;
        studioObject(pc, object, time);
        Vec3 lo(1e9f), hi(-1e9f);
        for (const auto& p : pc.parts) {
            Vec3 a, b;
            partBounds(p.mesh, p.M, a, b);
            lo = Vec3(std::min(lo.x, a.x), std::min(lo.y, a.y), std::min(lo.z, a.z));
            hi = Vec3(std::max(hi.x, b.x), std::max(hi.y, b.y), std::max(hi.z, b.z));
        }
        target = (lo + hi) * 0.5f;
        float r = (hi - lo).length() * 0.5f;
        camPos = target + normalize(camPos0 - target0) * (r / std::sin(toRadians(fov) * 0.5f));
    }
    FrameInfo f;
    f.proj = perspective(toRadians(fov), (float)w / h, 0.05f, 200.0f);
    f.view = lookAt(camPos, target, Vec3(0, 1, 0));
    f.camPos = camPos;
    f.time = time;
    Environment& e = f.env;
    e.lightDir = normalize(Vec3(0.55f, 0.85f, 0.65f));
    e.lightColor = Vec3(1.05f, 1.0f, 0.95f);
    e.skyAmbient = Vec3(0.45f, 0.47f, 0.52f);
    e.groundAmbient = Vec3(0.26f, 0.25f, 0.23f);
    e.fogDensity = 0.0f;
    e.exposure = 1.25f;
    e.castShadows = true;
    f.drawStatic = false;
    f.drawSky = false;
    f.clearColor = white >= 0.0f ? Vec3(1.0f) : Vec3(0.86f, 0.88f, 0.91f);
    f.shadowFocus = target;
    f.indoorBox = Vec4(1e6f, 1e6f, 1e6f + 1, 1e6f + 1);
    f.farDist = 200.0f;
    bool fog = m_renderer.settings.fog;
    float rad = m_renderer.settings.shadowRadius;
    m_renderer.settings.fog = false;
    m_renderer.settings.shadowRadius = std::max(6.0f, (camPos - target).length() * 0.9f);
    DrawSink& dyn = m_renderer.dynamicSink();
    if (object != SO_PENDANT && object != SO_CLOCK && white < 0.0f)
        dyn.add(M_CYLINDER, scale(translate(Mat4(), Vec3(0, -0.02f, 0)), Vec3(30.0f, 0.04f, 30.0f)), Material(Vec3(0.78f, 0.79f, 0.80f), 0.1f, 8.0f));
    {
        LimitSink lim(dyn, partLimit);
        studioObject(lim, object, time);
    }
    m_renderer.beginFrame(f, w, h);
    if (partLimit > 100000) {   // screens are only shown on the finished object
        m_text.begin3D(m_renderer.viewProj());
        if (object == SO_VENDING) g_sVend.drawScreen(m_text);
        if (object == SO_ATM) g_sAtm.drawScreen(m_text);
        if (object == SO_COFFEE) g_sCoffee.drawScreen(m_text);
        if (object == SO_TV) g_sTv.drawScreen(m_text, 11.0f, "11:00 AM");
        m_text.end3D();
    }
    m_renderer.drawTransparent();
    m_renderer.endFrame();
    m_renderer.settings.fog = fog;
    m_renderer.settings.shadowRadius = rad;
}

// Renders one part alone (same viewing direction as the studio), zoomed to fill
// the middle square of the frame, and saves that square as a small PNG.
void App::renderPartTile(int w, int h, const Vec3& viewDir, int mesh, const Mat4& M0, const Material& mat, const std::string& path) {
    Mat4 M = M0;
    M.m[12] = M.m[13] = M.m[14] = 0.0f;   // keep the part's rotation and size, drop its position
    Vec3 lo, hi;
    partBounds((MeshId)mesh, M, lo, hi);
    Vec3 c = (lo + hi) * 0.5f;
    float r = std::max(0.02f, (hi - lo).length() * 0.5f);
    if (mesh == M_LEAVES || mesh == M_CROSS) r *= 0.6f;   // most of a leaf card is see-through: zoom in
    const float fov = 30.0f;
    Vec3 dir = viewDir;
    if (mesh == M_QUAD) {   // a flat card: look at its face, not its edge
        Vec3 nrm = normalize(Vec3(M.m[8], M.m[9], M.m[10]));
        if (dot(nrm, viewDir) < 0) nrm = nrm * -1.0f;
        dir = normalize(nrm * 0.85f + viewDir * 0.15f);
    }
    Vec3 cam = c + dir * (r / std::sin(toRadians(fov) * 0.5f) * 1.08f);
    FrameInfo f;
    f.proj = perspective(toRadians(fov), (float)w / h, r * 0.05f, r * 20.0f);
    f.view = lookAt(cam, c, std::fabs(dir.y) > 0.98f ? Vec3(0, 0, -1) : Vec3(0, 1, 0));
    f.camPos = cam;
    f.time = 1.0f;
    Environment& e = f.env;
    e.lightDir = normalize(Vec3(0.55f, 0.85f, 0.65f));
    e.lightColor = Vec3(1.05f, 1.0f, 0.95f);
    e.skyAmbient = Vec3(0.50f, 0.52f, 0.56f);
    e.groundAmbient = Vec3(0.30f, 0.29f, 0.27f);
    e.fogDensity = 0.0f;
    e.exposure = 1.25f;
    e.castShadows = false;
    f.drawStatic = false;
    f.drawSky = false;
    f.clearColor = Vec3(1.0f);
    f.shadowFocus = c;
    f.indoorBox = Vec4(1e6f, 1e6f, 1e6f + 1, 1e6f + 1);
    f.farDist = 200.0f;
    bool fog = m_renderer.settings.fog;
    m_renderer.settings.fog = false;
    Material m = mat;
    m.lod = 0;
    if (m.alpha < 0.99f) m.alpha = std::max(m.alpha, 0.55f);   // glass: a bit stronger, so it can be seen on white
    m_renderer.dynamicSink().add((MeshId)mesh, M, m);
    m_renderer.beginFrame(f, w, h);
    m_renderer.drawTransparent();
    m_renderer.endFrame();
    m_renderer.settings.fog = fog;
    glFinish();
    // read the middle h x h square and shrink it 3 times (box filter)
    std::vector<uint8_t> px((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    const int k = 3, n = h / k, x0 = (w - h) / 2;
    std::vector<uint8_t> out((size_t)n * n * 3);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            for (int ch = 0; ch < 3; ++ch) {
                int sum = 0;
                for (int dy = 0; dy < k; ++dy)
                    for (int dx = 0; dx < k; ++dx) sum += px[((size_t)(h - 1 - (y * k + dy)) * w + x0 + x * k + dx) * 3 + ch];
                out[((size_t)y * n + x) * 3 + ch] = (uint8_t)(sum / (k * k));
            }
    writePNG(path, n, n, out.data(), 3);
}

// =============================================================================
void App::runShots(const std::string& dir) {
    CreateDirectoryA(dir.c_str(), nullptr);
    int w, h;
    glfwGetFramebufferSize(m_win, &w, &h);
    m_hudVisible = true;
    m_showHelp = false;
    Messages::get().push("", 0.0f);
    std::ofstream log(dir + "/shots_log.txt");

    auto save = [&](const std::string& name) {
        glFinish();
        std::string p = dir + "/" + name + ".png";
        saveFramebufferPNG(p, w, h);
        log << name << "\n";
        std::cout << "  saved " << p << std::endl;
    };
    auto frame = [&](bool hud, int count = 3, float dt = 1.0f / 30.0f) {
        for (int i = 0; i < count; ++i) {
            update(dt);
            render(w, h, hud);
            glfwSwapBuffers(m_win);
            glfwPollEvents();
        }
    };
    auto freeCam = [&](const Vec3& pos, const Vec3& target, float fov) {
        m_player.mode = CAM_FREE;
        m_player.freePos = pos;
        Vec3 d = normalize(target - pos);
        m_player.freeYaw = toDegrees(std::atan2(d.x, d.z));
        m_player.freePitch = toDegrees(std::asin(d.y));
        m_player.fov = fov;
    };
    auto fpsCam = [&](const Vec3& feet, float yaw, float pitch, float fov) {
        m_player.mode = CAM_FIRST;
        m_player.pos = feet;
        m_player.yaw = yaw;
        m_player.pitch = pitch;
        m_player.fov = fov;
    };
    auto hour = [&](float hr) { m_day.hour = hr; m_day.paused = true; };
    // views for the picture tour at the start of the report (no HUD)
    auto tourShots = [&]() {
        hour(11.0f);
        freeCam(Vec3(5.9f, 2.3f, 4.5f), Vec3(-1.5f, 0.8f, -2.8f), 72);
        frame(false, 3); save("tour_lounge");
        freeCam(Vec3(2.9f, 1.7f, -1.7f), Vec3(3.75f, 1.0f, -4.5f), 62);
        frame(false, 3); save("tour_vending");
        freeCam(Vec3(-8.2f, 1.6f, -2.3f), Vec3(-9.5f, 1.15f, -4.5f), 55);
        frame(false, 3); save("tour_atm");
        m_coffee.start(1, false);
        freeCam(Vec3(-4.15f, 1.55f, -3.3f), Vec3(-5.0f, 1.2f, -4.55f), 50);
        frame(false, 100); save("tour_coffee");
    };

    // ---- quick debug renders (set JRS_DEBUG=1): close-ups + vending benchmark only ----
    if (std::getenv("JRS_DEBUG")) {
        std::ofstream bf(dir + "/debug_bench.csv");
        auto benchHere = [&](const char* label) {
            for (int pass = 0; pass < 2; ++pass) {
                m_renderer.settings.enhanced = pass == 0;
                for (int i = 0; i < 5; ++i) { update(1.0f / 60); render(w, h, true); glfwSwapBuffers(m_win); }
                glFinish();
                double t0 = glfwGetTime();
                for (int i = 0; i < 40; ++i) { update(1.0f / 60); render(w, h, true); glfwSwapBuffers(m_win); glfwPollEvents(); }
                glFinish();
                double ms = (glfwGetTime() - t0) * 1000.0 / 40;
                bf << label << "," << (pass ? "basic" : "enhanced") << "," << ms << "," << m_renderer.statDrawCalls << ","
                   << m_renderer.statInstances << "," << m_renderer.statTriangles << std::endl;
            }
            m_renderer.settings.enhanced = true;
        };
        auto cam = [&](const Vec3& pos, const Vec3& target, float fov) {
            m_player.mode = CAM_FREE;
            m_player.freePos = pos;
            Vec3 d = normalize(target - pos);
            m_player.freeYaw = toDegrees(std::atan2(d.x, d.z));
            m_player.freePitch = toDegrees(std::asin(d.y));
            m_player.fov = fov;
        };
        m_day.hour = 11.0f;
        if (std::string(std::getenv("JRS_DEBUG")) == "tour") { tourShots(); return; }
        if (std::string(std::getenv("JRS_DEBUG")) == "wall") {
            hour(11.0f);
            cam(Vec3(-4.2f, 1.6f, 0.5f), Vec3(-4.2f, 1.6f, 5.0f), 75); frame(false, 3); save("w_front_left");
            cam(Vec3(-1.0f, 1.6f, 1.0f), Vec3(-6.5f, 1.6f, 4.5f), 75); frame(false, 3); save("w_corner");
            cam(Vec3(-4.2f, 1.6f, 9.0f), Vec3(-4.2f, 1.6f, 5.0f), 75); frame(false, 3); save("w_outside");
            freeCam(Vec3(5.9f, 2.3f, 4.5f), Vec3(-1.5f, 0.8f, -2.8f), 72); frame(false, 3); save("w_tour");
            return;
        }
        if (std::string(std::getenv("JRS_DEBUG")) == "views") {   // only the problem views
            m_day.paused = true;
            if (std::getenv("JRS_NO_OCC")) m_renderer.settings.occlusion = false;
            {
                BoxRecorder rec;
                CollisionWorld dummy;
                m_building.buildStatic(rec, dummy);
                m_building.drawDynamic(rec, m_time, 12.0f, false, Vec3(0, 1.6f, 0));
                for (auto& v : m_vend) { v.drawStatic(rec); v.drawDynamic(rec, v.standPos()); }
                m_atm.drawStatic(rec); m_atm.drawDynamic(rec);
                m_coffee.drawStatic(rec); m_coffee.drawDynamic(rec);
                m_tv.drawStatic(rec); m_tv.drawDynamic(rec);
                m_aquarium.drawStatic(rec); m_aquarium.drawDynamic(rec);
                std::ofstream zf(dir + "/zfight.txt");
                zf << rec.boxes.size() << " boxes\n";
                findCoplanarFaces(rec.boxes, zf);
            }
            for (float x = 18; x <= 40; x += 2)
                for (float z = 0; z <= 30; z += 2)
                    for (int sd = -1; sd <= 1; sd += 2)
                        if (std::fabs(terrainHeight(sd * x, z)) > 0.05f) log << "h(" << sd * x << "," << z << ")=" << terrainHeight(sd * x, z) << "\n";
            cam(Vec3(27.0f, 25.0f, 12.01f), Vec3(27.0f, 0.0f, 12.0f), 50); frame(false, 3); save("v_top_e");
            cam(Vec3(27.0f, 6.0f, 12.01f), Vec3(27.0f, 0.0f, 12.0f), 60); frame(false, 3); save("v_top_e_low");
            const char* hrs[2] = {"day", "night"};
            for (int k = 0; k < 2; ++k) {
                m_day.hour = k == 0 ? 11.0f : 21.0f;
                cam(Vec3(34.0f, 7.0f, 4.0f), Vec3(24.0f, 0.0f, 15.0f), 70); frame(false, 3); save(std::string("v_road_e_") + hrs[k]);
                cam(Vec3(-34.0f, 7.0f, 4.0f), Vec3(-24.0f, 0.0f, 15.0f), 70); frame(false, 3); save(std::string("v_road_w_") + hrs[k]);
                cam(Vec3(30.0f, 8.0f, 28.0f), Vec3(24.0f, 0.0f, 14.0f), 70); frame(false, 3); save(std::string("v_road_e2_") + hrs[k]);
                Vec3 sp = m_atm.standPos();
                fpsCam(sp + Vec3(0.25f, 0, -0.15f), 180.0f, -18.0f, 70); frame(false, 3); save(std::string("v_atm_") + hrs[k]);
                cam(Vec3(2.5f, 5.0f, 7.0f), Vec3(1.0f, 4.6f, 5.3f), 60); frame(false, 3); save(std::string("v_sign_") + hrs[k]);
            }
            return;
        }
        if (std::getenv("JRS_NOLIGHTS")) for (int g = 0; g < LG_COUNT; ++g) LightManager::get().groupOn[g] = false;
        m_day.paused = true;
        m_player.mode = CAM_FIRST;
        m_player.pos = m_vend[1].standPos();
        m_player.yaw = 180.0f;
        m_player.pitch = -8.0f;
        m_player.fov = 70;
        benchHere("vending_close");
        save("dbg_vending_close");
        {
            Profiler& pr = Profiler::get();
            std::ofstream pf(dir + "/debug_profile.txt");
            auto prof = [&](const char* label) {
                pr.enabled = true;
                pr.reset();
                double upd = 0;
                for (int i = 0; i < 30; ++i) {
                    auto t0 = std::chrono::high_resolution_clock::now();
                    update(1.0f / 60);
                    upd += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
                    render(w, h, true);
                    glfwSwapBuffers(m_win);
                }
                pr.enabled = false;
                pf << "== " << label << "  (update cpu " << upd / 30 << " ms)" << std::endl;
                for (auto& kv : pr.gpu) pf << "  " << kv.first << "  gpu " << kv.second / pr.frames << "  cpu " << pr.cpu[kv.first] / pr.frames << std::endl;
            };
            prof("vending_close");
            m_player.pos = Vec3(0, 0, 4.3f);
            m_player.yaw = 180.0f;
            prof("lounge");
            m_player.pos = m_vend[1].standPos();
        }
        m_player.pos = Vec3(0, 0, 4.3f);
        benchHere("lounge");
        // studio close-ups
        const Vec3 hv[3] = {Vec3(0, 1.2f, 2.2f), Vec3(2.2f, 1.2f, 0.0f), Vec3(-1.2f, 1.6f, -1.8f)};
        const char* hn[3] = {"front", "side", "back"};
        for (int k = 0; k < 3; ++k) {
            renderStudio(w, h, hv[k], Vec3(0, 0.95f, 0), 40, 1 << 30, SO_HUMAN, 1.0f);
            save(std::string("dbg_human_") + hn[k]);
        }
        renderStudio(w, h, Vec3(0, 1.0f, 2.0f), Vec3(0, 0.6f, 0), 40, 1 << 30, SO_HUMAN_SIT, 1.0f);
        save("dbg_human_sit");
        renderStudio(w, h, Vec3(-3.6f, 1.7f, 1.2f), Vec3(-0.6f, 0.8f, 0.2f), 40, 1 << 30, SO_SEDAN, 1.0f);
        save("dbg_car_door");
        renderStudio(w, h, Vec3(-2.6f, 2.2f, -2.0f), Vec3(0, 0.9f, 0), 45, 1 << 30, SO_SEDAN, 1.0f);
        save("dbg_car_rear");
        renderStudio(w, h, Vec3(6.0f, 6.5f, 9.0f), Vec3(0, 6.0f, 0), 50, 1 << 30, SO_TREE, 1.0f);
        save("dbg_tree");
        renderStudio(w, h, Vec3(2.6f, 1.6f, 3.4f), Vec3(0, 0.6f, 0), 45, 1 << 30, SO_UNDERGROWTH, 1.0f);
        save("dbg_bush");
        renderStudio(w, h, Vec3(0.5f, 1.4f, 1.6f), Vec3(0, 1.1f, 0), 45, 1 << 30, SO_BOOKSHELF, 1.0f);
        save("dbg_books");
        renderStudio(w, h, Vec3(0.25f, 0.55f, 0.6f), Vec3(0, 0.45f, 0), 40, 1 << 30, SO_COFFEE, 1.0f);
        save("dbg_coffee");
        renderStudio(w, h, Vec3(0.5f, 1.5f, 1.6f), Vec3(0, 1.3f, 0), 40, 1 << 30, SO_VENDING, 1.0f);
        save("dbg_vending_studio");
        m_player.mode = CAM_FIRST;
        m_player.pos = Vec3(-20.0f, 0, 26.5f);
        m_player.yaw = 150.0f; m_player.pitch = 8.0f;
        frame(false, 3); save("dbg_jungle_edge");
        cam(Vec3(-34.0f, 2.0f, -26.0f), Vec3(-40.0f, 3.0f, -34.0f), 70);
        frame(false, 3); save("dbg_jungle_close");
        // doorways (the views from the bug report)
        m_player.mode = CAM_FIRST;
        m_player.pos = Vec3(-5.6f, 0, -2.2f); m_player.yaw = -95.0f; m_player.pitch = 4.0f;
        frame(true, 3); save("dbg_atm_door");
        m_player.pos = Vec3(0.3f, 0, 7.2f); m_player.yaw = 180.0f; m_player.pitch = -14.0f;
        frame(true, 30); save("dbg_entrance");
        for (int k = 0; k < 3; ++k) {
            renderStudio(w, h, Vec3(-4.2f, 2.2f, 4.8f), Vec3(0, 0.7f, 0), 40, 1 << 30, SO_SEDAN + k, 1.0f);
            save("dbg_car_" + std::to_string(k));
        }
        renderStudio(w, h, Vec3(2.4f, 1.5f, 0.9f), Vec3(0, 1.1f, 0.2f), 50, 1 << 30, SO_SEDAN + 2, 1.0f);
        save("dbg_car_glass_side");
        renderStudio(w, h, Vec3(1.8f, 1.9f, 4.2f), Vec3(0, 1.0f, 0.3f), 45, 1 << 30, SO_SEDAN + 2, 1.0f);
        save("dbg_car_glass_front");
        return;
    }

    // ---- part statistics for the report ----
    {
        std::ofstream csv(dir + "/parts.csv");
        csv << "object,total";
        for (int m = 0; m < M_COUNT; ++m) csv << "," << meshName((MeshId)m);
        csv << "\n";
        for (int o = 0; o < SO_COUNT; ++o) {
            CountSink c;
            studioObject(c, o, 0.0f);
            csv << SO_NAMES[o] << "," << c.count;
            for (int m = 0; m < M_COUNT; ++m) csv << "," << c.perMesh[m];
            csv << "\n";
        }
        std::ofstream pl(dir + "/partlist.csv");
        pl << "object,shape,texture,r,g,b,alpha,glow,count,sx,sy,sz" << "\n";
        for (int o = 0; o < SO_COUNT; ++o) {
            PartListSink ps;
            studioObject(ps, o, 0.0f);
            for (auto& g : ps.groups)
                pl << SO_NAMES[o] << "," << meshName(g.mesh) << "," << g.m.tex << "," << g.m.color.x << "," << g.m.color.y << "," << g.m.color.z << ","
                   << g.m.alpha << "," << (g.m.emission.length() > 0.01f ? 1 : 0) << "," << g.count << "," << g.size.x << "," << g.size.y << "," << g.size.z << "\n";
        }
        std::ofstream st(dir + "/stats.txt");
        st << "static_instances " << m_renderer.staticInstanceCount() << "\nchunks " << m_renderer.statChunksTotal
           << "\nstatic_lights " << LightManager::get().staticCount() << "\ncolliders " << m_col.staticCount()
           << "\njungle_trees " << m_jungle.trees << "\npalms " << m_jungle.palms << "\nbamboo " << m_jungle.bamboo
           << "\nbushes " << m_jungle.bushes << "\nferns " << m_jungle.ferns << "\ngrass " << m_jungle.grass
           << "\nrocks " << m_jungle.rocks << "\nlogs " << m_jungle.logs << "\nbuild_seconds " << m_buildSeconds << "\n";
    }
    // ---- texture swatches ----
    for (int l = 1; l < TEX_COUNT; ++l) {
        const auto& px = m_renderer.textures().layerPixels(l);
        std::vector<uint8_t> rgb(TextureArray::SIZE * TextureArray::SIZE * 3);
        for (int i = 0; i < TextureArray::SIZE * TextureArray::SIZE; ++i) {
            float a = px[i * 4 + 3] / 255.0f;
            for (int c = 0; c < 3; ++c) {
                float v = px[i * 4 + c] / 255.0f * 1.5f;
                v = v * a + 0.92f * (1 - a);   // cut-out areas shown light grey
                rgb[i * 3 + c] = (uint8_t)clampf(v * 255.0f, 0, 255);
            }
        }
        char name[128];
        std::snprintf(name, sizeof(name), "%s/tex_%02d.png", dir.c_str(), l);
        writePNG(name, TextureArray::SIZE, TextureArray::SIZE, rgb.data(), 3);
    }

    // ---- studio renders + build stages ----
    for (int o = 0; o < SO_COUNT; ++o) {
        StudioCam c = studioCam(o);
        int total = studioPartCount(o);
        renderStudio(w, h, c.pos, c.target, c.fov, 1 << 30, o, 1.0f);
        save(std::string("obj_") + SO_NAMES[o]);
        if (o != SO_SHAPES && o != SO_SUV && o != SO_HATCH && o != SO_HUMAN_SIT) {
            renderStudio(w, h, c.pos, c.target, c.fov, 1 << 30, o, 1.0f, 0.0f);
            save(std::string("white_") + SO_NAMES[o]);
            // parts catalogue: every different part alone in its own tile
            PartCollector pc;
            g_catalogMode = true;
            studioObject(pc, o, 1.0f);
            g_catalogMode = false;
            std::vector<CatalogEntry> cat = buildCatalog(pc);
            Vec3 viewDir = normalize(c.pos - c.target);
            std::ofstream cf(dir + "/catalog_" + SO_NAMES[o] + ".csv");
            cf << "idx,shape,count,sx,sy,sz,parts_total,entries_total\n";
            const int MAXT = 36;
            for (int i = 0; i < (int)cat.size() && i < MAXT; ++i) {
                char nm[256];
                std::snprintf(nm, sizeof(nm), "%s/cat_%s_%02d.png", dir.c_str(), SO_NAMES[o], i);
                renderPartTile(w, h, viewDir, cat[i].mesh, cat[i].M, cat[i].m, nm);
                cf << i << "," << meshName(cat[i].mesh) << "," << cat[i].count << "," << cat[i].size.x << "," << cat[i].size.y << ","
                   << cat[i].size.z << "," << pc.parts.size() << "," << cat.size() << "\n";
            }
        }
        if (o == SO_VENDING || o == SO_HUMAN || o == SO_SEDAN || o == SO_TREE || o == SO_PALM || o == SO_SOFA || o == SO_ATM || o == SO_STREETLAMP) {
            const float stages[3] = {0.12f, 0.35f, 0.65f};
            for (int k = 0; k < 3; ++k) {
                renderStudio(w, h, c.pos, c.target, c.fov, std::max(1, (int)(total * stages[k])), o, 1.0f);
                save(std::string("stage_") + SO_NAMES[o] + "_" + std::to_string(k + 1));
            }
        }
    }

    // ---- populate the world with visitors ----
    m_visitors.spawnMin = 3.0f;
    m_visitors.spawnMax = 6.0f;
    {
        std::ofstream vlog(dir + "/visitor_trace.txt");
        m_visitors.prewarm(150.0f, m_col, &vlog);
        vlog << "arrived " << m_visitors.totalArrived << ", left " << m_visitors.totalLeft << std::endl;
    }
    m_visitors.spawnMin = 8.0f;
    m_visitors.spawnMax = 18.0f;

    // ---- scene views ----
    m_renderer.settings.enhanced = true;
    hour(10.5f);
    freeCam(Vec3(-4.0f, 11.0f, 24.5f), Vec3(1.5f, 1.2f, 1.0f), 64);   // above the far edge of the lot: no plants in front
    frame(false); save("ext_day");
    m_renderer.settings.enhanced = false;
    frame(false); save("ext_day_basic");
    m_renderer.settings.enhanced = true;
    m_renderer.settings.shadows = false; frame(false); save("ext_day_noshadow");
    m_renderer.settings.shadows = true;
    m_renderer.settings.textures = false; frame(false); save("ext_day_notex");
    m_renderer.settings.textures = true;
    hour(17.6f); frame(false); save("ext_sunset");
    hour(21.0f); frame(false); save("ext_night");
    hour(6.3f); frame(false); save("ext_sunrise");

    hour(11.0f);
    freeCam(Vec3(5.0f, 70.0f, 70.0f), Vec3(0, 0, 4.0f), 55);
    frame(false); save("aerial_day");
    freeCam(Vec3(30.0f, 40.0f, 60.0f), Vec3(0, 0, -10.0f), 70);
    frame(false); save("aerial_jungle");
    hour(21.5f);
    m_cutaway = true;
    freeCam(Vec3(1.5f, 22.0f, 14.0f), Vec3(1.5f, 0, -0.5f), 60);
    frame(false); save("cutaway_night");
    hour(12.0f);
    frame(false); save("cutaway_day");
    m_cutaway = false;

    // interiors
    hour(11.0f);
    fpsCam(Vec3(0.0f, 0, 4.3f), 180.0f, -6.0f, 75);
    frame(true); save("lounge_day");
    hour(21.0f);
    frame(true); save("lounge_night");
    for (int g = 0; g < LG_COUNT; ++g) if (g != LG_MACHINES) LightManager::get().groupOn[g] = false;
    frame(false); save("lounge_night_lamps_off");
    for (int g = 0; g < LG_COUNT; ++g) LightManager::get().groupOn[g] = true;
    m_renderer.settings.enhanced = false;
    frame(false); save("lounge_night_basic");
    m_renderer.settings.enhanced = true;
    hour(11.0f);
    fpsCam(Vec3(5.6f, 0, 3.6f), 220.0f, -8.0f, 70);
    frame(false); save("lounge_corner");
    fpsCam(Vec3(-10.8f, 0, 3.8f), 165.0f, -8.0f, 75);
    frame(false); save("atm_room");
    hour(20.0f);
    fpsCam(Vec3(7.6f, 0, -0.3f), 85.0f, -6.0f, 75);
    frame(false); save("library_night");
    hour(11.0f);
    frame(false); save("library_day");
    fpsCam(Vec3(9.8f, 0, 2.2f), 0.0f, -12.0f, 55);
    frame(false, 6); save("aquarium");

    tourShots();

    // machines
    fpsCam(Vec3(3.6f, 0, -1.9f), 180.0f, -12.0f, 60);
    frame(true); save("vending_row");
    fpsCam(Vec3(2.35f, 0, -3.15f), 180.0f, -14.0f, 50);
    m_vend[0].m_credit = 60;
    m_vend[0].m_entry = "B1";
    frame(true); save("vending_code");
    m_vend[0].press("OK");
    frame(true, 25); save("vending_spiral");
    frame(true, 27); save("vending_dispense");
    frame(true, 30); save("vending_ready");

    fpsCam(Vec3(-9.5f, 0, -3.45f), 180.0f, -14.0f, 55);
    frame(true); save("atm_1_welcome");
    m_atm.cardSlot(); frame(true, 8); save("atm_1b_card");
    frame(true, 40);
    m_atm.keypad("1"); m_atm.keypad("2"); frame(true); save("atm_2_pin");
    m_atm.keypad("3"); m_atm.keypad("4"); m_atm.keypad("ENTER"); frame(true, 45); save("atm_3_menu");
    m_atm.sideKey(0); frame(true); save("atm_4_amount");
    m_atm.sideKey(1); frame(true, 20); save("atm_5_processing");
    frame(true, 60); save("atm_6_takecard");
    m_atm.cardSlot(); frame(true, 40); save("atm_7_takecash");
    m_atm.cashSlot(); frame(true, 45); save("atm_8_receipt");
    m_atm.receiptSlot(); frame(true, 10); save("atm_9_thanks");
    frame(true, 90);

    fpsCam(Vec3(-5.0f, 0, -3.55f), 180.0f, -24.0f, 45);
    m_coffee.start(1, false);
    frame(true, 120); save("coffee_brewing");
    frame(true, 90); save("coffee_ready");
    m_coffee.take(true);

    fpsCam(Vec3(0.0f, 0, -1.7f), 180.0f, 4.0f, 50);
    for (int ch = 1; ch < Television::CHANNELS; ++ch) {
        m_tv.channel = ch;
        m_tv.m_switchFx = 0;
        frame(false, 4);
        save("tv_ch" + std::to_string(ch));
    }
    m_tv.channel = 1;

    // story shots: a visitor's car coming along the road, then people walking in
    hour(15.5f);
    {
        m_visitors.forceSpawn();
        Vec3 car;
        bool found = false;
        for (int i = 0; i < 2400 && !found; ++i) {   // up to 80 s of simulation
            update(1.0f / 30.0f);
            std::vector<Vec3> cars;
            m_visitors.arrivingCars(cars);
            for (auto& c : cars)
                if (c.x > 45.0f && c.x < 60.0f) { car = c; found = true; }
        }
        if (found) {
            float cx = car.x - 16.0f;
            freeCam(Vec3(cx, 2.4f, roadCenterZ(cx) - 5.5f), car + Vec3(2.0f, 0.6f, 0), 55);
            frame(false, 3);
            save("story_car");
        }
        found = false;
        for (int i = 0; i < 3600 && !found; ++i) {   // up to 120 s: wait for someone at the entrance
            update(1.0f / 30.0f);
            std::vector<Vec3> ppl;
            m_visitors.peoplePositions(ppl);
            for (auto& p : ppl)
                if (std::fabs(p.x) < 2.0f && p.z > 5.8f && p.z < 8.0f) found = true;
        }
        freeCam(Vec3(4.5f, 2.3f, 13.5f), Vec3(0.0f, 1.2f, 5.5f), 55);
        frame(false, 3);
        save("story_enter");
    }

    // people + cars
    hour(16.5f);
    freeCam(Vec3(15.0f, 4.5f, 24.6f), Vec3(-2.0f, 0.8f, 12.0f), 60);   // between the ornamental palms
    frame(false, 30); save("parking_lot");
    fpsCam(Vec3(-1.5f, 0, 2.6f), 200.0f, -10.0f, 70);
    frame(false, 4); save("visitors_inside");
    hour(20.5f);
    freeCam(Vec3(28.0f, 3.0f, 19.0f), Vec3(0.0f, 1.0f, 14.0f), 65);
    frame(false, 30); save("parking_night");

    // jungle & road
    hour(9.5f);
    fpsCam(Vec3(60.0f, 0, roadCenterZ(60.0f)), 270.0f, -2.0f, 70);
    frame(false); save("road_day");
    hour(21.0f);
    frame(false); save("road_night");
    hour(10.0f);
    fpsCam(Vec3(-36.0f, terrainHeight(-36.0f, -30.0f), -30.0f), 200.0f, 4.0f, 75);
    frame(false); save("jungle_inside");
    hour(15.0f);
    freeCam(Vec3(-8.0f, 2.2f, 12.0f), Vec3(-4.0f, 1.0f, 5.5f), 60);
    frame(false); save("shadows_facade");
    m_renderer.saveShadowMap(dir + "/shadow_map.png");

    // third person + FOV comparison
    hour(11.0f);
    m_player.mode = CAM_THIRD;
    m_player.pos = Vec3(0.0f, 0, 9.0f);
    m_player.yaw = 180.0f;
    m_player.pitch = -10.0f;
    m_player.fov = 70;
    frame(true); save("third_person");
    fpsCam(Vec3(0.0f, 0, 12.0f), 180.0f, -2.0f, 35);
    frame(false); save("fov_35");
    m_player.fov = 100;
    frame(false); save("fov_100");
    m_player.fov = 70;
    frame(false); save("fov_70");
    // ---- interaction probes: aim the camera at a part and read the prompt ----
    {
        std::ofstream pr(dir + "/interaction_probes.txt");
        auto probe = [&](const char* label, const Vec3& feet, const Vec3& target) {
            m_player.mode = CAM_FIRST;
            m_player.pos = feet;
            m_player.fov = 70;
            Vec3 eye = feet + Vec3(0, m_player.eyeHeight, 0);
            Vec3 d = normalize(target - eye);
            m_player.yaw = toDegrees(std::atan2(d.x, d.z));
            m_player.pitch = toDegrees(std::asin(d.y));
            update(0.001f);
            pr << label << " -> [" << InteractionSystem::get().prompt() << "]" << std::endl;
        };
        Mat4 vr = m_vend[1].root();
        Vec3 vs = m_vend[1].standPos();
        probe("vending coin slot", vs, vr.transformPoint({0.30f, 1.23f, 0.41f}));
        probe("vending note slot", vs, vr.transformPoint({0.33f, 1.12f, 0.41f}));
        probe("vending key B", vs, vr.transformPoint({0.255f, 1.44f, 0.41f}));
        probe("vending key 2", vs, vr.transformPoint({0.325f, 1.44f, 0.41f}));
        probe("vending OK", vs, vr.transformPoint({0.395f, 1.44f, 0.41f}));
        probe("vending flap", vs, vr.transformPoint({-0.15f, 0.38f, 0.41f}));
        Mat4 ar = m_atm.root();
        Vec3 as = m_atm.standPos();
        probe("ATM card slot", as, ar.transformPoint({0.34f, 1.30f, 0.21f}));
        probe("ATM side key L1", as, ar.transformPoint({-0.245f, 1.53f, 0.20f}));
        Mat4 kf = rotate(translate(ar, Vec3(0.0f, 1.035f, 0.40f)), toRadians(12.0f), Vec3(1, 0, 0));
        probe("ATM keypad 5", as, kf.transformPoint({-0.06f, 0.04f, -0.033f}));
        probe("ATM cash slot", as, ar.transformPoint({0.0f, 0.88f, 0.37f}));
        probe("coffee button 1", m_coffee.standPos(), Layout::COFFEE_POS + Vec3(-0.11f, 0.497f, 0.21f));
        probe("TV", Vec3(0, 0, -2.5f), Layout::TV_POS);
        probe("aquarium", m_aquarium.viewPos(), Layout::AQUARIUM_POS + Vec3(0, 1.0f, 0));
        probe("light switch", Vec3(-1.75f, 0, 3.6f), Vec3(-1.75f, 1.3f, 4.89f));
    }
    // ---- performance benchmark (no frame cap) ----
    {
        std::ofstream bf(dir + "/benchmark.csv");
        bf << "view,mode,ms,fps,draw_calls,triangles" << std::endl;
        auto bench = [&](const char* label) {
            for (int pass = 0; pass < 4; ++pass) {
                if (pass >= 2 && std::getenv("JRS_BENCH_EXTRA") == nullptr) break;
                m_renderer.settings.enhanced = pass != 1;
                m_renderer.settings.shadows = pass != 3;
                for (int g = 0; g < LG_COUNT; ++g) LightManager::get().groupOn[g] = pass != 2;
                for (int i = 0; i < 5; ++i) { update(1.0f / 60); render(w, h, false); glfwSwapBuffers(m_win); }
                glFinish();
                double t0 = glfwGetTime();
                const int N = 60;
                for (int i = 0; i < N; ++i) { update(1.0f / 60); render(w, h, false); glfwSwapBuffers(m_win); glfwPollEvents(); }
                glFinish();
                double ms = (glfwGetTime() - t0) * 1000.0 / N;
                const char* modeNames[4] = {"enhanced", "basic", "enhanced_no_lamps", "enhanced_no_shadows"};
                bf << label << "," << modeNames[pass] << "," << ms << "," << 1000.0 / ms << ","
                   << m_renderer.statDrawCalls << "," << m_renderer.statTriangles << std::endl;
            }
            m_renderer.settings.enhanced = true;
            m_renderer.settings.shadows = true;
            for (int g = 0; g < LG_COUNT; ++g) LightManager::get().groupOn[g] = true;
        };
        hour(10.5f);
        freeCam(Vec3(-4.0f, 11.0f, 24.5f), Vec3(1.5f, 1.2f, 1.0f), 64);   // above the far edge of the lot: no plants in front
        bench("exterior");
        fpsCam(Vec3(0.0f, 0, 4.3f), 180.0f, -6.0f, 75);
        bench("lounge");
        fpsCam(Vec3(-36.0f, terrainHeight(-36.0f, -30.0f), -30.0f), 200.0f, 4.0f, 75);
        bench("jungle");
        hour(21.0f);
        freeCam(Vec3(28.0f, 3.0f, 19.0f), Vec3(0.0f, 1.0f, 14.0f), 65);
        bench("parking_night");
        hour(11.0f);
        freeCam(Vec3(5.0f, 70.0f, 70.0f), Vec3(0, 0, 4.0f), 55);
        bench("aerial");
    }
    std::cout << "Report screenshots written to " << dir << std::endl;
}
