#include "Car.h"
#include "../scene/Materials.h"
#include <cmath>

namespace Car {

CarDims dims() {
    CarDims d = {4.4f, 1.78f, 1.42f, 2.65f, 0.32f, 2.3f, -0.20f, 0.86f, Vec3(), Vec3()};
    d.driverSeat = Vec3(-0.38f, 0.05f, -0.05f);
    d.doorHinge = Vec3(-d.width * 0.5f - 0.015f, 0.0f, d.driverSeat.z + 0.78f);
    return d;
}

// three paint colours: red, blue, white
CarLook randomLook(unsigned seed) {
    const Vec3 paints[3] = {Vec3(0.75f, 0.06f, 0.08f), Vec3(0.10f, 0.25f, 0.62f), Vec3(0.92f, 0.92f, 0.92f)};
    CarLook l;
    l.paint = paints[seed % 3];
    return l;
}

void draw(const Node& n, const CarLook& look, const CarState& st) {
    TagScope tagScope;
    CarDims d = dims();
    const float L = d.length, W = d.width, R = d.wheelR;
    Material paint = Mat::carPaint(look.paint);
    Material plastic = Material(Vec3(0.08f, 0.08f, 0.09f), 0.3f, 24.0f);
    Material chrome = Mat::chrome();
    Material glass = Material(Vec3(0.10f, 0.14f, 0.18f), 1.0f, 160.0f).a(0.45f).fl(MF_NO_SHADOW);
    Material interior = Material(Vec3(0.12f, 0.11f, 0.10f), 0.1f, 8.0f);
    float bodyBot = R * 0.95f, bodyTop = d.hoodTop;
    float bodyH = bodyTop - bodyBot;

    // The driver door spans z in [dz0, dz1] on the -X side. The body there is
    // left open (only the floor and the far side skin), so when the door swings
    // out you see the inside of the car - not a second, closed door.
    float dz1 = d.doorHinge.z, dz0 = dz1 - 1.0f;
    float zf = L * 0.5f - 0.05f, zr = -L * 0.5f + 0.05f;
    float yMid = bodyBot + bodyH * 0.5f;

    // ---------------- body ----------------
    partTag("Front block");
    n.box({0, yMid, (dz1 + zf) * 0.5f}, {W, bodyH, zf - dz1}, paint);                      // front block (wings, engine bay)
    partTag("Rear block");
    n.box({0, yMid, (zr + dz0) * 0.5f}, {W, bodyH, dz0 - zr}, paint);                      // rear block (rear doors, boot)
    partTag("Side skin");
    n.box({W * 0.5f - 0.03f, yMid, (dz0 + dz1) * 0.5f}, {0.06f, bodyH, dz1 - dz0}, paint);  // passenger side skin
    partTag("Floor + sills");
    n.box({0, bodyBot + 0.06f, (dz0 + dz1) * 0.5f}, {W - 0.02f, 0.12f, dz1 - dz0}, interior); // floor + sill
    n.box({-W * 0.5f + 0.04f, bodyBot + 0.05f, (dz0 + dz1) * 0.5f}, {0.08f, 0.1f, dz1 - dz0}, paint);  // driver sill
    partTag("Rounded nose");
    n.part(M_CYLINDER, {0, yMid, zf}, {bodyH * 0.98f, W * 0.98f, 0.24f}, paint, {0, 0, 90});   // rounded nose
    partTag("Bumpers + underbody");
    n.box({0, bodyBot + 0.12f, L * 0.5f + 0.02f}, {W * 0.96f, 0.22f, 0.14f}, plastic);    // front bumper
    n.box({0, bodyBot + 0.12f, -L * 0.5f}, {W * 0.96f, 0.22f, 0.14f}, plastic);           // rear bumper
    n.box({0, bodyBot - 0.03f, 0}, {W * 0.9f, 0.08f, L * 0.86f}, plastic);               // underbody
    // bonnet (sloping wedge) and boot lid
    float hoodLen = 1.15f;
    partTag("Bonnet (wedge)");
    n.part(M_WEDGE, {0, bodyTop + 0.04f, L * 0.5f - hoodLen * 0.5f - 0.05f}, {W * 0.97f, 0.08f, hoodLen}, paint);
    partTag("Boot lid");
    n.box({0, bodyTop + 0.02f, -L * 0.5f + 0.45f}, {W * 0.96f, 0.04f, 0.8f}, paint);

    // ---------------- cabin: roof, pillars and separate glass panes ----------------
    float cz = d.cabinZ;
    float bw = W * 0.46f, bl = d.cabinLen * 0.5f;
    float tw = W * 0.322f, tl = bl * 0.6f;
    float y0 = bodyTop, y1 = d.height - 0.04f;
    Vec3 FLt(-tw, y1, cz + tl), RRt(tw, y1, cz - tl);
    // the B pillar sits at the back edge of the driver door
    Vec3 BLb(-bw, y0, dz0), BLt(-tw, y1, dz0);
    // Every window is cut into exact pieces so no glass pokes out past a pillar:
    //  windscreen / rear window = parallelogram (width 2*tw) + painted corner triangles
    //  front side window = parallelogram + small triangular quarter glass (divider bar on the seam)
    //  rear side window  = parallelogram + painted triangle (thick C pillar)
    const float gT = 0.012f;
    partTag("Windscreen");
    n.panel({-tw, y0, cz + bl}, {tw, y0, cz + bl}, FLt, gT, glass);   // windscreen
    partTag("Rear window");
    n.panel({tw, y0, cz - bl}, {-tw, y0, cz - bl}, RRt, gT, glass);   // rear window
    for (int s = -1; s <= 1; s += 2) {
        float x0 = s * bw, x1 = s * tw;
        Vec3 Fb(x0, y0, cz + bl), Ft(x1, y1, cz + tl), Rb(x0, y0, cz - bl), Rt(x1, y1, cz - tl);
        Vec3 Bb(x0, y0, dz0), Bt(x1, y1, dz0);
        Vec3 P(x0, y0, cz + tl), Q(x0, y0, cz - tl);       // Ft and Rt dropped straight down the window
        partTag("Pillar panels");
        n.tri({x1, y0, cz + bl}, Fb, Ft, 0.03f, paint);     // windscreen corner
        n.tri({x1, y0, cz - bl}, Rb, Rt, 0.03f, paint);     // rear window corner
        partTag("Quarter glass");
        n.tri(P, Fb, Ft, gT, glass);                        // quarter glass (fixed to the body)
        partTag("Pillars");
        n.beam(M_CUBE, P, Ft, 0.04f, paint);                // quarter glass divider
        partTag("Pillar panels");
        n.tri(Q, Rb, Rt, 0.03f, paint);                     // C pillar panel
        partTag("Side windows");
        n.panel(Bb, Q, Bt, gT, glass);                      // rear side window
        if (s > 0) n.panel(P, Bb, Ft, gT, glass);           // passenger front window (driver's is on the door)
        partTag("Pillars");
        n.beam(M_CUBE, Fb, Ft, 0.07f, paint);               // A pillar
        n.beam(M_CUBE, Rb, Rt, 0.09f, paint);               // C pillar
        n.beam(M_CUBE, Bb, Bt, 0.08f, paint);               // B pillar
    }
    partTag("Roof");
    n.box({0, d.height - 0.03f, cz}, {tw * 2.0f + 0.04f, 0.06f, tl * 2.0f + 0.04f}, paint);   // roof
    // interior: seats, dashboard, steering wheel
    partTag("Dashboard");
    n.box({0, bodyTop - 0.02f, d.cabinZ + d.cabinLen * 0.45f}, {W * 0.85f, 0.16f, 0.3f}, interior);
    partTag("Seats");
    for (int s = -1; s <= 1; s += 2) {
        Vec3 seat(s * 0.38f, d.driverSeat.y + 0.33f, d.driverSeat.z - 0.05f);
        n.box(seat, {0.48f, 0.12f, 0.5f}, interior);
        n.box(seat + Vec3(0, 0.33f, -0.27f), {0.46f, 0.6f, 0.1f}, interior, {-12, 0, 0});
    }
    n.box({0, d.driverSeat.y + 0.36f, d.driverSeat.z - 0.85f}, {W * 0.8f, 0.14f, 0.45f}, interior);   // rear bench
    partTag("Steering wheel");
    n.part(M_TORUS, {d.driverSeat.x, bodyTop + 0.12f, d.driverSeat.z + 0.55f}, {0.38f, 0.38f, 0.38f}, plastic, {70, 0, 0});

    // ---------------- lights ----------------
    Material head = st.headlights ? Mat::glow(Vec3(1.0f, 0.95f, 0.82f), 3.0f) : Material(Vec3(0.85f, 0.88f, 0.92f), 1.0f, 128.0f);
    Material tail = Mat::glow(Vec3(0.9f, 0.05f, 0.05f), st.brake ? 3.0f : (st.headlights ? 1.2f : 0.25f));
    bool blink = st.indicator && std::fmod(st.time, 0.8f) < 0.4f;
    Material ind = Mat::glow(Vec3(1.0f, 0.55f, 0.05f), blink ? 2.5f : 0.15f);
    for (int s = -1; s <= 1; s += 2) {
        partTag("Headlights");
        n.box({s * W * 0.33f, bodyTop - 0.12f, L * 0.5f + 0.005f}, {0.36f, 0.12f, 0.06f}, head);
        partTag("Indicators");
        n.box({s * W * 0.44f, bodyTop - 0.12f, L * 0.5f - 0.02f}, {0.08f, 0.1f, 0.06f}, ind);
        partTag("Tail lights");
        n.box({s * W * 0.36f, bodyTop - 0.08f, -L * 0.5f + 0.02f}, {0.32f, 0.12f, 0.06f}, tail);
        n.box({s * W * 0.20f, bodyTop - 0.08f, -L * 0.5f + 0.02f}, {0.08f, 0.1f, 0.06f},
              st.reversing ? Mat::glow(Vec3(1.0f), 2.0f) : Material(Vec3(0.85f), 0.8f, 96.0f));
        partTag("Indicators");
        n.box({s * W * 0.44f, bodyTop - 0.08f, -L * 0.5f + 0.03f}, {0.07f, 0.08f, 0.05f}, ind);
    }
    partTag("Grille + plates");
    n.box({0, bodyTop - 0.2f, L * 0.5f + 0.03f}, {W * 0.4f, 0.14f, 0.04f}, plastic);           // grille
    n.box({0, bodyBot + 0.14f, L * 0.5f + 0.1f}, {0.46f, 0.11f, 0.01f}, Material(Vec3(0.95f), 0.2f, 16.0f));   // plates
    n.box({0, bodyTop - 0.24f, -L * 0.5f - 0.01f}, {0.46f, 0.11f, 0.01f}, Material(Vec3(0.95f), 0.2f, 16.0f));
    // mirrors, door handles, door seams
    for (int s = -1; s <= 1; s += 2) {
        partTag("Mirrors");
        n.box({s * (W * 0.5f + 0.08f), bodyTop + 0.12f, d.cabinZ + d.cabinLen * 0.42f}, {0.12f, 0.1f, 0.16f}, paint);
        partTag("Handles + seams");
        if (s > 0) n.box({s * (W * 0.5f + 0.005f), bodyTop - 0.12f, d.driverSeat.z + 0.05f}, {0.02f, 0.03f, 0.14f}, chrome);
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
        partTag("Wheel wells");
        wh.part(M_CYLINDER, {0, 0, 0}, {2.05f * R, 0.24f, 2.05f * R}, Material(Vec3(0.02f), 0.05f, 8.0f), {0, 0, 90});  // wheel well shadow
        Node spin = wh.rotX(toDegrees(st.wheelSpin));
        partTag("Tyres");
        spin.part(M_CYLINDER, {0, 0, 0}, {2.0f * R, 0.22f, 2.0f * R}, Mat::rubber(), {0, 0, 90});
        partTag("Rims + hubs");
        spin.part(M_CYLINDER, {sx * 0.005f, 0, 0}, {1.3f * R, 0.235f, 1.3f * R}, chrome, {0, 0, 90});
        for (int k = 0; k < 3; ++k)
            spin.box({sx * 0.12f, 0, 0}, {0.02f, 1.2f * R, 0.06f}, Material(Vec3(0.3f), 0.6f, 64.0f), {k * 60.0f, 0, 0});
        spin.part(M_CYLINDER, {sx * 0.125f, 0, 0}, {0.3f * R, 0.02f, 0.3f * R}, chrome, {0, 0, 90});
    }

    // ---------------- driver door (hinged child node) ----------------
    // door node = T(hinge) * Ry(angle). Its window corners are the same points as
    // in the car, minus the hinge position, so the glass swings with the door.
    Node door = n.at(d.doorHinge).rotY(st.doorAngle);
    float doorLen = dz1 - dz0;
    partTag("Driver door");
    door.box({-0.01f, yMid, -doorLen * 0.5f}, {0.04f, bodyH * 0.96f, doorLen}, paint);
    door.box({0.03f, yMid, -doorLen * 0.5f}, {0.03f, bodyH * 0.9f, doorLen * 0.96f}, interior);   // inner trim
    door.box({0.05f, yMid - 0.05f, -doorLen * 0.5f}, {0.04f, 0.05f, doorLen * 0.6f}, interior);   // arm rest
    // door glass = the same parallelogram as the passenger side, mirrored
    Vec3 h = d.doorHinge;
    Vec3 dFb(-bw, y0, cz + tl), dFt(-tw, y1, cz + tl);
    partTag("Door glass");
    door.panel(dFb - h, BLb - h, dFt - h, gT, glass);
    partTag("Driver door");
    door.beam(M_CUBE, BLb - h, BLt - h, 0.035f, paint);         // window frame (rear edge)
    door.beam(M_CUBE, BLt - h, dFt - h, 0.035f, paint);         // window frame (top edge)
    door.box({-0.035f, bodyTop - 0.12f, -doorLen + 0.15f}, {0.02f, 0.03f, 0.14f}, chrome);   // handle
}

}  // namespace Car
