#ifndef CAMERA_H
#define CAMERA_H

#include "math3d.h"
#include <cmath>
#include <algorithm>

enum CameraViewPreset {
    PRESET_ROOM_OVERVIEW = 0,
    PRESET_MACHINE_FRONT = 1,
    PRESET_SHELVES_CLOSEUP = 2,
    PRESET_KEYPAD_PANEL = 3,
    PRESET_COLLECTION_DOOR = 4,
    PRESET_COUNT = 5
};

class Camera {
public:
    Vec3 target;
    float distance;
    float yaw;    // in degrees
    float pitch;  // in degrees
    float fov;

    bool autoRotate;
    float rotateSpeed; // deg/sec

    bool isDraggingLeft;
    bool isDraggingRight;
    double lastMouseX;
    double lastMouseY;

    int currentPresetIndex;

    Camera()
        : target(Vec3(0.0f, 2.0f, 0.0f)),
          distance(18.0f),
          yaw(-35.0f),
          pitch(28.0f),
          fov(50.0f),
          autoRotate(false),
          rotateSpeed(15.0f),
          isDraggingLeft(false),
          isDraggingRight(false),
          lastMouseX(0.0),
          lastMouseY(0.0),
          currentPresetIndex(0)
    {}

    Vec3 getPosition() const {
        float radYaw = toRadians(yaw);
        float radPitch = toRadians(pitch);
        float x = target.x + distance * std::cos(radPitch) * std::sin(radYaw);
        float y = target.y + distance * std::sin(radPitch);
        float z = target.z + distance * std::cos(radPitch) * std::cos(radYaw);
        return Vec3(x, y, z);
    }

    Mat4 getViewMatrix() const {
        return lookAt(getPosition(), target, Vec3(0.0f, 1.0f, 0.0f));
    }

    void update(float dt) {
        if (autoRotate) {
            yaw += rotateSpeed * dt;
            if (yaw > 360.0f) yaw -= 360.0f;
            if (yaw < -360.0f) yaw += 360.0f;
        }
    }

    void applyPreset(CameraViewPreset preset) {
        currentPresetIndex = preset;
        autoRotate = false;
        switch (preset) {
            case PRESET_ROOM_OVERVIEW:
                target = Vec3(0.0f, 2.0f, 0.0f);
                distance = 18.0f;
                yaw = -35.0f;
                pitch = 28.0f;
                fov = 50.0f;
                break;
            case PRESET_MACHINE_FRONT:
                target = Vec3(-0.05f, 1.25f, 0.0f);
                distance = 2.4f;
                yaw = 0.0f;
                pitch = 4.0f;
                fov = 45.0f;
                break;
            case PRESET_SHELVES_CLOSEUP:
                target = Vec3(-0.16f, 1.35f, 0.08f);
                distance = 1.4f;
                yaw = 0.0f;
                pitch = 6.0f;
                fov = 40.0f;
                break;
            case PRESET_KEYPAD_PANEL:
                target = Vec3(0.35f, 1.38f, 0.30f);
                distance = 1.0f;
                yaw = -15.0f;
                pitch = 5.0f;
                fov = 38.0f;
                break;
            case PRESET_COLLECTION_DOOR:
                target = Vec3(-0.15f, 0.42f, 0.22f);
                distance = 1.25f;
                yaw = -10.0f;
                pitch = 18.0f;
                fov = 40.0f;
                break;
            default:
                break;
        }
    }

    void cycleNextPreset() {
        int next = (currentPresetIndex + 1) % PRESET_COUNT;
        applyPreset(static_cast<CameraViewPreset>(next));
    }

    const char* getPresetName() const {
        switch (currentPresetIndex) {
            case PRESET_ROOM_OVERVIEW:   return "Room Overview";
            case PRESET_MACHINE_FRONT:   return "Vending Machine Front";
            case PRESET_SHELVES_CLOSEUP: return "Shelves & Spirals";
            case PRESET_KEYPAD_PANEL:    return "Keypad & Display Panel";
            case PRESET_COLLECTION_DOOR: return "Collection Door & Tray";
            default:                     return "Custom View";
        }
    }

    void reset() {
        applyPreset(PRESET_ROOM_OVERVIEW);
    }
};

enum CameraMode {
    MODE_FIRST_PERSON = 0,
    MODE_FREE_ORBIT = 1
};

struct Player {
    Vec3 pos;          // Position at feet (Y = 0 is ground)
    float eyeHeight;   // 1.65m
    float yaw;         // Look yaw (degrees), 180 = looking forward into rooms (-Z)
    float pitch;       // Look pitch (degrees)
    float velY;        // Vertical velocity for jumping
    bool isGrounded;
    float walkSpeed;
    float jumpSpeed;
    float gravity;

    Player()
        : pos(Vec3(0.0f, 0.0f, 3.5f)),
          eyeHeight(1.65f),
          yaw(180.0f),
          pitch(0.0f),
          velY(0.0f),
          isGrounded(true),
          walkSpeed(3.8f),
          jumpSpeed(5.0f),
          gravity(13.0f)
    {}

    Vec3 getEyePos() const {
        return Vec3(pos.x, pos.y + eyeHeight, pos.z);
    }

    Vec3 getLookDir() const {
        float rYaw = toRadians(yaw);
        float rPitch = toRadians(pitch);
        float x = std::sin(rYaw) * std::cos(rPitch);
        float y = std::sin(rPitch);
        float z = -std::cos(rYaw) * std::cos(rPitch);
        return normalize(Vec3(x, y, z));
    }

    Mat4 getViewMatrix() const {
        Vec3 eye = getEyePos();
        return lookAt(eye, eye + getLookDir(), Vec3(0.0f, 1.0f, 0.0f));
    }
};

#endif // CAMERA_H
