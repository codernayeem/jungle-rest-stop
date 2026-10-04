// =============================================================================
//  Lights.h  -  Registry of every light in the scene.
//  Static lights (bulbs, street lamps ...) are registered once; dynamic ones
//  (car head lights, TV glow) every frame. Each frame the closest lights to the
//  camera are sent to the shader (max 12 point + 8 spot lights).
// =============================================================================
#pragma once
#include <vector>
#include "../gfx/Renderer.h"

enum LightGroup {
    LG_LOUNGE = 0,   // ceiling bulbs of the main lounge
    LG_ATM,          // ATM room tube light
    LG_LIBRARY,      // library pendants
    LG_SPOTS,        // track / recessed spot lights + reading lamp
    LG_OUTDOOR,      // street & parking lamps, porch (automatic at night)
    LG_MACHINES,     // vending / coffee / aquarium glow
    LG_COUNT
};

enum LightKind { LIGHT_POINT, LIGHT_SPOT };

struct LightDef {
    LightKind kind = LIGHT_POINT;
    Vec3 pos, dir = Vec3(0, -1, 0);
    Vec3 color = Vec3(1.0f);
    float intensity = 1.0f;
    float range = 8.0f;
    float innerDeg = 25.0f, outerDeg = 40.0f;
    int group = LG_LOUNGE;
};

class LightManager {
public:
    static LightManager& get() { static LightManager m; return m; }

    bool groupOn[LG_COUNT] = {true, true, true, true, true, true};
    bool outdoorAuto = true;   // outdoor lamps follow the day/night cycle

    void clearStatic() { m_static.clear(); }
    void addStatic(const LightDef& l) { m_static.push_back(l); }
    void addFrame(const LightDef& l) { m_frame.push_back(l); }

    // Is a light group currently producing light? (used by lamp models to glow)
    bool isOn(int group) const { return group >= 0 && group < LG_COUNT && m_effective[group]; }
    void updateEffective(bool nightLamps);

    // Choose the closest lights to the camera and copy them into the frame
    void collect(FrameInfo& f, const Vec3& cam);
    // building box + partition walls -> light regions (0..2 rooms, 3 outdoors)
    Vec4 indoorBox = Vec4(1e6f, 1e6f, -1e6f, -1e6f);
    float splitX0 = -7.0f, splitX1 = 7.0f;
    int regionOf(const Vec3& p) const;
    void endFrame() { m_frame.clear(); }

    size_t staticCount() const { return m_static.size(); }

private:
    std::vector<LightDef> m_static, m_frame;
    bool m_effective[LG_COUNT] = {true, true, true, true, true, true};
};
