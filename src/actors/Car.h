// =============================================================================
//  Car.h  -  One car model (a sedan, in red, blue or white) built from basic shapes.
//    body       = front + rear boxes, open under the driver door, wedge bonnet
//    windows    = 6 transparent glass panes (you can see the driver)
//    wheels     = tyre cylinder + rim cylinder + hub (spin + front steering)
//    lights     = emissive head / tail lights (+ real spot lights at night)
//    driver door= hinged child node (opens for the NPC)
// Right-hand drive (Bangladesh): the driver sits on the right (-X local).
// =============================================================================
#pragma once
#include "../gfx/Draw.h"

struct CarLook {
    Vec3 paint = Vec3(0.7f, 0.1f, 0.1f);   // one car model (sedan) in 3 colours
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
CarDims dims();
CarLook randomLook(unsigned seed);
void draw(const Node& root, const CarLook& look, const CarState& st);
}  // namespace Car
