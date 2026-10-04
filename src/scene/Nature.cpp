#include "Nature.h"
#include "Layout.h"
#include "Materials.h"
#include <cmath>

namespace Nature {

namespace {
Vec3 jitterColor(const Vec3& c, Rng& r, float amt) {
    float k = r.range(1.0f - amt, 1.0f + amt);
    return Vec3(c.x * k * r.range(0.95f, 1.05f), c.y * k, c.z * k * r.range(0.9f, 1.1f));
}
Material foliageBlob(const Vec3& c) {
    Material m(c, 0.05f, 8.0f);
    m.tx(TEX_LEAVES, 0.7f);   // gives the blob a leafy texture (box mapped)
    return m;
}
}  // namespace

// -----------------------------------------------------------------------------
// Rainforest tree: trunk + upper trunk + buttress-root fins (wedges) +
// 3 angled branches, each ending in a foliage blob with leaf cards around it
// -----------------------------------------------------------------------------
void rainforestTree(const Node& n, float s, Rng& rng) {
    Material bark = Mat::bark(jitterColor(Vec3(0.36f, 0.28f, 0.20f), rng, 0.15f));
    Vec3 leafCol = jitterColor(Vec3(0.24f, 0.46f, 0.16f), rng, 0.2f);
    Material blob = foliageBlob(leafCol * 0.70f);
    Material cards = Mat::leafCard(leafCol).lv(1);
    float H = rng.range(6.5f, 9.0f) * s;

    // trunk (two stacked cylinders) and buttress-root fins (wedges)
    n.part(M_CYLINDER_LOW, {0, H * 0.5f, 0}, {0.62f * s, H, 0.62f * s}, bark);
    n.part(M_CYLINDER_LOW, {0, H + 0.8f * s, 0}, {0.42f * s, 1.8f * s, 0.42f * s}, bark);
    for (int i = 0; i < 4; ++i) {
        Node fin = n.rotY(i * 90.0f + rng.range(-20.0f, 20.0f));
        fin.part(M_WEDGE, {0, 0.75f * s, 0.6f * s}, {0.13f * s, 1.5f * s, 1.2f * s}, bark);
    }
    // three branches leaving the trunk at different angles
    Vec3 crown(0, H + 1.9f * s, 0);
    for (int b = 0; b < 3; ++b) {
        float yaw = b * 120.0f + rng.range(-25.0f, 25.0f);
        float len = rng.range(2.0f, 2.8f) * s;
        Node br = n.at({0, H * rng.range(0.80f, 0.95f), 0}, {0, yaw, 0}).rotX(rng.range(45.0f, 60.0f));
        br.part(M_CYLINDER_LOW, {0, len * 0.5f, 0}, {0.2f * s, len, 0.2f * s}, bark);
        if (rng.chance(0.35f)) {   // hanging vine (world aligned, hangs straight down)
            float vl = rng.range(2.5f, 4.5f) * s;
            Vec3 tip = br.worldPos(Vec3(0, len * 0.9f, 0));
            Node vw(*n.sink, translate(Mat4(), tip));
            vw.part(M_QUAD, {0, -vl * 0.5f, 0}, {0.6f * s, vl, 1}, Mat::leafCard(leafCol * 0.9f, TEX_VINE).lv(1), {0, rng.range(0.0f, 180.0f), 0});
        }
    }
    // irregular canopy: 6-8 overlapping foliage blobs around the crown, each
    // covered by alpha-tested leaf cards so the outline looks leafy
    int blobs = rng.irange(6, 8);
    for (int k = 0; k < blobs; ++k) {
        float a = rng.range(0.0f, 2.0f * PI), r = rng.range(0.6f, 2.4f) * s;
        Vec3 c = crown + Vec3(std::cos(a) * r, rng.range(-0.9f, 1.3f) * s, std::sin(a) * r);
        float bs = rng.range(1.9f, 2.7f) * s;
        n.part(M_SPHERE_LOW, c, {bs, bs * rng.range(0.70f, 0.85f), bs}, blob, {0, rng.range(0.0f, 180.0f), 0});
        for (int q = 0; q < 2; ++q)
            n.part(M_LEAVES, c + Vec3(rng.range(-0.5f, 0.5f) * s, rng.range(0.0f, 0.7f) * s, rng.range(-0.5f, 0.5f) * s),
                   Vec3(bs * rng.range(1.1f, 1.35f)), cards, {rng.range(-20.0f, 20.0f), rng.range(0.0f, 180.0f), rng.range(-20.0f, 20.0f)});
    }
}

// -----------------------------------------------------------------------------
// Coconut palm: chain of 7 trunk segments, each rotated a little relative to
// the previous one (hierarchical bend), crown of arched 2-part fronds
// -----------------------------------------------------------------------------
void palmTree(const Node& n, float s, Rng& rng) {
    Material bark = Mat::bark(jitterColor(Vec3(0.52f, 0.44f, 0.34f), rng, 0.1f)).tx(TEX_BARK, 2.5f);
    Material ring = Mat::matte(Vec3(0.30f, 0.25f, 0.19f));
    Vec3 frondCol = jitterColor(Vec3(0.34f, 0.52f, 0.18f), rng, 0.15f);
    Material frond = Mat::leafCard(frondCol, TEX_PALM);
    float bend = rng.range(3.0f, 7.0f);
    Node seg = n.rotY(rng.range(0.0f, 360.0f));
    const int segs = 7;
    float L = rng.range(1.1f, 1.45f) * s;
    for (int i = 0; i < segs; ++i) {
        float r = (0.36f - i * 0.022f) * s;
        seg.part(M_CYLINDER_LOW, {0, L * 0.5f, 0}, {r, L, r}, bark);
        seg.part(M_CYLINDER_LOW, {0, L * 0.98f, 0}, {r * 1.12f, 0.08f * s, r * 1.12f}, ring);
        seg = seg.at({0, L, 0}).rotX(bend);
    }
    // crown
    Node crown = seg.rotX(-bend * segs * 0.8f);
    crown.part(M_SPHERE_LOW, {0, 0.1f * s, 0}, {0.55f * s, 0.6f * s, 0.55f * s}, Mat::matte(frondCol * 0.6f));
    int fronds = 9;
    for (int f = 0; f < fronds; ++f) {
        float len = rng.range(1.6f, 2.1f) * s;
        Node fr = crown.rotY(f * (360.0f / fronds) + rng.range(-12.0f, 12.0f)).rotX(rng.range(55.0f, 70.0f));
        fr.part(M_QUAD, {0, len * 0.5f, 0}, {1.0f * s, len, 1}, frond);
        Node fr2 = fr.at({0, len * 0.98f, 0}).rotX(rng.range(30.0f, 45.0f));
        fr2.part(M_QUAD, {0, len * 0.5f, 0}, {0.85f * s, len, 1}, frond);
    }
    for (int c = 0; c < 4; ++c)
        crown.part(M_SPHERE_LOW, {std::cos(c * 1.6f) * 0.28f * s, -0.15f * s, std::sin(c * 1.6f) * 0.28f * s}, Vec3(0.26f * s),
                   Mat::plastic(Vec3(0.40f, 0.36f, 0.12f)));
}

// -----------------------------------------------------------------------------
// Emergent giant: very tall trunk, large buttresses, wide flat umbrella canopy
// -----------------------------------------------------------------------------
void emergentTree(const Node& n, float s, Rng& rng) {
    Material bark = Mat::bark(jitterColor(Vec3(0.55f, 0.50f, 0.44f), rng, 0.1f));
    Vec3 leafCol = jitterColor(Vec3(0.20f, 0.42f, 0.17f), rng, 0.15f);
    Material blob = foliageBlob(leafCol * 0.68f);
    Material cards = Mat::leafCard(leafCol).lv(1);
    float H = rng.range(14.0f, 18.0f) * s;
    n.part(M_CYLINDER_LOW, {0, H * 0.5f, 0}, {0.95f * s, H, 0.95f * s}, bark);
    for (int i = 0; i < 5; ++i) {
        Node fin = n.rotY(i * 72.0f + rng.range(-15.0f, 15.0f));
        fin.part(M_WEDGE, {0, 1.3f * s, 1.0f * s}, {0.2f * s, 2.6f * s, 2.0f * s}, bark);
    }
    for (int b = 0; b < 4; ++b) {
        float len = rng.range(3.0f, 4.2f) * s;
        Node br = n.at({0, H * 0.92f, 0}, {0, b * 90.0f + rng.range(-20.0f, 20.0f), 0}).rotX(rng.range(55.0f, 68.0f));
        br.part(M_CYLINDER_LOW, {0, len * 0.5f, 0}, {0.3f * s, len, 0.3f * s}, bark);
    }
    // broad, layered crown
    Vec3 crown(0, H + 1.6f * s, 0);
    int blobs = rng.irange(8, 10);
    for (int k = 0; k < blobs; ++k) {
        float a = rng.range(0.0f, 2.0f * PI), r = rng.range(0.5f, 3.6f) * s;
        Vec3 c = crown + Vec3(std::cos(a) * r, rng.range(-0.8f, 1.2f) * s - r * 0.12f, std::sin(a) * r);
        float bs = rng.range(2.3f, 3.2f) * s;
        n.part(M_SPHERE_LOW, c, {bs, bs * 0.68f, bs}, blob, {0, rng.range(0.0f, 180.0f), 0});
        for (int q = 0; q < 2; ++q)
            n.part(M_LEAVES, c + Vec3(rng.range(-0.6f, 0.6f) * s, rng.range(0.0f, 0.8f) * s, rng.range(-0.6f, 0.6f) * s),
                   Vec3(bs * 1.25f), cards, {rng.range(-15.0f, 15.0f), rng.range(0.0f, 180.0f), 0});
    }
}

void bambooClump(const Node& n, float s, Rng& rng) {
    Material culm = Material(jitterColor(Vec3(0.55f, 0.62f, 0.25f), rng, 0.1f), 0.35f, 40.0f);
    Material node = Mat::matte(Vec3(0.40f, 0.42f, 0.18f)).lv(1);
    Material cards = Mat::leafCard(Vec3(0.40f, 0.58f, 0.20f)).lv(1);
    int count = rng.irange(9, 15);
    for (int i = 0; i < count; ++i) {
        float a = rng.range(0.0f, 2 * PI), r = rng.range(0.0f, 0.9f) * s;
        float h = rng.range(6.0f, 9.5f) * s;
        Node c = n.at({std::cos(a) * r, 0, std::sin(a) * r}, {rng.range(-8.0f, 8.0f), rng.range(0.0f, 360.0f), rng.range(-8.0f, 8.0f)});
        c.part(M_CYLINDER_LOW, {0, h * 0.5f, 0}, {0.11f * s, h, 0.11f * s}, culm);
        for (float y = 1.1f * s; y < h - 0.5f; y += 1.3f * s) c.part(M_CYLINDER_LOW, {0, y, 0}, {0.125f * s, 0.05f * s, 0.125f * s}, node);
        for (int k = 0; k < 2; ++k)
            c.part(M_LEAVES, {rng.range(-0.4f, 0.4f) * s, h - rng.range(0.3f, 1.8f) * s, rng.range(-0.4f, 0.4f) * s}, Vec3(1.7f * s), cards,
                   {0, rng.range(0.0f, 180.0f), 0});
    }
}

void bananaPlant(const Node& n, float s, Rng& rng) {
    Material stem = Mat::matte(Vec3(0.42f, 0.48f, 0.22f));
    Vec3 lc = jitterColor(Vec3(0.30f, 0.55f, 0.16f), rng, 0.15f);
    Material leaf = Material(lc, 0.2f, 24.0f).fl(MF_FOLIAGE | MF_WIND);
    int stems = rng.irange(1, 3);
    for (int st = 0; st < stems; ++st) {
        float h = rng.range(1.8f, 2.6f) * s;
        Node b = n.at({rng.range(-0.4f, 0.4f) * s, 0, rng.range(-0.4f, 0.4f) * s});
        b.part(M_CYLINDER_LOW, {0, h * 0.5f, 0}, {0.22f * s, h, 0.22f * s}, stem);
        Node top = b.at({0, h, 0});
        int leaves = rng.irange(5, 8);
        for (int l = 0; l < leaves; ++l) {
            Node lf = top.rotY(l * (360.0f / leaves) + rng.range(-15.0f, 15.0f)).rotX(rng.range(-35.0f, 15.0f));
            lf.part(M_SPHERE_LOW, {0, 0.05f * s, 0.95f * s}, {0.55f * s, 0.04f * s, 1.9f * s}, leaf);
        }
        if (rng.chance(0.4f)) {
            top.part(M_CYLINDER_LOW, {0.15f * s, -0.4f * s, 0.15f * s}, {0.08f * s, 0.6f * s, 0.08f * s}, stem, {20, 0, 0});
            top.part(M_SPHERE_LOW, {0.18f * s, -0.75f * s, 0.25f * s}, {0.3f * s, 0.45f * s, 0.3f * s}, Mat::matte(Vec3(0.55f, 0.62f, 0.2f)));
        }
    }
}

void bush(const Node& n, float s, Rng& rng) {
    Vec3 lc = jitterColor(Vec3(0.22f, 0.42f, 0.15f), rng, 0.25f);
    Material blob = foliageBlob(lc * 0.6f).lv(1);
    Material cards = Mat::leafCard(lc).lv(1);
    int k = rng.irange(2, 3);
    for (int i = 0; i < k; ++i) {
        Vec3 p(rng.range(-0.5f, 0.5f) * s, rng.range(0.35f, 0.6f) * s, rng.range(-0.5f, 0.5f) * s);
        n.part(M_SPHERE_LOW, p, Vec3(rng.range(1.0f, 1.5f) * s, rng.range(0.8f, 1.1f) * s, rng.range(1.0f, 1.5f) * s), blob);
        n.part(M_LEAVES, p + Vec3(0, 0.2f * s, 0), Vec3(1.4f * s), cards, {0, rng.range(0.0f, 180.0f), 0});
    }
    if (rng.chance(0.3f)) {  // flowering bush
        Vec3 fc = rng.chance(0.5f) ? Vec3(0.95f, 0.25f, 0.35f) : Vec3(0.98f, 0.75f, 0.2f);
        for (int i = 0; i < 6; ++i)
            n.part(M_SPHERE_LOW, {rng.range(-0.6f, 0.6f) * s, rng.range(0.6f, 1.0f) * s, rng.range(-0.6f, 0.6f) * s}, Vec3(0.12f * s),
                   Material(fc, 0.1f, 8.0f).lv(2));
    }
}

void fern(const Node& n, float s) {
    Material f = Mat::leafCard(Vec3(0.30f, 0.52f, 0.18f), TEX_FERN).lv(1);
    for (int i = 0; i < 5; ++i)
        n.rotY(i * 72.0f).part(M_QUAD, {0, 0.3f * s, 0.32f * s}, {0.55f * s, 0.9f * s, 1}, f, {55, 0, 0});
    n.part(M_CROSS, {0, 0, 0}, {0.8f * s, 0.7f * s, 0.8f * s}, f);
}

void rock(const Node& n, float s, Rng& rng) {
    Material m = Material(jitterColor(Vec3(0.48f, 0.46f, 0.43f), rng, 0.15f), 0.1f, 16.0f).tx(TEX_ROCK, 0.6f);
    int k = rng.irange(1, 3);
    for (int i = 0; i < k; ++i)
        n.part(M_SPHERE_LOW, {rng.range(-0.4f, 0.4f) * s, 0.1f * s, rng.range(-0.4f, 0.4f) * s},
               {rng.range(0.8f, 1.6f) * s, rng.range(0.5f, 0.9f) * s, rng.range(0.8f, 1.4f) * s}, m,
               {rng.range(-15.0f, 15.0f), rng.range(0.0f, 360.0f), rng.range(-15.0f, 15.0f)});
}

void fallenLog(const Node& n, float len, Rng& rng) {
    Material bark = Mat::bark(Vec3(0.30f, 0.23f, 0.16f));
    float r = rng.range(0.35f, 0.55f);
    n.part(M_CYLINDER_LOW, {0, r * 0.45f, 0}, {r, len, r}, bark, {0, 0, 90});
    Material moss = Mat::matte(Vec3(0.25f, 0.42f, 0.14f)).lv(1);
    for (int i = 0; i < 3; ++i)
        n.part(M_SPHERE_LOW, {rng.range(-len * 0.4f, len * 0.4f), r * 0.85f, rng.range(-0.1f, 0.1f)}, {0.6f, 0.12f, r * 0.8f}, moss);
    for (int i = 0; i < 3; ++i) {
        Vec3 p(rng.range(-len * 0.3f, len * 0.3f), r * 0.3f, r * 0.45f);
        n.part(M_CYLINDER_LOW, p, {0.035f, 0.12f, 0.035f}, Mat::matte(Vec3(0.9f, 0.88f, 0.8f)).lv(2));
        n.part(M_HEMISPHERE, p + Vec3(0, 0.06f, 0), {0.14f, 0.08f, 0.14f}, Mat::matte(Vec3(0.75f, 0.35f, 0.15f)).lv(2));
    }
}

// -----------------------------------------------------------------------------
JungleStats buildJungle(DrawSink& s, CollisionWorld& col, bool low) {
    JungleStats st;
    Rng rng(20261004u);
    const float W = Layout::WORLD_HALF - 4.0f;
    const float cell = low ? 9.0f : 6.6f;
    Material grassM = Mat::leafCard(Vec3(0.36f, 0.56f, 0.20f), TEX_GRASS_BLADES).lv(2);
    Material flowerM = Mat::leafCard(Vec3(1.0f), TEX_FLOWERS).lv(2);

    for (float gx = -W; gx <= W; gx += cell)
        for (float gz = -W; gz <= W; gz += cell) {
            float x = gx + rng.range(-0.45f, 0.45f) * cell;
            float z = gz + rng.range(-0.45f, 0.45f) * cell;
            if (isClearedArea(x, z, 1.5f)) continue;
            bool edge = isClearedArea(x, z, 9.0f);   // first rows facing the clearing
            float y = terrainHeight(x, z);
            Node base(s, translate(Mat4(), Vec3(x, y - 0.05f, z)));
            float sc = rng.range(0.8f, 1.25f);
            float pick = rng.uniform();
            if (pick < 0.40f) { rainforestTree(base, sc, rng); col.addCircle(x, z, 0.4f * sc); st.trees++; }
            else if (pick < 0.58f) { palmTree(base, sc, rng); col.addCircle(x, z, 0.3f * sc); st.palms++; }
            else if (pick < 0.65f) { emergentTree(base, sc, rng); col.addCircle(x, z, 0.6f * sc); st.trees++; }
            else if (pick < 0.73f) { bambooClump(base, sc, rng); col.addCircle(x, z, 0.8f * sc); st.bamboo++; }
            else if (pick < 0.84f) { bananaPlant(base, sc, rng); col.addCircle(x, z, 0.2f * sc); st.bushes++; }
            else if (pick < 0.92f) { bush(base, sc * 1.3f, rng); st.bushes++; }

            // undergrowth around the cell centre
            int ferns = rng.irange(1, edge ? 5 : 3);
            for (int i = 0; i < ferns; ++i) {
                float fx = x + rng.range(-0.5f, 0.5f) * cell, fz = z + rng.range(-0.5f, 0.5f) * cell;
                if (isClearedArea(fx, fz, 0.5f)) continue;
                Node fn(s, rotateYXZ(translate(Mat4(), Vec3(fx, terrainHeight(fx, fz) - 0.05f, fz)), {0, rng.range(0.0f, 360.0f), 0}));
                if (rng.chance(0.65f)) { fern(fn, rng.range(0.9f, 1.6f)); st.ferns++; }
                else { bush(fn, rng.range(0.7f, 1.3f), rng); st.bushes++; }
            }
            bool near = std::fabs(x) < 70.0f && std::fabs(z - 6.0f) < 70.0f;
            int tufts = near ? rng.irange(4, edge ? 12 : 7) : rng.irange(1, 3);
            for (int i = 0; i < tufts; ++i) {
                float tx = x + rng.range(-0.5f, 0.5f) * cell, tz = z + rng.range(-0.5f, 0.5f) * cell;
                if (isClearedArea(tx, tz, 0.0f)) continue;
                float ts = rng.range(0.6f, 1.1f);
                Material& gm = rng.chance(0.12f) ? flowerM : grassM;
                s.add(M_CROSS, scale(rotateYXZ(translate(Mat4(), Vec3(tx, terrainHeight(tx, tz) - 0.03f, tz)), {0, rng.range(0.0f, 90.0f), 0}),
                                     Vec3(ts * 0.9f, ts * 0.6f, ts * 0.9f)), gm);
                st.grass++;
            }
            if (rng.chance(0.12f)) {
                float rx = x + rng.range(-2.0f, 2.0f), rz = z + rng.range(-2.0f, 2.0f);
                Node rn(s, translate(Mat4(), Vec3(rx, terrainHeight(rx, rz), rz)));
                float rs = rng.range(0.5f, 1.6f);
                rock(rn, rs, rng);
                if (rs > 1.0f) col.addCircle(rx, rz, 0.5f * rs);
                st.rocks++;
            }
            if (rng.chance(0.05f)) {
                float lx = x + rng.range(-2.0f, 2.0f), lz = z + rng.range(-2.0f, 2.0f);
                Node ln(s, rotateYXZ(translate(Mat4(), Vec3(lx, terrainHeight(lx, lz), lz)), {0, rng.range(0.0f, 180.0f), 0}));
                fallenLog(ln, rng.range(3.0f, 6.0f), rng);
                st.logs++;
            }
        }
    return st;
}

// -----------------------------------------------------------------------------
// Garden: lawn tufts around the building, shrubs in planters, ornamental palms
// -----------------------------------------------------------------------------
void buildGarden(DrawSink& s, CollisionWorld& col) {
    using namespace Layout;
    Rng rng(777);
    Material grassM = Mat::leafCard(Vec3(0.40f, 0.60f, 0.22f), TEX_GRASS_BLADES).lv(2);
    // lawn tufts on unpaved parts of the clearing
    for (int i = 0; i < 2600; ++i) {
        float x = rng.range(LOT_X0 - 6.0f, LOT_X1 + 6.0f), z = rng.range(BLD_Z0 - 9.0f, LOT_Z1 + 4.0f);
        bool paved = (x > BLD_X0 - 0.3f && x < BLD_X1 + 0.3f && z > BLD_Z0 - 0.3f && z < SIDEWALK_Z1) ||
                     (x > LOT_X0 && x < LOT_X1 && z > LOT_Z0 - 0.2f && z < LOT_Z1 + 0.3f) ||
                     (x > -14.0f && x < 17.0f && z > BLD_Z1 - 0.2f && z < SIDEWALK_Z1 + 0.3f) ||
                     std::fabs(z - roadCenterZ(x)) < 4.2f;
        if (paved) continue;
        float ts = rng.range(0.5f, 0.9f);
        s.add(M_CROSS, scale(rotateYXZ(translate(Mat4(), Vec3(x, terrainHeight(x, z) - 0.02f, z)), {0, rng.range(0.0f, 90.0f), 0}),
                             Vec3(ts, ts * 0.55f, ts)), grassM);
    }
    // ornamental palms along the far edge of the lot and beside the building
    const float palmX[] = {-20.0f, -10.0f, 10.0f, 20.0f};
    for (float px : palmX) {
        Node p(s, translate(Mat4(), Vec3(px, 0, LOT_Z1 + 1.6f)));
        palmTree(p, 1.05f, rng);
        col.addCircle(px, LOT_Z1 + 1.6f, 0.35f);
    }
    const Vec3 sidePalms[] = {Vec3(-14.5f, 0, 3.0f), Vec3(-14.5f, 0, -3.5f), Vec3(17.5f, 0, 3.0f), Vec3(17.5f, 0, -3.5f)};
    for (const auto& sp : sidePalms) {
        palmTree(Node(s, translate(Mat4(), sp)), 1.0f, rng);
        col.addCircle(sp.x, sp.z, 0.35f);
    }
    // planters on the sidewalk with shrubs and flowers
    const float planterX[] = {-9.0f, -6.0f, 6.0f, 9.0f};
    for (float px : planterX) {
        Node pl(s, translate(Mat4(), Vec3(px, 0, 7.4f)));
        pl.box({0, 0.25f, 0}, {2.0f, 0.5f, 0.8f}, Mat::concrete(Vec3(0.72f, 0.70f, 0.66f), 0.5f));
        pl.box({0, 0.49f, 0}, {1.8f, 0.04f, 0.6f}, Mat::matte(Vec3(0.20f, 0.14f, 0.09f)));
        for (int i = 0; i < 3; ++i) bush(pl.at({-0.6f + i * 0.6f, 0.45f, 0}), 0.5f, rng);
        Material flowerM = Mat::leafCard(Vec3(1.0f), TEX_FLOWERS).lv(2);
        for (int i = 0; i < 4; ++i) pl.part(M_CROSS, {-0.7f + i * 0.45f, 0.5f, 0.15f}, {0.4f, 0.35f, 0.4f}, flowerM, {0, i * 37.0f, 0});
        col.addBox(px - 1.0f, 7.0f, px + 1.0f, 7.8f, 0.0f, 0.6f);
    }
    // hedge rows along the lot sides
    for (int side = -1; side <= 1; side += 2)
        for (float z = LOT_Z0 + 0.6f; z < LOT_Z1; z += 1.1f) {
            if (std::fabs(z - AISLE_Z) < 4.5f) continue;   // keep the road open
            bush(Node(s, translate(Mat4(), Vec3(side * (LOT_X1 + 0.9f), 0, z))), 0.8f, rng);
        }
}

// -----------------------------------------------------------------------------
// Wildlife
// -----------------------------------------------------------------------------
void Wildlife::init() {
    Rng rng(99);
    for (int i = 0; i < 110; ++i)
        m_flies.push_back({Vec3(rng.range(-30.0f, 30.0f), rng.range(0.4f, 2.6f), rng.range(-30.0f, 30.0f)), rng.range(0.0f, 6.28f), rng.range(0.4f, 1.0f)});
    for (int i = 0; i < 9; ++i)
        m_birds.push_back({Vec3(rng.range(-40.0f, 40.0f), 0, rng.range(-40.0f, 40.0f)), rng.range(10.0f, 28.0f), rng.range(0.0f, 6.28f),
                           rng.range(0.25f, 0.5f) * (rng.chance(0.5f) ? 1.0f : -1.0f), rng.range(16.0f, 30.0f), rng.range(0.0f, 6.28f)});
}

void Wildlife::update(float dt, float night, const Vec3& cam) {
    (void)night;
    m_center = cam;
    for (auto& b : m_birds) {
        b.angle += b.speed * dt;
        b.flap += dt * 9.0f;
    }
}

void Wildlife::draw(DrawSink& dyn, float night, float t) const {
    // fireflies: tiny glowing spheres that drift and blink (only at night, outside)
    if (night > 0.4f) {
        float k = smoothstepf(0.4f, 0.8f, night);
        for (const auto& f : m_flies) {
            // wrap the swarm around the camera so it is always nearby
            float wx = std::fmod(f.base.x - m_center.x + 300.0f, 60.0f) - 30.0f + m_center.x;
            float wz = std::fmod(f.base.z - m_center.z + 300.0f, 60.0f) - 30.0f + m_center.z;
            if (wx > Layout::BLD_X0 && wx < Layout::BLD_X1 && wz > Layout::BLD_Z0 && wz < Layout::BLD_Z1) continue;  // not indoors
            Vec3 p(wx + std::sin(t * f.speed + f.phase) * 0.8f, terrainHeight(wx, wz) + f.base.y + std::sin(t * 1.3f + f.phase) * 0.3f,
                   wz + std::cos(t * f.speed * 0.8f + f.phase) * 0.8f);
            float blink = 0.5f + 0.5f * std::sin(t * 3.0f + f.phase * 3.0f);
            if (blink < 0.25f) continue;
            Material m = Mat::glow(Vec3(0.75f, 1.0f, 0.35f), 3.0f * blink * k);
            dyn.add(M_SPHERE_LOW, scale(translate(Mat4(), p), Vec3(0.05f)), m);
        }
    }
    // birds: body + two flapping wings (hierarchical) circling over the jungle
    if (night < 0.6f) {
        Material body = Mat::matte(Vec3(0.12f, 0.1f, 0.09f));
        for (const auto& b : m_birds) {
            Vec3 p = b.center + Vec3(std::cos(b.angle) * b.radius, b.height + std::sin(b.angle * 2.0f) * 1.5f, std::sin(b.angle) * b.radius);
            float heading = toDegrees(std::atan2(-std::sin(b.angle) * b.speed, std::cos(b.angle) * b.speed));
            Node n(dyn, rotateYXZ(translate(Mat4(), p), {0, heading, 0}));
            n.part(M_SPHERE_LOW, {0, 0, 0}, {0.18f, 0.16f, 0.5f}, body);
            n.part(M_CONE, {0, 0.02f, 0.3f}, {0.06f, 0.14f, 0.06f}, Mat::matte(Vec3(0.8f, 0.6f, 0.1f)), {90, 0, 0});
            float flap = std::sin(b.flap) * 35.0f;
            n.rotZ(flap).part(M_CUBE, {0.35f, 0, 0}, {0.7f, 0.02f, 0.28f}, body);
            n.rotZ(-flap).part(M_CUBE, {-0.35f, 0, 0}, {0.7f, 0.02f, 0.28f}, body);
        }
    }
}

}  // namespace Nature
