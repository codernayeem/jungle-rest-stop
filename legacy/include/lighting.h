#ifndef LIGHTING_H
#define LIGHTING_H

#include <glad/gl.h>
#include "math3d.h"

// -----------------------------------------------------------------------------
// Material Properties
// -----------------------------------------------------------------------------
struct Material {
    Vec3 color = Vec3(1.0f, 1.0f, 1.0f);
    Vec3 emission = Vec3(0.0f, 0.0f, 0.0f);
    float shininess = 32.0f;
    float specularStrength = 0.5f;
    float alpha = 1.0f;
};

// -----------------------------------------------------------------------------
// Light Type 1: Directional Light (Room Daylight / Overhead Studio Fill)
// -----------------------------------------------------------------------------
struct DirectionalLight {
    Vec3 direction = Vec3(-0.4f, -1.0f, -0.6f); // Shines from front-top-right into the room
    Vec3 color = Vec3(1.0f, 0.98f, 0.95f);
    float intensity = 0.90f;
    bool enabled = true;
};

// -----------------------------------------------------------------------------
// Light Type 2: Point Light (Interior Vending Machine Fluorescent Tube)
// -----------------------------------------------------------------------------
struct PointLight {
    Vec3 position = Vec3(-0.15f, 1.82f, 0.10f); // Top shelf tube light
    Vec3 color = Vec3(0.95f, 1.0f, 1.0f);       // Crisp white-cyan tube light
    float intensity = 1.8f;
    float constant = 1.0f;
    float linear = 0.5f;
    float quadratic = 0.9f;
    bool enabled = true;
};

// -----------------------------------------------------------------------------
// Light Type 3: Spot Light (Ceiling Track Spotlight focused on Vending Machine)
// -----------------------------------------------------------------------------
struct SpotLight {
    Vec3 position = Vec3(0.0f, 3.2f, 1.4f);       // Mounted in front-top
    Vec3 direction = Vec3(0.0f, -0.78f, -0.58f);  // Angled downward toward center of vending machine
    Vec3 color = Vec3(1.0f, 0.95f, 0.88f);        // Warm halogen spot
    float intensity = 2.4f;
    float cutOff = std::cos(toRadians(25.0f));     // Inner cone angle (25 deg)
    float outerCutOff = std::cos(toRadians(36.0f));// Outer soft falloff angle (36 deg)
    float constant = 1.0f;
    float linear = 0.25f;
    float quadratic = 0.35f;
    bool enabled = true;
};

// Collection Tray Indicator Point Light (Glows inside the chute when item drops)
struct TrayLight {
    Vec3 position = Vec3(-0.15f, 0.40f, 0.15f);
    Vec3 color = Vec3(0.2f, 0.7f, 1.0f);
    float intensity = 1.8f;
    float constant = 1.0f;
    float linear = 0.8f;
    float quadratic = 1.4f;
    bool enabled = false;
};

#endif // LIGHTING_H
