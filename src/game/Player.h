// =============================================================================
//  Player.h  -  The controllable person + the three camera modes
//    First person : camera at the eyes, mouse look, head bob
//    Third person : camera behind the visible character (pulled in by walls)
//    Free camera  : fly anywhere (no collision) to inspect the scene
//  Mouse wheel / +,- change the field of view (FOV).
// =============================================================================
#pragma once
#include "../actors/Human.h"
#include "Collision.h"

struct GLFWwindow;

enum CameraMode { CAM_FIRST = 0, CAM_THIRD, CAM_FREE, CAM_COUNT };

class Player {
public:
    Vec3 pos = Vec3(0.0f, 0.0f, 9.8f);   // feet position
    float yaw = 180.0f, pitch = -4.0f;   // degrees, yaw 0 = facing +Z
    float fov = 70.0f;
    CameraMode mode = CAM_FIRST;
    float eyeHeight = 1.62f;
    float walkSpeed = 2.4f, runSpeed = 5.0f;
    HumanLook look;

    // free camera
    Vec3 freePos = Vec3(-30.0f, 22.0f, 40.0f);
    float freeYaw = 145.0f, freePitch = -28.0f;

    void init();
    void update(GLFWwindow* w, float dt, const CollisionWorld& col, bool inputEnabled);
    void mouseLook(float dx, float dy);
    void changeFov(float delta) { fov = clampf(fov - delta, 25.0f, 110.0f); }
    void cycleMode();
    const char* modeName() const;

    Vec3 eye() const;                 // character eye position
    Vec3 lookDir() const;             // direction the camera looks
    Vec3 cameraPos() const { return m_camPos; }
    Mat4 viewMatrix() const;
    void updateCamera(const CollisionWorld& col);
    Vec3 interactOrigin() const;      // ray origin for "look at & use"
    void draw(DrawSink& dyn, float time) const;
    bool isWalking() const { return m_walking; }

private:
    float m_velY = 0.0f;
    bool m_grounded = true;
    float m_walkPhase = 0.0f, m_bob = 0.0f;
    bool m_walking = false, m_running = false;
    Vec3 m_camPos, m_camTarget;
};
