#include "Car.h"
#include "../scene/Materials.h"
#include <cmath>

namespace Car {

CarDims dims(int type) {
    CarDims d;
    if (type == 1) {        // SUV
        d = {4.6f, 1.88f, 1.78f, 2.75f, 0.38f, 2.9f, -0.30f, 1.02f, Vec3(), Vec3()};
        d.driverSeat = Vec3(-0.40f, 0.24f, -0.15f);
    } else if (type == 2) { // hatchback
        d = {3.9f, 1.72f, 1.48f, 2.45f, 0.30f, 2.3f, -0.40f, 0.84f, Vec3(), Vec3()};
        d.driverSeat = Vec3(-0.36f, 0.07f, -0.20f);
    } else {                // sedan
        d = {4.4f, 1.78f, 1.42f, 2.65f, 0.32f, 2.3f, -0.20f, 0.86f, Vec3(), Vec3()};
        d.driverSeat = Vec3(-0.38f, 0.05f, -0.05f);
    }
    d.doorHinge = Vec3(-d.width * 0.5f - 0.015f, 0.0f, d.driverSeat.z + 0.78f);
    return d;
}

CarLook randomLook(unsigned seed) {
    Rng r(seed * 747796405u + 1u);
    const Vec3 paints[] = {Vec3(0.75f, 0.06f, 0.08f), Vec3(0.92f, 0.92f, 0.92f), Vec3(0.08f, 0.08f, 0.09f), Vec3(0.55f, 0.57f, 0.60f),
                           Vec3(0.10f, 0.22f, 0.55f), Vec3(0.12f, 0.35f, 0.18f), Vec3(0.85f, 0.55f, 0.10f), Vec3(0.35f, 0.05f, 0.08f),
                           Vec3(0.25f, 0.55f, 0.75f)};
    CarLook l;
    l.paint = paints[r.irange(0, 8)];
    l.type = r.irange(0, 2);
    return l;
}

void draw(const Node& n, const CarLook& look, const CarState& st) {
    CarDims d = dims(look.type);
    const float L = d.length, W = d.width, R = d.wheelR;
    Material paint = Mat::carPaint(look.paint);
    Material plastic = Material(Vec3(0.08f, 0.08f, 0.09f), 0.3f, 24.0f);
    Material chrome = Mat::chrome();
    Material glass = Material(Vec3(0.10f, 0.14f, 0.18f), 1.0f, 160.0f).a(0.45f).fl(MF_NO_SHADOW);
    Material interior = Material(Vec3(0.12f, 0.11f, 0.10f), 0.1f, 8.0f);
    float bodyBot = R * 0.95f, bodyTop = d.hoodTop;
    float bodyH = bodyTop - bodyBot;

    // ---------------- body ----------------
    n.box({0, bodyBot + bodyH * 0.5f, 0}, {W, bodyH, L - 0.1f}, paint);
    n.part(M_CYLINDER, {0, bodyBot + bodyH * 0.5f, L * 0.5f - 0.05f}, {bodyH * 0.98f, W * 0.98f, 0.24f}, paint, {0, 0, 90});
    n.box({0, bodyBot + 0.12f, L * 0.5f + 0.02f}, {W * 0.96f, 0.22f, 0.14f}, plastic);    // front bumper
    n.box({0, bodyBot + 0.12f, -L * 0.5f - 0.0f}, {W * 0.96f, 0.22f, 0.14f}, plastic);    // rear bumper
    n.box({0, bodyBot - 0.03f, 0}, {W * 0.9f, 0.08f, L * 0.86f}, plastic);               // underbody / sills
    // bonnet (sloping wedge) and boot lid
    float hoodLen = look.type == 2 ? 1.0f : 1.15f;
    n.part(M_WEDGE, {0, bodyTop + 0.04f, L * 0.5f - hoodLen * 0.5f - 0.05f}, {W * 0.97f, 0.08f, hoodLen}, paint);
    if (look.type != 2) n.box({0, bodyTop + 0.02f, -L * 0.5f + 0.45f}, {W * 0.96f, 0.04f, 0.8f}, paint);

    // ---------------- cabin ----------------
    float cabH = d.height - bodyTop - 0.06f;
    Vec3 cabC(0, bodyTop + cabH * 0.5f, d.cabinZ);
    if (look.type == 1) {
        // SUV: upright glass box + roof + roof rails
        n.box(cabC, {W * 0.9f, cabH, d.cabinLen}, glass);
        n.box({0, d.height - 0.04f, d.cabinZ}, {W * 0.92f, 0.08f, d.cabinLen + 0.05f}, paint);
        for (int s = -1; s <= 1; s += 2) n.box({s * W * 0.38f, d.height + 0.03f, d.cabinZ}, {0.05f, 0.05f, d.cabinLen * 0.9f}, chrome);
        for (int s = -1; s <= 1; s += 2)
            for (int k = 0; k < 3; ++k) n.box({s * W * 0.45f, cabC.y, d.cabinZ - d.cabinLen * 0.5f + 0.04f + k * (d.cabinLen - 0.08f) * 0.5f}, {0.04f, cabH, 0.09f}, paint);
    } else {
        // sedan / hatch: tapered glass cabin + roof + slanted pillars
        n.part(M_FRUSTUM_BOX, cabC, {W * 0.92f, cabH, d.cabinLen}, glass);
        float topW = W * 0.92f * 0.70f, topL = d.cabinLen * 0.60f;
        n.box({0, d.height - 0.03f, d.cabinZ}, {topW + 0.02f, 0.06f, topL + 0.02f}, paint);
        float bw = W * 0.46f, bl = d.cabinLen * 0.5f, tw = topW * 0.5f, tl = topL * 0.5f;
        float y0 = bodyTop, y1 = d.height - 0.04f;
        for (int s = -1; s <= 1; s += 2) {
            n.beam(M_CUBE, {s * bw, y0, d.cabinZ + bl}, {s * tw, y1, d.cabinZ + tl}, 0.07f, paint);   // A pillar
            n.beam(M_CUBE, {s * bw, y0, d.cabinZ - bl}, {s * tw, y1, d.cabinZ - tl}, 0.09f, paint);   // C pillar
            n.beam(M_CUBE, {s * bw, y0, d.cabinZ}, {s * tw, y1, d.cabinZ}, 0.07f, paint);             // B pillar
        }
    }
    // interior: seats, dashboard, steering wheel
    n.box({0, bodyTop - 0.02f, d.cabinZ + d.cabinLen * 0.45f}, {W * 0.85f, 0.16f, 0.3f}, interior);
    for (int s = -1; s <= 1; s += 2) {
        Vec3 seat(s * 0.38f, d.driverSeat.y + 0.33f, d.driverSeat.z - 0.05f);
        n.box(seat, {0.48f, 0.12f, 0.5f}, interior);
        n.box(seat + Vec3(0, 0.33f, -0.27f), {0.46f, 0.6f, 0.1f}, interior, {-12, 0, 0});
    }
    n.box({0, d.driverSeat.y + 0.36f, d.driverSeat.z - 0.85f}, {W * 0.8f, 0.14f, 0.45f}, interior);   // rear bench
    n.part(M_TORUS, {d.driverSeat.x, bodyTop + 0.12f, d.driverSeat.z + 0.55f}, {0.38f, 0.38f, 0.38f}, plastic, {70, 0, 0});

    // ---------------- lights ----------------
    Material head = st.headlights ? Mat::glow(Vec3(1.0f, 0.95f, 0.82f), 3.0f) : Material(Vec3(0.85f, 0.88f, 0.92f), 1.0f, 128.0f);
    Material tail = Mat::glow(Vec3(0.9f, 0.05f, 0.05f), st.brake ? 3.0f : (st.headlights ? 1.2f : 0.25f));
    bool blink = st.indicator && std::fmod(st.time, 0.8f) < 0.4f;
    Material ind = Mat::glow(Vec3(1.0f, 0.55f, 0.05f), blink ? 2.5f : 0.15f);
    for (int s = -1; s <= 1; s += 2) {
        n.box({s * W * 0.33f, bodyTop - 0.12f, L * 0.5f + 0.005f}, {0.36f, 0.12f, 0.06f}, head);
        n.box({s * W * 0.44f, bodyTop - 0.12f, L * 0.5f - 0.02f}, {0.08f, 0.1f, 0.06f}, ind);
        n.box({s * W * 0.36f, bodyTop - 0.08f, -L * 0.5f + 0.02f}, {0.32f, 0.12f, 0.06f}, tail);
        n.box({s * W * 0.20f, bodyTop - 0.08f, -L * 0.5f + 0.02f}, {0.08f, 0.1f, 0.06f},
              st.reversing ? Mat::glow(Vec3(1.0f), 2.0f) : Material(Vec3(0.85f), 0.8f, 96.0f));
        n.box({s * W * 0.44f, bodyTop - 0.08f, -L * 0.5f + 0.03f}, {0.07f, 0.08f, 0.05f}, ind);
    }
    n.box({0, bodyTop - 0.2f, L * 0.5f + 0.03f}, {W * 0.4f, 0.14f, 0.04f}, plastic);           // grille
    n.box({0, bodyBot + 0.14f, L * 0.5f + 0.1f}, {0.46f, 0.11f, 0.01f}, Material(Vec3(0.95f), 0.2f, 16.0f));   // plates
    n.box({0, bodyTop - 0.24f, -L * 0.5f - 0.01f}, {0.46f, 0.11f, 0.01f}, Material(Vec3(0.95f), 0.2f, 16.0f));
    // mirrors, door handles, door seams
    for (int s = -1; s <= 1; s += 2) {
        n.box({s * (W * 0.5f + 0.08f), bodyTop + 0.12f, d.cabinZ + d.cabinLen * 0.42f}, {0.12f, 0.1f, 0.16f}, paint);
        n.box({s * (W * 0.5f + 0.005f), bodyTop - 0.12f, d.driverSeat.z + 0.05f}, {0.02f, 0.03f, 0.14f}, chrome);
        n.box({s * (W * 0.5f + 0.005f), bodyTop - 0.12f, d.driverSeat.z - 0.95f}, {0.02f, 0.03f, 0.14f}, chrome);
        n.box({s * (W * 0.5f + 0.003f), bodyBot + bodyH * 0.5f, d.driverSeat.z - 0.45f}, {0.01f, bodyH * 0.95f, 0.012f}, plastic);
    }

    // ---------------- wheels (spin + steering) ----------------
    float wx = W * 0.5f - 0.11f, wz = d.wheelBase * 0.5f;
    for (int i = 0; i < 4; ++i) {
        float sx = (i % 2) ? 1.0f : -1.0f;
        bool front = i < 2;
        Node wh = n.at({sx * wx, R, front ? wz : -wz});
        if (front) wh = wh.rotY(st.steer);
        wh.part(M_CYLINDER, {0, 0, 0}, {2.05f * R, 0.24f, 2.05f * R}, Material(Vec3(0.02f), 0.05f, 8.0f), {0, 0, 90});  // wheel well shadow
        Node spin = wh.rotX(toDegrees(st.wheelSpin));
        spin.part(M_CYLINDER, {0, 0, 0}, {2.0f * R, 0.22f, 2.0f * R}, Mat::rubber(), {0, 0, 90});
        spin.part(M_CYLINDER, {sx * 0.005f, 0, 0}, {1.3f * R, 0.235f, 1.3f * R}, chrome, {0, 0, 90});
        for (int k = 0; k < 3; ++k)
            spin.box({sx * 0.12f, 0, 0}, {0.02f, 1.2f * R, 0.06f}, Material(Vec3(0.3f), 0.6f, 64.0f), {k * 60.0f, 0, 0});
        spin.part(M_CYLINDER, {sx * 0.125f, 0, 0}, {0.3f * R, 0.02f, 0.3f * R}, chrome, {0, 0, 90});
    }

    // ---------------- driver door (hinged child node) ----------------
    Node door = n.at(d.doorHinge).rotY(st.doorAngle);
    float doorLen = 1.0f;
    door.box({-0.01f, bodyBot + bodyH * 0.5f, -doorLen * 0.5f}, {0.04f, bodyH * 0.96f, doorLen}, paint);
    if (st.doorAngle > 1.0f) {
        door.box({0.02f, bodyBot + bodyH * 0.5f, -doorLen * 0.5f}, {0.04f, bodyH * 0.9f, doorLen * 0.95f}, interior);
        door.box({-0.02f, bodyTop + cabH * 0.45f, -doorLen * 0.5f}, {0.025f, cabH * 0.8f, doorLen * 0.85f}, glass, {0, 0, -18});
    }
    door.box({-0.035f, bodyTop - 0.12f, -doorLen + 0.15f}, {0.02f, 0.03f, 0.14f}, chrome);
}

}  // namespace Car
