/**
 * ============================================================================
 * Academic Project: Interactive 3D Vending Machine
 * Course: Image & Computer Graphics Lab
 * Student Name: Md. Nayeem
 * Roll Number:  2107050
 * ============================================================================
 * Key Features & Technical Implementation:
 *  1. Basic Shapes & Hierarchical Transformations:
 *     - All 3D objects (vending cabinet, shelves, products, coils, buttons,
 *       collection door, room walls, furniture, potted plant, trash bin) are
 *       built from basic shapes (cube, cylinder, sphere, coil, plane)
 *       using translation, rotation, and scaling transformations.
 *  2. Three Types of Lighting (Phong/Blinn-Phong Lighting Model):
 *     - Directional Light (Type 1): Overall room daylight / overhead fill.
 *     - Point Light (Type 2): Interior cabinet fluorescent tube lamp.
 *     - Spot Light (Type 3): Focused ceiling track spotlight directed on machine.
 *     - Plus collection bay indicator light!
 *  3. Dynamic Dispensing Mechanism & Physics:
 *     - Rotating product spiral moves selected item forward along the shelf.
 *     - Tipping animation over shelf rail, gravity drop down the internal chute,
 *       and damped bouncing into collection delivery bay.
 *  4. Opening Collection Door:
 *     - Hierarchical rotation around top hinge axis with "PUSH" plate.
 *  5. Interactive Buttons & Dynamic Indicators:
 *     - Keypad buttons with physical depression animation when pressed.
 *     - Dynamic LED status screen and multi-state machine indicator lights.
 *  6. Interactive Camera System:
 *     - Mouse orbit, pan, zoom, auto-turntable, and 5 preset views.
 *  7. Framerate Management:
 *     - Max FPS capped at 100 for smooth, consistent physics and simulation.
 * ============================================================================
 */

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "math3d.h"
#include "basic_shapes.h"
#include "lighting.h"
#include "camera.h"

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>

// -----------------------------------------------------------------------------
// Window & System Constants
// -----------------------------------------------------------------------------
const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;
const double TARGET_FPS = 100.0;
const double TARGET_FRAME_TIME = 1.0 / TARGET_FPS; // 10ms per frame

// Global Camera Instance
Camera g_camera;

// Global Basic Shape Meshes
Mesh g_meshCube;
Mesh g_meshCylinder;
Mesh g_meshSphere;
Mesh g_meshCoil;
Mesh g_meshPlane;

// Lighting Configurations
DirectionalLight g_dirLight;
PointLight       g_cabinetPointLight;
SpotLight        g_ceilingSpotLight;
TrayLight        g_trayIndicatorLight;

// -----------------------------------------------------------------------------
// Interactive Collection Door State (Smooth opening & closing animation)
// -----------------------------------------------------------------------------
float g_doorFlapAngle = 0.0f;       // Current door flap angle in degrees (0 = closed)
float g_targetDoorFlapAngle = 0.0f; // Target angle for smooth transition
float g_doorHoldTimer = 0.0f;       // Timer to hold door open after item lands
bool  g_isDoorManuallyOpened = false;

// -----------------------------------------------------------------------------
// Interactive Keypad Button Depression Animation
// -----------------------------------------------------------------------------
struct KeypadButton {
    int id;           // 0-9 for numbers, 10 for Cancel, 11 for Enter
    Vec3 localPos;    // position on keypad plate
    Vec3 size;        // width, height, depth
    Material mat;     // material color
    float pressOffset;// current depression distance (Z axis)
};
std::vector<KeypadButton> g_keypadButtons;

// -----------------------------------------------------------------------------
// Product Categories & Slot Structure
// -----------------------------------------------------------------------------
enum ProductType {
    PROD_CAN_COLA,
    PROD_CAN_LIME,
    PROD_CAN_ENERGY,
    PROD_CAN_ORANGE,
    PROD_BOTTLE_WATER,
    PROD_BOTTLE_JUICE,
    PROD_CHIPS_BBQ,
    PROD_CHIPS_CREAM,
    PROD_CHIPS_NACHO,
    PROD_CHOCOLATE,
    PROD_CANDY_BAR,
    PROD_COOKIES
};

enum DispenseState {
    STATE_IDLE_ON_SHELF,
    STATE_SPIRAL_TURNING,
    STATE_FALLING_CHUTE,
    STATE_RESTING_IN_TRAY,
    STATE_DISPENSED_GONE
};

struct ProductSlot {
    std::string code;
    std::string name;
    float price;
    ProductType type;
    Vec3 primaryColor;
    Vec3 secondaryColor;
    int shelfRow;
    int shelfCol;

    // Position & Physics Animation State
    DispenseState state;
    float spiralAngle;      // degrees
    float forwardProgress;  // 0.0 to 1.0 along shelf
    Vec3  currentPos;
    Vec3  currentRot;
    Vec3  velocity;
    float trayTimer;        // seconds item stays in tray before collection/reset
};

std::vector<ProductSlot> g_slots;

// Machine Status & Message Display
std::string g_statusMessage = "READY - SELECT 1-9";
float g_statusTimer = 0.0f;

// Shelf Y coordinates (from top row to bottom row)
const float SHELF_Y[4] = { 1.70f, 1.40f, 1.10f, 0.80f };
// Slot X coordinates (3 columns per shelf)
const float SLOT_X[3]  = { -0.36f, -0.18f, 0.00f };

// Shelf geometry constants: Shelves pushed back to leave generous internal drop space
const float SHELF_CENTER_Z = -0.05f;
const float SHELF_DEPTH_Z  = 0.38f;
const float SHELF_FRONT_Z  = 0.14f; // Shelf front lip (behind glass at 0.33f)
const float CHUTE_DROP_Z   = 0.20f; // Center of vertical drop chute

// -----------------------------------------------------------------------------
// GLSL Shaders (Support 3 Types of Lighting: Directional, Point, Spot)
// -----------------------------------------------------------------------------
const char* vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    Normal = normalize(normalMatrix * aNormal);
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

// --- Material Uniforms ---
uniform vec3 objectColor;
uniform vec3 emissionColor;
uniform float shininess;
uniform float specularStrength;
uniform float alpha;

uniform vec3 viewPos;

// --- Light Type 1: Directional Light (Room Daylight / Overhead) ---
struct DirLight {
    vec3 direction;
    vec3 color;
    float intensity;
    bool enabled;
};
uniform DirLight dirLight;

// --- Light Type 2: Point Light (Interior Vending Cabinet Tube) ---
struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    bool enabled;
};
uniform PointLight pointLight;

// --- Collection Tray Indicator Point Light ---
uniform PointLight trayLight;

// --- Light Type 3: Spot Light (Ceiling Spotlight Focused on Machine) ---
struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    bool enabled;
};
uniform SpotLight spotLight;

// Calculate Directional Light contribution
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(-light.direction);

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);

    vec3 diffuse = diff * light.color * light.intensity * objectColor;
    vec3 specular = specularStrength * spec * light.color * light.intensity;
    return diffuse + specular;
}

// Calculate Point Light contribution with distance attenuation
vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(light.position - fragPos);
    float dist = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * (dist * dist));

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);

    vec3 diffuse = diff * light.color * light.intensity * objectColor * attenuation;
    vec3 specular = specularStrength * spec * light.color * light.intensity * attenuation;
    return diffuse + specular;
}

// Calculate Spot Light contribution with angular cone falloff and attenuation
vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(light.position - fragPos);
    float dist = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * (dist * dist));

    // Conical cutoffs
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float spotIntensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);

    vec3 diffuse = diff * light.color * light.intensity * objectColor * attenuation * spotIntensity;
    vec3 specular = specularStrength * spec * light.color * light.intensity * attenuation * spotIntensity;
    return diffuse + specular;
}

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Balanced ambient room illumination
    vec3 ambient = 0.28 * objectColor;

    // Accumulate the 3 types of lighting
    vec3 result = ambient;
    result += calcDirLight(dirLight, norm, viewDir);
    result += calcPointLight(pointLight, norm, FragPos, viewDir);
    result += calcPointLight(trayLight, norm, FragPos, viewDir);
    result += calcSpotLight(spotLight, norm, FragPos, viewDir);

    // Self-illumination (LED signs, displays, indicators)
    result += emissionColor;

    FragColor = vec4(result, alpha);
}
)";

GLuint g_shaderProgram = 0;

// -----------------------------------------------------------------------------
// Shader Creation Helpers
// -----------------------------------------------------------------------------
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "[Shader Error] " << infoLog << std::endl;
    }
    return shader;
}

GLuint createShaderProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "[Program Link Error] " << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// -----------------------------------------------------------------------------
// Unified Object Drawing Function (Built on basic shapes via Transformations)
// -----------------------------------------------------------------------------
void drawShape(const Mesh& mesh, const Mat4& model, const Material& mat) {
    glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "model"), 1, GL_FALSE, model.m);
    glUniform3f(glGetUniformLocation(g_shaderProgram, "objectColor"), mat.color.x, mat.color.y, mat.color.z);
    glUniform3f(glGetUniformLocation(g_shaderProgram, "emissionColor"), mat.emission.x, mat.emission.y, mat.emission.z);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "shininess"), mat.shininess);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "specularStrength"), mat.specularStrength);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "alpha"), mat.alpha);

    mesh.draw();
}

// Draw Cube Primitive with translation, rotation, and scaling
void drawCube(const Vec3& pos, const Vec3& sz, const Material& mat, const Vec3& rotDeg = Vec3(0,0,0)) {
    Mat4 m = Mat4::Identity();
    m = translate(m, pos);
    if (rotDeg.y != 0.0f) m = rotate(m, toRadians(rotDeg.y), Vec3(0, 1, 0));
    if (rotDeg.x != 0.0f) m = rotate(m, toRadians(rotDeg.x), Vec3(1, 0, 0));
    if (rotDeg.z != 0.0f) m = rotate(m, toRadians(rotDeg.z), Vec3(0, 0, 1));
    m = scale(m, sz);
    drawShape(g_meshCube, m, mat);
}

// Draw Cylinder Primitive with translation, rotation, and scaling
void drawCylinder(const Vec3& pos, const Vec3& sz, const Material& mat, const Vec3& rotDeg = Vec3(0,0,0)) {
    Mat4 m = Mat4::Identity();
    m = translate(m, pos);
    if (rotDeg.y != 0.0f) m = rotate(m, toRadians(rotDeg.y), Vec3(0, 1, 0));
    if (rotDeg.x != 0.0f) m = rotate(m, toRadians(rotDeg.x), Vec3(1, 0, 0));
    if (rotDeg.z != 0.0f) m = rotate(m, toRadians(rotDeg.z), Vec3(0, 0, 1));
    m = scale(m, sz);
    drawShape(g_meshCylinder, m, mat);
}

// Draw Sphere Primitive with translation, rotation, and scaling
void drawSphere(const Vec3& pos, const Vec3& sz, const Material& mat, const Vec3& rotDeg = Vec3(0,0,0)) {
    Mat4 m = Mat4::Identity();
    m = translate(m, pos);
    if (rotDeg.y != 0.0f) m = rotate(m, toRadians(rotDeg.y), Vec3(0, 1, 0));
    if (rotDeg.x != 0.0f) m = rotate(m, toRadians(rotDeg.x), Vec3(1, 0, 0));
    if (rotDeg.z != 0.0f) m = rotate(m, toRadians(rotDeg.z), Vec3(0, 0, 1));
    m = scale(m, sz);
    drawShape(g_meshSphere, m, mat);
}

// Draw Helical Coil Primitive
void drawCoil(const Vec3& pos, const Vec3& scaleVec, const Material& mat, float angleDeg) {
    Mat4 m = Mat4::Identity();
    m = translate(m, pos);
    m = rotate(m, toRadians(angleDeg), Vec3(0, 0, 1));
    m = scale(m, scaleVec);
    drawShape(g_meshCoil, m, mat);
}

// -----------------------------------------------------------------------------
// Composite Products Constructed from Basic Shapes (True Hierarchical Modeling)
// -----------------------------------------------------------------------------

// Helper to construct root model transformation for a product
inline Mat4 makeProductRoot(const Vec3& pos, const Vec3& rot) {
    Mat4 root = Mat4::Identity();
    root = translate(root, pos);
    if (rot.y != 0.0f) root = rotate(root, toRadians(rot.y), Vec3(0.0f, 1.0f, 0.0f));
    if (rot.x != 0.0f) root = rotate(root, toRadians(rot.x), Vec3(1.0f, 0.0f, 0.0f));
    if (rot.z != 0.0f) root = rotate(root, toRadians(rot.z), Vec3(0.0f, 0.0f, 1.0f));
    return root;
}

// 1. Soda Can (Cola, Lime, Energy, Orange)
void drawSodaCan(const Vec3& pos, const Vec3& rot, const Vec3& mainCol, const Vec3& accentCol) {
    Mat4 root = makeProductRoot(pos, rot);

    Material bodyMat;
    bodyMat.color = mainCol;
    bodyMat.shininess = 64.0f;
    bodyMat.specularStrength = 0.85f;

    Material metalMat;
    metalMat.color = Vec3(0.86f, 0.87f, 0.90f);
    metalMat.shininess = 128.0f;
    metalMat.specularStrength = 0.95f;

    Material logoMat;
    logoMat.color = accentCol;
    logoMat.shininess = 48.0f;
    logoMat.specularStrength = 0.6f;

    // Main cylindrical body
    Mat4 bodyM = scale(root, Vec3(0.065f, 0.125f, 0.065f));
    drawShape(g_meshCylinder, bodyM, bodyMat);

    // Silver top rim
    Mat4 topRimM = translate(root, Vec3(0.0f, 0.063f, 0.0f));
    topRimM = scale(topRimM, Vec3(0.064f, 0.008f, 0.064f));
    drawShape(g_meshCylinder, topRimM, metalMat);

    // Silver bottom rim
    Mat4 botRimM = translate(root, Vec3(0.0f, -0.063f, 0.0f));
    botRimM = scale(botRimM, Vec3(0.062f, 0.008f, 0.062f));
    drawShape(g_meshCylinder, botRimM, metalMat);

    // Logo banner band
    Mat4 logoM = scale(root, Vec3(0.0655f, 0.038f, 0.0655f));
    drawShape(g_meshCylinder, logoM, logoMat);

    // Pull-tab on top
    Mat4 tabM = translate(root, Vec3(0.01f, 0.068f, 0.0f));
    tabM = scale(tabM, Vec3(0.02f, 0.003f, 0.012f));
    drawShape(g_meshCube, tabM, metalMat);
}

// 2. Beverage Bottle (Water, Juice)
void drawBottle(const Vec3& pos, const Vec3& rot, const Vec3& liquidCol, const Vec3& capCol) {
    Mat4 root = makeProductRoot(pos, rot);

    Material plasticMat;
    plasticMat.color = liquidCol;
    plasticMat.shininess = 96.0f;
    plasticMat.specularStrength = 0.9f;

    Material capMat;
    capMat.color = capCol;
    capMat.shininess = 32.0f;

    Material labelMat;
    labelMat.color = Vec3(0.96f, 0.96f, 0.96f);
    labelMat.shininess = 16.0f;

    // Lower bottle body
    Mat4 bodyM = translate(root, Vec3(0.0f, -0.015f, 0.0f));
    bodyM = scale(bodyM, Vec3(0.065f, 0.10f, 0.065f));
    drawShape(g_meshCylinder, bodyM, plasticMat);

    // Bottle neck
    Mat4 neckM = translate(root, Vec3(0.0f, 0.052f, 0.0f));
    neckM = scale(neckM, Vec3(0.032f, 0.045f, 0.032f));
    drawShape(g_meshCylinder, neckM, plasticMat);

    // Cap
    Mat4 capM = translate(root, Vec3(0.0f, 0.076f, 0.0f));
    capM = scale(capM, Vec3(0.035f, 0.015f, 0.035f));
    drawShape(g_meshCylinder, capM, capMat);

    // Label wrap
    Mat4 labelM = translate(root, Vec3(0.0f, -0.015f, 0.0f));
    labelM = scale(labelM, Vec3(0.0655f, 0.04f, 0.0655f));
    drawShape(g_meshCylinder, labelM, labelMat);
}

// 3. Snack Pouch / Bag (Chips, Crisps)
void drawSnackBag(const Vec3& pos, const Vec3& rot, const Vec3& mainCol, const Vec3& secondaryCol) {
    Mat4 root = makeProductRoot(pos, rot);

    Material bagMat;
    bagMat.color = mainCol;
    bagMat.shininess = 64.0f;
    bagMat.specularStrength = 0.7f;

    Material crimpMat;
    crimpMat.color = secondaryCol;
    crimpMat.shininess = 32.0f;

    // Pouch body
    Mat4 bodyM = scale(root, Vec3(0.095f, 0.13f, 0.045f));
    drawShape(g_meshCube, bodyM, bagMat);

    // Top crimp seal
    Mat4 topCrimpM = translate(root, Vec3(0.0f, 0.068f, 0.0f));
    topCrimpM = scale(topCrimpM, Vec3(0.098f, 0.01f, 0.012f));
    drawShape(g_meshCube, topCrimpM, crimpMat);

    // Bottom crimp seal
    Mat4 botCrimpM = translate(root, Vec3(0.0f, -0.068f, 0.0f));
    botCrimpM = scale(botCrimpM, Vec3(0.098f, 0.01f, 0.012f));
    drawShape(g_meshCube, botCrimpM, crimpMat);

    // Brand emblem on pouch
    Mat4 emblemM = translate(root, Vec3(0.0f, 0.0f, 0.001f));
    emblemM = scale(emblemM, Vec3(0.08f, 0.045f, 0.048f));
    drawShape(g_meshCube, emblemM, crimpMat);
}

// 4. Candy Bar & Cookies
void drawCandyBar(const Vec3& pos, const Vec3& rot, const Vec3& wrapperCol, const Vec3& accentCol) {
    Mat4 root = makeProductRoot(pos, rot);

    Material wrapMat;
    wrapMat.color = wrapperCol;
    wrapMat.shininess = 48.0f;
    wrapMat.specularStrength = 0.75f;

    Material accMat;
    accMat.color = accentCol;
    accMat.shininess = 32.0f;

    // Main rectangular wrapper
    Mat4 wrapM = scale(root, Vec3(0.105f, 0.038f, 0.055f));
    drawShape(g_meshCube, wrapM, wrapMat);

    // Central branding band
    Mat4 bandM = scale(root, Vec3(0.055f, 0.040f, 0.057f));
    drawShape(g_meshCube, bandM, accMat);
}

// -----------------------------------------------------------------------------
// Keypad & Slots Initialization
// -----------------------------------------------------------------------------
void initKeypadButtons() {
    g_keypadButtons.clear();
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 3; ++c) {
            KeypadButton btn;
            float bx = 0.29f + c * 0.06f;
            float by = 1.49f - r * 0.058f;
            btn.localPos = Vec3(bx, by, 0.351f);
            btn.size = Vec3(0.042f, 0.040f, 0.008f);
            btn.pressOffset = 0.0f;

            if (r < 3) {
                btn.id = r * 3 + c + 1; // 1 to 9
                btn.mat.color = Vec3(0.78f, 0.80f, 0.84f);
                btn.mat.shininess = 64.0f;
                btn.mat.specularStrength = 0.8f;
            } else {
                if (c == 0) {
                    btn.id = 10; // Cancel
                    btn.mat.color = Vec3(0.85f, 0.18f, 0.15f); // Red
                    btn.mat.emission = Vec3(0.15f, 0.03f, 0.03f);
                } else if (c == 1) {
                    btn.id = 0; // '0' key
                    btn.mat.color = Vec3(0.78f, 0.80f, 0.84f);
                } else {
                    btn.id = 11; // Enter
                    btn.mat.color = Vec3(0.15f, 0.75f, 0.25f); // Green
                    btn.mat.emission = Vec3(0.03f, 0.15f, 0.03f);
                }
            }
            g_keypadButtons.push_back(btn);
        }
    }
}

void triggerButtonPress(int btnId) {
    for (auto& btn : g_keypadButtons) {
        if (btn.id == btnId) {
            btn.pressOffset = 0.004f; // Depress 4mm inward
            break;
        }
    }
}

void initProductSlots() {
    g_slots.clear();

    // Row 1 (Top): Soda Cans
    g_slots.push_back({ "1", "Cola Classic",    1.50f, PROD_CAN_COLA,     Vec3(0.85f, 0.12f, 0.12f), Vec3(0.95f, 0.95f, 0.95f), 0, 0, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });
    g_slots.push_back({ "2", "Citrus Lime",     1.50f, PROD_CAN_LIME,     Vec3(0.15f, 0.82f, 0.25f), Vec3(0.95f, 0.95f, 0.15f), 0, 1, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });
    g_slots.push_back({ "3", "Energy Boost",    2.00f, PROD_CAN_ENERGY,   Vec3(0.10f, 0.40f, 0.90f), Vec3(0.10f, 0.90f, 0.95f), 0, 2, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });

    // Row 2: Mineral Water & Juices
    g_slots.push_back({ "4", "Pure Water",      1.25f, PROD_BOTTLE_WATER, Vec3(0.35f, 0.75f, 0.95f), Vec3(1.0f, 1.0f, 1.0f), 1, 0, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });
    g_slots.push_back({ "5", "Orange Soda",     1.50f, PROD_CAN_ORANGE,   Vec3(0.95f, 0.50f, 0.08f), Vec3(1.0f, 0.85f, 0.20f), 1, 1, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });
    g_slots.push_back({ "6", "Ruby Fruit Juice",1.75f, PROD_BOTTLE_JUICE, Vec3(0.90f, 0.18f, 0.45f), Vec3(0.95f, 0.85f, 0.10f), 1, 2, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });

    // Row 3: Crispy Snacks & Chips
    g_slots.push_back({ "7", "BBQ Potato Chips",1.75f, PROD_CHIPS_BBQ,    Vec3(0.90f, 0.20f, 0.15f), Vec3(1.0f, 0.85f, 0.20f), 2, 0, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });
    g_slots.push_back({ "8", "Sour Cream Crisps",1.75f, PROD_CHIPS_CREAM, Vec3(0.20f, 0.70f, 0.30f), Vec3(1.0f, 1.0f, 1.0f), 2, 1, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });
    g_slots.push_back({ "9", "Cheese Nachos",   1.80f, PROD_CHIPS_NACHO,  Vec3(0.95f, 0.75f, 0.10f), Vec3(0.85f, 0.15f, 0.15f), 2, 2, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });

    // Row 4 (Extra): Candies
    g_slots.push_back({ "0", "Choco Fudge Bar", 1.50f, PROD_CHOCOLATE,    Vec3(0.40f, 0.22f, 0.12f), Vec3(0.90f, 0.75f, 0.20f), 3, 0, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });
    g_slots.push_back({ "E1","Caramel Wafer",   1.35f, PROD_CANDY_BAR,    Vec3(0.85f, 0.10f, 0.20f), Vec3(0.95f, 0.95f, 0.95f), 3, 1, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });
    g_slots.push_back({ "E2","Cookies Delight", 1.60f, PROD_COOKIES,      Vec3(0.12f, 0.35f, 0.75f), Vec3(0.95f, 0.85f, 0.30f), 3, 2, STATE_IDLE_ON_SHELF, 0.0f, 0.0f, Vec3(), Vec3(), Vec3(), 0.0f });

    // Initial item positions: resting inside spirals on the shelves
    for (auto& s : g_slots) {
        float y = SHELF_Y[s.shelfRow] + 0.08f;
        float x = SLOT_X[s.shelfCol];
        float z = SHELF_CENTER_Z;
        s.currentPos = Vec3(x, y, z);
        s.currentRot = Vec3(0, 0, 0);
        s.state = STATE_IDLE_ON_SHELF;
        s.spiralAngle = 0.0f;
        s.forwardProgress = 0.0f;
    }
}

// -----------------------------------------------------------------------------
// Interactive Vending Trigger
// -----------------------------------------------------------------------------
void selectAndVendItem(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= (int)g_slots.size()) return;
    ProductSlot& s = g_slots[slotIndex];

    // Trigger visual button press
    int btnId = (slotIndex == 9) ? 0 : (slotIndex + 1);
    triggerButtonPress(btnId);

    if (s.state == STATE_DISPENSED_GONE) {
        std::ostringstream ss;
        ss << "OUT OF STOCK [" << s.code << "] - PRESS [L] TO LOAD ALL";
        g_statusMessage = ss.str();
        g_statusTimer = 3.5f;
        std::cout << "[VENDING] Slot [" << s.code << "] (" << s.name << ") is OUT OF STOCK! Press [L] or [ENTER] to reload all." << std::endl;
        return;
    }

    if (s.state == STATE_SPIRAL_TURNING || s.state == STATE_FALLING_CHUTE || s.state == STATE_RESTING_IN_TRAY) {
        return; // Busy vending this product
    }

    // Reset and start dispensing sequence
    s.state = STATE_SPIRAL_TURNING;
    s.forwardProgress = 0.0f;
    s.velocity = Vec3(0, 0, 0);
    s.currentRot = Vec3(0, 0, 0);
    s.trayTimer = 0.0f;

    std::ostringstream ss;
    ss << "DISPENSING [" << s.code << "] " << s.name << " ($" << std::fixed << std::setprecision(2) << s.price << ")";
    g_statusMessage = ss.str();
    g_statusTimer = 6.0f;
}

// -----------------------------------------------------------------------------
// Restock / Reload All Products Onto Shelves
// -----------------------------------------------------------------------------
void restockAllItems() {
    triggerButtonPress(11); // Animate green ENTER button on keypad
    for (auto& s : g_slots) {
        float y = SHELF_Y[s.shelfRow] + 0.08f;
        float x = SLOT_X[s.shelfCol];
        float z = SHELF_CENTER_Z;
        s.currentPos = Vec3(x, y, z);
        s.currentRot = Vec3(0, 0, 0);
        s.velocity = Vec3(0, 0, 0);
        s.state = STATE_IDLE_ON_SHELF;
        s.spiralAngle = 0.0f;
        s.forwardProgress = 0.0f;
        s.trayTimer = 0.0f;
    }
    g_doorHoldTimer = 0.0f;
    g_targetDoorFlapAngle = 0.0f;
    g_trayIndicatorLight.enabled = false;
    g_statusMessage = "RESTOCKED - ALL 10 ITEMS LOADED";
    g_statusTimer = 3.5f;
    std::cout << "[RESTOCK] All 10 items reloaded onto shelves! Ready to vend." << std::endl;
}

// -----------------------------------------------------------------------------
// Physics & Animation Tick (Hierarchical Motion & State Machine)
// -----------------------------------------------------------------------------
void updateSimulation(float dt) {
    g_camera.update(dt);

    if (g_statusTimer > 0.0f) {
        g_statusTimer -= dt;
        if (g_statusTimer <= 0.0f) {
            g_statusMessage = "READY - SELECT 1-9";
        }
    }

    // Keypad button depression return spring
    for (auto& btn : g_keypadButtons) {
        if (btn.pressOffset > 0.0f) {
            btn.pressOffset = std::max(0.0f, btn.pressOffset - 0.04f * dt);
        }
    }

    // Check if any item is ready for pickup in the tray
    bool anyItemInTray = false;

    // Update dispensing items
    for (auto& s : g_slots) {
        if (s.state == STATE_SPIRAL_TURNING) {
            // Spiral rotation & forward linear translation along shelf
            float speed = 0.65f; // progress per second
            s.forwardProgress += speed * dt;
            s.spiralAngle += 360.0f * speed * dt;

            float initZ = SHELF_CENTER_Z;
            float ledgeZ = SHELF_FRONT_Z;
            s.currentPos.z = initZ + s.forwardProgress * (ledgeZ - initZ);

            // Progressive tilt forward as it nears the shelf edge
            if (s.forwardProgress > 0.75f) {
                float tiltFactor = (s.forwardProgress - 0.75f) / 0.25f;
                s.currentRot.x = tiltFactor * 30.0f;
            }

            if (s.forwardProgress >= 1.0f) {
                // Item slides off shelf lip into the internal drop chute!
                s.state = STATE_FALLING_CHUTE;
                s.velocity = Vec3(0.0f, -0.4f, 0.05f); // Gentle forward nudge into chute
            }
        }
        else if (s.state == STATE_FALLING_CHUTE) {
            // Gravity acceleration
            s.velocity.y -= 6.0f * dt;
            s.currentPos += s.velocity * dt;

            // Restrict Z position strictly inside the chute behind the front glass pane (Z = 0.33f)
            s.currentPos.z = std::clamp(s.currentPos.z, SHELF_FRONT_Z, 0.26f);

            // Tumbling rotation while falling
            s.currentRot.x += 160.0f * dt;
            s.currentRot.z += 80.0f * dt;

            // Bottom delivery tray floor level
            float floorY = 0.32f;

            if (s.currentPos.y <= floorY) {
                s.currentPos.y = floorY;
                s.currentPos.z = 0.18f + ((s.shelfCol - 1) * 0.04f);
                s.velocity.y = -s.velocity.y * 0.24f; // Damped bounce
                s.velocity.z = 0.0f;

                if (std::abs(s.velocity.y) < 0.20f) {
                    // Settled safely in delivery chamber
                    s.state = STATE_RESTING_IN_TRAY;
                    s.trayTimer = 5.0f; // Remains for 5 seconds before reset
                    s.currentRot = Vec3(12.0f, 22.0f, 0.0f); // Natural resting angle
                    g_targetDoorFlapAngle = 38.0f; // Trigger smooth opening from inside
                    g_doorHoldTimer = 2.4f;        // Keep open for 2.4 seconds then close smoothly
                }
            }
        }
        else if (s.state == STATE_RESTING_IN_TRAY) {
            anyItemInTray = true;
            s.trayTimer -= dt;
            if (s.trayTimer <= 0.0f) {
                // Item collected by customer - disappears forever until reloaded!
                s.state = STATE_DISPENSED_GONE;
                g_statusMessage = "COLLECTED [" + s.code + "] " + s.name + " - ENJOY!";
                g_statusTimer = 3.0f;
            }
        }
    }

    // Update collection tray indicator light
    g_trayIndicatorLight.enabled = anyItemInTray;

    // Smooth Collection Door Opening & Closing Physics
    if (g_isDoorManuallyOpened) {
        g_targetDoorFlapAngle = 45.0f; // Kept open manually via [O] key
    } else if (g_doorHoldTimer > 0.0f) {
        g_doorHoldTimer -= dt;
        g_targetDoorFlapAngle = 38.0f; // Pushed open by dispensed item
        if (g_doorHoldTimer <= 0.0f) {
            g_targetDoorFlapAngle = 0.0f; // Return smoothly to closed position
        }
    } else {
        g_targetDoorFlapAngle = 0.0f;
    }

    // Smoothly interpolate angle towards target (opens at 55 deg/s, closes at 35 deg/s)
    float doorSpeed = (g_doorFlapAngle < g_targetDoorFlapAngle) ? 55.0f : 35.0f;
    if (g_doorFlapAngle < g_targetDoorFlapAngle) {
        g_doorFlapAngle = std::min(g_targetDoorFlapAngle, g_doorFlapAngle + doorSpeed * dt);
    } else if (g_doorFlapAngle > g_targetDoorFlapAngle) {
        g_doorFlapAngle = std::max(g_targetDoorFlapAngle, g_doorFlapAngle - doorSpeed * dt);
    }
}

// -----------------------------------------------------------------------------
// Scene Rendering: Vending Machine, Room Environment, and Furniture
// -----------------------------------------------------------------------------
void renderScene() {
    // -------------------------------------------------------------------------
    // Material Definitions
    // -------------------------------------------------------------------------
    Material cabinetBodyMat;
    cabinetBodyMat.color = Vec3(0.14f, 0.16f, 0.20f); // Sleek graphite charcoal
    cabinetBodyMat.shininess = 32.0f;
    cabinetBodyMat.specularStrength = 0.5f;

    Material cabinetAccentMat;
    cabinetAccentMat.color = Vec3(0.22f, 0.26f, 0.32f);
    cabinetAccentMat.shininess = 48.0f;

    Material chromeMat;
    chromeMat.color = Vec3(0.88f, 0.90f, 0.94f);
    chromeMat.shininess = 128.0f;
    chromeMat.specularStrength = 0.95f;

    Material interiorWallMat;
    interiorWallMat.color = Vec3(0.78f, 0.80f, 0.84f);
    interiorWallMat.shininess = 16.0f;
    interiorWallMat.specularStrength = 0.35f;

    Material shelfMat;
    shelfMat.color = Vec3(0.42f, 0.45f, 0.50f);
    shelfMat.shininess = 64.0f;
    shelfMat.specularStrength = 0.7f;

    Material coilMat;
    coilMat.color = Vec3(0.92f, 0.92f, 0.96f);
    coilMat.shininess = 128.0f;
    coilMat.specularStrength = 0.95f;

    Material rubberMat;
    rubberMat.color = Vec3(0.06f, 0.06f, 0.06f);
    rubberMat.shininess = 12.0f;

    // -------------------------------------------------------------------------
    // 1. ROOM ENVIRONMENT (Floor, Back Wall, Poster, Ceiling Light Fixture)
    // -------------------------------------------------------------------------
    // Floor Slab
    Material floorBaseMat;
    floorBaseMat.color = Vec3(0.24f, 0.26f, 0.29f);
    floorBaseMat.shininess = 48.0f;
    drawCube(Vec3(0.0f, -0.02f, 0.0f), Vec3(7.0f, 0.04f, 7.0f), floorBaseMat);

    // Tiled Floor Inlays (Alternating Checkerboard Tiles)
    Material tileMatA;
    tileMatA.color = Vec3(0.18f, 0.20f, 0.23f);
    tileMatA.shininess = 64.0f;
    tileMatA.specularStrength = 0.5f;

    Material tileMatB;
    tileMatB.color = Vec3(0.32f, 0.34f, 0.38f);
    tileMatB.shininess = 64.0f;
    tileMatB.specularStrength = 0.5f;

    for (int ix = -3; ix <= 3; ++ix) {
        for (int iz = -3; iz <= 3; ++iz) {
            Material& tMat = ((ix + iz) % 2 == 0) ? tileMatA : tileMatB;
            drawCube(Vec3(ix * 0.95f, 0.002f, iz * 0.95f), Vec3(0.92f, 0.005f, 0.92f), tMat);
        }
    }

    // Back Wall
    Material backWallMat;
    backWallMat.color = Vec3(0.42f, 0.45f, 0.50f);
    backWallMat.shininess = 16.0f;
    drawCube(Vec3(0.0f, 1.80f, -1.35f), Vec3(7.0f, 3.6f, 0.08f), backWallMat);

    // Wall Skirting / Baseboard Trim
    Material baseboardMat;
    baseboardMat.color = Vec3(0.15f, 0.16f, 0.18f);
    drawCube(Vec3(0.0f, 0.10f, -1.30f), Vec3(7.0f, 0.20f, 0.04f), baseboardMat);

    // Decorative Wall Neon Trim Stripe
    Material wallStripeMat;
    wallStripeMat.color = Vec3(0.12f, 0.70f, 0.90f);
    wallStripeMat.emission = Vec3(0.10f, 0.45f, 0.60f);
    drawCube(Vec3(0.0f, 2.70f, -1.30f), Vec3(7.0f, 0.05f, 0.02f), wallStripeMat);

    // Wall Poster: Student Project Banner
    Material posterFrameMat;
    posterFrameMat.color = Vec3(0.10f, 0.10f, 0.12f);
    drawCube(Vec3(1.50f, 1.90f, -1.30f), Vec3(1.20f, 0.90f, 0.03f), posterFrameMat);

    Material posterMat;
    posterMat.color = Vec3(0.92f, 0.94f, 0.98f);
    posterMat.emission = Vec3(0.05f, 0.05f, 0.06f);
    drawCube(Vec3(1.50f, 1.90f, -1.28f), Vec3(1.10f, 0.80f, 0.01f), posterMat);

    // Poster Header Bar & Design Lines
    Material posterBarMat;
    posterBarMat.color = Vec3(0.85f, 0.15f, 0.20f);
    drawCube(Vec3(1.50f, 2.18f, -1.27f), Vec3(1.00f, 0.08f, 0.005f), posterBarMat);
    Material posterSubMat;
    posterSubMat.color = Vec3(0.15f, 0.45f, 0.85f);
    drawCube(Vec3(1.50f, 2.05f, -1.27f), Vec3(0.85f, 0.03f, 0.005f), posterSubMat);

    // Overhead Ceiling Spotlight Fixture (Track Light)
    Material spotFixtureMat;
    spotFixtureMat.color = Vec3(0.20f, 0.22f, 0.25f);
    drawCylinder(Vec3(0.0f, 3.35f, 1.40f), Vec3(0.18f, 0.15f, 0.18f), spotFixtureMat);
    drawCylinder(Vec3(0.0f, 3.22f, 1.40f), Vec3(0.12f, 0.12f, 0.12f), chromeMat, Vec3(35.0f, 0, 0));
    Material spotBulbMat;
    spotBulbMat.color = Vec3(1.0f, 0.95f, 0.85f);
    spotBulbMat.emission = g_ceilingSpotLight.enabled ? Vec3(1.8f, 1.6f, 1.4f) : Vec3(0, 0, 0);
    drawSphere(Vec3(0.0f, 3.16f, 1.36f), Vec3(0.08f, 0.08f, 0.08f), spotBulbMat);

    // -------------------------------------------------------------------------
    // 2. SURROUNDING OBJECTS (Trash Can, Room Bench, Potted Plant)
    // -------------------------------------------------------------------------
    // Object A: Modern Stainless Steel Trash & Recycling Can (Left of machine)
    float binX = -1.15f;
    float binZ = 0.15f;
    Material binBodyMat;
    binBodyMat.color = Vec3(0.68f, 0.70f, 0.75f);
    binBodyMat.shininess = 96.0f;
    binBodyMat.specularStrength = 0.8f;

    // Solid Black Base Ring (sits firmly on floor)
    drawCylinder(Vec3(binX, 0.03f, binZ), Vec3(0.36f, 0.06f, 0.36f), rubberMat);
    // Cylindrical Can Body
    drawCylinder(Vec3(binX, 0.40f, binZ), Vec3(0.33f, 0.70f, 0.33f), binBodyMat);
    // Bin Chrome Top Rim
    drawCylinder(Vec3(binX, 0.75f, binZ), Vec3(0.35f, 0.03f, 0.35f), chromeMat);
    // Bin Lid Top Dome
    Material binLidMat;
    binLidMat.color = Vec3(0.18f, 0.20f, 0.22f);
    drawCylinder(Vec3(binX, 0.77f, binZ), Vec3(0.33f, 0.02f, 0.33f), binLidMat);
    // Push swing flap on trash lid
    drawCube(Vec3(binX, 0.775f, binZ), Vec3(0.20f, 0.01f, 0.14f), chromeMat, Vec3(-20.0f, 0, 0));

    // Object B: Potted Indoor Plant (Right of machine)
    float plantX = 1.35f;
    float plantZ = 0.20f;
    Material potMat;
    potMat.color = Vec3(0.88f, 0.90f, 0.92f);
    potMat.shininess = 64.0f;
    potMat.specularStrength = 0.6f;

    // Solid Ceramic Pot Base Plate (sits firmly on floor)
    drawCylinder(Vec3(plantX, 0.04f, plantZ), Vec3(0.32f, 0.08f, 0.32f), potMat);
    // Pot Body
    drawCylinder(Vec3(plantX, 0.28f, plantZ), Vec3(0.38f, 0.42f, 0.38f), potMat);
    // Pot Upper Rim
    drawCylinder(Vec3(plantX, 0.50f, plantZ), Vec3(0.42f, 0.04f, 0.42f), potMat);
    // Pot Dark Soil
    Material soilMat;
    soilMat.color = Vec3(0.16f, 0.12f, 0.09f);
    drawCylinder(Vec3(plantX, 0.49f, plantZ), Vec3(0.37f, 0.02f, 0.37f), soilMat);
    // Plant Main Trunk
    Material stemMat;
    stemMat.color = Vec3(0.22f, 0.48f, 0.18f);
    drawCylinder(Vec3(plantX, 0.68f, plantZ), Vec3(0.045f, 0.38f, 0.045f), stemMat);
    // Plant Foliage Clusters (Spherical Topiary Leaves)
    Material leafMat;
    leafMat.color = Vec3(0.15f, 0.65f, 0.22f);
    leafMat.shininess = 32.0f;
    drawSphere(Vec3(plantX, 1.05f, plantZ), Vec3(0.28f, 0.28f, 0.28f), leafMat);
    drawSphere(Vec3(plantX, 0.88f, plantZ + 0.12f), Vec3(0.24f, 0.24f, 0.24f), leafMat);
    drawSphere(Vec3(plantX, 0.88f, plantZ - 0.12f), Vec3(0.24f, 0.24f, 0.24f), leafMat);
    drawSphere(Vec3(plantX - 0.12f, 0.88f, plantZ), Vec3(0.24f, 0.24f, 0.24f), leafMat);
    drawSphere(Vec3(plantX + 0.12f, 0.88f, plantZ), Vec3(0.24f, 0.24f, 0.24f), leafMat);

    // Object C: Modern Room Bench / Waiting Table (Far left against wall)
    Material benchWoodMat;
    benchWoodMat.color = Vec3(0.46f, 0.30f, 0.20f);
    benchWoodMat.shininess = 32.0f;
    // Bench Top
    drawCube(Vec3(-2.10f, 0.48f, 0.6f), Vec3(0.60f, 0.05f, 1.40f), benchWoodMat);
    // 4 Bench Steel Legs
    drawCylinder(Vec3(-2.30f, 0.24f, 0.05f), Vec3(0.04f, 0.48f, 0.04f), chromeMat);
    drawCylinder(Vec3(-1.90f, 0.24f, 0.05f), Vec3(0.04f, 0.48f, 0.04f), chromeMat);
    drawCylinder(Vec3(-2.30f, 0.24f, 1.15f), Vec3(0.04f, 0.48f, 0.04f), chromeMat);
    drawCylinder(Vec3(-1.90f, 0.24f, 1.15f), Vec3(0.04f, 0.48f, 0.04f), chromeMat);

    // -------------------------------------------------------------------------
    // 3. VENDING MACHINE MAIN CABINET (Built with basic shape transformations)
    // -------------------------------------------------------------------------
    // Dimensions: Width: 1.06m, Height: 2.05m, Depth: 0.68m
    // Outer Shell
    drawCube(Vec3(0.0f, 1.10f, -0.32f), Vec3(1.06f, 2.05f, 0.04f), cabinetBodyMat); // Back
    drawCube(Vec3(-0.51f, 1.10f, 0.0f), Vec3(0.04f, 2.05f, 0.68f), cabinetBodyMat); // Left
    drawCube(Vec3(0.51f, 1.10f, 0.0f), Vec3(0.04f, 2.05f, 0.68f), cabinetBodyMat);  // Right
    drawCube(Vec3(0.0f, 2.10f, 0.0f), Vec3(1.06f, 0.05f, 0.68f), cabinetBodyMat);   // Top
    drawCube(Vec3(0.0f, 0.10f, 0.0f), Vec3(1.06f, 0.05f, 0.68f), cabinetBodyMat);   // Bottom Base

    // 4 Adjustable Chrome Leveling Legs
    float legX = 0.45f;
    float legZ = 0.26f;
    drawCylinder(Vec3(-legX, 0.04f, -legZ), Vec3(0.05f, 0.08f, 0.05f), chromeMat);
    drawCylinder(Vec3( legX, 0.04f, -legZ), Vec3(0.05f, 0.08f, 0.05f), chromeMat);
    drawCylinder(Vec3(-legX, 0.04f,  legZ), Vec3(0.05f, 0.08f, 0.05f), chromeMat);
    drawCylinder(Vec3( legX, 0.04f,  legZ), Vec3(0.05f, 0.08f, 0.05f), chromeMat);

    // Rubber Foot Pads
    drawCylinder(Vec3(-legX, 0.008f, -legZ), Vec3(0.065f, 0.016f, 0.065f), rubberMat);
    drawCylinder(Vec3( legX, 0.008f, -legZ), Vec3(0.065f, 0.016f, 0.065f), rubberMat);
    drawCylinder(Vec3(-legX, 0.008f,  legZ), Vec3(0.065f, 0.016f, 0.065f), rubberMat);
    drawCylinder(Vec3( legX, 0.008f,  legZ), Vec3(0.065f, 0.016f, 0.065f), rubberMat);

    // -------------------------------------------------------------------------
    // 4. TOP ILLUMINATED MARQUEE HEADER
    // -------------------------------------------------------------------------
    // Header Enclosure Frame
    drawCube(Vec3(0.0f, 1.95f, 0.32f), Vec3(1.04f, 0.22f, 0.04f), cabinetAccentMat);
    // Glowing Acrylic Marquee Panel
    Material marqueeMat;
    marqueeMat.color = Vec3(0.95f, 0.95f, 0.98f);
    marqueeMat.emission = g_cabinetPointLight.enabled ? Vec3(0.85f, 0.90f, 1.0f) : Vec3(0.1f, 0.1f, 0.1f);
    drawCube(Vec3(-0.15f, 1.95f, 0.342f), Vec3(0.68f, 0.16f, 0.01f), marqueeMat);

    // Marquee Brand Accent Strip
    Material brandAccentMat;
    brandAccentMat.color = Vec3(0.90f, 0.22f, 0.15f);
    brandAccentMat.emission = g_cabinetPointLight.enabled ? Vec3(0.7f, 0.15f, 0.08f) : Vec3(0, 0, 0);
    drawCube(Vec3(-0.15f, 2.01f, 0.345f), Vec3(0.64f, 0.02f, 0.008f), brandAccentMat);

    // -------------------------------------------------------------------------
    // 5. INTERIOR DISPLAY CAVITY & PRODUCT SHELVES
    // -------------------------------------------------------------------------
    // Interior Cavity Walls
    drawCube(Vec3(-0.15f, 1.25f, -0.29f), Vec3(0.68f, 1.18f, 0.02f), interiorWallMat); // Cavity Back
    drawCube(Vec3(-0.485f, 1.25f, 0.0f),  Vec3(0.01f, 1.18f, 0.58f), interiorWallMat); // Cavity Left
    drawCube(Vec3( 0.19f, 1.25f, 0.0f),   Vec3(0.02f, 1.18f, 0.58f), cabinetAccentMat);// Cavity Divider
    drawCube(Vec3(-0.15f, 1.83f, 0.0f),   Vec3(0.68f, 0.02f, 0.58f), interiorWallMat); // Cavity Roof

    // Interior Fluorescent Lamp Fixture (Light Source 2 Indicator Mesh)
    Material tubeMat;
    tubeMat.color = Vec3(1.0f, 1.0f, 1.0f);
    tubeMat.emission = g_cabinetPointLight.enabled ? Vec3(2.0f, 2.0f, 2.2f) : Vec3(0.05f, 0.05f, 0.05f);
    drawCylinder(Vec3(-0.15f, 1.82f, 0.05f), Vec3(0.025f, 0.62f, 0.025f), tubeMat, Vec3(0, 0, 90.0f));

    // 4 Stock Shelves + Price Label Rails + Rotating Helical Coils
    // Note: Shelves are positioned at SHELF_CENTER_Z with SHELF_DEPTH_Z, leaving 19cm vertical drop space in front
    for (int row = 0; row < 4; ++row) {
        float y = SHELF_Y[row];
        // Shelf Tray Plane
        drawCube(Vec3(-0.15f, y, SHELF_CENTER_Z), Vec3(0.66f, 0.018f, SHELF_DEPTH_Z), shelfMat);
        // Shelf Front Rail (Green Price Tag Strip)
        Material railMat;
        railMat.color = Vec3(0.12f, 0.62f, 0.38f);
        drawCube(Vec3(-0.15f, y, SHELF_FRONT_Z), Vec3(0.66f, 0.024f, 0.015f), railMat);

        // Price Tag Stickers for Each Slot
        Material tagMat;
        tagMat.color = Vec3(0.96f, 0.96f, 0.96f);
        tagMat.emission = g_cabinetPointLight.enabled ? Vec3(0.2f, 0.2f, 0.2f) : Vec3(0, 0, 0);
        for (int col = 0; col < 3; ++col) {
            drawCube(Vec3(SLOT_X[col], y, SHELF_FRONT_Z + 0.008f), Vec3(0.06f, 0.018f, 0.002f), tagMat);
        }

        // Helical Wire Spirals (Product Spirals - rotate when dispensing)
        for (int col = 0; col < 3; ++col) {
            int slotIdx = row * 3 + col;
            float angle = g_slots[slotIdx].spiralAngle;
            drawCoil(Vec3(SLOT_X[col], y + 0.068f, SHELF_CENTER_Z), Vec3(1.0f, 1.0f, 0.85f), coilMat, angle);
        }
    }

    // -------------------------------------------------------------------------
    // 6. STOCKED PRODUCTS (Simulated & Animated)
    // -------------------------------------------------------------------------
    for (const auto& s : g_slots) {
        if (s.state == STATE_DISPENSED_GONE) {
            continue; // Item has been dispensed and collected - disappeared forever until restocked!
        }
        switch (s.type) {
            case PROD_CAN_COLA:
            case PROD_CAN_LIME:
            case PROD_CAN_ENERGY:
            case PROD_CAN_ORANGE:
                drawSodaCan(s.currentPos, s.currentRot, s.primaryColor, s.secondaryColor);
                break;
            case PROD_BOTTLE_WATER:
            case PROD_BOTTLE_JUICE:
                drawBottle(s.currentPos, s.currentRot, s.primaryColor, s.secondaryColor);
                break;
            case PROD_CHIPS_BBQ:
            case PROD_CHIPS_CREAM:
            case PROD_CHIPS_NACHO:
                drawSnackBag(s.currentPos, s.currentRot, s.primaryColor, s.secondaryColor);
                break;
            case PROD_CHOCOLATE:
            case PROD_CANDY_BAR:
            case PROD_COOKIES:
                drawCandyBar(s.currentPos, s.currentRot, s.primaryColor, s.secondaryColor);
                break;
        }
    }

    // -------------------------------------------------------------------------
    // 7. CONTROL & PAYMENT PANEL (Right Column)
    // -------------------------------------------------------------------------
    // Column Backing Frame
    drawCube(Vec3(0.35f, 1.10f, 0.32f), Vec3(0.30f, 1.48f, 0.04f), cabinetAccentMat);

    // Multi-color Machine Status Indicator LEDs
    bool isAnyVending = false;
    for (const auto& s : g_slots) {
        if (s.state == STATE_SPIRAL_TURNING || s.state == STATE_FALLING_CHUTE) {
            isAnyVending = true;
            break;
        }
    }

    // Ready Green LED
    Material readyLedMat;
    readyLedMat.color = Vec3(0.1f, 0.8f, 0.2f);
    readyLedMat.emission = (!isAnyVending) ? Vec3(0.2f, 1.6f, 0.3f) : Vec3(0.02f, 0.1f, 0.02f);
    drawSphere(Vec3(0.25f, 1.70f, 0.345f), Vec3(0.022f, 0.022f, 0.022f), readyLedMat);

    // Busy / Vending Amber LED
    Material busyLedMat;
    busyLedMat.color = Vec3(0.95f, 0.65f, 0.1f);
    busyLedMat.emission = (isAnyVending) ? Vec3(1.8f, 1.1f, 0.15f) : Vec3(0.08f, 0.04f, 0.01f);
    drawSphere(Vec3(0.35f, 1.70f, 0.345f), Vec3(0.022f, 0.022f, 0.022f), busyLedMat);

    // Pickup / Door Blue LED
    Material doorLedMat;
    doorLedMat.color = Vec3(0.2f, 0.6f, 1.0f);
    doorLedMat.emission = (g_trayIndicatorLight.enabled) ? Vec3(0.3f, 1.2f, 2.0f) : Vec3(0.02f, 0.05f, 0.1f);
    drawSphere(Vec3(0.45f, 1.70f, 0.345f), Vec3(0.022f, 0.022f, 0.022f), doorLedMat);

    // Digital LED Screen Frame
    Material screenFrameMat;
    screenFrameMat.color = Vec3(0.04f, 0.04f, 0.05f);
    drawCube(Vec3(0.35f, 1.61f, 0.342f), Vec3(0.25f, 0.09f, 0.015f), screenFrameMat);

    // Digital LED Status Screen Glass
    Material screenMat;
    screenMat.color = Vec3(0.02f, 0.15f, 0.25f);
    screenMat.emission = isAnyVending ? Vec3(0.1f, 0.75f, 0.4f) : Vec3(0.04f, 0.60f, 0.85f);
    drawCube(Vec3(0.35f, 1.61f, 0.351f), Vec3(0.23f, 0.07f, 0.005f), screenMat);

    // Interactive Keypad (Keys 1-9, 0, Cancel, Enter with physical depression)
    Material keypadPlateMat;
    keypadPlateMat.color = Vec3(0.12f, 0.13f, 0.16f);
    drawCube(Vec3(0.35f, 1.40f, 0.342f), Vec3(0.22f, 0.25f, 0.01f), keypadPlateMat);

    for (const auto& btn : g_keypadButtons) {
        Vec3 btnPos = btn.localPos - Vec3(0.0f, 0.0f, btn.pressOffset);
        drawCube(btnPos, btn.size, btn.mat);
    }

    // Bill Insertion Validator (with glowing green acceptance slot)
    Material billSlotMat;
    billSlotMat.color = Vec3(0.08f, 0.08f, 0.09f);
    drawCube(Vec3(0.35f, 1.18f, 0.342f), Vec3(0.22f, 0.08f, 0.015f), billSlotMat);

    Material billGlowMat;
    billGlowMat.color = Vec3(0.15f, 0.85f, 0.25f);
    billGlowMat.emission = Vec3(0.25f, 1.4f, 0.35f);
    drawCube(Vec3(0.35f, 1.18f, 0.351f), Vec3(0.14f, 0.012f, 0.005f), billGlowMat);

    // Coin Slot & Return Lever
    drawCube(Vec3(0.35f, 1.05f, 0.342f), Vec3(0.08f, 0.09f, 0.015f), chromeMat);
    Material coinSlotMat;
    coinSlotMat.color = Vec3(0.02f, 0.02f, 0.02f);
    drawCube(Vec3(0.35f, 1.065f, 0.351f), Vec3(0.01f, 0.04f, 0.005f), coinSlotMat);

    // Contactless NFC / Card Tap Pad
    Material cardPadMat;
    cardPadMat.color = Vec3(0.15f, 0.18f, 0.24f);
    drawCube(Vec3(0.35f, 0.92f, 0.342f), Vec3(0.18f, 0.10f, 0.012f), cardPadMat);
    Material nfcGlowMat;
    nfcGlowMat.color = Vec3(0.25f, 0.70f, 0.95f);
    nfcGlowMat.emission = Vec3(0.1f, 0.35f, 0.6f);
    drawCube(Vec3(0.35f, 0.92f, 0.350f), Vec3(0.06f, 0.04f, 0.004f), nfcGlowMat);

    // Coin Return Pocket
    drawCube(Vec3(0.35f, 0.76f, 0.33f), Vec3(0.14f, 0.12f, 0.035f), cabinetBodyMat);
    drawCube(Vec3(0.35f, 0.76f, 0.348f), Vec3(0.10f, 0.08f, 0.005f), rubberMat);

    // -------------------------------------------------------------------------
    // 8. BOTTOM DISPENSER BAY & OPENING COLLECTION DOOR (Hierarchical Rotation)
    // -------------------------------------------------------------------------
    // Chute Chamber Interior Cavity
    Material chamberMat;
    chamberMat.color = Vec3(0.08f, 0.09f, 0.10f);
    chamberMat.shininess = 16.0f;
    drawCube(Vec3(-0.15f, 0.26f, 0.12f), Vec3(0.66f, 0.03f, 0.40f), chamberMat); // Chamber Floor
    drawCube(Vec3(-0.15f, 0.45f, -0.06f), Vec3(0.66f, 0.36f, 0.04f), chamberMat); // Chamber Back
    drawCube(Vec3(-0.47f, 0.45f, 0.12f), Vec3(0.03f, 0.36f, 0.40f), chamberMat); // Chamber Left
    drawCube(Vec3( 0.17f, 0.45f, 0.12f), Vec3(0.03f, 0.36f, 0.40f), chamberMat); // Chamber Right

    // Chute Exterior Border Frame
    drawCube(Vec3(-0.15f, 0.62f, 0.32f), Vec3(0.68f, 0.06f, 0.04f), cabinetAccentMat);
    drawCube(Vec3(-0.15f, 0.26f, 0.32f), Vec3(0.68f, 0.06f, 0.04f), cabinetAccentMat);
    drawCube(Vec3(-0.47f, 0.44f, 0.32f), Vec3(0.04f, 0.30f, 0.04f), cabinetAccentMat);
    drawCube(Vec3( 0.17f, 0.44f, 0.32f), Vec3(0.04f, 0.30f, 0.04f), cabinetAccentMat);

    // Collection Chute Interior Indicator Light Fixture
    Material trayLampMat;
    trayLampMat.color = Vec3(0.2f, 0.7f, 1.0f);
    trayLampMat.emission = g_trayIndicatorLight.enabled ? Vec3(0.5f, 1.5f, 2.2f) : Vec3(0, 0, 0);
    drawCylinder(Vec3(-0.15f, 0.58f, 0.15f), Vec3(0.02f, 0.40f, 0.02f), trayLampMat, Vec3(0, 0, 90.0f));

    // Opening Collection Door:
    // Hierarchical transformation rotating around the top hinge axis!
    // Top hinge remains locked at (hingeY, hingeZ) while the bottom swings inward into the machine!
    float hingeY = 0.59f;
    float hingeZ = 0.32f;
    float flapH  = 0.27f;

    Material doorFlapMat;
    doorFlapMat.color = Vec3(0.20f, 0.22f, 0.26f);
    doorFlapMat.shininess = 32.0f;
    doorFlapMat.specularStrength = 0.5f;

    Material pushPlateMat;
    pushPlateMat.color = Vec3(0.92f, 0.92f, 0.96f);
    pushPlateMat.shininess = 32.0f;

    // 1. Move to the stationary top hinge
    Mat4 hingeMat = Mat4::Identity();
    hingeMat = translate(hingeMat, Vec3(-0.15f, hingeY, hingeZ));

    // 2. Rotate around the top hinge (+X axis).
    // Rotating by +angle swings the bottom of the flap inward into the machine (-Z direction)
    // while the top of the flap remains anchored at the hinge!
    hingeMat = rotate(hingeMat, toRadians(g_doorFlapAngle), Vec3(1.0f, 0.0f, 0.0f));

    // 3. Flap body extends downward by flapH from the hinge
    Mat4 flapBodyM = translate(hingeMat, Vec3(0.0f, -flapH * 0.5f, 0.0f));
    flapBodyM = scale(flapBodyM, Vec3(0.58f, flapH, 0.015f));
    drawShape(g_meshCube, flapBodyM, doorFlapMat);

    // 4. "PUSH" text plate attached rigidly to the front face of the moving flap
    Mat4 badgeM = translate(hingeMat, Vec3(0.0f, -flapH * 0.5f, 0.009f));
    badgeM = scale(badgeM, Vec3(0.22f, 0.065f, 0.005f));
    drawShape(g_meshCube, badgeM, pushPlateMat);

    // -------------------------------------------------------------------------
    // 9. FRONT PROTECTIVE GLASS WINDOW (Semi-Transparent Blend Pass)
    // -------------------------------------------------------------------------
    // Outer Glass Window Trim Frames
    drawCube(Vec3(-0.15f, 1.83f, 0.32f),  Vec3(0.68f, 0.03f, 0.04f), cabinetAccentMat);
    drawCube(Vec3(-0.15f, 0.67f, 0.32f),  Vec3(0.68f, 0.03f, 0.04f), cabinetAccentMat);
    drawCube(Vec3(-0.485f, 1.25f, 0.32f), Vec3(0.03f, 1.15f, 0.04f), cabinetAccentMat);
    drawCube(Vec3( 0.185f, 1.25f, 0.32f), Vec3(0.03f, 1.15f, 0.04f), cabinetAccentMat);

    // Transparent Glass Pane
    Material glassMat;
    glassMat.color = Vec3(0.85f, 0.92f, 0.98f);
    glassMat.shininess = 128.0f;
    glassMat.specularStrength = 1.0f;
    glassMat.alpha = 0.28f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawCube(Vec3(-0.15f, 1.25f, 0.33f), Vec3(0.64f, 1.12f, 0.008f), glassMat);
    glDisable(GL_BLEND);
}

// -----------------------------------------------------------------------------
// GLFW Input Callbacks
// -----------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_camera.isDraggingLeft = true;
            glfwGetCursorPos(window, &g_camera.lastMouseX, &g_camera.lastMouseY);
        } else if (action == GLFW_RELEASE) {
            g_camera.isDraggingLeft = false;
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            g_camera.isDraggingRight = true;
            glfwGetCursorPos(window, &g_camera.lastMouseX, &g_camera.lastMouseY);
        } else if (action == GLFW_RELEASE) {
            g_camera.isDraggingRight = false;
        }
    }
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    double dx = xpos - g_camera.lastMouseX;
    double dy = ypos - g_camera.lastMouseY;
    g_camera.lastMouseX = xpos;
    g_camera.lastMouseY = ypos;

    if (g_camera.isDraggingLeft) {
        // Orbit Camera
        g_camera.yaw += (float)dx * 0.35f;
        g_camera.pitch += (float)dy * 0.35f;
        if (g_camera.pitch > 85.0f)  g_camera.pitch = 85.0f;
        if (g_camera.pitch < -10.0f) g_camera.pitch = -10.0f;
    } else if (g_camera.isDraggingRight) {
        // Pan Camera Target
        float radYaw = toRadians(g_camera.yaw);
        Vec3 right = normalize(Vec3(std::cos(radYaw), 0.0f, -std::sin(radYaw)));
        Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
        g_camera.target -= right * ((float)dx * 0.003f);
        g_camera.target += up * ((float)dy * 0.003f);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;
    g_camera.distance -= (float)yoffset * 0.25f;
    if (g_camera.distance < 0.8f)  g_camera.distance = 0.8f;
    if (g_camera.distance > 12.0f) g_camera.distance = 12.0f;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    (void)mods;
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, true);
        }
        // Product Selection Keys [1] - [9]
        else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
            int idx = key - GLFW_KEY_1;
            selectAndVendItem(idx);
        }
        // '0' Key for 10th slot (Chocolate Bar)
        else if (key == GLFW_KEY_0) {
            selectAndVendItem(9);
        }
        // SPACE: Vend random available item
        else if (key == GLFW_KEY_SPACE) {
            static int randIdx = 0;
            int startIdx = randIdx;
            bool found = false;
            for (int i = 0; i < (int)g_slots.size(); ++i) {
                int checkIdx = (startIdx + i) % (int)g_slots.size();
                if (g_slots[checkIdx].state == STATE_IDLE_ON_SHELF) {
                    randIdx = (checkIdx + 1) % (int)g_slots.size();
                    selectAndVendItem(checkIdx);
                    found = true;
                    break;
                }
            }
            if (!found) {
                g_statusMessage = "ALL SOLD OUT - PRESS [L] TO LOAD ALL";
                g_statusTimer = 3.5f;
                std::cout << "[VENDING] All items sold out! Press [L] or [ENTER] to reload all." << std::endl;
            }
        }
        // [L] or [ENTER]: Load / Restock all items back onto shelves
        else if (key == GLFW_KEY_L || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            restockAllItems();
        }
        // [O]: Toggle Collection Door Push / Open & collect resting item
        else if (key == GLFW_KEY_O) {
            g_isDoorManuallyOpened = !g_isDoorManuallyOpened;
            for (auto& s : g_slots) {
                if (s.state == STATE_RESTING_IN_TRAY) {
                    s.state = STATE_DISPENSED_GONE;
                    g_statusMessage = "COLLECTED [" + s.code + "] " + s.name + " - ENJOY!";
                    g_statusTimer = 3.0f;
                    std::cout << "[TAKE] Collected [" << s.code << "] " << s.name << " from delivery tray!" << std::endl;
                }
            }
            std::cout << "[DOOR] Collection Door " << (g_isDoorManuallyOpened ? "OPENED" : "CLOSED") << std::endl;
        }
        // [D] or [F1]: Toggle Directional Light (Room Sunlight)
        else if (key == GLFW_KEY_D || key == GLFW_KEY_F1) {
            g_dirLight.enabled = !g_dirLight.enabled;
            std::cout << "[LIGHT 1] Directional Light: " << (g_dirLight.enabled ? "ON" : "OFF") << std::endl;
        }
        // [P] or [F2]: Toggle Point Light (Vending Cabinet Interior)
        else if (key == GLFW_KEY_P || key == GLFW_KEY_F2) {
            g_cabinetPointLight.enabled = !g_cabinetPointLight.enabled;
            std::cout << "[LIGHT 2] Point Light (Cabinet): " << (g_cabinetPointLight.enabled ? "ON" : "OFF") << std::endl;
        }
        // [S] or [F3]: Toggle Spot Light (Ceiling Spotlight)
        else if (key == GLFW_KEY_S || key == GLFW_KEY_F3) {
            g_ceilingSpotLight.enabled = !g_ceilingSpotLight.enabled;
            std::cout << "[LIGHT 3] Spot Light (Ceiling): " << (g_ceilingSpotLight.enabled ? "ON" : "OFF") << std::endl;
        }
        // [V]: Cycle Preset Camera Views
        else if (key == GLFW_KEY_V) {
            g_camera.cycleNextPreset();
            std::cout << "[CAMERA] View changed to: " << g_camera.getPresetName() << std::endl;
        }
        // [C]: Toggle Turntable Auto-Rotation
        else if (key == GLFW_KEY_C) {
            g_camera.autoRotate = !g_camera.autoRotate;
            std::cout << "[CAMERA] Auto-rotation: " << (g_camera.autoRotate ? "ON" : "OFF") << std::endl;
        }
        // [R]: Reset Camera
        else if (key == GLFW_KEY_R) {
            g_camera.reset();
            std::cout << "[CAMERA] Reset to Room Overview" << std::endl;
        }
    }
}

// -----------------------------------------------------------------------------
// Uniform Setup Helper (Uploads 3 Types of Lights and Camera to Shader)
// -----------------------------------------------------------------------------
void uploadLightUniforms(const Mat4& viewMat, const Mat4& projMat) {
    glUseProgram(g_shaderProgram);

    // Matrix uniforms
    glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "view"), 1, GL_FALSE, viewMat.m);
    glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "projection"), 1, GL_FALSE, projMat.m);

    // Camera view position
    Vec3 camPos = g_camera.getPosition();
    glUniform3f(glGetUniformLocation(g_shaderProgram, "viewPos"), camPos.x, camPos.y, camPos.z);

    // 1. Directional Light
    glUniform3f(glGetUniformLocation(g_shaderProgram, "dirLight.direction"), g_dirLight.direction.x, g_dirLight.direction.y, g_dirLight.direction.z);
    glUniform3f(glGetUniformLocation(g_shaderProgram, "dirLight.color"), g_dirLight.color.x, g_dirLight.color.y, g_dirLight.color.z);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "dirLight.intensity"), g_dirLight.intensity);
    glUniform1i(glGetUniformLocation(g_shaderProgram, "dirLight.enabled"), g_dirLight.enabled ? 1 : 0);

    // 2. Point Light (Vending Machine Cabinet)
    glUniform3f(glGetUniformLocation(g_shaderProgram, "pointLight.position"), g_cabinetPointLight.position.x, g_cabinetPointLight.position.y, g_cabinetPointLight.position.z);
    glUniform3f(glGetUniformLocation(g_shaderProgram, "pointLight.color"), g_cabinetPointLight.color.x, g_cabinetPointLight.color.y, g_cabinetPointLight.color.z);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "pointLight.intensity"), g_cabinetPointLight.intensity);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "pointLight.constant"), g_cabinetPointLight.constant);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "pointLight.linear"), g_cabinetPointLight.linear);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "pointLight.quadratic"), g_cabinetPointLight.quadratic);
    glUniform1i(glGetUniformLocation(g_shaderProgram, "pointLight.enabled"), g_cabinetPointLight.enabled ? 1 : 0);

    // Tray Indicator Light (Point Light)
    glUniform3f(glGetUniformLocation(g_shaderProgram, "trayLight.position"), g_trayIndicatorLight.position.x, g_trayIndicatorLight.position.y, g_trayIndicatorLight.position.z);
    glUniform3f(glGetUniformLocation(g_shaderProgram, "trayLight.color"), g_trayIndicatorLight.color.x, g_trayIndicatorLight.color.y, g_trayIndicatorLight.color.z);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "trayLight.intensity"), g_trayIndicatorLight.intensity);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "trayLight.constant"), g_trayIndicatorLight.constant);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "trayLight.linear"), g_trayIndicatorLight.linear);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "trayLight.quadratic"), g_trayIndicatorLight.quadratic);
    glUniform1i(glGetUniformLocation(g_shaderProgram, "trayLight.enabled"), g_trayIndicatorLight.enabled ? 1 : 0);

    // 3. Spot Light (Ceiling Track Spotlight)
    glUniform3f(glGetUniformLocation(g_shaderProgram, "spotLight.position"), g_ceilingSpotLight.position.x, g_ceilingSpotLight.position.y, g_ceilingSpotLight.position.z);
    glUniform3f(glGetUniformLocation(g_shaderProgram, "spotLight.direction"), g_ceilingSpotLight.direction.x, g_ceilingSpotLight.direction.y, g_ceilingSpotLight.direction.z);
    glUniform3f(glGetUniformLocation(g_shaderProgram, "spotLight.color"), g_ceilingSpotLight.color.x, g_ceilingSpotLight.color.y, g_ceilingSpotLight.color.z);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "spotLight.intensity"), g_ceilingSpotLight.intensity);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "spotLight.cutOff"), g_ceilingSpotLight.cutOff);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "spotLight.outerCutOff"), g_ceilingSpotLight.outerCutOff);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "spotLight.constant"), g_ceilingSpotLight.constant);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "spotLight.linear"), g_ceilingSpotLight.linear);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "spotLight.quadratic"), g_ceilingSpotLight.quadratic);
    glUniform1i(glGetUniformLocation(g_shaderProgram, "spotLight.enabled"), g_ceilingSpotLight.enabled ? 1 : 0);
}

// -----------------------------------------------------------------------------
// Application Entry Point
// -----------------------------------------------------------------------------
int main() {
    std::cout << "====================================================================\n";
    std::cout << "        INTERACTIVE 3D VENDING MACHINE - OPENGL ASSIGNMENT         \n";
    std::cout << "  Student Name: Md. Nayeem | Roll: 2107050                          \n";
    std::cout << "====================================================================\n";
    std::cout << "CONTROLS & INTERACTION:\n";
    std::cout << "  [1] - [9] / [0] : Select Product & Trigger Dispensing Spiral\n";
    std::cout << "  [SPACE]         : Vend Random Item\n";
    std::cout << "  [L] / [ENTER]   : Load / Restock All Products Onto Shelves\n";
    std::cout << "  [O]             : Toggle Open / Close Collection Door (Collect Item)\n";
    std::cout << "  [D] / [F1]      : Toggle Light Type 1 (Directional Room Sunlight)\n";
    std::cout << "  [P] / [F2]      : Toggle Light Type 2 (Point Cabinet Tube Lamp)\n";
    std::cout << "  [S] / [F3]      : Toggle Light Type 3 (Ceiling Track Spot Light)\n";
    std::cout << "  [V]             : Cycle Camera Views (Room / Front / Shelves / Keypad / Tray)\n";
    std::cout << "  [C]             : Toggle Turntable Auto-Rotation\n";
    std::cout << "  [R]             : Reset Camera to Overview\n";
    std::cout << "  Mouse Left-Drag : Orbit Camera\n";
    std::cout << "  Mouse Right-Drag: Pan Camera Target\n";
    std::cout << "  Mouse Scroll    : Zoom In / Out\n";
    std::cout << "  [ESC]           : Exit Application\n";
    std::cout << "====================================================================\n\n";

    // 1. Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // 4x Antialiasing

    // 2. Create Window
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Interactive 3D Vending Machine - Md. Nayeem (2107050)", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    // 3. Initialize GLAD
    int version = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
    if (!version) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL Version: " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version) << std::endl;
    std::cout << "Renderer:       " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "GLSL Version:   " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl << std::endl;

    // OpenGL State
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_MULTISAMPLE);
    // Disable face culling so cylinders, thin flaps, and curved surfaces are completely solid from all angles
    glDisable(GL_CULL_FACE);

    // 4. Build Shaders and Basic Geometric Meshes
    g_shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    g_meshCube = createCubeMesh();
    g_meshCylinder = createCylinderMesh(32);
    g_meshSphere = createSphereMesh(20, 24);
    g_meshCoil = createCoilMesh(5, 24, 0.055f, 0.005f, 0.22f);
    g_meshPlane = createPlaneMesh();

    // 5. Initialize Interactive Scene State
    initKeypadButtons();
    initProductSlots();

    // Timing Variables
    double lastTime = glfwGetTime();
    double titleTimer = 0.0;
    int frameCount = 0;

    // 6. Main Application Loop
    while (!glfwWindowShouldClose(window)) {
        double frameStartTime = glfwGetTime();
        float dt = static_cast<float>(frameStartTime - lastTime);
        lastTime = frameStartTime;
        if (dt > 0.05f) dt = 0.05f; // Prevent leap on window focus lag

        // Physics & Animation
        updateSimulation(dt);

        // Update Window Title with live diagnostics
        titleTimer += dt;
        frameCount++;
        if (titleTimer >= 0.25) {
            int fps = (int)(frameCount / titleTimer);
            std::string lightsStatus = std::string("Lights[Dir:") + (g_dirLight.enabled ? "ON" : "off") +
                                       "|Pt:" + (g_cabinetPointLight.enabled ? "ON" : "off") +
                                       "|Spot:" + (g_ceilingSpotLight.enabled ? "ON" : "off") + "]";
            std::string doorStatus = g_doorFlapAngle > 5.0f ? "Door:OPEN" : "Door:Closed";
            std::string title = "Vending 3D | " + g_statusMessage + " | " + lightsStatus + " | " + doorStatus + " | FPS: " + std::to_string(fps);
            glfwSetWindowTitle(window, title.c_str());
            titleTimer = 0.0;
            frameCount = 0;
        }

        // Viewport and Projection Setup
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);

        // Atmospheric studio background color
        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (displayH > 0) ? ((float)displayW / (float)displayH) : 1.777f;
        Mat4 projectionMat = perspective(toRadians(g_camera.fov), aspect, 0.1f, 100.0f);
        Mat4 viewMat = g_camera.getViewMatrix();

        // Upload lighting uniforms and camera transforms
        uploadLightUniforms(viewMat, projectionMat);

        // Render entire scene
        renderScene();

        // Swap buffers and poll input
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Cap Maximum Framerate to 100 FPS
        double frameEndTime = glfwGetTime();
        double frameDuration = frameEndTime - frameStartTime;
        if (frameDuration < TARGET_FRAME_TIME) {
            double sleepSeconds = TARGET_FRAME_TIME - frameDuration;
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepSeconds));
        }
    }

    // 7. Cleanup Resources
    g_meshCube.cleanup();
    g_meshCylinder.cleanup();
    g_meshSphere.cleanup();
    g_meshCoil.cleanup();
    g_meshPlane.cleanup();
    if (g_shaderProgram) glDeleteProgram(g_shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
