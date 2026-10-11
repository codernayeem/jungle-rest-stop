#include "Human.h"
#include "../scene/Materials.h"
#include <cmath>

namespace Human {

// ---------------------------------------------------------------- poses ------
HumanPose idle(float t) {
    HumanPose p;
    float b = std::sin(t * 1.6f);
    p.bob = b * 0.004f;
    p.shoulderL = b * 1.5f;
    p.shoulderR = -b * 1.5f;
    p.headYaw = std::sin(t * 0.37f) * 12.0f;
    return p;
}

HumanPose walk(float ph, float amt) {
    HumanPose p;
    float s = std::sin(ph), c = std::cos(ph);
    p.thighL = -s * 26.0f * amt;
    p.thighR = s * 26.0f * amt;
    p.kneeL = (std::max(0.0f, c) * 38.0f + 4.0f) * amt;
    p.kneeR = (std::max(0.0f, -c) * 38.0f + 4.0f) * amt;
    p.shoulderL = s * 22.0f * amt;
    p.shoulderR = -s * 22.0f * amt;
    p.elbowL = -12.0f - std::max(0.0f, -s) * 18.0f * amt;
    p.elbowR = -12.0f - std::max(0.0f, s) * 18.0f * amt;
    p.bob = (1.0f - std::fabs(s)) * 0.035f * amt;
    p.torsoPitch = 3.0f * amt;
    return p;
}

HumanPose sit(float t) {
    HumanPose p;
    p.pelvisDrop = 0.44f;
    p.thighL = p.thighR = -88.0f;
    p.kneeL = p.kneeR = 88.0f;
    p.shoulderL = p.shoulderR = -22.0f;
    p.elbowL = p.elbowR = -48.0f;
    p.armOutL = p.armOutR = 2.0f;
    p.torsoPitch = -4.0f;
    p.headYaw = std::sin(t * 0.25f) * 10.0f;
    p.headPitch = std::sin(t * 0.4f) * 3.0f;
    return p;
}

HumanPose drive(float t) {
    HumanPose p = sit(t);
    p.pelvisDrop = 0.52f;
    p.thighL = p.thighR = -80.0f;
    p.kneeL = p.kneeR = 60.0f;
    p.shoulderL = p.shoulderR = -55.0f;
    p.elbowL = p.elbowR = -30.0f;
    p.armOutL = p.armOutR = 8.0f;
    p.headYaw = std::sin(t * 0.3f) * 6.0f;
    return p;
}

HumanPose press(float t) {
    HumanPose p = idle(t);
    p.shoulderR = -62.0f + std::sin(t * 6.0f) * 6.0f;
    p.elbowR = -35.0f + std::sin(t * 6.0f) * 12.0f;
    p.armOutR = 2.0f;
    p.headPitch = 8.0f;
    return p;
}

HumanPose bend(float a) {
    HumanPose p;
    p.torsoPitch = 50.0f * a;
    p.thighL = p.thighR = -20.0f * a;
    p.kneeL = p.kneeR = 30.0f * a;
    p.pelvisDrop = 0.10f * a;
    p.shoulderR = -60.0f * a;
    p.shoulderL = -20.0f * a;
    p.elbowR = -10.0f;
    p.headPitch = 15.0f * a;
    return p;
}

HumanPose blend(const HumanPose& a, const HumanPose& b, float t) {
    HumanPose r;
    const float* pa = &a.thighL;
    const float* pb = &b.thighL;
    float* pr = &r.thighL;
    const int n = sizeof(HumanPose) / sizeof(float);
    for (int i = 0; i < n; ++i) pr[i] = lerpf(pa[i], pb[i], t);
    return r;
}

// Two kinds of people, each with one fixed colour scheme:
//   type 0 - man   : short hair, blue shirt, dark trousers
//   type 1 - woman : long hair, red kurta (long top over the legs), light trousers
HumanLook look(int type) {
    HumanLook l;
    if (type % 2 == 0) {
        l.skin = Vec3(0.72f, 0.52f, 0.38f);
        l.hair = Vec3(0.06f, 0.05f, 0.04f);
        l.shirt = Vec3(0.15f, 0.35f, 0.72f);
        l.pants = Vec3(0.13f, 0.14f, 0.18f);
        l.shoes = Vec3(0.10f, 0.08f, 0.07f);
        l.hairStyle = 0;
        l.skirt = false;
        l.height = 1.0f;
    } else {
        l.skin = Vec3(0.80f, 0.60f, 0.46f);
        l.hair = Vec3(0.08f, 0.06f, 0.05f);
        l.shirt = Vec3(0.78f, 0.14f, 0.20f);
        l.pants = Vec3(0.86f, 0.80f, 0.68f);
        l.shoes = Vec3(0.45f, 0.20f, 0.12f);
        l.hairStyle = 1;
        l.skirt = true;
        l.height = 0.95f;
        l.width = 0.94f;
    }
    return l;
}

HumanLook randomLook(unsigned seed) { return look((int)(seed % 2)); }

// ---------------------------------------------------------------- model ------
static void heldItem(const Node& hand, HeldItem item) {
    TagScope tagScope;
    partTag("Held item");
    switch (item) {
        case HELD_CAN:
            hand.cyl({0, -0.06f, 0.03f}, {0.065f, 0.12f, 0.065f}, Material(Vec3(0.8f, 0.1f, 0.1f), 0.7f, 64.0f));
            break;
        case HELD_CUP:
            hand.tcone({0, -0.06f, 0.04f}, {0.08f, 0.09f, 0.08f}, Material(Vec3(0.96f), 0.5f, 64.0f), {180, 0, 0});
            break;
        case HELD_SNACK:
            hand.box({0, -0.07f, 0.03f}, {0.12f, 0.15f, 0.04f}, Material(Vec3(0.9f, 0.7f, 0.1f), 0.4f, 32.0f));
            break;
        case HELD_PHONE:
            hand.box({0, -0.05f, 0.03f}, {0.07f, 0.14f, 0.01f}, Mat::black());
            break;
        case HELD_KEYS:
            hand.torus({0, -0.05f, 0.02f}, {0.05f, 0.05f, 0.05f}, Mat::chrome(), {90, 0, 0});
            break;
        default: break;
    }
}

void draw(const Node& root0, const HumanLook& L, const HumanPose& P, HeldItem held) {
    TagScope tagScope;
    const float w = L.width;
    Node root = root0.scl(Vec3(L.height, L.height, L.height));
    Material skin(L.skin, 0.15f, 16.0f);
    Material hair(L.hair, 0.25f, 24.0f);
    Material shirt = Mat::fabric(L.shirt, 8.0f);
    Material pants = Mat::fabric(L.pants, 8.0f);
    Material shoe(L.shoes, 0.35f, 32.0f);
    Material dark = Mat::black();

    Node pelvis = root.at({0, 0.93f - P.pelvisDrop + P.bob, 0});
    // with a kurta the hips are covered by the top, so no belt and no trouser band shows
    partTag("Pelvis");
    pelvis.cyl({0, 0.02f, 0}, {0.34f * w, 0.18f, 0.21f}, L.skirt ? shirt : pants);
    partTag("Belt");
    if (!L.skirt) pelvis.box({0, 0.10f, 0}, {0.335f * w, 0.035f, 0.205f}, Material(Vec3(0.12f, 0.08f, 0.05f), 0.4f, 32.0f));  // belt

    // ---------------- legs ----------------
    for (int side = -1; side <= 1; side += 2) {
        bool left = side > 0;   // +X is the character's left (it faces +Z)
        Node hip = pelvis.at({side * 0.092f * w, -0.03f, 0}).rotX(left ? P.thighL : P.thighR);
        partTag("Hip joints");
        hip.sph({0, 0, 0}, {0.16f * w, 0.16f, 0.17f}, pants);                                  // hip joint
        partTag("Thighs");
        hip.tcone({0, -0.225f, 0}, {0.17f * w, 0.45f, 0.17f}, pants, {180, 0, 0});             // thigh (wide at the hip)
        Node knee = hip.at({0, -0.45f, 0}).rotX(left ? P.kneeL : P.kneeR);
        partTag("Knees");
        knee.sph({0, 0, 0}, {0.12f * w, 0.12f, 0.125f}, pants);
        partTag("Shins");
        knee.tcone({0, -0.21f, 0}, {0.135f * w, 0.42f, 0.135f}, pants, {180, 0, 0});           // shin (calf at the top)
        Node ankle = knee.at({0, -0.42f, 0}).rotX(-(left ? P.kneeL : P.kneeR) * 0.3f - (left ? P.thighL : P.thighR) * 0.4f);
        partTag("Ankles");
        ankle.sph({0, 0, 0}, Vec3(0.075f), pants);
        partTag("Shoes");
        ankle.box({0, -0.025f, 0.045f}, {0.11f, 0.07f, 0.26f}, shoe);
        ankle.part(M_CYLINDER, {0, -0.025f, 0.172f}, {0.07f, 0.11f, 0.06f}, shoe, {0, 0, 90});   // rounded toe
        ankle.box({0, -0.058f, 0.045f}, {0.115f, 0.015f, 0.265f}, Material(Vec3(0.9f), 0.2f, 16.0f));
    }
    partTag("Kurta hem");
    if (L.skirt) pelvis.tcone({0, -0.17f, 0}, {0.40f * w, 0.36f, 0.30f}, shirt, {180, 0, 0});   // kurta hem

    // ---------------- torso ----------------
    Node torso = pelvis.at({0, 0.08f, 0}).rotX(P.torsoPitch);
    partTag("Waist");
    torso.cyl({0, 0.10f, 0}, {0.30f * w, 0.22f, 0.19f}, shirt);                       // waist (elliptic cylinder)
    partTag("Chest");
    torso.part(M_FRUSTUM_BOX, {0, 0.30f, 0}, {0.42f * w, 0.30f, 0.23f}, shirt, {0, 0, 180});   // chest: tapered box upside down (V shape)
    partTag("Shoulder line");
    torso.part(M_CYLINDER, {0, 0.425f, 0}, {0.12f, 0.40f * w, 0.17f}, shirt, {0, 0, 90});      // rounded shoulder line
    partTag("Shoulder joints");
    for (int side = -1; side <= 1; side += 2) torso.sph({side * 0.215f * w, 0.425f, 0}, Vec3(0.115f), shirt);   // shoulder joints
    partTag("Collar");
    torso.part(M_TORUS, {0, 0.47f, 0}, {0.2f, 0.12f, 0.17f}, shirt);                 // collar ring

    // ---------------- head ----------------
    Node neck = torso.at({0, 0.46f, 0});
    partTag("Neck");
    neck.cyl({0, 0.04f, 0}, {0.095f, 0.09f, 0.095f}, skin);
    Node head = neck.at({0, 0.065f, 0}).rotY(P.headYaw).rotX(P.headPitch);
    partTag("Head/Jaw");
    head.sph({0, 0.03f, 0.03f}, {0.13f, 0.09f, 0.12f}, skin);                           // jaw / chin
    partTag("Head/Head");
    head.sph({0, 0.105f, 0.005f}, {0.20f, 0.245f, 0.215f}, skin);
    partTag("Head/Nose");
    head.sph({0, 0.085f, 0.105f}, {0.032f, 0.045f, 0.04f}, skin);                      // nose
    for (int e = -1; e <= 1; e += 2) {
        partTag("Head/Eyes");
        head.sph({e * 0.042f, 0.125f, 0.092f}, Vec3(0.026f), Material(Vec3(0.95f), 0.4f, 32.0f));   // eye white
        head.sph({e * 0.042f, 0.125f, 0.103f}, Vec3(0.014f), dark);                                 // pupil
        partTag("Head/Eyebrows");
        head.box({e * 0.042f, 0.158f, 0.098f}, {0.04f, 0.008f, 0.01f}, hair);                       // eyebrow
        partTag("Head/Ears");
        head.sph({e * 0.102f, 0.10f, 0.0f}, {0.03f, 0.055f, 0.035f}, skin);                         // ear
    }
    partTag("Head/Mouth");
    head.box({0, 0.045f, 0.098f}, {0.05f, 0.009f, 0.01f}, Material(L.skin * 0.7f + Vec3(0.12f, 0.0f, 0.0f), 0.2f, 16.0f));  // mouth
    partTag("Head/Glasses");
    if (L.glasses) {
        for (int e = -1; e <= 1; e += 2) head.torus({e * 0.042f, 0.125f, 0.112f}, {0.07f, 0.07f, 0.07f}, dark, {90, 0, 0});
        head.box({0, 0.128f, 0.113f}, {0.03f, 0.006f, 0.006f}, dark);
    }
    partTag("Head/Hair");
    switch (L.hairStyle) {
        case 0:
            head.part(M_HEMISPHERE, {0, 0.12f, -0.018f}, {0.215f, 0.25f, 0.232f}, hair);
            head.sph({0, 0.11f, -0.045f}, {0.207f, 0.21f, 0.17f}, hair);
            break;
        case 1:
            head.part(M_HEMISPHERE, {0, 0.12f, -0.02f}, {0.222f, 0.26f, 0.236f}, hair);
            head.sph({0, 0.02f, -0.07f}, {0.215f, 0.36f, 0.13f}, hair);
            break;
        case 2:
            head.part(M_HEMISPHERE, {0, 0.12f, -0.018f}, {0.218f, 0.25f, 0.234f}, hair);
            head.sph({0, 0.20f, -0.10f}, Vec3(0.10f), hair);
            break;
        default: {
            Material cap = Mat::fabric(L.shirt * 0.6f + Vec3(0.1f), 6.0f);
            head.part(M_HEMISPHERE, {0, 0.13f, -0.012f}, {0.225f, 0.23f, 0.238f}, cap);
            head.box({0, 0.145f, 0.13f}, {0.18f, 0.012f, 0.1f}, cap);
            break;
        }
    }

    // ---------------- arms ----------------
    for (int side = -1; side <= 1; side += 2) {
        bool left = side > 0;
        Node sh = torso.at({side * 0.225f * w, 0.42f, 0}).rotX(left ? P.shoulderL : P.shoulderR).rotZ(side * (left ? P.armOutL : P.armOutR));
        partTag("Upper arms");
        sh.tcone({0, -0.145f, 0}, {0.11f, 0.29f, 0.11f}, shirt, {180, 0, 0});               // upper arm (thicker at the shoulder)
        Node el = sh.at({0, -0.29f, 0}).rotX(left ? P.elbowL : P.elbowR);
        Material lower = L.longSleeve ? shirt : skin;
        partTag("Elbows");
        el.sph({0, 0, 0}, Vec3(0.078f), lower);
        partTag("Forearms");
        el.tcone({0, -0.13f, 0}, {0.088f, 0.26f, 0.088f}, lower, {180, 0, 0});               // forearm (thinner at the wrist)
        Node hand = el.at({0, -0.29f, 0});
        partTag("Hands");
        hand.sph({0, 0, 0.005f}, {0.068f, 0.10f, 0.045f}, skin);
        hand.sph({-side * 0.022f, 0.01f, 0.028f}, {0.025f, 0.05f, 0.025f}, skin, {-20, 0, 0});   // thumb
        if (!left) heldItem(hand, held);
    }
}

}  // namespace Human
