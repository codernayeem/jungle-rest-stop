#include "Building.h"
#include "../game/Audio.h"
#include "Furniture.h"
#include "Layout.h"
#include "Lights.h"
#include "Materials.h"
#include "Nature.h"
#include "../game/Interaction.h"
#include "../game/Messages.h"
#include <algorithm>
#include <cmath>

using namespace Layout;

namespace {

// Colours of each room
const Vec3 LOUNGE_WALL(0.86f, 0.80f, 0.70f);
const Vec3 ATM_WALL(0.78f, 0.83f, 0.88f);
const Vec3 LIB_WALL(0.64f, 0.72f, 0.60f);

struct Opening {
    float a0, a1;   // along the wall
    float y0, y1;   // bottom / top of the hole
    bool window;
};

// -----------------------------------------------------------------------------
// Builds one straight wall with holes for doors / windows.
//   alongX = true : wall runs along X at z = c (from a0 to a1)
//   alongX = false: wall runs along Z at x = c
//   matNeg / matPos: material of the side facing -axis / +axis
// The wall is made of two half-thickness layers so the inside can be plaster
// and the outside brick. Windows get a glass pane, an aluminium frame and sills.
// -----------------------------------------------------------------------------
void buildWall(DrawSink& s, CollisionWorld& col, bool alongX, float c, float a0, float a1, std::vector<Opening> ops,
               const Material& matNeg, const Material& matPos, bool skirtNeg, bool skirtPos) {
    std::sort(ops.begin(), ops.end(), [](const Opening& a, const Opening& b) { return a.a0 < b.a0; });
    const float T = WALL_T, H = WALL_H;
    Material skirt = Mat::wood(Vec3(0.25f, 0.17f, 0.10f));
    Material frame = Mat::darkMetal();
    Material glassM = Mat::glass(0.20f);
    Material sill = Mat::marble(Vec3(0.85f, 0.84f, 0.80f));

    auto piece = [&](float p0, float p1, float y0, float y1, bool collide) {
        if (p1 - p0 < 0.001f || y1 - y0 < 0.001f) return;
        float mid = (p0 + p1) * 0.5f, len = p1 - p0, ym = (y0 + y1) * 0.5f, h = y1 - y0;
        if (alongX) {
            s.add(M_CUBE, scale(translate(Mat4(), Vec3(mid, ym, c - T * 0.25f)), Vec3(len, h, T * 0.5f)), matNeg);
            s.add(M_CUBE, scale(translate(Mat4(), Vec3(mid, ym, c + T * 0.25f)), Vec3(len, h, T * 0.5f)), matPos);
            if (collide) col.addBox(p0, c - T * 0.5f, p1, c + T * 0.5f, y0, y1);
            if (y0 < 0.01f) {
                if (skirtNeg) s.add(M_CUBE, scale(translate(Mat4(), Vec3(mid, 0.05f, c - T * 0.5f - 0.01f)), Vec3(len, 0.10f, 0.02f)), skirt);
                if (skirtPos) s.add(M_CUBE, scale(translate(Mat4(), Vec3(mid, 0.05f, c + T * 0.5f + 0.01f)), Vec3(len, 0.10f, 0.02f)), skirt);
            }
        } else {
            s.add(M_CUBE, scale(translate(Mat4(), Vec3(c - T * 0.25f, ym, mid)), Vec3(T * 0.5f, h, len)), matNeg);
            s.add(M_CUBE, scale(translate(Mat4(), Vec3(c + T * 0.25f, ym, mid)), Vec3(T * 0.5f, h, len)), matPos);
            if (collide) col.addBox(c - T * 0.5f, p0, c + T * 0.5f, p1, y0, y1);
            if (y0 < 0.01f) {
                if (skirtNeg) s.add(M_CUBE, scale(translate(Mat4(), Vec3(c - T * 0.5f - 0.01f, 0.05f, mid)), Vec3(0.02f, 0.10f, len)), skirt);
                if (skirtPos) s.add(M_CUBE, scale(translate(Mat4(), Vec3(c + T * 0.5f + 0.01f, 0.05f, mid)), Vec3(0.02f, 0.10f, len)), skirt);
            }
        }
    };
    auto boxAt = [&](float along, float y, float across, float lenAlong, float h, float depth, const Material& m) {
        if (alongX) s.add(M_CUBE, scale(translate(Mat4(), Vec3(along, y, c + across)), Vec3(lenAlong, h, depth)), m);
        else s.add(M_CUBE, scale(translate(Mat4(), Vec3(c + across, y, along)), Vec3(depth, h, lenAlong)), m);
    };

    float cur = a0;
    for (const auto& o : ops) {
        piece(cur, o.a0, 0.0f, H, true);
        piece(o.a0, o.a1, 0.0f, o.y0, true);    // below window
        piece(o.a0, o.a1, o.y1, H, false);      // above opening (lintel)
        float mid = (o.a0 + o.a1) * 0.5f, w = o.a1 - o.a0;
        if (o.window) {
            float ym = (o.y0 + o.y1) * 0.5f, h = o.y1 - o.y0;
            boxAt(mid, ym, 0.0f, w, h, 0.02f, glassM);                               // glass
            boxAt(mid, o.y0 + 0.03f, 0.0f, w, 0.06f, T + 0.02f, frame);              // bottom frame
            boxAt(mid, o.y1 - 0.03f, 0.0f, w, 0.06f, T + 0.02f, frame);              // top frame
            boxAt(o.a0 + 0.03f, ym, 0.0f, 0.06f, h - 0.12f, T + 0.02f, frame);       // side frames (between top/bottom)
            boxAt(o.a1 - 0.03f, ym, 0.0f, 0.06f, h - 0.12f, T + 0.02f, frame);
            boxAt(mid, ym, 0.0f, 0.04f, h - 0.12f, 0.06f, frame);                     // mullion
            boxAt(mid, o.y0 - 0.01f, 0.0f, w + 0.1f, 0.04f, T + 0.16f, sill);       // sill (1 cm above the wall top, no shared face)
        } else {
            // door frame trim
            // trims overlap the wall end by 2 cm, so none of their faces lies on a wall face
            boxAt(o.a0 + 0.01f, (o.y1 - 0.04f) * 0.5f, 0.0f, 0.06f, o.y1 - 0.04f, T + 0.05f, frame);
            boxAt(o.a1 - 0.01f, (o.y1 - 0.04f) * 0.5f, 0.0f, 0.06f, o.y1 - 0.04f, T + 0.05f, frame);
            boxAt(mid, o.y1 - 0.01f, 0.0f, w + 0.04f, 0.06f, T + 0.05f, frame);
        }
        cur = o.a1;
    }
    piece(cur, a1, 0.0f, H, true);
}

Material plasterOf(const Vec3& c) { return Mat::plaster(c); }

}  // namespace

// =============================================================================
//  STATIC BUILDING
// =============================================================================
void Building::buildStatic(DrawSink& s, CollisionWorld& col) {
    const float T = WALL_T;
    Material brick = Mat::brick();
    Material lounge = plasterOf(LOUNGE_WALL), atm = plasterOf(ATM_WALL), lib = plasterOf(LIB_WALL);

    // ---------------- floors ----------------
    Node w(s);
    w.box({(ATM_X0 + ATM_X1) * 0.5f, -0.05f, 0}, {ATM_X1 - ATM_X0, 0.1f, Z_FRONT - Z_BACK},
          Material(Vec3(0.30f, 0.34f, 0.42f), 0.05f, 8.0f).tx(TEX_CARPET, 1.0f).fl(MF_NO_SHADOW));
    w.box({0, -0.05f, 0}, {LNG_X1 - LNG_X0, 0.1f, Z_FRONT - Z_BACK},
          Material(Vec3(0.86f, 0.84f, 0.80f), 0.45f, 80.0f).tx(TEX_TILE, 0.5f).fl(MF_NO_SHADOW));
    w.box({(LIB_X0 + LIB_X1) * 0.5f, -0.05f, 0}, {LIB_X1 - LIB_X0, 0.1f, Z_FRONT - Z_BACK},
          Mat::wood(Vec3(0.62f, 0.44f, 0.28f), 0.6f).fl(MF_NO_SHADOW));
    // exterior plinth (concrete base strip)
    Material plinth = Mat::concrete(Vec3(0.55f, 0.55f, 0.53f), 0.6f);
    // front plinth in two parts, leaving the entrance open
    {
        float l0 = BLD_X0 - 0.05f, l1 = -ENTRANCE_HALF - 0.05f, r0 = ENTRANCE_HALF + 0.05f, r1 = BLD_X1 + 0.05f;
        w.box({(l0 + l1) * 0.5f, 0.17f, Z_FRONT + T * 0.5f + 0.03f}, {l1 - l0, 0.34f, 0.06f}, plinth);
        w.box({(r0 + r1) * 0.5f, 0.17f, Z_FRONT + T * 0.5f + 0.03f}, {r1 - r0, 0.34f, 0.06f}, plinth);
    }
    w.box({1.5f, 0.17f, Z_BACK - T * 0.5f - 0.03f}, {BLD_X1 - BLD_X0 + 0.1f, 0.34f, 0.06f}, plinth);
    w.box({ATM_X0 - T * 0.5f - 0.03f, 0.17f, 0}, {0.06f, 0.34f, Z_FRONT - Z_BACK + 0.3f}, plinth);
    w.box({LIB_X1 + T * 0.5f + 0.03f, 0.17f, 0}, {0.06f, 0.34f, Z_FRONT - Z_BACK + 0.3f}, plinth);

    // ---------------- exterior walls ----------------
    // front wall (z = 5): three parts, one per room (inside colour changes)
    buildWall(s, col, true, Z_FRONT, ATM_X0 - T * 0.5f, ATM_X1, {{-11.0f, -8.0f, 0.9f, 2.6f, true}}, atm, brick, true, false);
    buildWall(s, col, true, Z_FRONT, LNG_X0, LNG_X1,
              {{-6.2f, -2.2f, 0.8f, 2.8f, true}, {2.2f, 6.2f, 0.8f, 2.8f, true}, {-ENTRANCE_HALF, ENTRANCE_HALF, 0.0f, 2.6f, false}},
              lounge, brick, true, false);
    buildWall(s, col, true, Z_FRONT, LIB_X0, LIB_X1 + T * 0.5f, {{11.8f, 14.4f, 0.9f, 2.6f, true}}, lib, brick, true, false);
    // back wall (z = -5)
    buildWall(s, col, true, Z_BACK, ATM_X0 - T * 0.5f, ATM_X1, {}, brick, atm, false, true);
    buildWall(s, col, true, Z_BACK, LNG_X0, LNG_X1, {}, brick, lounge, false, true);
    buildWall(s, col, true, Z_BACK, LIB_X0, LIB_X1 + T * 0.5f, {{9.5f, 12.5f, 2.3f, 3.1f, true}}, brick, lib, false, true);
    // side walls
    buildWall(s, col, false, ATM_X0, Z_BACK + T * 0.5f, Z_FRONT - T * 0.5f, {{-1.0f, 3.0f, 0.9f, 2.6f, true}}, brick, atm, false, true);
    buildWall(s, col, false, LIB_X1, Z_BACK + T * 0.5f, Z_FRONT - T * 0.5f, {{1.6f, 4.2f, 0.9f, 2.6f, true}}, lib, brick, true, false);
    // interior partitions with doorways
    buildWall(s, col, false, LNG_X0, Z_BACK + T * 0.5f, Z_FRONT - T * 0.5f, {{DOOR_ATM_Z0, DOOR_ATM_Z1, 0.0f, DOOR_H, false}}, atm, lounge, true, true);
    buildWall(s, col, false, LNG_X1, Z_BACK + T * 0.5f, Z_FRONT - T * 0.5f, {{DOOR_LIB_Z0, DOOR_LIB_Z1, 0.0f, DOOR_H, false}}, lounge, lib, true, true);

    // cornice band around the top of the outside walls
    Material cornice = Mat::concrete(Vec3(0.80f, 0.78f, 0.74f), 0.6f);
    w.box({1.5f, 3.45f, Z_FRONT + T * 0.5f + 0.05f}, {BLD_X1 - BLD_X0 + 0.3f, 0.3f, 0.1f}, cornice);
    w.box({1.5f, 3.45f, Z_BACK - T * 0.5f - 0.05f}, {BLD_X1 - BLD_X0 + 0.3f, 0.3f, 0.1f}, cornice);
    w.box({ATM_X0 - T * 0.5f - 0.05f, 3.45f, 0}, {0.1f, 0.3f, Z_FRONT - Z_BACK + 0.4f}, cornice);
    w.box({LIB_X1 + T * 0.5f + 0.05f, 3.45f, 0}, {0.1f, 0.3f, Z_FRONT - Z_BACK + 0.4f}, cornice);

    // porch canopy over the entrance
    Material canopy = Mat::concrete(Vec3(0.85f, 0.84f, 0.82f), 0.5f);
    w.box({0, 3.06f, 6.35f}, {5.4f, 0.14f, 2.5f}, canopy);
    w.box({0, 3.16f, 7.57f}, {5.44f, 0.10f, 0.1f}, Mat::darkMetal());   // fascia sticks 2 cm out of the canopy
    for (int sx = -1; sx <= 1; sx += 2) {
        w.cyl({sx * 2.45f, 1.5f, 7.35f}, {0.16f, 3.0f, 0.16f}, Mat::chrome());   // polished chrome posts
        col.addCircle(sx * 2.45f, 7.35f, 0.12f);
    }
    // entrance door header with sensor
    w.box({0, 2.68f, Z_FRONT + 0.2f}, {3.8f, 0.16f, 0.2f}, Mat::darkMetal());
    w.box({0, 2.56f, Z_FRONT + 0.31f}, {0.3f, 0.06f, 0.04f}, Mat::black());
    // door mat
    w.box({0, 0.01f, 4.3f}, {2.0f, 0.02f, 1.0f}, Mat::fabric(Vec3(0.30f, 0.24f, 0.18f), 8.0f).tx(TEX_CARPET, 2.0f).fl(MF_NO_SHADOW));
    w.box({0, 0.075f, 6.0f}, {2.0f, 0.02f, 1.0f}, Mat::fabric(Vec3(0.30f, 0.26f, 0.2f), 8.0f).tx(TEX_CARPET, 2.0f).fl(MF_NO_SHADOW));

    // sliding door collider (enabled while the doors are closed)
    col.addBox(-ENTRANCE_HALF, Z_FRONT - 0.1f, ENTRANCE_HALF, Z_FRONT + 0.25f, 0.0f, 2.6f, &m_doorClosed);

    buildInterior(s, col);
    buildRoadAndLot(s, col);
}

// =============================================================================
//  INTERIOR FURNITURE (static parts)
// =============================================================================
void Building::buildInterior(DrawSink& s, CollisionWorld& col) {
    using namespace Furniture;
    Node w(s);
    // ---------------- MAIN LOUNGE ----------------
    // accent wall behind the TV: dark panel with vertical wooden slats
    w.box({0, 1.8f, Z_BACK + 0.11f}, {3.8f, 3.2f, 0.02f}, Mat::plaster(Vec3(0.16f, 0.27f, 0.30f)));
    for (int i = 0; i < 17; ++i) {
        float x = -1.8f + i * 0.225f;
        if (std::fabs(x) < 1.05f) continue;   // TV covers the middle
        w.box({x, 1.8f, Z_BACK + 0.14f}, {0.06f, 3.2f, 0.04f}, Mat::wood(Vec3(0.55f, 0.36f, 0.2f), 2.0f));
    }
    // TV console + sound bar
    Node con = w.at({0, 0, -4.66f});
    con.box({0, 0.27f, 0}, {2.6f, 0.46f, 0.44f}, Mat::wood(Vec3(0.28f, 0.18f, 0.10f), 1.2f));
    for (int i = 0; i < 3; ++i) con.box({-0.87f + i * 0.87f, 0.27f, 0.225f}, {0.84f, 0.40f, 0.01f}, Mat::wood(Vec3(0.34f, 0.22f, 0.13f), 1.2f));
    con.box({0, 0.04f, 0}, {2.5f, 0.08f, 0.40f}, Mat::black());
    con.box({0, 0.54f, 0.05f}, {1.0f, 0.08f, 0.10f}, Mat::black());          // sound bar
    con.box({0.85f, 0.53f, 0.0f}, {0.3f, 0.06f, 0.22f}, Mat::black());       // set top box
    col.addBox(-1.3f, -4.9f, 1.3f, -4.44f, 0, 0.6f);

    // main sofa facing the TV + armchairs + coffee table + rug
    rug(w.at({0, 0, -1.5f}), 4.2f, 3.0f, Vec3(0.65f, 0.55f, 0.42f), Vec3(0.35f, 0.25f, 0.18f));
    sofa(w.at({0, 0, -0.6f}, {0, 180, 0}), 3, Vec3(0.28f, 0.36f, 0.48f));
    col.addBox(-1.24f, -1.05f, 1.24f, -0.15f, 0, 0.9f);
    armchair(w.at({-2.45f, 0, -2.3f}, {0, 90, 0}), Vec3(0.55f, 0.30f, 0.22f));
    armchair(w.at({2.45f, 0, -2.3f}, {0, -90, 0}), Vec3(0.55f, 0.30f, 0.22f));
    col.addBox(-2.9f, -2.8f, -2.0f, -1.8f, 0, 0.9f);
    col.addBox(2.0f, -2.8f, 2.9f, -1.8f, 0, 0.9f);
    coffeeTable(w.at({0, 0, -2.25f}), 1.2f, 0.6f);
    col.addBox(-0.6f, -2.55f, 0.6f, -1.95f, 0, 0.5f);

    // front waiting sofas by the windows, with side tables
    for (int sx = -1; sx <= 1; sx += 2) {
        float cx = sx * 4.6f;
        sofa(w.at({cx, 0, 4.3f}, {0, 180, 0}), 2, Vec3(0.30f, 0.45f, 0.36f));
        col.addBox(cx - 0.9f, 3.85f, cx + 0.9f, 4.85f, 0, 0.9f);
        coffeeTable(w.at({cx, 0, 3.1f}), 1.0f, 0.5f);
        col.addBox(cx - 0.5f, 2.85f, cx + 0.5f, 3.35f, 0, 0.5f);
        sideTable(w.at({cx - 1.25f, 0, 4.45f}));
        sideTable(w.at({cx + 1.25f, 0, 4.45f}));
        col.addCircle(cx - 1.25f, 4.45f, 0.25f);
        col.addCircle(cx + 1.25f, 4.45f, 0.25f);
    }
    // coffee counter with wall cabinets, water cooler and bin
    counter(w.at({-5.0f, 0, -4.575f}), 3.2f, 0.65f, 0.92f);
    col.addBox(-6.6f, -4.9f, -3.4f, -4.25f, 0, 1.0f);
    wallCabinet(w.at({-5.0f, 2.15f, -4.72f}), 3.2f);
    w.box({-5.0f, 1.25f, Z_BACK + 0.11f}, {3.2f, 0.62f, 0.01f}, Material(Vec3(0.85f, 0.88f, 0.86f), 0.5f, 64.0f).tx(TEX_TILE, 4.0f));
    // items on counter: cup stack, sugar jar, napkins, small plant
    Node top = w.at({0, 0.92f, -4.55f});
    for (int i = 0; i < 6; ++i) top.tcone({-6.2f, 0.05f + i * 0.025f, 0.05f}, {0.08f, 0.09f, 0.08f}, Mat::plastic(Vec3(0.95f)), {180, 0, 0});
    top.cyl({-3.75f, 0.07f, 0.0f}, {0.1f, 0.14f, 0.1f}, Mat::glass(0.5f, Vec3(0.9f, 0.9f, 0.85f)));
    top.cyl({-3.75f, 0.05f, 0.0f}, {0.09f, 0.08f, 0.09f}, Mat::matte(Vec3(0.97f)));
    top.box({-3.95f, 0.06f, 0.05f}, {0.12f, 0.12f, 0.08f}, Mat::chrome());
    pottedPlant(top.at({-6.45f, 0, -0.05f}), 0.35f, 0);
    // water cooler
    Node wc = w.at({-2.85f, 0, -4.6f});
    wc.box({0, 0.55f, 0}, {0.34f, 1.1f, 0.34f}, Mat::plastic(Vec3(0.92f, 0.93f, 0.95f)));
    wc.box({0, 0.82f, 0.172f}, {0.2f, 0.16f, 0.01f}, Mat::black());
    wc.box({-0.05f, 0.86f, 0.19f}, {0.03f, 0.04f, 0.03f}, Mat::plastic(Vec3(0.2f, 0.4f, 0.95f)));
    wc.box({0.05f, 0.86f, 0.19f}, {0.03f, 0.04f, 0.03f}, Mat::plastic(Vec3(0.95f, 0.2f, 0.15f)));
    wc.cyl({0, 1.32f, 0}, {0.30f, 0.42f, 0.30f}, Mat::glass(0.45f, Vec3(0.55f, 0.75f, 0.95f)));
    col.addBox(-3.05f, -4.8f, -2.65f, -4.4f, 0, 1.5f);
    trashBin(w.at({-2.2f, 0, -4.55f}), Vec3(0.2f, 0.45f, 0.25f));
    col.addCircle(-2.2f, -4.55f, 0.22f);
    // plants
    pottedPlant(w.at({-6.4f, 0, 0.6f}), 1.3f, 0);
    pottedPlant(w.at({6.4f, 0, 2.6f}), 1.2f, 1);
    pottedPlant(w.at({1.6f, 0, -4.5f}), 0.85f, 2);
    pottedPlant(w.at({6.35f, 0, -4.4f}), 1.1f, 0);
    pottedPlant(w.at({-1.85f, 0, 4.55f}), 0.75f, 2);
    pottedPlant(w.at({1.85f, 0, 4.55f}), 0.75f, 2);
    col.addCircle(-6.4f, 0.6f, 0.3f);
    col.addCircle(6.4f, 2.6f, 0.3f);
    col.addCircle(6.35f, -4.4f, 0.3f);
    // paintings
    painting(w.at({LNG_X0 + 0.11f, 1.85f, 1.0f}, {0, 90, 0}), 1.4f, 0.9f, 0);
    painting(w.at({LNG_X1 - 0.11f, 1.85f, 2.6f}, {0, -90, 0}), 1.2f, 0.9f, 1);
    painting(w.at({-3.8f, 1.9f, Z_BACK + 0.11f}), 0.7f, 0.5f, 2);
    // smoke detectors
    smokeDetector(w.at({0, WALL_H, 1.5f}));
    smokeDetector(w.at({-9.5f, WALL_H, 1.0f}));
    smokeDetector(w.at({11.0f, WALL_H, 1.0f}));

    // ---------------- ATM ROOM ----------------
    for (int i = 0; i < 2; ++i) {
        float x = i == 0 ? -10.4f : -8.6f;
        stanchion(w.at({x, 0, -2.5f}));
        stanchion(w.at({x, 0, -0.5f}));
        w.part(M_CYLINDER, {x, 0.88f, -1.5f}, {0.03f, 2.0f, 0.03f}, Mat::fabric(Vec3(0.7f, 0.1f, 0.12f), 6.0f), {90, 0, 0});
        col.addBox(x - 0.08f, -2.55f, x + 0.08f, -0.45f, 0, 1.0f);
    }
    bench(w.at({-11.55f, 0, 1.8f}, {0, 90, 0}), 1.8f);
    col.addBox(-11.9f, 0.9f, -11.2f, 2.7f, 0, 0.6f);
    counter(w.at({-7.45f, 0, 2.8f}, {0, -90, 0}), 1.6f, 0.5f, 1.02f);
    col.addBox(-7.75f, 2.0f, -7.1f, 3.6f, 0, 1.1f);
    w.cyl({-7.45f, 1.08f, 2.4f}, {0.08f, 0.12f, 0.08f}, Mat::darkMetal());
    w.box({-7.45f, 1.035f, 3.0f}, {0.3f, 0.01f, 0.22f}, Mat::matte(Vec3(0.95f)), {0, 12, 0});
    pottedPlant(w.at({-11.5f, 0, -4.45f}), 1.0f, 1);
    col.addCircle(-11.5f, -4.45f, 0.3f);
    trashBin(w.at({-10.9f, 0, -4.6f}), Vec3(0.18f, 0.25f, 0.45f));
    col.addCircle(-10.9f, -4.6f, 0.22f);
    signBoard(w.at({-11.9f + 0.05f, 2.0f, -2.0f}, {0, 90, 0}), 1.6f, 0.7f, Vec3(0.10f, 0.30f, 0.65f), false);
    painting(w.at({-7.9f, 1.9f, Z_BACK + 0.11f}), 0.8f, 0.6f, 0);

    // ---------------- LIBRARY ----------------
    for (int i = 0; i < 3; ++i) {
        bookshelf(w.at({9.0f + i * 2.0f, 0, Z_BACK + 0.31f}), 1.8f, 2.3f);
        col.addBox(8.1f + i * 2.0f, Z_BACK + 0.1f, 9.9f + i * 2.0f, Z_BACK + 0.52f, 0, 2.3f);
    }
    for (int i = 0; i < 2; ++i) {
        float z = i == 0 ? -2.3f : -0.1f;
        bookshelf(w.at({LIB_X1 - 0.31f, 0, z}, {0, -90, 0}), 1.8f, 2.3f);
        col.addBox(LIB_X1 - 0.52f, z - 0.9f, LIB_X1 - 0.1f, z + 0.9f, 0, 2.3f);
    }
    readingTable(w.at({11.0f, 0, -1.7f}));
    col.addCircle(11.0f, -1.7f, 0.55f);
    chair(w.at({11.0f, 0, -0.9f}, {0, 180, 0}), Vec3(0.45f, 0.2f, 0.15f));
    chair(w.at({10.2f, 0, -2.1f}, {0, 60, 0}), Vec3(0.45f, 0.2f, 0.15f));
    chair(w.at({11.8f, 0, -2.1f}, {0, -60, 0}), Vec3(0.45f, 0.2f, 0.15f));
    rug(w.at({10.4f, 0, 2.2f}), 3.6f, 2.6f, Vec3(0.55f, 0.20f, 0.18f), Vec3(0.85f, 0.70f, 0.40f));
    armchair(w.at({9.0f, 0, 2.15f}), Vec3(0.30f, 0.40f, 0.30f));
    armchair(w.at({11.2f, 0, 2.15f}), Vec3(0.30f, 0.40f, 0.30f));
    col.addBox(8.55f, 1.7f, 9.45f, 2.6f, 0, 0.9f);
    col.addBox(10.75f, 1.7f, 11.65f, 2.6f, 0, 0.9f);
    sideTable(w.at({12.4f, 0, 2.2f}));
    col.addCircle(12.4f, 2.2f, 0.25f);
    pottedPlant(w.at({14.4f, 0, 4.45f}), 1.2f, 0);
    pottedPlant(w.at({7.6f, 0, -4.45f}), 1.0f, 1);
    col.addCircle(14.4f, 4.45f, 0.3f);
    painting(w.at({LIB_X0 + 0.11f, 1.8f, -3.2f}, {0, 90, 0}), 1.0f, 0.8f, 2);

    // ---------------- light switch positions ----------------
    m_switches = {
        {Vec3(-1.75f, 1.3f, Z_FRONT - 0.1f), 180.0f, LG_LOUNGE, "lounge lights"},
        {Vec3(-1.95f, 1.3f, Z_FRONT - 0.1f), 180.0f, LG_SPOTS, "spot lights"},
        {Vec3(LNG_X0 - 0.1f, 1.3f, -1.4f), -90.0f, LG_ATM, "ATM room light"},
        {Vec3(LNG_X1 + 0.1f, 1.3f, 0.8f), 90.0f, LG_LIBRARY, "library lights"},
    };
}

// =============================================================================
//  ROAD, SIDEWALK, PARKING LOT
// =============================================================================
void Building::buildRoadAndLot(DrawSink& s, CollisionWorld& col) {
    Node w(s);
    Material asphalt = Material(Vec3(0.26f, 0.26f, 0.27f), 0.08f, 12.0f).tx(TEX_ASPHALT, 0.25f).fl(MF_NO_SHADOW);
    Material concrete = Mat::concrete(Vec3(0.70f, 0.69f, 0.66f), 0.3f).fl(MF_NO_SHADOW);
    Material line = Material(Vec3(0.92f, 0.92f, 0.88f), 0.1f, 8.0f).fl(MF_NO_SHADOW);
    Material yellow = Material(Vec3(0.95f, 0.78f, 0.15f), 0.1f, 8.0f).fl(MF_NO_SHADOW);

    // sidewalk in front of and around the building
    w.box({1.5f, 0.03f, (Z_FRONT + SIDEWALK_Z1) * 0.5f}, {31.0f, 0.06f, SIDEWALK_Z1 - Z_FRONT}, concrete);
    w.box({1.5f, 0.07f, SIDEWALK_Z1}, {31.0f, 0.14f, 0.2f}, Mat::concrete(Vec3(0.78f, 0.77f, 0.74f), 0.8f));
    w.box({1.5f, 0.02f, Z_BACK - 0.8f}, {28.0f, 0.04f, 1.4f}, concrete);   // back path
    // parking lot
    w.box({0, 0.0f, (LOT_Z0 + LOT_Z1) * 0.5f}, {LOT_X1 - LOT_X0, 0.03f, LOT_Z1 - LOT_Z0}, asphalt);
    w.box({0, 0.06f, LOT_Z1 + 0.1f}, {LOT_X1 - LOT_X0, 0.12f, 0.2f}, concrete);
    for (int i = 0; i < SPOTS_PER_ROW; ++i) {
        float sx = spotX(i);
        for (int row = 0; row < 2; ++row) {
            float zc = row == 0 ? SPOT_NORTH_Z : SPOT_SOUTH_Z;
            for (int e = -1; e <= 1; e += 2)
                w.box({sx + e * 1.5f, 0.024f, zc}, {0.1f, 0.006f, 4.6f}, line);
            float stopZ = row == 0 ? zc - 2.0f : zc + 2.0f;
            w.box({sx, 0.06f, stopZ}, {1.6f, 0.12f, 0.18f}, Mat::concrete(Vec3(0.85f, 0.85f, 0.8f), 1.0f));
        }
    }
    // zebra crossing at x = 0 + walkway between the rows
    for (int i = 0; i < 6; ++i) w.box({0, 0.024f, 13.9f + i * 0.84f}, {2.4f, 0.006f, 0.42f}, line);
    w.box({0, 0.04f, 11.0f}, {2.6f, 0.06f, 4.8f}, concrete);
    w.box({0, 0.04f, 21.0f}, {2.6f, 0.06f, 4.8f}, concrete);
    // one-way arrows (pointing west, -X)
    for (int i = -1; i <= 1; i += 2) {
        Node a = w.at({i * 10.0f, 0.024f, AISLE_Z});
        a.box({0.4f, 0, 0}, {1.6f, 0.006f, 0.25f}, line);
        a.box({-0.55f, 0, 0.18f}, {0.6f, 0.006f, 0.2f}, line, {0, -40, 0});
        a.box({-0.55f, 0, -0.18f}, {0.6f, 0.006f, 0.2f}, line, {0, 40, 0});
    }
    // road through the jungle (segments following the curve)
    // Short straight boxes along the curve. They overlap by 40 cm, so the small
    // wedge-shaped gap on the outside of each bend is always covered.
    Material shoulder = Material(Vec3(0.40f, 0.33f, 0.24f), 0.02f, 4.0f).tx(TEX_FOREST_FLOOR, 0.4f).fl(MF_NO_SHADOW);
    const float step = 2.0f, ov = 0.4f;
    for (int side = -1; side <= 1; side += 2) {
        int k = 0;
        for (float a = LOT_X1 + ov * 0.5f; a < WORLD_HALF + 4.0f; a += step, ++k) {   // first box starts at the lot edge
            float x0 = side * a, x1 = side * (a + step);
            float z0 = roadCenterZ(x0), z1 = roadCenterZ(x1);
            float dx = x1 - x0, dz = z1 - z0, len = std::sqrt(dx * dx + dz * dz);
            float yaw = toDegrees(std::atan2(-dz, dx));
            Node seg = w.at({(x0 + x1) * 0.5f, 0, (z0 + z1) * 0.5f}, {0, yaw, 0});
            // overlapping neighbours: every second one is 6 mm higher (no z-fighting)
            float lift = (k % 2) * 0.006f;
            seg.box({0, lift, 0}, {len + ov, 0.03f, 7.4f}, asphalt);
            seg.box({0, 0.022f + lift, 3.35f}, {len + ov, 0.006f, 0.14f}, line);
            seg.box({0, 0.022f + lift, -3.35f}, {len + ov, 0.006f, 0.14f}, line);
            if (k % 2 == 0) seg.box({0, 0.022f + lift, 0}, {len * 0.5f, 0.006f, 0.12f}, yellow);
            seg.box({0, -0.005f + lift, 4.4f}, {len + ov, 0.02f, 1.4f}, shoulder);
            seg.box({0, -0.005f + lift, -4.4f}, {len + ov, 0.02f, 1.4f}, shoulder);
        }
    }

    // lamp posts (heads drawn dynamically so they can glow)
    m_lotLamps = {Vec3(-18, 0, 8.25f), Vec3(-6, 0, 8.25f), Vec3(6, 0, 8.25f), Vec3(18, 0, 8.25f),
                  Vec3(-18, 0, 23.75f), Vec3(-6, 0, 23.75f), Vec3(6, 0, 23.75f), Vec3(18, 0, 23.75f)};
    for (auto& p : m_lotLamps) col.addCircle(p.x, p.z, 0.2f);
    m_roadLamps.clear();
    for (int side = -1; side <= 1; side += 2)
        for (float a = 40.0f; a < 125.0f; a += 28.0f) {
            float x = side * a;
            m_roadLamps.push_back(Vec3(x, 0, roadCenterZ(x) - 4.6f));
        }
    for (auto& p : m_roadLamps) col.addCircle(p.x, p.z, 0.2f);

    // outdoor benches + bins near the entrance
    Furniture::bench(w.at({-4.0f, 0.06f, 5.75f}, {0, 180, 0}), 1.8f);
    Furniture::bench(w.at({4.0f, 0.06f, 5.75f}, {0, 180, 0}), 1.8f);
    col.addBox(-4.9f, 5.45f, -3.1f, 6.05f, 0, 0.6f);
    col.addBox(3.1f, 5.45f, 4.9f, 6.05f, 0, 0.6f);
    Furniture::trashBin(w.at({-2.9f, 0.06f, 5.6f}), Vec3(0.2f, 0.45f, 0.25f));
    Furniture::trashBin(w.at({2.9f, 0.06f, 5.6f}), Vec3(0.2f, 0.45f, 0.25f));
    col.addCircle(-2.9f, 5.6f, 0.22f);
    col.addCircle(2.9f, 5.6f, 0.22f);
    // parking sign post
    Node ps = w.at({LOT_X1 + 0.9f, 0, 10.3f});   // on the grass beside the road, not on it
    ps.cyl({0, 1.2f, 0}, {0.08f, 2.4f, 0.08f}, Mat::steel());
    ps.box({0, 2.3f, 0.05f}, {0.7f, 0.7f, 0.04f}, Mat::plastic(Vec3(0.12f, 0.3f, 0.75f)), {0, -90, 0});
    col.addCircle(LOT_X1 + 0.9f, 10.3f, 0.1f);

    // lawn edge stones at both ends of the lot, with a gap where the road comes in
    const float roadZ0 = AISLE_Z - 3.7f, roadZ1 = AISLE_Z + 3.7f;
    for (float ex : {LOT_X0 - 0.1f, LOT_X1 + 0.1f}) {
        Node(s).box({ex, 0.06f, (LOT_Z0 + roadZ0) * 0.5f}, {0.2f, 0.12f, roadZ0 - LOT_Z0}, concrete);
        Node(s).box({ex, 0.06f, (roadZ1 + LOT_Z1) * 0.5f}, {0.2f, 0.12f, LOT_Z1 - roadZ1}, concrete);
    }
}

// =============================================================================
//  LIGHTS
// =============================================================================
void Building::registerLights() {
    LightManager& lm = LightManager::get();
    lm.clearStatic();
    const Vec3 warm(1.0f, 0.82f, 0.60f), cool(0.88f, 0.94f, 1.0f), sodium(1.0f, 0.72f, 0.42f);
    // lounge pendant bulbs
    const Vec3 loungePend[] = {Vec3(-3.6f, 2.78f, 2.4f), Vec3(3.6f, 2.78f, 2.4f), Vec3(-3.6f, 2.78f, -2.0f), Vec3(3.6f, 2.78f, -2.0f)};
    for (auto& p : loungePend) {
        LightDef l; l.pos = p; l.color = warm; l.intensity = 3.0f; l.range = 8.5f; l.group = LG_LOUNGE;
        lm.addStatic(l);
    }
    // recessed spot lights (vending row, TV wall, coffee counter)
    auto spot = [&](const Vec3& p, const Vec3& d, float inten, float range, float in, float out, int g, const Vec3& c) {
        LightDef l; l.kind = LIGHT_SPOT; l.pos = p; l.dir = normalize(d); l.intensity = inten; l.range = range;
        l.innerDeg = in; l.outerDeg = out; l.group = g; l.color = c;
        lm.addStatic(l);
    };
    spot(Vec3(3.7f, 3.55f, -3.2f), Vec3(0, -0.85f, -0.55f), 5.0f, 7.0f, 28.0f, 48.0f, LG_SPOTS, Vec3(1.0f, 0.95f, 0.88f));
    spot(Vec3(0.0f, 3.55f, -3.6f), Vec3(0, -0.7f, -0.7f), 3.5f, 6.0f, 25.0f, 50.0f, LG_SPOTS, warm);
    spot(Vec3(-5.0f, 3.55f, -3.9f), Vec3(0, -0.9f, -0.4f), 4.0f, 6.0f, 25.0f, 45.0f, LG_SPOTS, warm);
    // ATM room fluorescent tubes
    {
        LightDef l; l.pos = Vec3(-9.5f, 3.35f, -1.8f); l.color = cool; l.intensity = 3.4f; l.range = 9.0f; l.group = LG_ATM;
        lm.addStatic(l);
        l.pos = Vec3(-9.5f, 3.35f, 2.5f); l.intensity = 2.4f;
        lm.addStatic(l);
    }
    // library pendants + reading floor lamp
    const Vec3 libPend[] = {Vec3(9.5f, 2.78f, -1.6f), Vec3(12.5f, 2.78f, -1.6f), Vec3(11.0f, 2.78f, 2.6f)};
    for (auto& p : libPend) {
        LightDef l; l.pos = p; l.color = warm; l.intensity = 2.6f; l.range = 8.0f; l.group = LG_LIBRARY;
        lm.addStatic(l);
    }
    spot(Vec3(8.1f, 1.52f, 2.4f), Vec3(0.2f, -1, 0.0f), 3.0f, 3.5f, 30.0f, 60.0f, LG_SPOTS, warm);
    // porch down lights
    spot(Vec3(-1.4f, 2.95f, 6.4f), Vec3(0, -1, 0), 5.0f, 6.0f, 25.0f, 50.0f, LG_OUTDOOR, warm);
    spot(Vec3(1.4f, 2.95f, 6.4f), Vec3(0, -1, 0), 5.0f, 6.0f, 25.0f, 50.0f, LG_OUTDOOR, warm);
    // street lamps (point lights at the lamp heads)
    for (auto& p : m_lotLamps) {
        float dirZ = p.z < 15.0f ? 1.0f : -1.0f;
        LightDef l; l.pos = p + Vec3(0, 5.6f, 1.15f * dirZ); l.color = sodium; l.intensity = 26.0f; l.range = 17.0f; l.group = LG_OUTDOOR;
        lm.addStatic(l);
    }
    for (auto& p : m_roadLamps) {
        LightDef l; l.pos = p + Vec3(0, 5.6f, 1.15f); l.color = sodium; l.intensity = 24.0f; l.range = 16.0f; l.group = LG_OUTDOOR;
        lm.addStatic(l);
    }
}

// =============================================================================
//  DYNAMIC: doors, light fixtures, clocks, switches, roof
// =============================================================================
void Building::update(float dt, const std::vector<Vec3>& people) {
    bool someone = false;
    for (const auto& p : people)
        if (std::fabs(p.x) < 2.2f && std::fabs(p.z - Z_FRONT) < 2.8f) someone = true;
    float before = m_doorOpen;
    m_doorOpen = approach(m_doorOpen, someone ? 1.0f : 0.0f, dt * 1.6f);
    // whoosh when the doors start to open or to close
    if ((before == 0.0f && m_doorOpen > 0.0f) || (before == 1.0f && m_doorOpen < 1.0f))
        Audio::playAt(SND_DOOR, Vec3(0, 1.2f, Z_FRONT), 0.8f);
    m_doorClosed = m_doorOpen < 0.65f;
}

void Building::registerInteractions() {
    if (m_idBase == 0) m_idBase = InteractionSystem::reserveIds(8);
    auto& is = InteractionSystem::get();
    for (size_t i = 0; i < m_switches.size(); ++i) {
        const auto& sw = m_switches[i];
        Mat4 m = rotateYXZ(translate(Mat4(), sw.pos), {0, sw.yaw, 0});
        int group = sw.group;
        std::string name = sw.name;
        bool on = LightManager::get().groupOn[group];
        is.add(m_idBase + (int)i, translate(m, {0, 0, 0.01f}), Vec3(0.05f, 0.07f, 0.03f),
               std::string(on ? "Turn OFF " : "Turn ON ") + name, [group, name]() {
                   bool& g = LightManager::get().groupOn[group];
                   g = !g;
                   notify(std::string("Switched ") + (g ? "ON " : "OFF ") + name);
               });
    }
}

void Building::drawRoof(DrawSink& dyn) const {
    Node w(dyn);
    Material ceil = Material(Vec3(0.93f, 0.93f, 0.91f), 0.05f, 8.0f).tx(TEX_CEILING, 0.5f);
    w.box({(ATM_X0 + ATM_X1) * 0.5f, ROOF_Y - 0.01f, 0}, {ATM_X1 - ATM_X0, 0.02f, Z_FRONT - Z_BACK}, ceil);
    w.box({0, ROOF_Y - 0.01f, 0}, {LNG_X1 - LNG_X0, 0.02f, Z_FRONT - Z_BACK}, ceil);
    w.box({(LIB_X0 + LIB_X1) * 0.5f, ROOF_Y - 0.01f, 0}, {LIB_X1 - LIB_X0, 0.02f, Z_FRONT - Z_BACK}, ceil);
    Material roof = Material(Vec3(0.55f, 0.55f, 0.53f), 0.05f, 8.0f).tx(TEX_ROOF, 0.5f);
    float cx = (BLD_X0 + BLD_X1) * 0.5f, wx = BLD_X1 - BLD_X0 + 0.5f, wz = Z_FRONT - Z_BACK + 0.6f;
    w.box({cx, ROOF_Y + 0.12f, 0}, {wx, 0.24f, wz}, roof);
    Material parapet = Mat::concrete(Vec3(0.80f, 0.78f, 0.74f), 0.6f);
    w.box({cx, ROOF_Y + 0.45f, wz * 0.5f - 0.1f}, {wx, 0.42f, 0.2f}, parapet);
    w.box({cx, ROOF_Y + 0.45f, -wz * 0.5f + 0.1f}, {wx, 0.42f, 0.2f}, parapet);
    w.box({BLD_X0 - 0.15f, ROOF_Y + 0.45f, 0}, {0.2f, 0.42f, wz}, parapet);
    w.box({BLD_X1 + 0.15f, ROOF_Y + 0.45f, 0}, {0.2f, 0.42f, wz}, parapet);
    Furniture::acUnit(w.at({-4.0f, ROOF_Y + 0.24f, -2.0f}));
    Furniture::acUnit(w.at({9.5f, ROOF_Y + 0.24f, -2.5f}));
    w.cyl({2.0f, ROOF_Y + 0.6f, -3.0f}, {0.3f, 0.7f, 0.3f}, Mat::steel());
    w.part(M_HEMISPHERE, {2.0f, ROOF_Y + 0.95f, -3.0f}, {0.4f, 0.2f, 0.4f}, Mat::steel());
    // sign board standing on the parapet above the entrance
    Furniture::signBoard(w.at({0, ROOF_Y + 1.1f, Z_FRONT + 0.3f}), 5.6f, 0.9f, Vec3(0.12f, 0.30f, 0.16f), false);
    // legs: their front face is 2 cm in front of the parapet face (not on it -> no flicker)
    w.box({-2.0f, ROOF_Y + 0.55f, Z_FRONT + 0.28f}, {0.1f, 0.6f, 0.08f}, Mat::darkMetal());
    w.box({2.0f, ROOF_Y + 0.55f, Z_FRONT + 0.28f}, {0.1f, 0.6f, 0.08f}, Mat::darkMetal());
}

void Building::drawDynamic(DrawSink& dyn, float time, float hour, bool cutaway, const Vec3& cam) const {
    using namespace Furniture;
    LightManager& lm = LightManager::get();
    auto& is = InteractionSystem::get();
    Node w(dyn);

    // ---- automatic sliding glass doors ----
    float off = 0.6f + 1.15f * m_doorOpen;
    for (int sx = -1; sx <= 1; sx += 2) {
        // one door leaf = glass pane + top and bottom rails + an edge post with the handle
        slidingDoorLeaf(w.at({sx * off, 0, Z_FRONT + 0.18f}), sx);
    }

    // ---- light fixtures (glow when their group is on) ----
    bool lounge = lm.isOn(LG_LOUNGE), spots = lm.isOn(LG_SPOTS), atm = lm.isOn(LG_ATM), lib = lm.isOn(LG_LIBRARY);
    bool outdoor = lm.isOn(LG_OUTDOOR);
    const Vec3 loungePend[] = {Vec3(-3.6f, WALL_H, 2.4f), Vec3(3.6f, WALL_H, 2.4f), Vec3(-3.6f, WALL_H, -2.0f), Vec3(3.6f, WALL_H, -2.0f)};
    for (auto& p : loungePend) pendantLamp(w.at(p), 0.6f, lounge, Vec3(0.12f, 0.12f, 0.13f));
    const Vec3 libPend[] = {Vec3(9.5f, WALL_H, -1.6f), Vec3(12.5f, WALL_H, -1.6f), Vec3(11.0f, WALL_H, 2.6f)};
    for (auto& p : libPend) pendantLamp(w.at(p), 0.6f, lib, Vec3(0.55f, 0.42f, 0.2f));
    spotFixture(w.at({3.7f, WALL_H - 0.02f, -3.2f}), spots);
    spotFixture(w.at({0.0f, WALL_H - 0.02f, -3.6f}), spots);
    spotFixture(w.at({-5.0f, WALL_H - 0.02f, -3.9f}), spots);
    for (float x : {-2.0f, 2.0f}) spotFixture(w.at({x, WALL_H - 0.02f, 0.5f}), lounge);
    tubeLight(w.at({-9.5f, WALL_H, -1.8f}), 1.4f, atm);
    tubeLight(w.at({-9.5f, WALL_H, 2.5f}), 1.4f, atm);
    floorLamp(w.at({8.1f, 0, 2.4f}), spots);
    for (int sx = -1; sx <= 1; sx += 2) {
        tableLamp(w.at({sx * 4.6f - 1.25f, 0.57f, 4.45f}), lounge);
        tableLamp(w.at({sx * 4.6f + 1.25f, 0.57f, 4.45f}), lounge);
    }
    tableLamp(w.at({12.4f, 0.57f, 2.2f}), lib);
    for (int sx = -1; sx <= 1; sx += 2) spotFixture(w.at({sx * 1.4f, 2.99f, 6.4f}), outdoor);
    // street lamps (all of them, so distant lamps keep glowing)
    for (auto& p : m_lotLamps) {
        float yaw = p.z < 15.0f ? 0.0f : 180.0f;
        streetLamp(w.at(p, {0, yaw, 0}), 5.8f, outdoor);
    }
    for (auto& p : m_roadLamps)
        streetLamp(w.at(p), 5.8f, outdoor);
    // glowing bollards along the sidewalk edge
    for (float x : {-15.0f, -12.0f, -3.0f, 3.0f, 12.0f, 15.0f}) bollard(w.at({x, 0.06f, SIDEWALK_Z1 - 0.35f}), outdoor);

    // ---- clocks show the in-game time ----
    wallClock(w.at({0, 3.0f, Z_BACK + 0.13f}), hour);
    wallClock(w.at({LIB_X0 + 0.12f, 2.4f, 3.4f}, {0, 90, 0}), hour);
    wallClock(w.at({ATM_X1 - 0.12f, 2.6f, 1.5f}, {0, -90, 0}), hour);

    // ---- security cameras ----
    securityCamera(w.at({-11.85f, 3.2f, -4.85f}, {0, 45, 0}), time);
    securityCamera(w.at({6.85f, 3.2f, 4.85f}, {0, -135, 0}), time + 3.0f);

    // ---- light switches ----
    for (size_t i = 0; i < m_switches.size(); ++i) {
        const auto& sw = m_switches[i];
        lightSwitch(w.at(sw.pos, {0, sw.yaw, 0}), lm.groupOn[sw.group], is.hovered(m_idBase + (int)i));
    }

    if (!cutaway) drawRoof(dyn);
}

void Building::drawText(TextRenderer& t, float time, bool lit) const {
    (void)time;
    // facade sign
    Canvas c;
    c.model = translate(Mat4(), Vec3(0, ROOF_Y + 1.1f, Z_FRONT + 0.3f + 0.075f));
    c.w = 5.6f; c.h = 0.9f;
    Vec4 gold = lit ? Vec4(1.0f, 0.88f, 0.45f, 1.0f) : Vec4(0.95f, 0.85f, 0.55f, 1.0f);
    t.ctext(c, 2.8f, 0.36f, 0.42f, "JUNGLE REST STOP", gold, 1);
    t.ctext(c, 2.8f, 0.08f, 0.2f, "LOUNGE  *  ATM  *  LIBRARY", Vec4(0.85f, 0.95f, 0.85f, 1.0f), 1);
    // room signs above the inner doorways
    Canvas a;
    a.model = rotateYXZ(translate(Mat4(), Vec3(LNG_X0 + 0.11f, 2.75f, (DOOR_ATM_Z0 + DOOR_ATM_Z1) * 0.5f)), {0, 90, 0});
    a.w = 1.6f; a.h = 0.35f;
    t.crect(a, 0, 0, 1.6f, 0.35f, Vec4(0.08f, 0.25f, 0.6f, 1.0f));
    t.ctext(a, 0.8f, 0.07f, 0.22f, "ATM  >", Vec4(1, 1, 1, 1), 1);
    Canvas b;
    b.model = rotateYXZ(translate(Mat4(), Vec3(LNG_X1 - 0.11f, 2.75f, (DOOR_LIB_Z0 + DOOR_LIB_Z1) * 0.5f)), {0, -90, 0});
    b.w = 1.6f; b.h = 0.35f;
    t.crect(b, 0, 0, 1.6f, 0.35f, Vec4(0.35f, 0.22f, 0.10f, 1.0f));
    t.ctext(b, 0.8f, 0.07f, 0.22f, "< LIBRARY", Vec4(1, 0.95f, 0.85f, 1), 1);
    // ATM room wall poster
    Canvas p;
    p.model = rotateYXZ(translate(Mat4(), Vec3(ATM_X0 + 0.2f, 2.0f, -2.0f)), {0, 90, 0});
    p.w = 1.6f; p.h = 0.7f;
    t.ctext(p, 0.8f, 0.38f, 0.2f, "24/7 BANKING", Vec4(1, 1, 1, 1), 1);
    t.ctext(p, 0.8f, 0.12f, 0.12f, "Cover the keypad", Vec4(0.8f, 0.9f, 1, 1), 1);
    // parking sign
    Canvas ps;
    ps.model = rotateYXZ(translate(Mat4(), Vec3(LOT_X1 + 0.9f - 0.03f, 2.3f, 10.3f + 0.05f)), {0, -90, 0});   // same place as the sign board
    ps.w = 0.7f; ps.h = 0.7f;
    t.ctext(ps, 0.35f, 0.1f, 0.55f, "P", Vec4(1, 1, 1, 1), 1);
}
