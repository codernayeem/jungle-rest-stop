// =============================================================================
//  DayNight.h  -  24 hour clock that drives the sun, moon, sky colours,
//  ambient light, fog and automatic street lights.
// =============================================================================
#pragma once
#include <string>
#include "../gfx/Renderer.h"

class DayNight {
public:
    float hour = 10.0f;            // 0..24
    float hoursPerSecond = 1.0f / 20.0f;  // 1 game hour = 20 real seconds
    bool paused = false;

    void update(float dt);
    void fillEnvironment(Environment& env) const;
    void toggleDayNight();         // jump to evening / morning
    void faster() { hoursPerSecond = std::min(hoursPerSecond * 2.0f, 0.8f); }
    void slower() { hoursPerSecond = std::max(hoursPerSecond * 0.5f, 1.0f / 160.0f); }

    float sunHeight() const;       // sin(elevation), <0 below horizon
    float night() const;           // 0 day .. 1 night
    bool lampsOn() const { return night() > 0.35f; }
    std::string clockText() const;
    const char* phaseName() const;
};
