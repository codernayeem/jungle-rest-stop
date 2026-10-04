// =============================================================================
//  Human.h  -  Articulated person built from basic shapes.
//
//  Hierarchy (every child inherits its parent's transform):
//    root (feet, facing +Z)
//     +- pelvis
//         +- torso (abdomen, chest = flipped tapered box, shoulders)
//         |    +- neck +- head (face, ears, hair)
//         |    +- upper arm L/R +- forearm +- hand (+ held item)
//         +- thigh L/R +- shin +- foot
//  A Pose stores the joint angles; animations only change the Pose.
// =============================================================================
#pragma once
#include "../gfx/Draw.h"

struct HumanLook {
    Vec3 skin = Vec3(0.78f, 0.58f, 0.43f);
    Vec3 hair = Vec3(0.08f, 0.06f, 0.05f);
    Vec3 shirt = Vec3(0.2f, 0.4f, 0.7f);
    Vec3 pants = Vec3(0.15f, 0.17f, 0.24f);
    Vec3 shoes = Vec3(0.12f, 0.1f, 0.09f);
    int hairStyle = 0;      // 0 short, 1 long, 2 bun, 3 cap
    float height = 1.0f;    // scale (1.0 = 1.74 m)
    float width = 1.0f;     // body width factor
    bool longSleeve = false;
    bool glasses = false;
    bool skirt = false;     // long kurta / skirt style lower body
};

struct HumanPose {
    float thighL = 0, thighR = 0;        // + = backwards (deg)
    float kneeL = 0, kneeR = 0;          // + = bend
    float shoulderL = 0, shoulderR = 0;  // + = backwards
    float armOutL = 6, armOutR = 6;      // sideways
    float elbowL = -8, elbowR = -8;      // - = bend forward
    float torsoPitch = 0;                // + = lean forward
    float headYaw = 0, headPitch = 0;
    float pelvisDrop = 0;                // metres (sitting)
    float bob = 0;                       // vertical bounce while walking
};

enum HeldItem { HELD_NONE = 0, HELD_CAN, HELD_CUP, HELD_SNACK, HELD_PHONE, HELD_KEYS };

namespace Human {
HumanPose idle(float t);
HumanPose walk(float phase, float amount);    // amount 0..1 (speed)
HumanPose sit(float t);
HumanPose drive(float t);
HumanPose press(float t);                      // pressing buttons with right hand
HumanPose bend(float amount);                  // picking something up
HumanPose blend(const HumanPose& a, const HumanPose& b, float t);

HumanLook randomLook(unsigned seed);
void draw(const Node& root, const HumanLook& look, const HumanPose& pose, HeldItem held = HELD_NONE);
}  // namespace Human
