// =============================================================================
//  Nature.h  -  Procedural jungle around the rest stop.
//  Tree species (all made from basic shapes + alpha-tested leaf cards):
//    rainforest tree (buttress roots + layered canopy), coconut palm (curved
//    segmented trunk + arched fronds), emergent giant tree, bamboo clump,
//    banana plant, bushes, ferns, grass tufts, flowers, rocks, fallen logs, vines.
// =============================================================================
#pragma once
#include <vector>
#include "../gfx/Draw.h"
#include "../game/Collision.h"

namespace Nature {
// single-object builders (also used by the report's object showcase)
void rainforestTree(const Node& n, float s, Rng& rng);
void palmTree(const Node& n, float s, Rng& rng);
void emergentTree(const Node& n, float s, Rng& rng);
void bambooClump(const Node& n, float s, Rng& rng);
void bananaPlant(const Node& n, float s, Rng& rng);
void bush(const Node& n, float s, Rng& rng);
void fern(const Node& n, float s);
void rock(const Node& n, float s, Rng& rng);
void fallenLog(const Node& n, float len, Rng& rng);
void bird(const Node& n, float flapDeg);

struct JungleStats { int trees = 0, palms = 0, bamboo = 0, bushes = 0, ferns = 0, grass = 0, rocks = 0, logs = 0; };

// Fill the whole world with vegetation (static geometry + trunk colliders)
JungleStats buildJungle(DrawSink& s, CollisionWorld& col, bool lowQuality);
// Decorative planting on the lawn strips next to the building / lot
void buildGarden(DrawSink& s, CollisionWorld& col);

// Night-time fireflies and day-time birds (dynamic)
class Wildlife {
public:
    void init();
    void update(float dt, float night, const Vec3& camPos);
    void draw(DrawSink& dyn, float night, float time) const;
private:
    struct Fly { Vec3 base; float phase, speed; };
    struct Bird { Vec3 center; float radius, angle, speed, height, flap; };
    std::vector<Fly> m_flies;
    std::vector<Bird> m_birds;
    Vec3 m_center;
};
}  // namespace Nature
