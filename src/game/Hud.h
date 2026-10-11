// =============================================================================
//  Hud.h  -  2D overlay: status panel, crosshair, interaction prompt,
//  ATM step list, wallet / items, notifications, help screen and clickable
//  buttons (Basic/Enhanced, Day/Night, Camera, FPS cap ...).
// =============================================================================
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "../gfx/Text.h"

struct HudButton {
    float x, y, w, h;
    std::string label;
    bool active;
    std::function<void()> onClick;
};

struct HudData {
    std::string mode, clock, phase, renderMode, prompt, fpsCapText;
    int fps = 0, drawCalls = 0, instances = 0, chunks = 0, chunksTotal = 0, visitors = 0, parked = 0;
    long long triangles = 0;
    float fov = 70;
    int cash = 0, bank = 0;
    std::vector<std::string> items;
    bool showAtmSteps = false;
    int atmStep = 0;
    bool cursorFree = false;
    bool showHelp = false;
    bool crosshair = true;
    bool compact = false;     // video: only crosshair, prompt, wallet, ATM steps and messages
    bool textures = true, shadows = true, fog = true, enhanced = true;
};

class Hud {
public:
    void draw(TextRenderer& t, int w, int h, const HudData& d);
    std::vector<HudButton> buttons;
    bool click(float x, float y);   // returns true if a button was hit
};
