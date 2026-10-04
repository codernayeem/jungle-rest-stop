// =============================================================================
//  Layout.h  -  Where everything is. All positions in metres, Y is up.
//  The building faces +Z (towards the parking lot).
//
//           z = -5  +-----------+---------------------+---------------+
//                   |  ATM ROOM |     MAIN LOUNGE     |    LIBRARY    |
//                   |           |  TV, sofas, coffee, |  + AQUARIUM   |
//                   |  (ATM)    |  3 vending machines |               |
//           z =  5  +-----------+-----[ entrance ]----+---------------+
//                  x=-12      x=-7                  x=7            x=15
//                        sidewalk (z 5 .. 8.5)
//                        parking lot (z 8.5 .. 23.5), road along z = 16
//                        jungle everywhere else
// =============================================================================
#pragma once
#include "../core/Math3D.h"

namespace Layout {
constexpr float WALL_H = 3.6f;     // floor to ceiling
constexpr float WALL_T = 0.2f;     // wall thickness
constexpr float ROOF_Y = 3.6f;

// rooms (inner faces of walls are at these lines +- WALL_T/2)
constexpr float ATM_X0 = -12.0f, ATM_X1 = -7.0f;
constexpr float LNG_X0 = -7.0f, LNG_X1 = 7.0f;
constexpr float LIB_X0 = 7.0f, LIB_X1 = 15.0f;
constexpr float Z_BACK = -5.0f, Z_FRONT = 5.0f;

// openings
constexpr float ENTRANCE_HALF = 1.2f;                         // sliding doors x in [-1.2, 1.2]
constexpr float DOOR_ATM_Z0 = -3.4f, DOOR_ATM_Z1 = -1.8f;     // in wall x = -7
constexpr float DOOR_LIB_Z0 = -1.2f, DOOR_LIB_Z1 = 0.4f;      // in wall x = 7
constexpr float DOOR_H = 2.4f;

// outside
constexpr float SIDEWALK_Z1 = 8.5f;
constexpr float LOT_X0 = -24.0f, LOT_X1 = 24.0f, LOT_Z0 = 8.5f, LOT_Z1 = 23.5f;
constexpr float AISLE_Z = 16.0f;
constexpr float SPOT_NORTH_Z = 11.2f, SPOT_SOUTH_Z = 20.8f;
constexpr int SPOTS_PER_ROW = 10;
inline float spotX(int i) {  // spots at +-3, 6, 9, 12, 15 (gap at x=0 is the walkway)
    int k = i % 5;
    float x = 3.0f + 3.0f * k;
    return (i < 5) ? -x : x;
}

// main lounge furniture anchors
const Vec3 VENDING_POS[3] = {Vec3(2.4f, 0, -4.5f), Vec3(3.7f, 0, -4.5f), Vec3(5.0f, 0, -4.5f)};
const Vec3 TV_POS = Vec3(0.0f, 1.75f, -4.86f);
const Vec3 COFFEE_POS = Vec3(-5.0f, 0.92f, -4.55f);   // machine base sits on counter
const Vec3 ATM_POS = Vec3(-9.5f, 0.0f, -4.55f);
const Vec3 AQUARIUM_POS = Vec3(9.8f, 0.0f, 4.45f);

// building bounding box (used for "indoor" ambient light)
constexpr float BLD_X0 = -12.1f, BLD_X1 = 15.1f, BLD_Z0 = -5.1f, BLD_Z1 = 5.1f;

// world extent
constexpr float WORLD_HALF = 140.0f;
}  // namespace Layout

// Ground height (flat clearing, gentle hills in the jungle)
float terrainHeight(float x, float z);
// Centre line of the road (straight through the lot, gently curving in the jungle)
float roadCenterZ(float x);
// Is a point part of the cleared area (building, lot, road) where no jungle grows?
bool isClearedArea(float x, float z, float margin = 0.0f);
