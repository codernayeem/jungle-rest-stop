// =============================================================================
//  Car.h  -  Cars built from basic shapes (sedan, SUV, hatchback).
//    body       = lower box + wedge bonnet + tapered-box cabin
//    windows    = transparent boxes (you can see the driver)
//    wheels     = tyre cylinder + rim cylinder + hub (spin + front steering)
//    lights     = emissive head / tail lights (+ real spot lights at night)
//    driver door= hinged child node (opens for the NPC)
// Right-hand drive (Bangladesh): the driver sits on the right (-X local).
// =============================================================================
#pragma once
#include "../gfx/Draw.h"

struct CarLook {
    Vec3 paint = Vec3(0.7f, 0.1f, 0.1f);
    int type = 0;   // 0 sedan, 1 SUV, 2 hatchback
};

struct CarState {
    float doorAngle = 0;     // driver door (deg)
    float wheelSpin = 0;     // radians
    float steer = 0;         // deg (front wheels)
    bool headlights = false;
    bool brake = false;
    bool reversing = false;
    bool indicator = false;  // blinking orange (parking)
    float time = 0;
};

struct CarDims {
    float length, width, height, wheelBase, wheelR, cabinLen, cabinZ, hoodTop;
    Vec3 driverSeat;   // local position of the driver's hip
    Vec3 doorHinge;    // local position of the driver door hinge
};

namespace Car {
CarDims dims(int type);
CarLook randomLook(unsigned seed);
void draw(const Node& root, const CarLook& look, const CarState& st);
}  // namespace Car
