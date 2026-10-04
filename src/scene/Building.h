// =============================================================================
//  Building.h  -  The three connected rooms, the outside paving (sidewalk,
//  parking lot, road), lamps, automatic sliding entrance doors, light switches.
// =============================================================================
#pragma once
#include <vector>
#include "../gfx/Draw.h"
#include "../gfx/Text.h"
#include "../game/Collision.h"

class Building {
public:
    void buildStatic(DrawSink& s, CollisionWorld& col);
    void registerLights();
    void update(float dt, const std::vector<Vec3>& people);
    void registerInteractions();
    void drawDynamic(DrawSink& dyn, float time, float hour, bool cutaway, const Vec3& cam) const;
    void drawText(TextRenderer& t, float time, bool lit) const;

    float entranceOpen() const { return m_doorOpen; }

private:
    void buildRoadAndLot(DrawSink& s, CollisionWorld& col);
    void buildInterior(DrawSink& s, CollisionWorld& col);
    void drawRoof(DrawSink& dyn) const;

    float m_doorOpen = 0.0f;
    bool m_doorClosed = true;     // collider switch for the sliding doors
    int m_idBase = 0;
    struct SwitchDef { Vec3 pos; float yaw; int group; const char* name; };
    std::vector<SwitchDef> m_switches;
    std::vector<Vec3> m_lotLamps, m_roadLamps;
};
