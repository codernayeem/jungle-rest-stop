// =============================================================================
//  Furniture.h  -  Reusable furniture models, each built from basic shapes.
//  Every function draws the object with its local origin on the floor, centred,
//  facing +Z. The caller positions it with a Node (translate + rotate).
// =============================================================================
#pragma once
#include "../gfx/Draw.h"

namespace Furniture {
void sofa(const Node& n, int seats, const Vec3& fabricColor);           // seat height 0.45
void armchair(const Node& n, const Vec3& fabricColor);
void coffeeTable(const Node& n, float w, float d);
void sideTable(const Node& n);
void tableLamp(const Node& n, bool on);                                // emissive shade when on
void rug(const Node& n, float w, float d, const Vec3& color, const Vec3& border);
void pottedPlant(const Node& n, float size, int variant);
void bookshelf(const Node& n, float w, float h);
void floorLamp(const Node& n, bool on);
void wallClock(const Node& n, float hour);                            // n at clock centre, facing +Z
void painting(const Node& n, float w, float h, int variant);           // n at centre, facing +Z
void readingTable(const Node& n);
void chair(const Node& n, const Vec3& color);
void bench(const Node& n, float len);                                  // outdoor bench
void trashBin(const Node& n, const Vec3& color);
void counter(const Node& n, float w, float d, float h);               // kitchen counter + cabinets
void wallCabinet(const Node& n, float w);
void pendantLamp(const Node& n, float cordLen, bool on, const Vec3& shadeColor); // n at ceiling point
void spotFixture(const Node& n, bool on);                              // recessed downlight
void tubeLight(const Node& n, float len, bool on);                     // ceiling fluorescent panel
void lightSwitch(const Node& n, bool on, bool hovered);
void streetLamp(const Node& n, float height, bool on);                 // n at post base, arm points +Z
void bollard(const Node& n, bool on);
void stanchion(const Node& n);
void securityCamera(const Node& n, float t);
void smokeDetector(const Node& n);
void acUnit(const Node& n);                                            // roof air conditioner
void signBoard(const Node& n, float w, float h, const Vec3& color, bool lit);
}  // namespace Furniture
