// =============================================================================
//  Appliances.h  -  Coffee machine, wall TV and aquarium
// =============================================================================
#pragma once
#include <string>
#include <vector>
#include "../gfx/Draw.h"
#include "../gfx/Text.h"

// -----------------------------------------------------------------------------
// Coffee machine: choose Espresso / Cappuccino / Tea -> cup drops, grinder runs,
// coffee stream fills the cup, steam rises, take the cup.
// -----------------------------------------------------------------------------
class CoffeeMachine {
    friend class App;   // the report showcase drives the machines directly
public:
    void init(const Vec3& pos, float yawDeg);
    void update(float dt);
    void drawStatic(DrawSink& s) const;
    void drawDynamic(DrawSink& dyn) const;
    void drawScreen(TextRenderer& t) const;
    void registerInteractions();

    bool available() const { return m_state == IDLE && !m_playerNear; }
    bool npcBrew(int drink);
    bool cupReady() const { return m_state == READY; }
    void npcTakeCup();
    Vec3 standPos() const;
    float standYaw() const { return m_yaw + 180.0f; }
    void setPlayerNear(bool b) { m_playerNear = b; }
    int cupsServed = 0;

private:
    enum State { IDLE, CUP_DROP, GRINDING, BREWING, READY };
    void start(int drink, bool npc);
    void take(bool player);
    Vec3 m_pos;
    float m_yaw = 0;
    Mat4 m_root;
    State m_state = IDLE;
    float m_t = 0, m_time = 0;
    int m_drink = 0;
    bool m_npc = false, m_playerNear = false;
    float m_btnAnim[3] = {};
    int m_idBase = 0;
    struct Puff { Vec3 p; float life; };
    std::vector<Puff> m_steam;
};

// -----------------------------------------------------------------------------
// Wall-mounted TV with animated channels (news, nature, sports, weather)
// -----------------------------------------------------------------------------
class Television {
    friend class App;   // the report showcase drives the machines directly
public:
    void init(const Vec3& pos, float yawDeg);
    void update(float dt);
    void drawStatic(DrawSink& s) const;
    void drawDynamic(DrawSink& dyn) const;
    void drawScreen(TextRenderer& t, float hour, const std::string& clock) const;
    void registerInteractions();
    void addFrameLight() const;     // flickering coloured light on the room
    int channel = 1;                // 0 = off
    static const int CHANNELS = 5;
private:
    Vec3 m_pos;
    float m_yaw = 0;
    Mat4 m_root;
    float m_time = 0, m_switchFx = 0;
    int m_idBase = 0;
};

// -----------------------------------------------------------------------------
// Aquarium: wooden stand, glass tank, water, sand, rocks, plants, swimming fish,
// bubbles and a hood light. "Feed the fish" makes them rise to the food.
// -----------------------------------------------------------------------------
class Aquarium {
    friend class App;   // the report showcase drives the machines directly
public:
    void init(const Vec3& pos, float yawDeg);
    void update(float dt);
    void drawStatic(DrawSink& s) const;
    void drawDynamic(DrawSink& dyn) const;
    void registerInteractions();
    void registerLights() const;
    Vec3 viewPos() const;
private:
    struct Fish { float phase, speed, radiusX, radiusZ, y, size; Vec3 color, fin; float x, z, yaw, cy; };
    struct Bubble { Vec3 p; float speed; };
    struct Food { Vec3 p; float life; };
    Vec3 m_pos;
    float m_yaw = 0;
    Mat4 m_root;
    float m_time = 0;
    std::vector<Fish> m_fish;
    std::vector<Bubble> m_bubbles;
    std::vector<Food> m_food;
    int m_idBase = 0;
};
