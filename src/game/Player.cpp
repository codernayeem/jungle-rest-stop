#include "Player.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include "../scene/Layout.h"

void Player::init() {
    look.skin = Vec3(0.72f, 0.52f, 0.38f);
    look.hair = Vec3(0.05f, 0.04f, 0.03f);
    look.shirt = Vec3(0.12f, 0.45f, 0.42f);
    look.pants = Vec3(0.12f, 0.14f, 0.22f);
    look.shoes = Vec3(0.9f, 0.9f, 0.9f);
    look.hairStyle = 0;
    look.glasses = true;
    look.height = 1.0f;
}

const char* Player::modeName() const {
    switch (mode) {
        case CAM_FIRST: return "First person";
        case CAM_THIRD: return "Third person";
        default: return "Free camera";
    }
}

void Player::cycleMode() {
    mode = (CameraMode)((mode + 1) % CAM_COUNT);
    if (mode == CAM_FREE) {
        // start the free camera where the character's eyes are, a bit higher
        freePos = eye() + Vec3(0, 1.5f, 0) - lookDir() * 2.0f;
        freeYaw = yaw;
        freePitch = pitch - 10.0f;
    }
}

Vec3 Player::eye() const { return pos + Vec3(0, eyeHeight + m_bob, 0); }

Vec3 Player::lookDir() const {
    float y = toRadians(mode == CAM_FREE ? freeYaw : yaw), p = toRadians(mode == CAM_FREE ? freePitch : pitch);
    return Vec3(std::sin(y) * std::cos(p), std::sin(p), std::cos(y) * std::cos(p));
}

void Player::mouseLook(float dx, float dy) {
    const float sens = 0.12f;
    if (mode == CAM_FREE) {
        freeYaw -= dx * sens;
        freePitch = clampf(freePitch - dy * sens, -89.0f, 89.0f);
    } else {
        yaw -= dx * sens;
        pitch = clampf(pitch - dy * sens, -85.0f, 85.0f);
    }
}

void Player::update(GLFWwindow* w, float dt, const CollisionWorld& col, bool input) {
    auto key = [&](int k) { return input && glfwGetKey(w, k) == GLFW_PRESS; };
    float fwd = (key(GLFW_KEY_W) ? 1.0f : 0.0f) - (key(GLFW_KEY_S) ? 1.0f : 0.0f);
    float side = (key(GLFW_KEY_D) ? 1.0f : 0.0f) - (key(GLFW_KEY_A) ? 1.0f : 0.0f);
    bool run = key(GLFW_KEY_LEFT_SHIFT) || key(GLFW_KEY_RIGHT_SHIFT);

    if (mode == CAM_FREE) {
        // fly in the look direction
        Vec3 f = lookDir();
        float yr = toRadians(freeYaw);
        Vec3 right(-std::cos(yr), 0, std::sin(yr));
        float up = (key(GLFW_KEY_SPACE) || key(GLFW_KEY_E) ? 1.0f : 0.0f) - (key(GLFW_KEY_LEFT_CONTROL) || key(GLFW_KEY_Q) ? 1.0f : 0.0f);
        float sp = run ? 22.0f : 7.0f;
        freePos += (f * fwd + right * side + Vec3(0, up, 0)) * (sp * dt);
        freePos.y = std::max(freePos.y, terrainHeight(freePos.x, freePos.z) + 0.3f);
        m_walking = false;
        m_camPos = freePos;
        return;
    }

    float yr = toRadians(yaw);
    Vec3 forward(std::sin(yr), 0, std::cos(yr));
    Vec3 right(-std::cos(yr), 0, std::sin(yr));
    Vec3 move = forward * fwd + right * side;
    m_walking = move.length() > 0.01f;
    m_running = run;
    if (m_walking) {
        move = normalize(move);
        float sp = run ? runSpeed : walkSpeed;
        Vec3 np = pos + move * (sp * dt);
        np = col.resolve(np, 0.3f, pos.y, pos.y + 1.8f);
        // keep inside the world
        np.x = clampf(np.x, -Layout::WORLD_HALF + 3.0f, Layout::WORLD_HALF - 3.0f);
        np.z = clampf(np.z, -Layout::WORLD_HALF + 3.0f, Layout::WORLD_HALF - 3.0f);
        pos.x = np.x;
        pos.z = np.z;
        m_walkPhase += dt * (run ? 10.0f : 7.0f);
    } else {
        Vec3 np = col.resolve(pos, 0.3f, pos.y, pos.y + 1.8f);   // pushed by cars / people
        pos.x = np.x;
        pos.z = np.z;
    }
    // gravity + jumping, the ground follows the terrain
    float ground = std::max(0.0f, terrainHeight(pos.x, pos.z));
    if (key(GLFW_KEY_SPACE) && m_grounded) { m_velY = 4.6f; m_grounded = false; }
    m_velY -= 12.0f * dt;
    pos.y += m_velY * dt;
    if (pos.y <= ground) { pos.y = ground; m_velY = 0; m_grounded = true; }
    // head bob
    float targetBob = (m_walking && m_grounded && mode == CAM_FIRST) ? std::sin(m_walkPhase * 2.0f) * (run ? 0.045f : 0.028f) : 0.0f;
    m_bob = lerpf(m_bob, targetBob, std::min(1.0f, dt * 12.0f));
}

void Player::updateCamera(const CollisionWorld& col) {
    if (mode == CAM_FIRST) {
        m_camPos = eye();
        m_camTarget = m_camPos + lookDir();
    } else if (mode == CAM_THIRD) {
        Vec3 head = pos + Vec3(0, 1.55f, 0);
        Vec3 d = lookDir();
        Vec3 back = -d;
        float want = 3.4f;
        float hit = col.raycast(head, back, want);
        float dist = std::max(0.6f, hit - 0.25f);
        m_camPos = head + back * dist + Vec3(0, 0.35f, 0);
        m_camTarget = m_camPos + d;
    } else {
        m_camPos = freePos;
        m_camTarget = freePos + lookDir();
    }
}

Mat4 Player::viewMatrix() const { return lookAt(m_camPos, m_camTarget, Vec3(0, 1, 0)); }

Vec3 Player::interactOrigin() const { return mode == CAM_THIRD ? eye() : m_camPos; }

void Player::draw(DrawSink& dyn, float time) const {
    if (mode == CAM_FIRST) return;
    HumanPose p = m_walking ? Human::walk(m_walkPhase, m_running ? 1.3f : 1.0f) : Human::idle(time);
    if (!m_grounded) { p.kneeL = p.kneeR = 35.0f; p.thighL = p.thighR = -20.0f; p.shoulderL = p.shoulderR = -40.0f; }
    Human::draw(Node(dyn, rotateYXZ(translate(Mat4(), pos), {0, yaw, 0})), look, p, HELD_NONE);
}
