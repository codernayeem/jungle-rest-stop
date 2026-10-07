#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "App.h"
#include "../game/Audio.h"
#include "../game/Interaction.h"
#include "../game/Inventory.h"
#include "../game/Messages.h"
#include "../gfx/Screenshot.h"
#include "../gfx/Profiler.h"
#include "../scene/Layout.h"
#include "../scene/Lights.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <thread>

namespace {
App* g_app = nullptr;
void cbKey(GLFWwindow*, int key, int, int action, int mods) { g_app->onKey(key, action, mods); }
void cbMouse(GLFWwindow*, double x, double y) { g_app->onMouseMove(x, y); }
void cbButton(GLFWwindow*, int b, int a, int) { g_app->onMouseButton(b, a); }
void cbScroll(GLFWwindow*, double, double dy) { g_app->onScroll(dy); }
float terrainMeshHeight(float x, float z) { return terrainHeight(x, z) - 0.03f; }
}  // namespace

// =============================================================================
bool App::init(const AppOptions& opt) {
    m_opt = opt;
    g_app = this;
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return false; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, opt.lowQuality ? 0 : 4);
    if (!opt.shotsDir.empty()) glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWmonitor* mon = opt.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    int w = opt.width, h = opt.height;
    if (mon) { const GLFWvidmode* vm = glfwGetVideoMode(mon); w = vm->width; h = vm->height; }
    m_win = glfwCreateWindow(w, h, "Jungle Rest Stop - OpenGL 3.3 | Md. Nayeem (2107050)", mon, nullptr);
    if (!m_win) { std::cerr << "Window creation failed\n"; glfwTerminate(); return false; }
    glfwMakeContextCurrent(m_win);
    glfwSwapInterval(0);   // we cap the frame rate ourselves
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) { std::cerr << "GLAD failed\n"; return false; }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "  |  " << glGetString(GL_RENDERER) << std::endl;

    glfwSetKeyCallback(m_win, cbKey);
    glfwSetCursorPosCallback(m_win, cbMouse);
    glfwSetMouseButtonCallback(m_win, cbButton);
    glfwSetScrollCallback(m_win, cbScroll);
    setCursorFree(!opt.shotsDir.empty());

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    if (!opt.lowQuality) glEnable(GL_MULTISAMPLE);

    auto t0 = std::chrono::high_resolution_clock::now();
    if (!m_renderer.init(terrainMeshHeight)) std::cerr << "Renderer shader problems (see above)\n";
    if (opt.lowQuality) { m_renderer.settings.shadowRes = 1024; m_renderer.settings.shadowRadius = 28.0f; }
    m_text.init();
    if (opt.shotsDir.empty()) std::cout << (Audio::init() ? "Sound on" : "No sound device - running silent") << std::endl;   // screenshots are taken silently
    buildWorld();
    m_buildSeconds = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
    std::cout << "World built in " << m_buildSeconds << " s: " << m_renderer.staticInstanceCount() << " static shape instances in "
              << m_renderer.statChunksTotal << " chunks, " << LightManager::get().staticCount() << " lights, " << m_col.staticCount()
              << " colliders" << std::endl;
    timeBeginPeriod(1);
    return true;
}

void App::buildWorld() {
    using namespace Layout;
    m_col.clearStatic();
    DrawSink& s = m_renderer.staticSink();
    // terrain (one displaced grid)
    Material ground = Material(Vec3(0.42f, 0.60f, 0.26f), 0.03f, 4.0f).tx(TEX_GRASS, 0.35f).fl(MF_TERRAIN | MF_NO_SHADOW);
    s.add(M_TERRAIN, Mat4(), ground);

    m_building.buildStatic(s, m_col);
    m_jungle = Nature::buildJungle(s, m_col, m_opt.lowQuality);
    Nature::buildGarden(s, m_col);

    // interactive objects
    for (int i = 0; i < 3; ++i) {
        m_vend[i].init(i, (VendKind)i, VENDING_POS[i], 0.0f);
        m_vend[i].drawStatic(s);
        m_col.addOriented(VENDING_POS[i], 0.5f, 0.42f, 0.0f, 0.0f, 2.0f);
    }
    m_atm.init(ATM_POS, 0.0f);
    m_atm.drawStatic(s);
    m_col.addOriented(ATM_POS + Vec3(0, 0, 0.05f), 0.52f, 0.42f, 0.0f, 0.0f, 2.1f);
    m_coffee.init(COFFEE_POS, 0.0f);
    m_coffee.drawStatic(s);
    m_tv.init(TV_POS, 0.0f);
    m_tv.drawStatic(s);
    m_aquarium.init(AQUARIUM_POS, 180.0f);
    m_aquarium.drawStatic(s);
    m_col.addOriented(AQUARIUM_POS, 0.9f, 0.26f, 180.0f, 0.0f, 1.5f);
    m_renderer.finalizeStatic();

    // lights
    m_building.registerLights();
    for (auto& v : m_vend) v.registerLights();
    m_atm.registerLights();
    m_aquarium.registerLights();

    WorldObjects wo;
    for (int i = 0; i < 3; ++i) wo.vending[i] = &m_vend[i];
    wo.atm = &m_atm;
    wo.coffee = &m_coffee;
    wo.aquarium = &m_aquarium;
    m_visitors.init(wo, m_opt.lowQuality);
    m_wildlife.init();
    m_player.init();
    LightManager::get().indoorBox = Vec4(BLD_X0, BLD_Z0, BLD_X1, BLD_Z1);
}

void App::shutdown() {
    Audio::shutdown();
    timeEndPeriod(1);
    m_text.shutdown();
    m_renderer.shutdown();
    if (m_win) glfwDestroyWindow(m_win);
    glfwTerminate();
}

// =============================================================================
//  Input
// =============================================================================
void App::setCursorFree(bool free) {
    m_cursorFree = free;
    glfwSetInputMode(m_win, GLFW_CURSOR, free ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    m_firstMouse = true;
}

void App::interact() {
    if (m_player.mode == CAM_FREE) return;
    InteractionSystem::get().useHovered();
}

void App::onKey(int key, int action, int mods) {
    (void)mods;
    if (action != GLFW_PRESS) return;
    RenderSettings& rs = m_renderer.settings;
    LightManager& lm = LightManager::get();
    switch (key) {
        case GLFW_KEY_ESCAPE:
            if (m_cursorFree && m_opt.shotsDir.empty()) setCursorFree(false);
            else glfwSetWindowShouldClose(m_win, 1);
            break;
        case GLFW_KEY_LEFT_ALT:
        case GLFW_KEY_RIGHT_ALT: setCursorFree(!m_cursorFree); break;
        case GLFW_KEY_E: interact(); break;
        case GLFW_KEY_V: m_player.cycleMode(); notify(std::string("Camera: ") + m_player.modeName()); break;
        case GLFW_KEY_B:
            rs.enhanced = !rs.enhanced;
            notify(rs.enhanced ? "ENHANCED rendering (textures, shadows, fog, sky)" : "BASIC rendering (plain colours + lighting)");
            break;
        case GLFW_KEY_T: rs.textures = !rs.textures; notify(rs.textures ? "Textures ON" : "Textures OFF"); break;
        case GLFW_KEY_G: rs.shadows = !rs.shadows; notify(rs.shadows ? "Shadows ON" : "Shadows OFF"); break;
        case GLFW_KEY_F: rs.fog = !rs.fog; notify(rs.fog ? "Fog ON" : "Fog OFF"); break;
        case GLFW_KEY_N: m_day.toggleDayNight(); notify(std::string("Time: ") + m_day.clockText()); break;
        case GLFW_KEY_LEFT_BRACKET: m_day.slower(); notify("Time slower"); break;
        case GLFW_KEY_RIGHT_BRACKET: m_day.faster(); notify("Time faster"); break;
        case GLFW_KEY_M: m_day.paused = !m_day.paused; notify(m_day.paused ? "Time paused" : "Time running"); break;
        case GLFW_KEY_O: Audio::toggleMute(); notify(Audio::muted() ? "Sound OFF" : "Sound ON"); break;
        case GLFW_KEY_R: m_cutaway = !m_cutaway; notify(m_cutaway ? "Roof hidden (cut-away)" : "Roof visible"); break;
        case GLFW_KEY_L: m_fpsCap = m_fpsCap == 60 ? 30 : 60; notify("FPS cap " + std::to_string(m_fpsCap)); break;
        case GLFW_KEY_H: m_showHelp = !m_showHelp; break;
        case GLFW_KEY_K: m_visitors.forceSpawn(); notify("A visitor car is on its way"); break;
        case GLFW_KEY_F1: lm.groupOn[LG_LOUNGE] = !lm.groupOn[LG_LOUNGE]; notify(lm.groupOn[LG_LOUNGE] ? "Lounge lights ON" : "Lounge lights OFF"); break;
        case GLFW_KEY_F2: lm.groupOn[LG_SPOTS] = !lm.groupOn[LG_SPOTS]; notify(lm.groupOn[LG_SPOTS] ? "Spot lights ON" : "Spot lights OFF"); break;
        case GLFW_KEY_F3:
            lm.groupOn[LG_ATM] = lm.groupOn[LG_LIBRARY] = !lm.groupOn[LG_ATM];
            notify(lm.groupOn[LG_ATM] ? "ATM + library lights ON" : "ATM + library lights OFF");
            break;
        case GLFW_KEY_F4:
            lm.groupOn[LG_OUTDOOR] = !lm.groupOn[LG_OUTDOOR];
            notify(lm.groupOn[LG_OUTDOOR] ? "Outdoor lamps enabled (auto at night)" : "Outdoor lamps OFF");
            break;
        case GLFW_KEY_F5: m_hudVisible = !m_hudVisible; break;
        case GLFW_KEY_F12: {
            CreateDirectoryA("screenshots", nullptr);
            char name[64];
            std::snprintf(name, sizeof(name), "screenshots/shot_%03d.png", m_shotCounter++);
            takeScreenshot(name);
            notify(std::string("Saved ") + name);
            break;
        }
        case GLFW_KEY_EQUAL:
        case GLFW_KEY_KP_ADD: m_player.changeFov(-5.0f); break;
        case GLFW_KEY_MINUS:
        case GLFW_KEY_KP_SUBTRACT: m_player.changeFov(5.0f); break;
        default: break;
    }
}

void App::onMouseMove(double x, double y) {
    if (m_firstMouse) { m_lastMouseX = x; m_lastMouseY = y; m_firstMouse = false; }
    double dx = x - m_lastMouseX, dy = y - m_lastMouseY;
    m_lastMouseX = x;
    m_lastMouseY = y;
    if (!m_cursorFree) m_player.mouseLook((float)dx, (float)dy);
}

void App::onMouseButton(int button, int action) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    if (m_cursorFree) {
        double x, y;
        glfwGetCursorPos(m_win, &x, &y);
        int ww, wh, fw, fh;
        glfwGetWindowSize(m_win, &ww, &wh);
        glfwGetFramebufferSize(m_win, &fw, &fh);
        float sx = ww > 0 ? (float)fw / ww : 1.0f, sy = wh > 0 ? (float)fh / wh : 1.0f;
        if (!m_hud.click((float)x * sx, (float)y * sy)) setCursorFree(false);
    } else {
        interact();
    }
}

void App::onScroll(double dy) { m_player.changeFov((float)dy * 4.0f); }

// =============================================================================
//  Update
// =============================================================================
void App::update(float dt) {
    m_time += dt;
    m_day.update(dt);
    Messages::get().update(dt);
    m_col.clearDynamic();

    // visitors first (they add car / people colliders)
    m_visitors.update(dt, m_time, m_player.pos, m_day.night(), m_col);
    m_player.update(m_win, dt, m_col, !m_cursorFree);
    m_player.updateCamera(m_col);

    // machines know when the player stands in front of them (NPCs then wait)
    for (auto& v : m_vend) v.setPlayerNear(distXZ(m_player.pos, v.standPos()) < 1.2f && m_player.mode != CAM_FREE);
    m_atm.setPlayerNear(distXZ(m_player.pos, m_atm.standPos()) < 1.3f && m_player.mode != CAM_FREE);
    m_coffee.setPlayerNear(distXZ(m_player.pos, m_coffee.standPos()) < 1.2f && m_player.mode != CAM_FREE);
    for (auto& v : m_vend) v.update(dt);
    m_atm.update(dt);
    m_coffee.update(dt);
    m_tv.update(dt);
    m_aquarium.update(dt);
    m_wildlife.update(dt, m_day.night(), m_player.cameraPos());

    // sound: the listener is the camera; birds by day, crickets at night, quieter indoors
    Vec3 ear = m_player.cameraPos();
    Audio::setListener(ear);
    bool indoor = ear.x > Layout::BLD_X0 && ear.x < Layout::BLD_X1 && ear.z > Layout::BLD_Z0 && ear.z < Layout::BLD_Z1 && ear.y < Layout::ROOF_Y;
    float amb = indoor ? 0.04f : 0.15f;   // background is only a faint bed under the other sounds
    Audio::setAmbience((1.0f - m_day.night()) * amb, m_day.night() * amb);

    std::vector<Vec3> people;
    people.push_back(m_player.pos);
    m_visitors.peoplePositions(people);
    m_building.update(dt, people);

    // interactive parts register themselves, then the picking ray is cast
    auto& is = InteractionSystem::get();
    is.beginFrame();
    m_building.registerInteractions();
    for (auto& v : m_vend) v.registerInteractions();
    m_atm.registerInteractions();
    m_coffee.registerInteractions();
    m_tv.registerInteractions();
    m_aquarium.registerInteractions();
    bool canUse = m_player.mode != CAM_FREE;
    float reach = m_player.mode == CAM_THIRD ? 2.8f : 2.4f;
    is.update(m_player.interactOrigin(), m_player.lookDir(), reach, canUse);
}

// =============================================================================
//  Render
// =============================================================================
void App::buildFrameInfo(FrameInfo& f, int w, int h) {
    using namespace Layout;
    float aspect = h > 0 ? (float)w / h : 1.0f;
    f.proj = perspective(toRadians(m_player.fov), aspect, 0.1f, 400.0f);
    f.view = m_player.viewMatrix();
    f.camPos = m_player.cameraPos();
    f.time = m_time;
    m_day.fillEnvironment(f.env);
    f.farDist = 165.0f;
    // thinner haze when the camera is high up (aerial views)
    f.env.fogDensity *= lerpf(1.0f, 0.35f, clampf(f.camPos.y / 45.0f, 0.0f, 1.0f));
    f.shadowFocus = m_player.mode == CAM_FREE ? m_player.cameraPos() + m_player.lookDir() * 25.0f : m_player.pos;
    f.indoorBox = Vec4(BLD_X0, BLD_Z0, BLD_X1, BLD_Z1);
    f.indoorRoofY = m_cutaway ? -100.0f : ROOF_Y - 0.05f;
}

void App::drawDynamicScene(const Vec3& cam) {
    DrawSink& dyn = m_renderer.dynamicSink();
    m_building.drawDynamic(dyn, m_time, m_day.hour, m_cutaway, cam);
    for (auto& v : m_vend) v.drawDynamic(dyn, cam);
    m_atm.drawDynamic(dyn);
    m_coffee.drawDynamic(dyn);
    m_tv.drawDynamic(dyn);
    m_aquarium.drawDynamic(dyn);
    m_visitors.draw(dyn, cam, m_time);
    m_player.draw(dyn, m_time);
    m_wildlife.draw(dyn, m_day.night(), m_time);
}

void App::drawWorldText(const Mat4& vp, const Vec3& cam) {
    m_text.begin3D(vp);
    m_building.drawText(m_text, m_time, LightManager::get().isOn(LG_OUTDOOR));
    for (auto& v : m_vend)
        if (distXZ(cam, v.standPos()) < 18.0f) v.drawScreen(m_text);
    if (distXZ(cam, m_atm.standPos()) < 15.0f) m_atm.drawScreen(m_text);
    if (distXZ(cam, m_coffee.standPos()) < 15.0f) m_coffee.drawScreen(m_text);
    if (distXZ(cam, Layout::TV_POS) < 22.0f) m_tv.drawScreen(m_text, m_day.hour, m_day.clockText());
    m_text.end3D();
}

void App::render(int w, int h, bool hud) {
    Profiler::get().beginFrame();
    FrameInfo f;
    buildFrameInfo(f, w, h);
    if (!std::getenv("JRS_NO_DYN")) drawDynamicScene(f.camPos);
    Profiler::get().mark("cpu_collect_shapes");

    LightManager& lm = LightManager::get();
    lm.updateEffective(m_day.lampsOn());
    m_tv.addFrameLight();
    m_visitors.addLights(m_day.night());
    lm.collect(f, f.camPos);
    Profiler::get().mark("cpu_lights");

    m_renderer.beginFrame(f, w, h);
    if (!std::getenv("JRS_NO_TEXT")) drawWorldText(m_renderer.viewProj(), f.camPos);
    Profiler::get().mark("screen_text");
    if (!std::getenv("JRS_NO_TRANS")) m_renderer.drawTransparent();
    if (hud && m_hudVisible) {
        HudData d;
        fillHud(d);
        m_hud.draw(m_text, w, h, d);
    }
    Profiler::get().mark("hud");
    m_renderer.endFrame();
    lm.endFrame();
    Profiler::get().endFrame();
}

void App::fillHud(HudData& d) {
    RenderSettings& rs = m_renderer.settings;
    d.mode = m_player.modeName();
    d.clock = m_day.clockText();
    d.phase = m_day.phaseName();
    if (rs.enhanced) {
        d.renderMode = std::string("ENHANCED") + (rs.textures ? " +tex" : "") + (rs.shadows ? " +shadow" : "") + (rs.fog ? " +fog" : "");
    } else {
        d.renderMode = "BASIC (flat colours)";
    }
    d.enhanced = rs.enhanced;
    d.prompt = InteractionSystem::get().prompt();
    d.fps = m_fps;
    d.fpsCapText = std::to_string(m_fpsCap);
    d.drawCalls = m_renderer.statDrawCalls;
    d.instances = m_renderer.statInstances;
    d.triangles = m_renderer.statTriangles;
    d.chunks = m_renderer.statChunksVisible;
    d.chunksTotal = m_renderer.statChunksTotal;
    d.visitors = m_visitors.visitorsActive();
    d.parked = m_visitors.carsParked();
    d.fov = m_player.fov;
    Inventory& inv = Inventory::player();
    d.cash = inv.cash;
    d.bank = inv.bank;
    d.items = inv.items;
    d.atmStep = m_atm.currentStep();
    d.showAtmSteps = m_atm.playerSession() || (distXZ(m_player.pos, m_atm.standPos()) < 3.0f && !m_atm.inUseByNpc() && m_player.mode != CAM_FREE);
    d.cursorFree = m_cursorFree;
    d.showHelp = m_showHelp;
    d.crosshair = m_player.mode != CAM_FREE;

    // clickable buttons
    m_hud.buttons.clear();
    m_hud.buttons.push_back({0, 0, 0, 0, rs.enhanced ? "Mode: ENHANCED" : "Mode: BASIC", rs.enhanced, [this]() { onKey(GLFW_KEY_B, GLFW_PRESS, 0); }});
    m_hud.buttons.push_back({0, 0, 0, 0, std::string("Textures ") + (rs.textures ? "ON" : "OFF"), rs.textures, [this]() { onKey(GLFW_KEY_T, GLFW_PRESS, 0); }});
    m_hud.buttons.push_back({0, 0, 0, 0, std::string("Shadows ") + (rs.shadows ? "ON" : "OFF"), rs.shadows, [this]() { onKey(GLFW_KEY_G, GLFW_PRESS, 0); }});
    m_hud.buttons.push_back({0, 0, 0, 0, m_day.night() > 0.5f ? "Switch to DAY" : "Switch to NIGHT", m_day.night() > 0.5f, [this]() { onKey(GLFW_KEY_N, GLFW_PRESS, 0); }});
    m_hud.buttons.push_back({0, 0, 0, 0, std::string("Camera: ") + m_player.modeName(), true, [this]() { onKey(GLFW_KEY_V, GLFW_PRESS, 0); }});
    m_hud.buttons.push_back({0, 0, 0, 0, "FPS cap: " + std::to_string(m_fpsCap), m_fpsCap == 60, [this]() { onKey(GLFW_KEY_L, GLFW_PRESS, 0); }});
}

void App::takeScreenshot(const std::string& path) {
    int w, h;
    glfwGetFramebufferSize(m_win, &w, &h);
    saveFramebufferPNG(path, w, h);
}

// =============================================================================
//  Main loop with frame cap
// =============================================================================
void App::run() {
    if (!m_opt.shotsDir.empty()) { runShots(m_opt.shotsDir); return; }
    notify("Welcome! Press H for help, Alt to use the on-screen buttons", 6.0f);
    double last = glfwGetTime(), fpsTimer = 0;
    int frames = 0;
    while (!glfwWindowShouldClose(m_win)) {
        double start = glfwGetTime();
        float dt = (float)std::min(0.1, start - last);
        last = start;
        glfwPollEvents();
        update(dt);
        int w, h;
        glfwGetFramebufferSize(m_win, &w, &h);
        if (w > 0 && h > 0) render(w, h, true);
        glfwSwapBuffers(m_win);

        frames++;
        fpsTimer += dt;
        if (fpsTimer >= 0.5) { m_fps = (int)(frames / fpsTimer + 0.5); frames = 0; fpsTimer = 0; }

        // frame cap: sleep most of the remaining time, then spin briefly
        double target = 1.0 / m_fpsCap;
        double elapsed = glfwGetTime() - start;
        if (elapsed < target) {
            double sleepFor = target - elapsed - 0.0015;
            if (sleepFor > 0) std::this_thread::sleep_for(std::chrono::duration<double>(sleepFor));
            while (glfwGetTime() - start < target) {}
        }
    }
}
