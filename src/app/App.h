// =============================================================================
//  App.h  -  Owns every module and runs the main loop.
//    update  -> input, player, day/night, machines, visitors, doors
//    render  -> shadow map, sky, opaque world, screens text, transparent, HUD
//  Frame rate is capped (60 or 30 FPS) with a precise sleep, because the
//  target PC has only an integrated GPU.
// =============================================================================
#pragma once
#include <string>
#include <vector>
#include "../gfx/Renderer.h"
#include "../gfx/Text.h"
#include "../game/Collision.h"
#include "../game/DayNight.h"
#include "../game/Hud.h"
#include "../game/Player.h"
#include "../scene/Building.h"
#include "../scene/Nature.h"
#include "../objects/VendingMachine.h"
#include "../objects/ATM.h"
#include "../objects/Appliances.h"
#include "../actors/Visitors.h"

struct GLFWwindow;

struct AppOptions {
    bool lowQuality = false;
    std::string shotsDir;      // non-empty -> render report screenshots and exit
    std::string videoPath;     // non-empty -> render the cinematic video (mp4) and exit
    int width = 1280, height = 720;
    bool fullscreen = false;
};

class App {
public:
    bool init(const AppOptions& opt);
    void run();
    void shutdown();

    // GLFW callbacks forward here
    void onKey(int key, int action, int mods);
    void onMouseMove(double x, double y);
    void onMouseButton(int button, int action);
    void onScroll(double dy);

private:
    void buildWorld();
    void update(float dt);
    void render(int w, int h, bool hud);
    void buildFrameInfo(FrameInfo& f, int w, int h);
    void drawDynamicScene(const Vec3& cam);
    void drawWorldText(const Mat4& viewProj, const Vec3& cam);
    void setCursorFree(bool free);
    void interact();
    void fillHud(HudData& d);
    void takeScreenshot(const std::string& path);

    // report figures (Showcase.cpp)
    void runShots(const std::string& dir);
    void runCinematic(const std::string& mp4Path);
    void renderStudio(int w, int h, const Vec3& camPos, const Vec3& target, float fov, int partLimit, int object, float time,
                      float white = -1.0f);   // >= 0: white background, no floor, camera fitted to the object
    Mat4 renderPartList(int w, int h, const Vec3& viewDir, const std::vector<PartCollector::P>& parts, bool wire, float fov);
    int studioPartCount(int object);
    void studioObject(DrawSink& s, int object, float time);

    AppOptions m_opt;
    GLFWwindow* m_win = nullptr;
    Renderer m_renderer;
    TextRenderer m_text;
    CollisionWorld m_col;
    Building m_building;
    VendingMachine m_vend[3];
    ATM m_atm;
    CoffeeMachine m_coffee;
    Television m_tv;
    Aquarium m_aquarium;
    VisitorSystem m_visitors;
    Nature::Wildlife m_wildlife;
    Nature::JungleStats m_jungle;
    Player m_player;
    DayNight m_day;
    Hud m_hud;

    float m_time = 0.0f;
    int m_fpsCap = 60;
    bool m_cutaway = false;
    bool m_showHelp = true;
    bool m_cursorFree = false;
    bool m_hudVisible = true;
    bool m_hudCompact = false;   // video: small HUD
    double m_lastMouseX = 0, m_lastMouseY = 0;
    bool m_firstMouse = true;
    int m_fps = 0;
    int m_shotCounter = 0;
    double m_buildSeconds = 0;
};
