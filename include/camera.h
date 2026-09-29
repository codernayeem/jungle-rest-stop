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
        : target(Vec3(0.0f, 1.25f, 0.0f)),
          distance(4.2f),
          yaw(-45.0f),
          pitch(14.0f),
          fov(45.0f),
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
                target = Vec3(0.0f, 1.25f, 0.0f);
                distance = 4.2f;
                yaw = -45.0f;
                pitch = 14.0f;
                fov = 45.0f;
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

#endif // CAMERA_H
