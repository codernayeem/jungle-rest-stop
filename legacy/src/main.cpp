/**
 * ============================================================================
 * Academic Project: Grand 3D Vending Facility & Transit Waiting Station
 * Course: Image & Computer Graphics Lab
 * Student Name: Md. Nayeem
 * Roll Number:  2107050
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
const double TARGET_FRAME_TIME = 1.0 / TARGET_FPS;

// Camera & Player System
Camera     g_camera;
Player     g_player;
CameraMode g_cameraMode = MODE_FIRST_PERSON;

// Animation & Walking variables for avatar
float g_walkAnimTimer = 0.0f;
bool  g_isWalking = false;

// Global Basic Shape Meshes
Mesh g_meshCube;
Mesh g_meshCylinder;
Mesh g_meshSphere;
Mesh g_meshCoil;
Mesh g_meshPlane;

// Lighting Configurations (Multi-source balanced architectural lighting)
DirectionalLight g_dirLight;
PointLight       g_roomPointLights[4];
SpotLight        g_ceilingSpotLight;
TrayLight        g_trayIndicatorLight;
bool             g_cabinetLightsEnabled = true;

// Interactive Collection Door State
float g_doorFlapAngle = 0.0f;
float g_targetDoorFlapAngle = 0.0f;
float g_doorHoldTimer = 0.0f;
bool  g_isDoorManuallyOpened = false;

// Active machine types
enum ActiveMachineType {
    MACHINE_NONE = 0,
    MACHINE_VENDING_DRINKS = 1,
    MACHINE_VENDING_SNACKS = 2,
    MACHINE_VENDING_SWEETS = 3,
    MACHINE_ATM_1 = 4,
    MACHINE_ATM_2 = 5,
    MACHINE_TICKET_1 = 6,
    MACHINE_TICKET_2 = 7
};

int g_activeMachine = MACHINE_NONE;

// Interactive Machine State Timers & Feedback
float g_atmFeedbackTimer = 0.0f;
std::string g_atmFeedbackMsg = "";
float g_ticketFeedbackTimer = 0.0f;
std::string g_ticketFeedbackMsg = "";
float g_ticketSlideAnim = 0.0f;

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
    int machineId;          // 1: Drinks, 2: Snacks, 3: Sweets
    int shelfRow;
    int shelfCol;

    // Physics Animation State
    DispenseState state;
    float spiralAngle;      // degrees
    float forwardProgress;  // 0.0 to 1.0 along shelf
    Vec3  currentPos;
    Vec3  currentRot;
    Vec3  velocity;
    float trayTimer;
};

std::vector<ProductSlot> g_slots;

// Machine Status & Message Display
std::string g_statusMessage = "READY - WALK TO A MACHINE";
float g_statusTimer = 0.0f;

// Vending machine local shelf layout constants
const float SHELF_Y[4] = { 1.70f, 1.40f, 1.10f, 0.80f };
const float SLOT_X[3]  = { -0.36f, -0.18f, 0.00f };
const float SHELF_CENTER_Z = -0.05f;
const float SHELF_FRONT_Z  = 0.14f;
const float SHELF_DEPTH_Z  = 0.38f;
const float CHUTE_DROP_Z   = 0.20f;

// Vending Machine World Positions (Spaced along Left Wall of Room 1)
const Vec3 VENDING_POS_1 = Vec3(-8.20f, 0.0f,  4.50f); // Drinks
const Vec3 VENDING_POS_2 = Vec3(-8.20f, 0.0f,  1.00f); // Snacks
const Vec3 VENDING_POS_3 = Vec3(-8.20f, 0.0f, -2.50f); // Sweets
const float VENDING_ROTY = 90.0f; // Facing into Room 1 (+X direction)

// Tree placement coordinates (Organic mature trees across outdoor landscape)
struct TreePos {
    float x, z, scale, rot;
    int type; // 0 = Mature Oak, 1 = Tall Pine
};
std::vector<TreePos> g_trees;

void initTrees() {
    g_trees.clear();
    // Trees along other side of the road
    float roadTreesX[] = { -70.0f, -56.0f, -42.0f, -28.0f, -14.0f, 0.0f, 14.0f, 28.0f, 44.0f, 58.0f, 72.0f };
    for (int i = 0; i < 11; ++i) {
        float x = roadTreesX[i];
        float z = 24.5f + (std::sin(x * 0.22f) * 4.5f);
        float sc = 1.0f + std::abs(std::sin(x * 0.35f)) * 0.35f;
        g_trees.push_back({ x, z, sc, x * 15.0f, i % 2 });
    }
    // Trees in northern meadow
    float deepTreesX[] = { -65.0f, -48.0f, -30.0f, -12.0f, 6.0f, 24.0f, 42.0f, 60.0f };
    for (int i = 0; i < 8; ++i) {
        float x = deepTreesX[i];
        float z = 38.0f + (std::cos(x * 0.28f) * 6.0f);
        float sc = 1.15f + std::abs(std::cos(x * 0.4f)) * 0.4f;
        g_trees.push_back({ x, z, sc, x * 25.0f, (i + 1) % 2 });
    }
    // Trees behind facility (South meadow)
    float backTreesX[] = { -60.0f, -44.0f, -26.0f, -10.0f, 8.0f, 26.0f, 44.0f, 64.0f };
    for (int i = 0; i < 8; ++i) {
        float x = backTreesX[i];
        float z = -22.0f - (std::abs(std::sin(x * 0.18f)) * 8.0f);
        float sc = 1.1f + std::abs(std::sin(x * 0.5f)) * 0.4f;
        g_trees.push_back({ x, z, sc, x * 18.0f, i % 2 });
    }
    // Trees flanking building
    g_trees.push_back({ -18.0f,  3.0f, 1.25f, 45.0f, 0 });
    g_trees.push_back({ -18.0f, -5.0f, 1.15f, 90.0f, 1 });
    g_trees.push_back({  18.0f,  3.0f, 1.30f, 60.0f, 0 });
    g_trees.push_back({  18.0f, -5.0f, 1.20f, 110.0f, 1 });
}

// -----------------------------------------------------------------------------
// GLSL Shaders (Multi-source Balanced Lighting)
// -----------------------------------------------------------------------------
const char* vertexShaderSource = R"(
#version 330 core
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
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    gl_Position = projection * view * worldPos;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 objectColor;
uniform vec3 emissionColor;
uniform float shininess;
uniform float specularStrength;
uniform float alpha;

uniform vec3 viewPos;

struct DirLight {
    vec3 direction;
    vec3 color;
    float intensity;
    bool enabled;
};
uniform DirLight dirLight;

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    bool enabled;
};
uniform PointLight roomLights[4];

struct TrayLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    bool enabled;
};
uniform TrayLight trayLight;

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

vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);

    vec3 diffuse = diff * light.color * light.intensity * objectColor;
    vec3 specular = specularStrength * spec * light.color * light.intensity;
    return diffuse + specular;
}

vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(light.position - fragPos);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);

    vec3 diffuse = diff * light.color * light.intensity * objectColor * attenuation;
    vec3 specular = specularStrength * spec * light.color * light.intensity * attenuation;
    return diffuse + specular;
}

vec3 calcTrayLight(TrayLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(light.position - fragPos);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);

    vec3 diffuse = diff * light.color * light.intensity * objectColor * attenuation;
    vec3 specular = specularStrength * spec * light.color * light.intensity * attenuation;
    return diffuse + specular;
}

vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(light.position - fragPos);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

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

    // Balanced base ambient light
    vec3 ambient = 0.30 * objectColor;
    vec3 result = ambient;

    // Directional Sunlight
    result += calcDirLight(dirLight, norm, viewDir);

    // 4 Architectural Room Downlights
    for (int i = 0; i < 4; ++i) {
        result += calcPointLight(roomLights[i], norm, FragPos, viewDir);
    }

    // Delivery bay and track lights
    result += calcTrayLight(trayLight, norm, FragPos, viewDir);
    result += calcSpotLight(spotLight, norm, FragPos, viewDir);

    // Emissive glowing surfaces
    result += emissionColor;

    // Soft tone clamp to eliminate harsh white hotspot blowout
    result = clamp(result, 0.0, 1.30);

    FragColor = vec4(result, alpha);
}
)";

GLuint g_shaderProgram = 0;

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

GLuint createShaderProgram(const char* vsSource, const char* fsSource) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "[Link Error] " << infoLog << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// -----------------------------------------------------------------------------
// Unified Object Drawing Function
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

void drawCube(const Vec3& pos, const Vec3& sz, const Material& mat, const Vec3& rotDeg = Vec3(0,0,0)) {
    Mat4 m = Mat4::Identity();
    m = translate(m, pos);
    if (rotDeg.y != 0.0f) m = rotate(m, toRadians(rotDeg.y), Vec3(0, 1, 0));
    if (rotDeg.x != 0.0f) m = rotate(m, toRadians(rotDeg.x), Vec3(1, 0, 0));
    if (rotDeg.z != 0.0f) m = rotate(m, toRadians(rotDeg.z), Vec3(0, 0, 1));
    m = scale(m, sz);
    drawShape(g_meshCube, m, mat);
}

void drawCylinder(const Vec3& pos, const Vec3& sz, const Material& mat, const Vec3& rotDeg = Vec3(0,0,0)) {
    Mat4 m = Mat4::Identity();
    m = translate(m, pos);
    if (rotDeg.y != 0.0f) m = rotate(m, toRadians(rotDeg.y), Vec3(0, 1, 0));
    if (rotDeg.x != 0.0f) m = rotate(m, toRadians(rotDeg.x), Vec3(1, 0, 0));
    if (rotDeg.z != 0.0f) m = rotate(m, toRadians(rotDeg.z), Vec3(0, 0, 1));
    m = scale(m, sz);
    drawShape(g_meshCylinder, m, mat);
}

void drawSphere(const Vec3& pos, const Vec3& sz, const Material& mat, const Vec3& rotDeg = Vec3(0,0,0)) {
    Mat4 m = Mat4::Identity();
    m = translate(m, pos);
    if (rotDeg.y != 0.0f) m = rotate(m, toRadians(rotDeg.y), Vec3(0, 1, 0));
    if (rotDeg.x != 0.0f) m = rotate(m, toRadians(rotDeg.x), Vec3(1, 0, 0));
    if (rotDeg.z != 0.0f) m = rotate(m, toRadians(rotDeg.z), Vec3(0, 0, 1));
    m = scale(m, sz);
    drawShape(g_meshSphere, m, mat);
}

void drawCoil(const Vec3& pos, const Vec3& scaleVec, const Material& mat, float angleDeg, float rotY = 0.0f) {
    Mat4 m = Mat4::Identity();
    m = translate(m, pos);
    if (rotY != 0.0f) m = rotate(m, toRadians(rotY), Vec3(0, 1, 0));
    m = rotate(m, toRadians(angleDeg), Vec3(0, 0, 1));
    m = scale(m, scaleVec);
    drawShape(g_meshCoil, m, mat);
}

// -----------------------------------------------------------------------------
// Composite Products Constructed from Basic Shapes
// -----------------------------------------------------------------------------
inline Mat4 makeProductRoot(const Vec3& pos, const Vec3& rot) {
    Mat4 root = Mat4::Identity();
    root = translate(root, pos);
    if (rot.y != 0.0f) root = rotate(root, toRadians(rot.y), Vec3(0.0f, 1.0f, 0.0f));
    if (rot.x != 0.0f) root = rotate(root, toRadians(rot.x), Vec3(1.0f, 0.0f, 0.0f));
    if (rot.z != 0.0f) root = rotate(root, toRadians(rot.z), Vec3(0.0f, 0.0f, 1.0f));
    return root;
}

void drawSodaCan(const Vec3& pos, const Vec3& rot, const Vec3& mainCol, const Vec3& accentCol) {
    Mat4 root = makeProductRoot(pos, rot);

    Material bodyMat;
    bodyMat.color = mainCol;
    bodyMat.shininess = 64.0f;
    bodyMat.specularStrength = 0.85f;

    Material metalMat;
    metalMat.color = Vec3(0.85f, 0.86f, 0.90f);
    metalMat.shininess = 128.0f;
    metalMat.specularStrength = 0.95f;

    Material bandMat;
    bandMat.color = accentCol;
    bandMat.shininess = 64.0f;

    Mat4 bodyM = scale(root, Vec3(0.065f, 0.12f, 0.065f));
    drawShape(g_meshCylinder, bodyM, bodyMat);

    Mat4 topRimM = translate(root, Vec3(0.0f, 0.062f, 0.0f));
    topRimM = scale(topRimM, Vec3(0.060f, 0.008f, 0.060f));
    drawShape(g_meshCylinder, topRimM, metalMat);

    Mat4 botRimM = translate(root, Vec3(0.0f, -0.062f, 0.0f));
    botRimM = scale(botRimM, Vec3(0.060f, 0.008f, 0.060f));
    drawShape(g_meshCylinder, botRimM, metalMat);

    Mat4 bandM = scale(root, Vec3(0.066f, 0.035f, 0.066f));
    drawShape(g_meshCylinder, bandM, bandMat);
}

void drawBottle(const Vec3& pos, const Vec3& rot, const Vec3& liquidCol, const Vec3& capCol) {
    Mat4 root = makeProductRoot(pos, rot);

    Material plasticMat;
    plasticMat.color = liquidCol;
    plasticMat.shininess = 96.0f;
    plasticMat.specularStrength = 0.9f;

    Material capMat;
    capMat.color = capCol;

    Material labelMat;
    labelMat.color = Vec3(0.96f, 0.96f, 0.96f);

    Mat4 bodyM = translate(root, Vec3(0.0f, -0.015f, 0.0f));
    bodyM = scale(bodyM, Vec3(0.065f, 0.10f, 0.065f));
    drawShape(g_meshCylinder, bodyM, plasticMat);

    Mat4 neckM = translate(root, Vec3(0.0f, 0.052f, 0.0f));
    neckM = scale(neckM, Vec3(0.032f, 0.045f, 0.032f));
    drawShape(g_meshCylinder, neckM, plasticMat);

    Mat4 capM = translate(root, Vec3(0.0f, 0.076f, 0.0f));
    capM = scale(capM, Vec3(0.035f, 0.015f, 0.035f));
    drawShape(g_meshCylinder, capM, capMat);

    Mat4 labelM = translate(root, Vec3(0.0f, -0.015f, 0.0f));
    labelM = scale(labelM, Vec3(0.0655f, 0.04f, 0.0655f));
    drawShape(g_meshCylinder, labelM, labelMat);
}

void drawSnackBag(const Vec3& pos, const Vec3& rot, const Vec3& mainCol, const Vec3& secondaryCol) {
    Mat4 root = makeProductRoot(pos, rot);

    Material bagMat;
    bagMat.color = mainCol;
    bagMat.shininess = 64.0f;

    Material crimpMat;
    crimpMat.color = secondaryCol;

    Mat4 bodyM = scale(root, Vec3(0.095f, 0.13f, 0.045f));
    drawShape(g_meshCube, bodyM, bagMat);

    Mat4 topCrimpM = translate(root, Vec3(0.0f, 0.068f, 0.0f));
    topCrimpM = scale(topCrimpM, Vec3(0.098f, 0.01f, 0.012f));
    drawShape(g_meshCube, topCrimpM, crimpMat);

    Mat4 botCrimpM = translate(root, Vec3(0.0f, -0.068f, 0.0f));
    botCrimpM = scale(botCrimpM, Vec3(0.098f, 0.01f, 0.012f));
    drawShape(g_meshCube, botCrimpM, crimpMat);
}

void drawCandyBar(const Vec3& pos, const Vec3& rot, const Vec3& wrapperCol, const Vec3& accentCol) {
    Mat4 root = makeProductRoot(pos, rot);

    Material wrapMat;
    wrapMat.color = wrapperCol;
    wrapMat.shininess = 48.0f;

    Material accMat;
    accMat.color = accentCol;

    Mat4 wrapM = scale(root, Vec3(0.105f, 0.038f, 0.055f));
    drawShape(g_meshCube, wrapM, wrapMat);

    Mat4 bandM = scale(root, Vec3(0.055f, 0.040f, 0.057f));
    drawShape(g_meshCube, bandM, accMat);
}

// -----------------------------------------------------------------------------
// Initialize Product Slots Across 3 Vending Machines
// -----------------------------------------------------------------------------
void initProductSlots() {
    g_slots.clear();

    // Machine 1: Drinks Vending Machine (6 Products)
    g_slots.push_back({ "1", "Cola Classic",     1.50f, PROD_CAN_COLA,     Vec3(0.85f, 0.12f, 0.12f), Vec3(0.95f, 0.95f, 0.95f), 1, 0, 0, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "2", "Citrus Lime",      1.50f, PROD_CAN_LIME,     Vec3(0.15f, 0.82f, 0.25f), Vec3(0.95f, 0.95f, 0.15f), 1, 0, 1, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "3", "Energy Boost",     2.00f, PROD_CAN_ENERGY,   Vec3(0.10f, 0.40f, 0.90f), Vec3(0.10f, 0.90f, 0.95f), 1, 0, 2, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "4", "Pure Spring Water",1.25f, PROD_BOTTLE_WATER, Vec3(0.35f, 0.75f, 0.95f), Vec3(1.0f, 1.0f, 1.0f),  1, 1, 0, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "5", "Orange Soda",      1.50f, PROD_CAN_ORANGE,   Vec3(0.95f, 0.50f, 0.08f), Vec3(1.0f, 0.85f, 0.20f), 1, 1, 1, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "6", "Ruby Fruit Juice", 1.75f, PROD_BOTTLE_JUICE, Vec3(0.90f, 0.18f, 0.45f), Vec3(0.95f, 0.85f, 0.10f), 1, 1, 2, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });

    // Machine 2: Snacks & Crisps Vending Machine (6 Products)
    g_slots.push_back({ "1", "BBQ Potato Chips", 1.75f, PROD_CHIPS_BBQ,    Vec3(0.90f, 0.20f, 0.15f), Vec3(1.0f, 0.85f, 0.20f), 2, 0, 0, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "2", "Sour Cream Crisps",1.75f, PROD_CHIPS_CREAM,  Vec3(0.20f, 0.70f, 0.30f), Vec3(1.0f, 1.0f, 1.0f),  2, 0, 1, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "3", "Cheese Nachos",    1.80f, PROD_CHIPS_NACHO,  Vec3(0.95f, 0.75f, 0.10f), Vec3(0.85f, 0.15f, 0.15f), 2, 0, 2, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "4", "Zesty Tortilla",   1.80f, PROD_CHIPS_NACHO,  Vec3(0.95f, 0.65f, 0.10f), Vec3(0.85f, 0.20f, 0.20f), 2, 1, 0, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "5", "Chili Pepper BBQ", 1.75f, PROD_CHIPS_BBQ,    Vec3(0.85f, 0.15f, 0.15f), Vec3(1.0f, 0.90f, 0.20f), 2, 1, 1, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "6", "Onion & Chive Dip",1.75f, PROD_CHIPS_CREAM,  Vec3(0.18f, 0.65f, 0.28f), Vec3(1.0f, 1.0f, 1.0f),  2, 1, 2, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });

    // Machine 3: Sweets & Chocolates Vending Machine (6 Products)
    g_slots.push_back({ "1", "Choco Fudge Bar",  1.50f, PROD_CHOCOLATE,    Vec3(0.40f, 0.22f, 0.12f), Vec3(0.90f, 0.75f, 0.20f), 3, 0, 0, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "2", "Caramel Wafer",    1.35f, PROD_CANDY_BAR,    Vec3(0.85f, 0.10f, 0.20f), Vec3(0.95f, 0.95f, 0.95f), 3, 0, 1, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "3", "Cookies Delight",  1.60f, PROD_COOKIES,      Vec3(0.12f, 0.35f, 0.75f), Vec3(0.95f, 0.85f, 0.30f), 3, 0, 2, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "4", "Dark Truffle Bar", 1.65f, PROD_CHOCOLATE,    Vec3(0.28f, 0.15f, 0.10f), Vec3(0.95f, 0.80f, 0.20f), 3, 1, 0, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "5", "Vanilla Biscuit",  1.40f, PROD_COOKIES,      Vec3(0.20f, 0.40f, 0.70f), Vec3(0.95f, 0.90f, 0.35f), 3, 1, 1, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });
    g_slots.push_back({ "6", "Nougat Crunch",    1.45f, PROD_CANDY_BAR,    Vec3(0.80f, 0.12f, 0.18f), Vec3(0.90f, 0.85f, 0.85f), 3, 1, 2, STATE_IDLE_ON_SHELF, 0, 0, Vec3(), Vec3(), Vec3(), 0 });

    for (auto& s : g_slots) {
        Vec3 machineBase = (s.machineId == 1) ? VENDING_POS_1 : ((s.machineId == 2) ? VENDING_POS_2 : VENDING_POS_3);
        float localX = SLOT_X[s.shelfCol];
        float localY = SHELF_Y[s.shelfRow] + 0.08f;
        float localZ = SHELF_CENTER_Z;

        // Vending machine rotates +90 deg around Y: (localX, localY, localZ) -> (base.x + localZ, base.y + localY, base.z - localX)
        float worldX = machineBase.x + localZ;
        float worldY = machineBase.y + localY;
        float worldZ = machineBase.z - localX;

        s.currentPos = Vec3(worldX, worldY, worldZ);
        s.currentRot = Vec3(0, 90.0f, 0);
        s.state = STATE_IDLE_ON_SHELF;
        s.spiralAngle = 0.0f;
        s.forwardProgress = 0.0f;
    }
}

void restockAllItems() {
    initProductSlots();
    g_doorHoldTimer = 0.0f;
    g_targetDoorFlapAngle = 0.0f;
    g_trayIndicatorLight.enabled = false;
    g_statusMessage = "RESTOCKED - ALL 3 VENDING MACHINES LOADED";
    g_statusTimer = 3.5f;
    std::cout << "[RESTOCK] All items reloaded into Vending Machines! Ready to vend." << std::endl;
}

void selectAndVendItem(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= (int)g_slots.size()) return;
    ProductSlot& s = g_slots[slotIndex];

    if (s.state == STATE_DISPENSED_GONE) {
        g_statusMessage = "OUT OF STOCK [" + s.name + "] - PRESS [L] TO RESTOCK";
        g_statusTimer = 3.0f;
        std::cout << "[VENDING] Slot (" << s.name << ") is OUT OF STOCK! Press [L] or [ENTER] to reload." << std::endl;
        return;
    }

    if (s.state == STATE_SPIRAL_TURNING || s.state == STATE_FALLING_CHUTE || s.state == STATE_RESTING_IN_TRAY) {
        return;
    }

    s.state = STATE_SPIRAL_TURNING;
    s.forwardProgress = 0.0f;
    s.velocity = Vec3(0, 0, 0);
    s.trayTimer = 0.0f;

    std::ostringstream ss;
    ss << "DISPENSING " << s.name << " ($" << std::fixed << std::setprecision(2) << s.price << ")";
    g_statusMessage = ss.str();
    g_statusTimer = 5.0f;
}

// -----------------------------------------------------------------------------
// Wall Collision and Floor Boundary Checks
// -----------------------------------------------------------------------------
bool isPointWalkable(float x, float z) {
    // 1. Outside Sidewalk, Road & Nature (Z in [8.0, 75.0])
    if (z >= 8.0f && z <= 75.0f && x >= -85.0f && x <= 85.0f) {
        if (z < 8.3f && (x < -2.5f || x > 2.5f)) return false;
        return true;
    }
    // 2. Room 1 (Grand Waiting Hall): X in [-8.6, 8.6], Z in [-2.0, 8.0]
    if (z >= -2.0f && z < 8.0f) {
        if (x < -7.4f || x > 8.6f) return false;
        if (z > 7.7f && (x < -2.5f || x > 2.5f)) return false;
        if (z < -1.7f) {
            bool inDoor2 = (x >= -6.5f && x <= -3.5f);
            bool inDoor3 = (x >= 3.5f && x <= 6.5f);
            if (!inDoor2 && !inDoor3) return false;
        }
        return true;
    }
    // 3. Room 2 (ATM Lounge): X in [-8.6, -1.4], Z in [-9.6, -2.0]
    if (z >= -9.0f && z < -2.0f && x >= -8.6f && x <= -1.4f) {
        return true;
    }
    // 4. Room 3 (Ticket Hub): X in [1.4, 8.6], Z in [-9.6, -2.0]
    if (z >= -9.0f && z < -2.0f && x >= 1.4f && x <= 8.6f) {
        return true;
    }
    // 5. Surrounding outdoor green fields
    if (z < -10.5f && z > -85.0f && x >= -85.0f && x <= 85.0f) return true;
    if ((x < -9.5f || x > 9.5f) && z >= -85.0f && z <= 85.0f) return true;

    return false;
}

void movePlayer(float dx, float dz) {
    float newX = g_player.pos.x + dx;
    float newZ = g_player.pos.z + dz;

    if (isPointWalkable(newX, g_player.pos.z)) {
        g_player.pos.x = newX;
    }
    if (isPointWalkable(g_player.pos.x, newZ)) {
        g_player.pos.z = newZ;
    }
}

int getActiveMachineNearPlayer(const Vec3& p) {
    if (std::hypot(p.x - (-7.3f), p.z - 4.5f) < 2.0f) return MACHINE_VENDING_DRINKS;
    if (std::hypot(p.x - (-7.3f), p.z - 1.0f) < 2.0f) return MACHINE_VENDING_SNACKS;
    if (std::hypot(p.x - (-7.3f), p.z - (-2.5f)) < 2.0f) return MACHINE_VENDING_SWEETS;

    if (std::hypot(p.x - (-6.5f), p.z - (-8.2f)) < 2.0f) return MACHINE_ATM_1;
    if (std::hypot(p.x - (-3.5f), p.z - (-8.2f)) < 2.0f) return MACHINE_ATM_2;

    if (std::hypot(p.x - 3.5f, p.z - (-8.2f)) < 2.0f) return MACHINE_TICKET_1;
    if (std::hypot(p.x - 6.5f, p.z - (-8.2f)) < 2.0f) return MACHINE_TICKET_2;

    return MACHINE_NONE;
}

// -----------------------------------------------------------------------------
// Update Player Movement (Now works in BOTH First Person & Free Orbit Mode!)
// -----------------------------------------------------------------------------
void updatePlayerInput(GLFWwindow* window, float dt) {
    Vec3 forward, right;
    if (g_cameraMode == MODE_FIRST_PERSON) {
        float rYaw = toRadians(g_player.yaw);
        forward = Vec3(std::sin(rYaw), 0.0f, -std::cos(rYaw));
        right   = Vec3(std::cos(rYaw), 0.0f,  std::sin(rYaw));
    } else {
        // Free Orbit Mode: Move relative to Orbit Camera view angle
        float rYaw = toRadians(g_camera.yaw);
        forward = Vec3(-std::sin(rYaw), 0.0f, -std::cos(rYaw));
        right   = Vec3( std::cos(rYaw), 0.0f, -std::sin(rYaw));
    }

    Vec3 moveDir(0, 0, 0);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= forward;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;

    g_isWalking = (moveDir.x != 0.0f || moveDir.z != 0.0f);
    if (g_isWalking) {
        Vec3 nMove = normalize(moveDir);
        float step = g_player.walkSpeed * dt;
        movePlayer(nMove.x * step, nMove.z * step);
        g_walkAnimTimer += dt * 10.0f;

        // In Free Orbit Mode, orient the avatar towards its moving direction
        if (g_cameraMode == MODE_FREE_ORBIT) {
            float targetYaw = toDegrees(std::atan2(nMove.x, -nMove.z));
            g_player.yaw = targetYaw;
            g_camera.target = g_player.pos + Vec3(0.0f, 1.2f, 0.0f);
        }
    } else {
        g_walkAnimTimer = 0.0f;
    }

    // Jump Physics (Available in both First Person & Free Mode)
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && g_player.isGrounded) {
        g_player.velY = g_player.jumpSpeed;
        g_player.isGrounded = false;
    }

    if (!g_player.isGrounded) {
        g_player.velY -= g_player.gravity * dt;
        g_player.pos.y += g_player.velY * dt;
        if (g_player.pos.y <= 0.0f) {
            g_player.pos.y = 0.0f;
            g_player.velY = 0.0f;
            g_player.isGrounded = true;
        }
    }
}

void updateSimulation(float dt) {
    g_camera.update(dt);
    g_activeMachine = getActiveMachineNearPlayer(g_player.pos);

    if (g_atmFeedbackTimer > 0.0f) g_atmFeedbackTimer -= dt;

    if (g_ticketFeedbackTimer > 0.0f) {
        g_ticketFeedbackTimer -= dt;
        g_ticketSlideAnim = std::min(1.0f, g_ticketSlideAnim + 2.0f * dt);
    } else {
        g_ticketSlideAnim = std::max(0.0f, g_ticketSlideAnim - 2.0f * dt);
    }

    if (g_statusTimer > 0.0f) g_statusTimer -= dt;

    // Update Vending Products Physics
    bool anyItemInTray = false;
    for (auto& s : g_slots) {
        if (s.state == STATE_SPIRAL_TURNING) {
            float speed = 0.65f;
            s.forwardProgress += speed * dt;
            s.spiralAngle += 360.0f * speed * dt;

            Vec3 machineBase = (s.machineId == 1) ? VENDING_POS_1 : ((s.machineId == 2) ? VENDING_POS_2 : VENDING_POS_3);
            float localX = SLOT_X[s.shelfCol];
            float localY = SHELF_Y[s.shelfRow] + 0.08f;
            float localZ = SHELF_CENTER_Z + s.forwardProgress * (SHELF_FRONT_Z - SHELF_CENTER_Z);

            s.currentPos = Vec3(machineBase.x + localZ, machineBase.y + localY, machineBase.z - localX);

            if (s.forwardProgress > 0.75f) {
                float tilt = (s.forwardProgress - 0.75f) / 0.25f;
                s.currentRot = Vec3(0, 90.0f, tilt * 30.0f);
            }

            if (s.forwardProgress >= 1.0f) {
                s.state = STATE_FALLING_CHUTE;
                s.velocity = Vec3(0.08f, -0.4f, 0.0f);
            }
        }
        else if (s.state == STATE_FALLING_CHUTE) {
            Vec3 machineBase = (s.machineId == 1) ? VENDING_POS_1 : ((s.machineId == 2) ? VENDING_POS_2 : VENDING_POS_3);
            s.velocity.y -= 6.5f * dt;
            s.currentPos += s.velocity * dt;

            // Constrain inside chute drop space behind the front glass pane
            s.currentPos.x = std::clamp(s.currentPos.x, machineBase.x + SHELF_FRONT_Z, machineBase.x + 0.25f);

            s.currentRot.x += 160.0f * dt;
            s.currentRot.z += 80.0f * dt;

            float floorY = 0.32f;
            if (s.currentPos.y <= floorY) {
                s.currentPos.y = floorY;
                s.velocity.y = -s.velocity.y * 0.24f;
                s.velocity.x = 0.0f;

                if (std::abs(s.velocity.y) < 0.20f) {
                    s.state = STATE_RESTING_IN_TRAY;
                    s.currentRot = Vec3(12.0f, 112.0f, 0.0f);
                    s.trayTimer = 5.0f;
                    g_targetDoorFlapAngle = 38.0f;
                    g_doorHoldTimer = 2.4f;
                }
            }
        }
        else if (s.state == STATE_RESTING_IN_TRAY) {
            anyItemInTray = true;
            s.trayTimer -= dt;
            if (s.trayTimer <= 0.0f) {
                s.state = STATE_DISPENSED_GONE;
                g_statusMessage = "COLLECTED " + s.name + " - ENJOY!";
                g_statusTimer = 3.0f;
            }
        }
    }

    g_trayIndicatorLight.enabled = anyItemInTray;

    if (g_isDoorManuallyOpened) {
        g_targetDoorFlapAngle = 45.0f;
    } else if (g_doorHoldTimer > 0.0f) {
        g_doorHoldTimer -= dt;
        g_targetDoorFlapAngle = 38.0f;
        if (g_doorHoldTimer <= 0.0f) g_targetDoorFlapAngle = 0.0f;
    } else {
        g_targetDoorFlapAngle = 0.0f;
    }

    float doorSpeed = (g_doorFlapAngle < g_targetDoorFlapAngle) ? 55.0f : 35.0f;
    if (g_doorFlapAngle < g_targetDoorFlapAngle) {
        g_doorFlapAngle = std::min(g_targetDoorFlapAngle, g_doorFlapAngle + doorSpeed * dt);
    } else if (g_doorFlapAngle > g_targetDoorFlapAngle) {
        g_doorFlapAngle = std::max(g_targetDoorFlapAngle, g_doorFlapAngle - doorSpeed * dt);
    }
}

// -----------------------------------------------------------------------------
// Modular Scene Drawing Functions
// -----------------------------------------------------------------------------

// 1. Natural Lush Green Landscape & Realistic Procedural Trees
void drawLandscapeAndTrees() {
    Material grassMat1;
    grassMat1.color = Vec3(0.20f, 0.40f, 0.16f); // Soft, natural meadow green
    grassMat1.shininess = 4.0f;

    Material grassMat2;
    grassMat2.color = Vec3(0.16f, 0.34f, 0.13f); // Shaded grass variation
    grassMat2.shininess = 4.0f;

    // Ground terrain slab (190m x 190m)
    drawCube(Vec3(0.0f, -0.05f, 0.0f), Vec3(190.0f, 0.08f, 190.0f), grassMat1);

    // Subtle lawn striping
    for (int gx = -4; gx <= 4; gx += 2) {
        for (int gz = -4; gz <= 4; gz += 2) {
            drawCube(Vec3(gx * 20.0f + 4.0f, -0.045f, gz * 20.0f + 6.0f), Vec3(18.0f, 0.082f, 18.0f), grassMat2);
        }
    }

    // Realistic Tree Materials
    Material barkMat;
    barkMat.color = Vec3(0.28f, 0.18f, 0.11f); // Dark textured bark
    barkMat.shininess = 8.0f;

    Material leafMatDeep;
    leafMatDeep.color = Vec3(0.10f, 0.28f, 0.12f); // Deep forest green

    Material leafMatMid;
    leafMatMid.color = Vec3(0.14f, 0.36f, 0.15f); // Natural foliage green

    Material leafMatBright;
    leafMatBright.color = Vec3(0.18f, 0.45f, 0.18f); // Sunlit top leaves

    auto drawRealisticTree = [&](const TreePos& t) {
        Vec3 base = Vec3(t.x, 0.0f, t.z);
        float s = t.scale;

        // Tree Root Base Flares (4 angled root supports)
        drawCylinder(base + Vec3( 0.25f*s, 0.15f*s,  0.25f*s), Vec3(0.22f*s, 0.4f*s, 0.22f*s), barkMat, Vec3(15, 45, 0));
        drawCylinder(base + Vec3(-0.25f*s, 0.15f*s, -0.25f*s), Vec3(0.22f*s, 0.4f*s, 0.22f*s), barkMat, Vec3(-15, 45, 0));
        drawCylinder(base + Vec3(-0.25f*s, 0.15f*s,  0.25f*s), Vec3(0.22f*s, 0.4f*s, 0.22f*s), barkMat, Vec3(15, -45, 0));
        drawCylinder(base + Vec3( 0.25f*s, 0.15f*s, -0.25f*s), Vec3(0.22f*s, 0.4f*s, 0.22f*s), barkMat, Vec3(-15, -45, 0));

        // Sturdy Main Trunk
        drawCylinder(base + Vec3(0, 2.2f * s, 0), Vec3(0.42f * s, 4.0f * s, 0.42f * s), barkMat);

        if (t.type == 0) {
            // Type A: Mature Deciduous Oak (Layered Spreading Organic Canopy)
            drawCylinder(base + Vec3( 0.6f*s, 3.8f*s,  0.4f*s), Vec3(0.20f*s, 2.0f*s, 0.20f*s), barkMat, Vec3(35.0f, t.rot, 0));
            drawCylinder(base + Vec3(-0.6f*s, 3.6f*s, -0.4f*s), Vec3(0.18f*s, 1.8f*s, 0.18f*s), barkMat, Vec3(-35.0f, t.rot, 0));

            // Layer 1: Wide Lower Skirt
            drawSphere(base + Vec3( 1.1f*s, 4.6f*s,  0.8f*s), Vec3(1.8f*s, 1.2f*s, 1.8f*s), leafMatDeep);
            drawSphere(base + Vec3(-1.1f*s, 4.4f*s, -0.7f*s), Vec3(1.7f*s, 1.2f*s, 1.7f*s), leafMatDeep);
            drawSphere(base + Vec3(-0.7f*s, 4.5f*s,  1.1f*s), Vec3(1.6f*s, 1.1f*s, 1.6f*s), leafMatDeep);
            drawSphere(base + Vec3( 0.8f*s, 4.3f*s, -1.0f*s), Vec3(1.7f*s, 1.2f*s, 1.7f*s), leafMatDeep);

            // Layer 2: Mid Canopy Clusters
            drawSphere(base + Vec3( 0.5f*s, 5.5f*s,  0.4f*s), Vec3(1.9f*s, 1.4f*s, 1.9f*s), leafMatMid);
            drawSphere(base + Vec3(-0.5f*s, 5.4f*s, -0.4f*s), Vec3(1.8f*s, 1.3f*s, 1.8f*s), leafMatMid);

            // Layer 3: Sunlit Crown Dome
            drawSphere(base + Vec3(0, 6.4f * s, 0), Vec3(1.5f * s, 1.4f * s, 1.5f * s), leafMatBright);
        } else {
            // Type B: Tall Tiered Conifer / Pine Tree
            drawCylinder(base + Vec3(0, 4.5f * s, 0), Vec3(0.30f * s, 4.6f * s, 0.30f * s), barkMat);

            // Tier 1 (Lowest wide cone skirt)
            drawSphere(base + Vec3(0, 3.8f * s, 0), Vec3(2.4f * s, 0.9f * s, 2.4f * s), leafMatDeep);
            // Tier 2
            drawSphere(base + Vec3(0, 4.8f * s, 0), Vec3(2.0f * s, 0.9f * s, 2.0f * s), leafMatMid);
            // Tier 3
            drawSphere(base + Vec3(0, 5.8f * s, 0), Vec3(1.5f * s, 0.85f * s, 1.5f * s), leafMatMid);
            // Tier 4 (Top tip)
            drawSphere(base + Vec3(0, 6.8f * s, 0), Vec3(1.0f * s, 1.1f * s, 1.0f * s), leafMatBright);
        }
    };

    for (const auto& t : g_trees) {
        drawRealisticTree(t);
    }
}

// 2. 160-Meter Highway Road & Street Sidewalk
void drawRoadAndSidewalk() {
    Material roadMat;
    roadMat.color = Vec3(0.14f, 0.15f, 0.17f); // Real asphalt road
    roadMat.shininess = 16.0f;

    Material curbMat;
    curbMat.color = Vec3(0.70f, 0.72f, 0.76f); // Concrete sidewalk
    curbMat.shininess = 32.0f;

    Material yellowLineMat;
    yellowLineMat.color = Vec3(0.96f, 0.86f, 0.15f); // Double yellow road stripes
    yellowLineMat.emission = Vec3(0.08f, 0.06f, 0.01f);

    Material whiteLineMat;
    whiteLineMat.color = Vec3(0.92f, 0.94f, 0.96f);

    // Front Sidewalk Slab (160m Long x 3.5m Wide)
    drawCube(Vec3(0.0f, 0.06f, 9.75f), Vec3(160.0f, 0.12f, 3.50f), curbMat);
    // Curb border stone
    drawCube(Vec3(0.0f, 0.10f, 11.50f), Vec3(160.0f, 0.20f, 0.20f), curbMat);

    // Highway Asphalt (160m Long x 8.0m Wide, Z in [11.5, 19.5])
    drawCube(Vec3(0.0f, -0.01f, 15.50f), Vec3(160.0f, 0.02f, 8.00f), roadMat);

    // White Shoulder Lines
    drawCube(Vec3(0.0f, 0.005f, 11.80f), Vec3(160.0f, 0.005f, 0.15f), whiteLineMat);
    drawCube(Vec3(0.0f, 0.005f, 19.20f), Vec3(160.0f, 0.005f, 0.15f), whiteLineMat);

    // Double Yellow Center Divider Lines down highway
    for (int i = -36; i <= 36; i += 2) {
        drawCube(Vec3(i * 2.2f, 0.006f, 15.42f), Vec3(1.8f, 0.006f, 0.10f), yellowLineMat);
        drawCube(Vec3(i * 2.2f, 0.006f, 15.58f), Vec3(1.8f, 0.006f, 0.10f), yellowLineMat);
    }

    // 8 Street Lamp Posts along Sidewalk
    Material lampPoleMat;
    lampPoleMat.color = Vec3(0.20f, 0.22f, 0.25f);
    Material lampLightMat;
    lampLightMat.color = Vec3(1.0f, 0.95f, 0.85f);
    lampLightMat.emission = Vec3(1.8f, 1.6f, 1.1f);

    float lampX[] = { -65.0f, -45.0f, -25.0f, -6.0f, 12.0f, 32.0f, 52.0f, 70.0f };
    for (float lx : lampX) {
        drawCylinder(Vec3(lx, 2.2f, 11.1f), Vec3(0.10f, 4.4f, 0.10f), lampPoleMat);
        drawCylinder(Vec3(lx, 4.4f, 11.8f), Vec3(0.07f, 1.4f, 0.07f), lampPoleMat, Vec3(90.0f, 0, 0));
        drawSphere(Vec3(lx, 4.3f, 12.4f), Vec3(0.28f, 0.20f, 0.28f), lampLightMat);
    }
}

// 3. Grand 3-Room Structure (Spacious: 18m Waiting Hall, 8m ATM Lounge, 8m Ticket Hub)
void drawRoomStructure() {
    Material wallMat;
    wallMat.color = Vec3(0.86f, 0.88f, 0.92f); // Modern pristine architectural wall
    wallMat.shininess = 8.0f;
    wallMat.specularStrength = 0.04f; // Soft, zero hotspot blowout!

    Material accentWallMat;
    accentWallMat.color = Vec3(0.22f, 0.26f, 0.32f);
    accentWallMat.shininess = 16.0f;
    accentWallMat.specularStrength = 0.05f;

    Material dividerWallMat;
    dividerWallMat.color = Vec3(0.76f, 0.78f, 0.82f);
    dividerWallMat.shininess = 8.0f;
    dividerWallMat.specularStrength = 0.04f;

    // Floor Materials
    Material floorMainMat;
    floorMainMat.color = Vec3(0.38f, 0.40f, 0.44f);
    floorMainMat.shininess = 48.0f;
    floorMainMat.specularStrength = 0.25f;

    Material floorAtmMat;
    floorAtmMat.color = Vec3(0.18f, 0.22f, 0.28f);
    floorAtmMat.shininess = 48.0f;
    floorAtmMat.specularStrength = 0.25f;

    Material floorTicketMat;
    floorTicketMat.color = Vec3(0.28f, 0.29f, 0.30f);
    floorTicketMat.shininess = 48.0f;
    floorTicketMat.specularStrength = 0.25f;

    // Floor Slabs
    drawCube(Vec3(0.0f, -0.02f, 3.0f), Vec3(18.0f, 0.04f, 10.0f), floorMainMat);

    // Floor Checkerboard Tile Inlays in Room 1
    Material tileInlayMat;
    tileInlayMat.color = Vec3(0.44f, 0.46f, 0.50f);
    tileInlayMat.shininess = 48.0f;
    tileInlayMat.specularStrength = 0.25f;
    for (int tx = -4; tx <= 4; ++tx) {
        for (int tz = -1; tz <= 3; ++tz) {
            if ((tx + tz) % 2 == 0) {
                drawCube(Vec3(tx * 1.8f, 0.002f, tz * 1.8f + 1.2f), Vec3(1.70f, 0.005f, 1.70f), tileInlayMat);
            }
        }
    }

    // Room 2 Floor (ATM Banking Lounge)
    drawCube(Vec3(-5.0f, -0.02f, -6.0f), Vec3(8.0f, 0.04f, 8.0f), floorAtmMat);

    // Room 3 Floor (Ticket & Transit Hub)
    drawCube(Vec3(5.0f, -0.02f, -6.0f), Vec3(8.0f, 0.04f, 8.0f), floorTicketMat);

    // Yellow safety tactile warning strip in Ticket Room
    Material yellowSafetyMat;
    yellowSafetyMat.color = Vec3(0.92f, 0.78f, 0.10f);
    drawCube(Vec3(5.0f, 0.003f, -7.2f), Vec3(7.4f, 0.006f, 0.22f), yellowSafetyMat);

    // --- Room 1 Perimeter Walls (Height: 4.2m) ---
    // Left Wall
    drawCube(Vec3(-9.0f, 2.1f, 3.0f), Vec3(0.20f, 4.2f, 10.0f), wallMat);
    // Right Wall
    drawCube(Vec3(9.0f, 2.1f, 3.0f), Vec3(0.20f, 4.2f, 10.0f), wallMat);

    // Front Wall with Entrance
    drawCube(Vec3(-5.75f, 2.1f, 8.0f), Vec3(6.5f, 4.2f, 0.20f), wallMat);
    drawCube(Vec3( 5.75f, 2.1f, 8.0f), Vec3(6.5f, 4.2f, 0.20f), wallMat);
    drawCube(Vec3(0.0f, 3.70f, 8.0f), Vec3(5.0f, 1.0f, 0.20f), accentWallMat);

    // Entrance Glass Door Frames
    Material doorFrameMat;
    doorFrameMat.color = Vec3(0.18f, 0.20f, 0.24f);
    drawCube(Vec3(-2.45f, 1.6f, 8.0f), Vec3(0.08f, 3.2f, 0.24f), doorFrameMat);
    drawCube(Vec3( 2.45f, 1.6f, 8.0f), Vec3(0.08f, 3.2f, 0.24f), doorFrameMat);
    drawCube(Vec3( 0.00f, 1.6f, 8.0f), Vec3(0.08f, 3.2f, 0.24f), doorFrameMat);

    // --- Back Partition Wall of Waiting Hall (Z = -2.0) ---
    drawCube(Vec3(-7.75f, 2.1f, -2.0f), Vec3(2.5f, 4.2f, 0.20f), dividerWallMat);
    drawCube(Vec3( 0.00f, 2.1f, -2.0f), Vec3(7.0f, 4.2f, 0.20f), dividerWallMat);
    drawCube(Vec3( 7.75f, 2.1f, -2.0f), Vec3(2.5f, 4.2f, 0.20f), dividerWallMat);

    // Doorway Lintel Headers
    drawCube(Vec3(-5.0f, 3.55f, -2.0f), Vec3(3.0f, 1.30f, 0.20f), dividerWallMat);
    drawCube(Vec3( 5.0f, 3.55f, -2.0f), Vec3(3.0f, 1.30f, 0.20f), dividerWallMat);

    // Backlit Directional Signage
    Material signAtmMat;
    signAtmMat.color = Vec3(0.08f, 0.40f, 0.90f);
    signAtmMat.emission = Vec3(0.2f, 0.6f, 1.2f);
    drawCube(Vec3(-5.0f, 3.10f, -1.89f), Vec3(2.4f, 0.35f, 0.02f), signAtmMat);

    Material signTicketMat;
    signTicketMat.color = Vec3(0.92f, 0.65f, 0.10f);
    signTicketMat.emission = Vec3(1.1f, 0.8f, 0.15f);
    drawCube(Vec3(5.0f, 3.10f, -1.89f), Vec3(2.4f, 0.35f, 0.02f), signTicketMat);

    // Central Wall Flight & Train Schedule Board
    Material boardMat;
    boardMat.color = Vec3(0.08f, 0.09f, 0.11f);
    drawCube(Vec3(0.0f, 2.2f, -1.89f), Vec3(4.5f, 1.8f, 0.03f), boardMat);
    Material timeLineMat;
    timeLineMat.color = Vec3(0.95f, 0.85f, 0.20f);
    timeLineMat.emission = Vec3(0.7f, 0.6f, 0.15f);
    for (int tl = 0; tl < 5; ++tl) {
        drawCube(Vec3(0.0f, 2.7f - tl * 0.25f, -1.87f), Vec3(4.1f, 0.04f, 0.005f), timeLineMat);
    }

    // --- Room 2 Enclosing Walls (ATM Banking Lounge) ---
    drawCube(Vec3(-9.0f, 2.1f, -6.0f), Vec3(0.20f, 4.2f, 8.0f), wallMat);
    drawCube(Vec3(-1.0f, 2.1f, -6.0f), Vec3(0.20f, 4.2f, 8.0f), wallMat);
    drawCube(Vec3(-5.0f, 2.1f, -10.0f), Vec3(8.0f, 4.2f, 0.20f), wallMat);

    // --- Room 3 Enclosing Walls (Ticket Hub) ---
    drawCube(Vec3(1.0f, 2.1f, -6.0f), Vec3(0.20f, 4.2f, 8.0f), wallMat);
    drawCube(Vec3(9.0f, 2.1f, -6.0f), Vec3(0.20f, 4.2f, 8.0f), wallMat);
    drawCube(Vec3(5.0f, 2.1f, -10.0f), Vec3(8.0f, 4.2f, 0.20f), wallMat);
}

// 4. Semi-Transparent Ceiling Slabs
void drawCeilings(float alpha) {
    if (alpha <= 0.01f) return;

    Material ceilMat;
    ceilMat.color = Vec3(0.92f, 0.94f, 0.98f);
    ceilMat.alpha = alpha;
    ceilMat.shininess = 16.0f;
    ceilMat.specularStrength = 0.1f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    drawCube(Vec3(0.0f, 4.20f, 3.0f), Vec3(18.0f, 0.06f, 10.0f), ceilMat);
    drawCube(Vec3(-5.0f, 4.20f, -6.0f), Vec3(8.0f, 0.06f, 8.0f), ceilMat);
    drawCube(Vec3(5.0f, 4.20f, -6.0f), Vec3(8.0f, 0.06f, 8.0f), ceilMat);

    glDepthMask(GL_TRUE);
}

// 5. Semi-Transparent Vending Machine Glass Panes (Transparent Pass)
void drawVendingMachineGlassPanes() {
    Material glassMat;
    glassMat.color = Vec3(0.85f, 0.92f, 0.98f);
    glassMat.alpha = 0.20f;
    glassMat.shininess = 128.0f;
    glassMat.specularStrength = 0.9f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    auto drawSingleGlass = [&](const Vec3& machinePos) {
        Mat4 m = translate(Mat4::Identity(), machinePos);
        m = rotate(m, toRadians(VENDING_ROTY), Vec3(0, 1, 0));
        m = translate(m, Vec3(-0.15f, 1.25f, 0.33f));
        m = scale(m, Vec3(0.66f, 1.12f, 0.005f));
        drawShape(g_meshCube, m, glassMat);
    };

    drawSingleGlass(VENDING_POS_1);
    drawSingleGlass(VENDING_POS_2);
    drawSingleGlass(VENDING_POS_3);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// -----------------------------------------------------------------------------
// 5. TRUE HIERARCHICAL VENDING MACHINE (100% Faithful to Commit 8209029)
// -----------------------------------------------------------------------------
void drawFullVendingMachine(const Mat4& rootM, int machineId, const Material& cabinetBodyMat, const std::string& marqueeText, const Vec3& marqueeColor) {
    (void)marqueeText;
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

    // Helper: Draw child cube transformed strictly relative to rootM
    auto drawChildCube = [&](const Vec3& localPos, const Vec3& localSize, const Material& mat) {
        Mat4 m = rootM;
        m = translate(m, localPos);
        m = scale(m, localSize);
        drawShape(g_meshCube, m, mat);
    };

    // Helper: Draw child cylinder transformed strictly relative to rootM
    auto drawChildCylinder = [&](const Vec3& localPos, const Vec3& localSize, const Material& mat, const Vec3& rotDeg = Vec3(0,0,0)) {
        Mat4 m = rootM;
        m = translate(m, localPos);
        if (rotDeg.y != 0.0f) m = rotate(m, toRadians(rotDeg.y), Vec3(0, 1, 0));
        if (rotDeg.x != 0.0f) m = rotate(m, toRadians(rotDeg.x), Vec3(1, 0, 0));
        if (rotDeg.z != 0.0f) m = rotate(m, toRadians(rotDeg.z), Vec3(0, 0, 1));
        m = scale(m, localSize);
        drawShape(g_meshCylinder, m, mat);
    };

    // 1. VENDING MACHINE MAIN CABINET SHELL (Width: 1.06m, Height: 2.05m, Depth: 0.68m)
    drawChildCube(Vec3(0.0f, 1.10f, -0.32f), Vec3(1.06f, 2.05f, 0.04f), cabinetBodyMat); // Back
    drawChildCube(Vec3(-0.51f, 1.10f, 0.0f), Vec3(0.04f, 2.05f, 0.68f), cabinetBodyMat); // Left
    drawChildCube(Vec3(0.51f, 1.10f, 0.0f),  Vec3(0.04f, 2.05f, 0.68f), cabinetBodyMat); // Right
    drawChildCube(Vec3(0.0f, 2.10f, 0.0f),   Vec3(1.06f, 0.05f, 0.68f), cabinetBodyMat); // Top
    drawChildCube(Vec3(0.0f, 0.10f, 0.0f),   Vec3(1.06f, 0.05f, 0.68f), cabinetBodyMat); // Bottom Base

    // 4 Adjustable Chrome Leveling Legs & Rubber Foot Pads
    float legX = 0.45f;
    float legZ = 0.26f;
    float legCoords[4][2] = { {-legX, -legZ}, {legX, -legZ}, {-legX, legZ}, {legX, legZ} };
    for (auto& lc : legCoords) {
        drawChildCylinder(Vec3(lc[0], 0.04f, lc[1]), Vec3(0.05f, 0.08f, 0.05f), chromeMat);
        drawChildCylinder(Vec3(lc[0], 0.008f, lc[1]), Vec3(0.065f, 0.016f, 0.065f), rubberMat);
    }

    // 2. TOP ILLUMINATED MARQUEE HEADER (Nested strictly inside side walls: inner width 0.98m)
    drawChildCube(Vec3(0.0f, 1.95f, 0.318f), Vec3(0.976f, 0.22f, 0.038f), cabinetAccentMat);
    Material marqueeMat;
    marqueeMat.color = Vec3(0.95f, 0.95f, 0.98f);
    marqueeMat.emission = g_cabinetLightsEnabled ? marqueeColor : Vec3(0.05f, 0.05f, 0.05f);
    drawChildCube(Vec3(-0.15f, 1.95f, 0.340f), Vec3(0.66f, 0.16f, 0.008f), marqueeMat);

    Material brandAccentMat;
    brandAccentMat.color = marqueeColor * 0.8f;
    brandAccentMat.emission = g_cabinetLightsEnabled ? marqueeColor * 1.2f : Vec3(0, 0, 0);
    drawChildCube(Vec3(-0.15f, 2.01f, 0.344f), Vec3(0.62f, 0.02f, 0.006f), brandAccentMat);

    // 3. INTERIOR DISPLAY CAVITY & PRODUCT SHELVES
    drawChildCube(Vec3(-0.15f, 1.25f, -0.29f), Vec3(0.68f, 1.18f, 0.02f), interiorWallMat); // Cavity Back
    drawChildCube(Vec3(-0.485f, 1.25f, 0.0f),  Vec3(0.01f, 1.18f, 0.58f), interiorWallMat); // Cavity Left
    drawChildCube(Vec3( 0.19f, 1.25f, 0.0f),   Vec3(0.02f, 1.18f, 0.58f), cabinetAccentMat);// Cavity Divider
    drawChildCube(Vec3(-0.15f, 1.83f, 0.0f),   Vec3(0.68f, 0.02f, 0.58f), interiorWallMat); // Cavity Roof

    // Interior Fluorescent Lamp Tube Fixture
    Material tubeMat;
    tubeMat.color = Vec3(1.0f, 1.0f, 1.0f);
    tubeMat.emission = g_cabinetLightsEnabled ? Vec3(1.8f, 1.8f, 2.0f) : Vec3(0.05f, 0.05f, 0.05f);
    drawChildCylinder(Vec3(-0.15f, 1.82f, 0.05f), Vec3(0.025f, 0.62f, 0.025f), tubeMat, Vec3(0, 0, 90.0f));

    // 4 Stock Shelves + Price Label Rails + Rotating Helical Coils
    for (int row = 0; row < 4; ++row) {
        float y = SHELF_Y[row];
        // Shelf Tray Plane
        drawChildCube(Vec3(-0.15f, y, SHELF_CENTER_Z), Vec3(0.66f, 0.018f, SHELF_DEPTH_Z), shelfMat);
        // Shelf Front Rail (Green Price Tag Strip)
        Material railMat;
        railMat.color = Vec3(0.12f, 0.62f, 0.38f);
        drawChildCube(Vec3(-0.15f, y, SHELF_FRONT_Z), Vec3(0.66f, 0.024f, 0.015f), railMat);

        // Price Tag Stickers for Each Slot
        Material tagMat;
        tagMat.color = Vec3(0.96f, 0.96f, 0.96f);
        tagMat.emission = g_cabinetLightsEnabled ? Vec3(0.15f, 0.15f, 0.15f) : Vec3(0, 0, 0);
        for (int col = 0; col < 3; ++col) {
            drawChildCube(Vec3(SLOT_X[col], y, SHELF_FRONT_Z + 0.008f), Vec3(0.06f, 0.018f, 0.002f), tagMat);
        }

        // Helical Wire Spirals (Product Spirals - rotate when dispensing)
        for (int col = 0; col < 3; ++col) {
            float angle = 0.0f;
            for (const auto& s : g_slots) {
                if (s.machineId == machineId && s.shelfRow == row && s.shelfCol == col) {
                    angle = s.spiralAngle;
                    break;
                }
            }
            Mat4 cm = rootM;
            cm = translate(cm, Vec3(SLOT_X[col], y + 0.068f, SHELF_CENTER_Z));
            cm = rotate(cm, toRadians(angle), Vec3(0, 0, 1));
            cm = scale(cm, Vec3(1.0f, 1.0f, 0.85f));
            drawShape(g_meshCoil, cm, coilMat);
        }
    }

    // 4. CONTROL & PAYMENT PANEL (Right Column - perfectly nested inside right wall)
    drawChildCube(Vec3(0.338f, 1.10f, 0.318f), Vec3(0.284f, 1.48f, 0.038f), cabinetAccentMat);

    // Multi-color Machine Status Indicator LEDs
    bool isMachineVending = false;
    for (const auto& s : g_slots) {
        if (s.machineId == machineId && (s.state == STATE_SPIRAL_TURNING || s.state == STATE_FALLING_CHUTE)) {
            isMachineVending = true;
            break;
        }
    }

    // Ready Green LED
    Material readyLedMat;
    readyLedMat.color = Vec3(0.1f, 0.8f, 0.2f);
    readyLedMat.emission = (!isMachineVending) ? Vec3(0.2f, 1.6f, 0.3f) : Vec3(0.02f, 0.1f, 0.02f);
    Mat4 rlm = rootM;
    rlm = translate(rlm, Vec3(0.25f, 1.70f, 0.345f));
    rlm = scale(rlm, Vec3(0.022f, 0.022f, 0.022f));
    drawShape(g_meshSphere, rlm, readyLedMat);

    // Busy Amber LED
    Material busyLedMat;
    busyLedMat.color = Vec3(0.95f, 0.65f, 0.1f);
    busyLedMat.emission = isMachineVending ? Vec3(1.8f, 1.1f, 0.15f) : Vec3(0.08f, 0.04f, 0.01f);
    Mat4 blm = rootM;
    blm = translate(blm, Vec3(0.35f, 1.70f, 0.345f));
    blm = scale(blm, Vec3(0.022f, 0.022f, 0.022f));
    drawShape(g_meshSphere, blm, busyLedMat);

    // Pickup Door Blue LED
    Material doorLedMat;
    doorLedMat.color = Vec3(0.2f, 0.6f, 1.0f);
    doorLedMat.emission = (g_trayIndicatorLight.enabled && machineId == 1) ? Vec3(0.3f, 1.4f, 2.2f) : Vec3(0.02f, 0.05f, 0.1f);
    Mat4 dlm = rootM;
    dlm = translate(dlm, Vec3(0.45f, 1.70f, 0.345f));
    dlm = scale(dlm, Vec3(0.022f, 0.022f, 0.022f));
    drawShape(g_meshSphere, dlm, doorLedMat);

    // Digital LED Screen Frame & Display Glass
    Material screenFrameMat;
    screenFrameMat.color = Vec3(0.04f, 0.04f, 0.05f);
    drawChildCube(Vec3(0.35f, 1.61f, 0.342f), Vec3(0.25f, 0.09f, 0.015f), screenFrameMat);

    Material screenMat;
    screenMat.color = Vec3(0.02f, 0.15f, 0.25f);
    screenMat.emission = isMachineVending ? Vec3(0.1f, 0.75f, 0.4f) : Vec3(0.04f, 0.60f, 0.85f);
    drawChildCube(Vec3(0.35f, 1.61f, 0.351f), Vec3(0.23f, 0.07f, 0.005f), screenMat);

    // Interactive Keypad (Keys 1-9, 0, Cancel, Enter with physical depression)
    Material keypadPlateMat;
    keypadPlateMat.color = Vec3(0.12f, 0.13f, 0.16f);
    drawChildCube(Vec3(0.35f, 1.40f, 0.342f), Vec3(0.22f, 0.25f, 0.01f), keypadPlateMat);

    Material btnMat;
    btnMat.color = Vec3(0.78f, 0.80f, 0.84f);
    btnMat.shininess = 64.0f;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 3; ++c) {
            float bx = 0.29f + c * 0.06f;
            float by = 1.49f - r * 0.058f;
            Material bMat = btnMat;
            if (r == 3 && c == 0) { bMat.color = Vec3(0.85f, 0.18f, 0.15f); } // Cancel Red
            if (r == 3 && c == 2) { bMat.color = Vec3(0.15f, 0.75f, 0.25f); } // Enter Green
            drawChildCube(Vec3(bx, by, 0.351f), Vec3(0.042f, 0.040f, 0.008f), bMat);
        }
    }

    // Bill Insertion Validator with glowing green acceptance slot
    Material billSlotMat;
    billSlotMat.color = Vec3(0.08f, 0.08f, 0.09f);
    drawChildCube(Vec3(0.35f, 1.18f, 0.342f), Vec3(0.22f, 0.08f, 0.015f), billSlotMat);

    Material billGlowMat;
    billGlowMat.color = Vec3(0.15f, 0.85f, 0.25f);
    billGlowMat.emission = Vec3(0.25f, 1.4f, 0.35f);
    drawChildCube(Vec3(0.35f, 1.18f, 0.351f), Vec3(0.14f, 0.012f, 0.005f), billGlowMat);

    // Coin Slot & Return Lever
    drawChildCube(Vec3(0.35f, 1.05f, 0.342f), Vec3(0.08f, 0.09f, 0.015f), chromeMat);
    Material coinSlotMat;
    coinSlotMat.color = Vec3(0.02f, 0.02f, 0.02f);
    drawChildCube(Vec3(0.35f, 1.065f, 0.351f), Vec3(0.01f, 0.04f, 0.005f), coinSlotMat);

    // Contactless NFC / Card Tap Pad
    Material cardPadMat;
    cardPadMat.color = Vec3(0.15f, 0.18f, 0.24f);
    drawChildCube(Vec3(0.35f, 0.92f, 0.342f), Vec3(0.18f, 0.10f, 0.012f), cardPadMat);
    Material nfcGlowMat;
    nfcGlowMat.color = Vec3(0.25f, 0.70f, 0.95f);
    nfcGlowMat.emission = Vec3(0.1f, 0.35f, 0.6f);
    drawChildCube(Vec3(0.35f, 0.92f, 0.350f), Vec3(0.06f, 0.04f, 0.004f), nfcGlowMat);

    // Coin Return Pocket
    drawChildCube(Vec3(0.35f, 0.76f, 0.33f), Vec3(0.14f, 0.12f, 0.035f), cabinetBodyMat);
    drawChildCube(Vec3(0.35f, 0.76f, 0.348f), Vec3(0.10f, 0.08f, 0.005f), rubberMat);

    // 5. BOTTOM DISPENSER BAY & OPENING COLLECTION DOOR
    Material chamberMat;
    chamberMat.color = Vec3(0.08f, 0.09f, 0.10f);
    chamberMat.shininess = 16.0f;
    drawChildCube(Vec3(-0.15f, 0.26f, 0.12f), Vec3(0.66f, 0.03f, 0.40f), chamberMat); // Chamber Floor
    drawChildCube(Vec3(-0.15f, 0.45f, -0.06f), Vec3(0.66f, 0.36f, 0.04f), chamberMat); // Chamber Back
    drawChildCube(Vec3(-0.47f, 0.45f, 0.12f), Vec3(0.03f, 0.36f, 0.40f), chamberMat);  // Chamber Left
    drawChildCube(Vec3( 0.17f, 0.45f, 0.12f), Vec3(0.03f, 0.36f, 0.40f), chamberMat);  // Chamber Right

    // Chute Exterior Border Frame
    drawChildCube(Vec3(-0.15f, 0.62f, 0.32f), Vec3(0.68f, 0.06f, 0.04f), cabinetAccentMat);
    drawChildCube(Vec3(-0.15f, 0.26f, 0.32f), Vec3(0.68f, 0.06f, 0.04f), cabinetAccentMat);
    drawChildCube(Vec3(-0.47f, 0.44f, 0.32f), Vec3(0.04f, 0.30f, 0.04f), cabinetAccentMat);
    drawChildCube(Vec3( 0.17f, 0.44f, 0.32f), Vec3(0.04f, 0.30f, 0.04f), cabinetAccentMat);

    // Collection Chute Interior Indicator Light Fixture
    Material trayLampMat;
    trayLampMat.color = Vec3(0.2f, 0.7f, 1.0f);
    trayLampMat.emission = (g_trayIndicatorLight.enabled && machineId == 1) ? Vec3(0.5f, 1.5f, 2.2f) : Vec3(0, 0, 0);
    drawChildCylinder(Vec3(-0.15f, 0.58f, 0.15f), Vec3(0.02f, 0.40f, 0.02f), trayLampMat, Vec3(0, 0, 90.0f));

    // Opening Collection Door:
    // Rotating around top hinge axis (+X) swinging inward into negative Z!
    float hingeY = 0.59f;
    float hingeZ = 0.32f;
    float flapH  = 0.27f;
    float flapAngle = (machineId == 1) ? g_doorFlapAngle : 0.0f;

    Material doorFlapMat;
    doorFlapMat.color = Vec3(0.20f, 0.22f, 0.26f);
    doorFlapMat.shininess = 32.0f;
    doorFlapMat.specularStrength = 0.5f;

    Material pushPlateMat;
    pushPlateMat.color = Vec3(0.92f, 0.92f, 0.96f);
    pushPlateMat.shininess = 32.0f;

    Mat4 hingeM = rootM;
    hingeM = translate(hingeM, Vec3(-0.15f, hingeY, hingeZ));
    hingeM = rotate(hingeM, toRadians(flapAngle), Vec3(1.0f, 0.0f, 0.0f)); // Inward rotation

    Mat4 flapBodyM = translate(hingeM, Vec3(0.0f, -flapH * 0.5f, 0.0f));
    flapBodyM = scale(flapBodyM, Vec3(0.58f, flapH, 0.015f));
    drawShape(g_meshCube, flapBodyM, doorFlapMat);

    Mat4 badgeM = translate(hingeM, Vec3(0.0f, -flapH * 0.5f, 0.009f));
    badgeM = scale(badgeM, Vec3(0.22f, 0.065f, 0.005f));
    drawShape(g_meshCube, badgeM, pushPlateMat);

    // 6. Outer Glass Window Trim Frames
    drawChildCube(Vec3(-0.15f, 1.83f, 0.32f),  Vec3(0.66f, 0.024f, 0.038f), cabinetAccentMat);
    drawChildCube(Vec3(-0.15f, 0.67f, 0.32f),  Vec3(0.66f, 0.024f, 0.038f), cabinetAccentMat);
    drawChildCube(Vec3(-0.472f, 1.25f, 0.32f), Vec3(0.024f, 1.15f, 0.038f), cabinetAccentMat);
    drawChildCube(Vec3( 0.178f, 1.25f, 0.32f), Vec3(0.024f, 1.15f, 0.038f), cabinetAccentMat);


    // Active selection ground halo ring
    if (g_activeMachine == machineId) {
        Material haloMat;
        haloMat.color = Vec3(0.2f, 0.8f, 1.0f);
        haloMat.emission = Vec3(0.4f, 1.5f, 2.0f);
        drawChildCube(Vec3(-0.15f, 0.02f, 0.90f), Vec3(0.90f, 0.02f, 0.70f), haloMat);
    }
}

// 6. Modern Waiting Lounge Furniture
void drawWaitingRoomFurniture() {
    Material tableMat;
    tableMat.color = Vec3(0.86f, 0.88f, 0.90f);
    tableMat.shininess = 48.0f;

    Material chairMat;
    chairMat.color = Vec3(0.18f, 0.48f, 0.78f);
    chairMat.shininess = 32.0f;

    Material chromeMat;
    chromeMat.color = Vec3(0.88f, 0.90f, 0.94f);
    chromeMat.shininess = 128.0f;

    auto drawTableSet = [&](const Vec3& tPos) {
        drawCylinder(Vec3(tPos.x, 0.75f, tPos.z), Vec3(1.10f, 0.04f, 1.10f), tableMat);
        drawCylinder(Vec3(tPos.x, 0.38f, tPos.z), Vec3(0.10f, 0.74f, 0.10f), chromeMat);
        drawCylinder(Vec3(tPos.x, 0.02f, tPos.z), Vec3(0.65f, 0.03f, 0.65f), chromeMat);

        drawCube(Vec3(tPos.x - 0.80f, 0.45f, tPos.z), Vec3(0.44f, 0.05f, 0.44f), chairMat);
        drawCube(Vec3(tPos.x - 1.00f, 0.75f, tPos.z), Vec3(0.05f, 0.55f, 0.44f), chairMat);
        drawCylinder(Vec3(tPos.x - 0.80f, 0.22f, tPos.z), Vec3(0.04f, 0.44f, 0.04f), chromeMat);

        drawCube(Vec3(tPos.x + 0.80f, 0.45f, tPos.z), Vec3(0.44f, 0.05f, 0.44f), chairMat);
        drawCube(Vec3(tPos.x + 1.00f, 0.75f, tPos.z), Vec3(0.05f, 0.55f, 0.44f), chairMat);
        drawCylinder(Vec3(tPos.x + 0.80f, 0.22f, tPos.z), Vec3(0.04f, 0.44f, 0.04f), chromeMat);
    };

    drawTableSet(Vec3(1.5f, 0.0f,  4.5f));
    drawTableSet(Vec3(1.5f, 0.0f,  1.0f));
    drawTableSet(Vec3(5.5f, 0.0f,  4.5f));
    drawTableSet(Vec3(5.5f, 0.0f,  1.0f));

    // Tandem Waiting Bench Seating Row
    Material benchMat;
    benchMat.color = Vec3(0.24f, 0.28f, 0.35f);
    drawCube(Vec3(-2.5f, 0.45f, 3.0f), Vec3(0.60f, 0.06f, 4.2f), benchMat);
    drawCube(Vec3(-2.8f, 0.75f, 3.0f), Vec3(0.06f, 0.55f, 4.2f), benchMat);
    drawCylinder(Vec3(-2.5f, 0.22f, 1.2f), Vec3(0.06f, 0.44f, 0.06f), chromeMat);
    drawCylinder(Vec3(-2.5f, 0.22f, 4.8f), Vec3(0.06f, 0.44f, 0.06f), chromeMat);

    // 2 Stainless Steel Recycling & Trash Stations
    Material binMat;
    binMat.color = Vec3(0.70f, 0.72f, 0.76f);
    binMat.shininess = 96.0f;
    drawCube(Vec3(-2.5f, 0.45f, 6.8f), Vec3(0.95f, 0.90f, 0.45f), binMat);
    drawCube(Vec3( 5.5f, 0.45f, 6.8f), Vec3(0.95f, 0.90f, 0.45f), binMat);

    // 4 Indoor Ceramic Planters with Topiary Palms
    Material potMat;
    potMat.color = Vec3(0.90f, 0.92f, 0.95f);
    Material leafMat;
    leafMat.color = Vec3(0.18f, 0.60f, 0.25f);
    Vec3 plantPositions[] = { {-7.5f, 0.0f, 6.8f}, {7.5f, 0.0f, 6.8f}, {-7.5f, 0.0f, -0.8f}, {7.5f, 0.0f, -0.8f} };
    for (const auto& pp : plantPositions) {
        drawCylinder(Vec3(pp.x, 0.35f, pp.z), Vec3(0.48f, 0.70f, 0.48f), potMat);
        drawSphere(Vec3(pp.x, 1.15f, pp.z), Vec3(0.55f, 0.60f, 0.55f), leafMat);
    }
}

// 7. Room 2: Two Full Walk-In Bank ATM Booths
void drawATMBooths() {
    Material atmFrameMat;
    atmFrameMat.color = Vec3(0.15f, 0.18f, 0.24f);
    atmFrameMat.shininess = 64.0f;

    Material atmBlueTrim;
    atmBlueTrim.color = Vec3(0.10f, 0.40f, 0.85f);

    Material screenMat;
    screenMat.color = Vec3(0.04f, 0.12f, 0.25f);
    screenMat.emission = Vec3(0.12f, 0.60f, 1.20f);

    Material cardSlotGlow;
    cardSlotGlow.color = Vec3(0.10f, 0.95f, 0.30f);
    cardSlotGlow.emission = (g_atmFeedbackTimer > 0.0f) ? Vec3(0.4f, 2.0f, 0.6f) : Vec3(0.1f, 0.8f, 0.2f);

    Material chromeMat;
    chromeMat.color = Vec3(0.88f, 0.90f, 0.94f);
    chromeMat.shininess = 128.0f;

    auto drawSingleATM = [&](const Vec3& pos, int atmNumber) {
        drawCube(Vec3(pos.x, 1.15f, pos.z), Vec3(1.10f, 2.30f, 0.85f), atmFrameMat);
        drawCube(Vec3(pos.x, 2.20f, pos.z + 0.05f), Vec3(1.14f, 0.24f, 0.88f), atmBlueTrim);

        drawCube(Vec3(pos.x - 0.58f, 1.30f, pos.z + 0.20f), Vec3(0.06f, 1.60f, 0.55f), atmFrameMat);
        drawCube(Vec3(pos.x + 0.58f, 1.30f, pos.z + 0.20f), Vec3(0.06f, 1.60f, 0.55f), atmFrameMat);

        Material signMat;
        signMat.color = Vec3(0.95f, 0.95f, 0.98f);
        signMat.emission = Vec3(0.3f, 0.7f, 1.4f);
        drawCube(Vec3(pos.x, 2.20f, pos.z + 0.44f), Vec3(0.96f, 0.16f, 0.01f), signMat);

        drawCube(Vec3(pos.x, 1.55f, pos.z + 0.435f), Vec3(0.68f, 0.44f, 0.01f), screenMat);

        drawCube(Vec3(pos.x, 1.18f, pos.z + 0.48f), Vec3(0.72f, 0.06f, 0.26f), atmFrameMat);
        drawCube(Vec3(pos.x, 1.22f, pos.z + 0.48f), Vec3(0.28f, 0.01f, 0.18f), chromeMat);

        drawCube(Vec3(pos.x + 0.28f, 1.35f, pos.z + 0.435f), Vec3(0.14f, 0.016f, 0.005f), cardSlotGlow);

        Material cashMouthMat;
        cashMouthMat.color = Vec3(0.04f, 0.04f, 0.05f);
        drawCube(Vec3(pos.x, 0.92f, pos.z + 0.435f), Vec3(0.42f, 0.07f, 0.02f), cashMouthMat);

        Material cameraMat;
        cameraMat.color = Vec3(0.02f, 0.02f, 0.03f);
        drawSphere(Vec3(pos.x, 2.30f, pos.z + 0.35f), Vec3(0.08f, 0.08f, 0.08f), cameraMat);

        if (g_activeMachine == (atmNumber == 1 ? MACHINE_ATM_1 : MACHINE_ATM_2)) {
            Material haloMat;
            haloMat.color = Vec3(0.10f, 0.70f, 1.0f);
            haloMat.emission = Vec3(0.25f, 1.2f, 2.0f);
            drawCube(Vec3(pos.x, 0.02f, pos.z + 0.95f), Vec3(1.10f, 0.02f, 0.85f), haloMat);
        }
    };

    drawSingleATM(Vec3(-6.5f, 0.0f, -9.2f), 1);
    drawSingleATM(Vec3(-3.5f, 0.0f, -9.2f), 2);

    Material partitionMat;
    partitionMat.color = Vec3(0.28f, 0.30f, 0.35f);
    drawCube(Vec3(-5.0f, 1.25f, -9.0f), Vec3(0.08f, 2.20f, 1.40f), partitionMat);
}

// 8. Room 3: Two Full Modern Ticket Vending Kiosks
void drawTicketMachines() {
    Material kioskBodyMat;
    kioskBodyMat.color = Vec3(0.20f, 0.22f, 0.25f);
    kioskBodyMat.shininess = 64.0f;

    Material transitOrangeMat;
    transitOrangeMat.color = Vec3(0.92f, 0.55f, 0.08f);

    Material screenMat;
    screenMat.color = Vec3(0.04f, 0.16f, 0.20f);
    screenMat.emission = Vec3(0.15f, 0.85f, 0.95f);

    Material ticketCardMat;
    ticketCardMat.color = Vec3(0.96f, 0.96f, 0.96f);
    ticketCardMat.emission = Vec3(0.3f, 0.3f, 0.3f);

    auto drawSingleTicketKiosk = [&](const Vec3& pos, int kioskNumber) {
        drawCube(Vec3(pos.x, 1.15f, pos.z), Vec3(1.05f, 2.25f, 0.80f), kioskBodyMat);
        drawCube(Vec3(pos.x, 2.15f, pos.z + 0.05f), Vec3(1.08f, 0.22f, 0.82f), transitOrangeMat);

        Material signMat;
        signMat.color = Vec3(1.0f, 0.95f, 0.85f);
        signMat.emission = Vec3(1.3f, 1.0f, 0.25f);
        drawCube(Vec3(pos.x, 2.15f, pos.z + 0.415f), Vec3(0.92f, 0.14f, 0.01f), signMat);

        drawCube(Vec3(pos.x, 1.52f, pos.z + 0.41f), Vec3(0.68f, 0.48f, 0.01f), screenMat);

        Material tapPadMat;
        tapPadMat.color = Vec3(0.12f, 0.75f, 0.35f);
        tapPadMat.emission = Vec3(0.2f, 1.2f, 0.35f);
        drawCylinder(Vec3(pos.x + 0.26f, 1.16f, pos.z + 0.46f), Vec3(0.12f, 0.02f, 0.12f), tapPadMat);

        Material chromeMat;
        chromeMat.color = Vec3(0.85f, 0.85f, 0.90f);
        drawCube(Vec3(pos.x - 0.26f, 1.16f, pos.z + 0.46f), Vec3(0.15f, 0.04f, 0.15f), chromeMat);

        Material slotMat;
        slotMat.color = Vec3(0.04f, 0.04f, 0.05f);
        drawCube(Vec3(pos.x, 0.90f, pos.z + 0.41f), Vec3(0.38f, 0.09f, 0.02f), slotMat);

        if (g_ticketSlideAnim > 0.01f) {
            float slideZ = 0.41f + g_ticketSlideAnim * 0.10f;
            drawCube(Vec3(pos.x, 0.90f, pos.z + slideZ), Vec3(0.20f, 0.005f, 0.14f), ticketCardMat);
        }

        if (g_activeMachine == (kioskNumber == 1 ? MACHINE_TICKET_1 : MACHINE_TICKET_2)) {
            Material haloMat;
            haloMat.color = Vec3(0.95f, 0.70f, 0.10f);
            haloMat.emission = Vec3(1.5f, 1.0f, 0.1f);
            drawCube(Vec3(pos.x, 0.02f, pos.z + 0.95f), Vec3(1.10f, 0.02f, 0.85f), haloMat);
        }
    };

    drawSingleTicketKiosk(Vec3(3.5f, 0.0f, -9.2f), 1);
    drawSingleTicketKiosk(Vec3(6.5f, 0.0f, -9.2f), 2);
}

// 9. Realistic Human 3D Avatar (Visible in Free Mode, Animated Walking & Stride!)
void drawRealisticHumanAvatar(const Vec3& pos, float yaw) {
    Material skinMat;
    skinMat.color = Vec3(0.88f, 0.72f, 0.60f); // Warm natural skin tone
    skinMat.shininess = 16.0f;

    Material hairMat;
    hairMat.color = Vec3(0.18f, 0.12f, 0.08f); // Dark hair

    Material glassesMat;
    glassesMat.color = Vec3(0.06f, 0.06f, 0.08f);
    glassesMat.shininess = 128.0f;

    Material jacketMat;
    jacketMat.color = Vec3(0.18f, 0.45f, 0.75f); // Stylish blue athletic jacket
    jacketMat.shininess = 24.0f;

    Material shirtCollarMat;
    shirtCollarMat.color = Vec3(0.95f, 0.95f, 0.95f);

    Material jeansMat;
    jeansMat.color = Vec3(0.14f, 0.17f, 0.24f); // Indigo denim jeans

    Material beltMat;
    beltMat.color = Vec3(0.10f, 0.08f, 0.06f);

    Material shoeMat;
    shoeMat.color = Vec3(0.16f, 0.18f, 0.22f);

    Material soleMat;
    soleMat.color = Vec3(0.95f, 0.95f, 0.95f); // White sneaker soles

    Mat4 root = Mat4::Identity();
    root = translate(root, pos);
    root = rotate(root, toRadians(yaw), Vec3(0, 1, 0));

    // Dynamic walking limb swing animation
    float legSwing = g_isWalking ? std::sin(g_walkAnimTimer) * 22.0f : 0.0f;
    float armSwing = g_isWalking ? -std::sin(g_walkAnimTimer) * 20.0f : 0.0f;

    // Head
    Mat4 headM = translate(root, Vec3(0.0f, 1.64f, 0.0f));
    headM = scale(headM, Vec3(0.20f, 0.24f, 0.20f));
    drawShape(g_meshSphere, headM, skinMat);

    // Hair Top & Back Styling
    Mat4 hairM = translate(root, Vec3(0.0f, 1.74f, -0.02f));
    hairM = scale(hairM, Vec3(0.21f, 0.10f, 0.22f));
    drawShape(g_meshSphere, hairM, hairMat);

    Mat4 hairBackM = translate(root, Vec3(0.0f, 1.66f, -0.07f));
    hairBackM = scale(hairBackM, Vec3(0.205f, 0.18f, 0.12f));
    drawShape(g_meshSphere, hairBackM, hairMat);

    // Sunglasses / Eyes
    Mat4 glassM = translate(root, Vec3(0.0f, 1.66f, 0.095f));
    glassM = scale(glassM, Vec3(0.16f, 0.045f, 0.03f));
    drawShape(g_meshCube, glassM, glassesMat);

    // Neck
    Mat4 neckM = translate(root, Vec3(0.0f, 1.48f, 0.0f));
    neckM = scale(neckM, Vec3(0.10f, 0.12f, 0.10f));
    drawShape(g_meshCylinder, neckM, skinMat);

    // Torso / Athletic Jacket
    Mat4 torsoM = translate(root, Vec3(0.0f, 1.18f, 0.0f));
    torsoM = scale(torsoM, Vec3(0.42f, 0.52f, 0.24f));
    drawShape(g_meshCube, torsoM, jacketMat);

    // White Shirt V-Collar
    Mat4 collarM = translate(root, Vec3(0.0f, 1.38f, 0.122f));
    collarM = scale(collarM, Vec3(0.12f, 0.12f, 0.01f));
    drawShape(g_meshCube, collarM, shirtCollarMat);

    // Metallic Front Zipper
    Material zipMat;
    zipMat.color = Vec3(0.82f, 0.85f, 0.90f);
    zipMat.shininess = 64.0f;
    Mat4 zipM = translate(root, Vec3(0.0f, 1.15f, 0.122f));
    zipM = scale(zipM, Vec3(0.015f, 0.44f, 0.005f));
    drawShape(g_meshCube, zipM, zipMat);

    // Belt & Chrome Buckle
    Mat4 beltM = translate(root, Vec3(0.0f, 0.90f, 0.0f));
    beltM = scale(beltM, Vec3(0.40f, 0.06f, 0.23f));
    drawShape(g_meshCube, beltM, beltMat);

    Material buckleMat;
    buckleMat.color = Vec3(0.88f, 0.90f, 0.94f);
    buckleMat.shininess = 128.0f;
    Mat4 buckleM = translate(root, Vec3(0.0f, 0.90f, 0.12f));
    buckleM = scale(buckleM, Vec3(0.08f, 0.045f, 0.01f));
    drawShape(g_meshCube, buckleM, buckleMat);

    // Shoulders & Arms (with dynamic walk swing)
    Material cuffMat;
    cuffMat.color = Vec3(0.14f, 0.38f, 0.65f);

    // Left Arm
    Mat4 armLM = root;
    armLM = translate(armLM, Vec3(-0.26f, 1.34f, 0.0f));
    armLM = rotate(armLM, toRadians(armSwing), Vec3(1, 0, 0));
    Mat4 armLMesh = translate(armLM, Vec3(0.0f, -0.28f, 0.0f));
    armLMesh = scale(armLMesh, Vec3(0.10f, 0.54f, 0.10f));
    drawShape(g_meshCylinder, armLMesh, jacketMat);
    // Left Cuff & Hand
    Mat4 cuffLM = translate(armLM, Vec3(0.0f, -0.52f, 0.0f));
    cuffLM = scale(cuffLM, Vec3(0.105f, 0.04f, 0.105f));
    drawShape(g_meshCylinder, cuffLM, cuffMat);
    Mat4 handLM = translate(armLM, Vec3(0.0f, -0.58f, 0.0f));
    handLM = scale(handLM, Vec3(0.07f, 0.08f, 0.07f));
    drawShape(g_meshSphere, handLM, skinMat);

    // Right Arm
    Mat4 armRM = root;
    armRM = translate(armRM, Vec3(0.26f, 1.34f, 0.0f));
    armRM = rotate(armRM, toRadians(-armSwing), Vec3(1, 0, 0));
    Mat4 armRMesh = translate(armRM, Vec3(0.0f, -0.28f, 0.0f));
    armRMesh = scale(armRMesh, Vec3(0.10f, 0.54f, 0.10f));
    drawShape(g_meshCylinder, armRMesh, jacketMat);
    // Right Cuff & Hand
    Mat4 cuffRM = translate(armRM, Vec3(0.0f, -0.52f, 0.0f));
    cuffRM = scale(cuffRM, Vec3(0.105f, 0.04f, 0.105f));
    drawShape(g_meshCylinder, cuffRM, cuffMat);
    Mat4 handRM = translate(armRM, Vec3(0.0f, -0.58f, 0.0f));
    handRM = scale(handRM, Vec3(0.07f, 0.08f, 0.07f));
    drawShape(g_meshSphere, handRM, skinMat);

    // Left Leg (with dynamic stride swing)
    Mat4 legLM = root;
    legLM = translate(legLM, Vec3(-0.11f, 0.86f, 0.0f));
    legLM = rotate(legLM, toRadians(legSwing), Vec3(1, 0, 0));
    Mat4 legLMesh = translate(legLM, Vec3(0.0f, -0.38f, 0.0f));
    legLMesh = scale(legLMesh, Vec3(0.13f, 0.72f, 0.14f));
    drawShape(g_meshCylinder, legLMesh, jeansMat);
    // Left Shoe & White Sole
    Mat4 shoeLM = translate(legLM, Vec3(0.0f, -0.74f, 0.05f));
    shoeLM = scale(shoeLM, Vec3(0.14f, 0.08f, 0.24f));
    drawShape(g_meshCube, shoeLM, shoeMat);
    Mat4 soleLM = translate(legLM, Vec3(0.0f, -0.79f, 0.05f));
    soleLM = scale(soleLM, Vec3(0.145f, 0.025f, 0.245f));
    drawShape(g_meshCube, soleLM, soleMat);

    // Right Leg (with dynamic stride swing)
    Mat4 legRM = root;
    legRM = translate(legRM, Vec3(0.11f, 0.86f, 0.0f));
    legRM = rotate(legRM, toRadians(-legSwing), Vec3(1, 0, 0));
    Mat4 legRMesh = translate(legRM, Vec3(0.0f, -0.38f, 0.0f));
    legRMesh = scale(legRMesh, Vec3(0.13f, 0.72f, 0.14f));
    drawShape(g_meshCylinder, legRMesh, jeansMat);
    // Right Shoe & White Sole
    Mat4 shoeRM = translate(legRM, Vec3(0.0f, -0.74f, 0.05f));
    shoeRM = scale(shoeRM, Vec3(0.14f, 0.08f, 0.24f));
    drawShape(g_meshCube, shoeRM, shoeMat);
    Mat4 soleRM = translate(legRM, Vec3(0.0f, -0.79f, 0.05f));
    soleRM = scale(soleRM, Vec3(0.145f, 0.025f, 0.245f));
    drawShape(g_meshCube, soleRM, soleMat);
}

// -----------------------------------------------------------------------------
// Master Scene Rendering Pass
// -----------------------------------------------------------------------------
void renderScene() {
    // 1. Expansive Green Landscape & Natural Procedural Trees
    drawLandscapeAndTrees();

    // 2. 160-Meter Highway Road & Sidewalk
    drawRoadAndSidewalk();

    // 3. Grand 3-Room Structure
    drawRoomStructure();

    // 4. Waiting Lounge Furniture
    drawWaitingRoomFurniture();

    // 5. Room 2: 2 Full Walk-In Bank ATM Booths
    drawATMBooths();

    // 6. Room 3: 2 Modern Transit Ticket Vending Kiosks
    drawTicketMachines();

    // 7. Room 1: 3 Ultra-Detailed Vending Machines (100% Faithful Hierarchical Model)
    // Machine 1: Red Cold Drinks
    Mat4 vRoot1 = translate(Mat4::Identity(), VENDING_POS_1);
    vRoot1 = rotate(vRoot1, toRadians(VENDING_ROTY), Vec3(0, 1, 0));
    Material redMat;
    redMat.color = Vec3(0.72f, 0.12f, 0.14f);
    drawFullVendingMachine(vRoot1, 1, redMat, "COLD DRINKS", Vec3(0.9f, 0.2f, 0.2f));

    // Machine 2: Orange Crispy Snacks
    Mat4 vRoot2 = translate(Mat4::Identity(), VENDING_POS_2);
    vRoot2 = rotate(vRoot2, toRadians(VENDING_ROTY), Vec3(0, 1, 0));
    Material orangeMat;
    orangeMat.color = Vec3(0.85f, 0.44f, 0.10f);
    drawFullVendingMachine(vRoot2, 2, orangeMat, "CRISPY SNACKS", Vec3(1.1f, 0.6f, 0.15f));

    // Machine 3: Royal Blue Sweet Delights
    Mat4 vRoot3 = translate(Mat4::Identity(), VENDING_POS_3);
    vRoot3 = rotate(vRoot3, toRadians(VENDING_ROTY), Vec3(0, 1, 0));
    Material blueMat;
    blueMat.color = Vec3(0.15f, 0.35f, 0.70f);
    drawFullVendingMachine(vRoot3, 3, blueMat, "SWEET DELIGHTS", Vec3(0.2f, 0.6f, 1.3f));

    // 8. Render Stocked Vending Products (Hierarchical Transforms)
    for (const auto& s : g_slots) {
        if (s.state == STATE_DISPENSED_GONE) continue;

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

    // 9. Realistic Human Avatar (Visible in Free Orbit Mode with animated walking!)
    if (g_cameraMode == MODE_FREE_ORBIT) {
        drawRealisticHumanAvatar(g_player.pos, g_player.yaw);
    }

    // 10. Semi-Transparent Vending Machine Glass Pass (Rendered after products to guarantee visibility!)
    drawVendingMachineGlassPanes();

    // 11. Semi-Transparent Ceiling Pass:
    // Visible with opacity = 0.3 in First Person Mode, 100% Transparent in Free Orbit Mode!
    float ceilAlpha = (g_cameraMode == MODE_FIRST_PERSON) ? 0.30f : 0.0f;
    drawCeilings(ceilAlpha);
}

// -----------------------------------------------------------------------------
// GLFW Input Callbacks
// -----------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)window;
    (void)mods;
    if (g_cameraMode == MODE_FREE_ORBIT) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            g_camera.isDraggingLeft = (action == GLFW_PRESS);
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            g_camera.isDraggingRight = (action == GLFW_PRESS);
        }
    }
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    static double s_lastX = xpos;
    static double s_lastY = ypos;
    static bool s_firstMouse = true;

    if (s_firstMouse) {
        s_lastX = xpos;
        s_lastY = ypos;
        s_firstMouse = false;
    }

    double dx = xpos - s_lastX;
    double dy = ypos - s_lastY;
    s_lastX = xpos;
    s_lastY = ypos;

    if (g_cameraMode == MODE_FIRST_PERSON) {
        g_player.yaw += (float)dx * 0.16f;
        g_player.pitch -= (float)dy * 0.16f;
        g_player.pitch = std::clamp(g_player.pitch, -85.0f, 85.0f);
    } else {
        if (g_camera.isDraggingLeft) {
            g_camera.yaw += (float)dx * 0.35f;
            g_camera.pitch += (float)dy * 0.35f;
            g_camera.pitch = std::clamp(g_camera.pitch, -10.0f, 85.0f);
        } else if (g_camera.isDraggingRight) {
            float radYaw = toRadians(g_camera.yaw);
            Vec3 right = normalize(Vec3(std::cos(radYaw), 0.0f, -std::sin(radYaw)));
            Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
            g_camera.target -= right * ((float)dx * 0.005f);
            g_camera.target += up * ((float)dy * 0.005f);
        }
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;
    if (g_cameraMode == MODE_FREE_ORBIT) {
        g_camera.distance -= (float)yoffset * 0.8f;
        g_camera.distance = std::clamp(g_camera.distance, 2.0f, 60.0f);
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    (void)mods;
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, true);
        }
        // [TAB] or [F]: Toggle between First Person View and Free Orbit Mode
        else if (key == GLFW_KEY_TAB || key == GLFW_KEY_F) {
            if (g_cameraMode == MODE_FIRST_PERSON) {
                g_cameraMode = MODE_FREE_ORBIT;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                std::cout << "[CAMERA] Switched to FREE ORBIT MODE (Ceilings Transparent from Top)" << std::endl;
            } else {
                g_cameraMode = MODE_FIRST_PERSON;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                std::cout << "[CAMERA] Switched to FIRST PERSON MODE (Ceilings 0.3 Opacity)" << std::endl;
            }
        }
        // Machine Interaction Keys [1] - [4]
        else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) {
            int num = key - GLFW_KEY_1 + 1; // 1 to 4

            if (g_activeMachine == MACHINE_ATM_1 || g_activeMachine == MACHINE_ATM_2) {
                g_atmFeedbackTimer = 3.5f;
                if (num == 1) g_atmFeedbackMsg = "ATM: ACCOUNT BALANCE: $3,450.00";
                else if (num == 2) g_atmFeedbackMsg = "ATM: DISPENSING $20 CASH... PLEASE TAKE BILLS";
                else if (num == 3) g_atmFeedbackMsg = "ATM: DISPENSING $100 CASH... PLEASE TAKE BILLS";
                else if (num == 4) g_atmFeedbackMsg = "ATM: PRINTING RECEIPT... TRANSACTION COMPLETE";
                g_statusMessage = g_atmFeedbackMsg;
                g_statusTimer = 4.0f;
                std::cout << "[" << g_atmFeedbackMsg << "]" << std::endl;
            }
            else if (g_activeMachine == MACHINE_TICKET_1 || g_activeMachine == MACHINE_TICKET_2) {
                g_ticketFeedbackTimer = 3.5f;
                if (num == 1) g_ticketFeedbackMsg = "TICKET: METRO SINGLE RIDE ($2.50) PRINTED";
                else if (num == 2) g_ticketFeedbackMsg = "TICKET: 24-HOUR UNLIMITED DAY PASS ($8.00) PRINTED";
                else if (num == 3) g_ticketFeedbackMsg = "TICKET: 7-DAY COMMUTER RAIL PASS ($30.00) PRINTED";
                else g_ticketFeedbackMsg = "TICKET: TRANSIT RECEIPT PRINTED";
                g_statusMessage = g_ticketFeedbackMsg;
                g_statusTimer = 4.0f;
                std::cout << "[" << g_ticketFeedbackMsg << "]" << std::endl;
            }
            else if (g_activeMachine == MACHINE_VENDING_DRINKS) {
                selectAndVendItem(num - 1);
            }
            else if (g_activeMachine == MACHINE_VENDING_SNACKS) {
                selectAndVendItem(6 + (num - 1));
            }
            else if (g_activeMachine == MACHINE_VENDING_SWEETS) {
                selectAndVendItem(12 + (num - 1));
            }
            else {
                selectAndVendItem(num - 1);
            }
        }
        else if (key == GLFW_KEY_5 || key == GLFW_KEY_6) {
            int num = key - GLFW_KEY_1 + 1; // 5 or 6
            if (g_activeMachine == MACHINE_VENDING_DRINKS) selectAndVendItem(num - 1);
            else if (g_activeMachine == MACHINE_VENDING_SNACKS) selectAndVendItem(6 + (num - 1));
            else if (g_activeMachine == MACHINE_VENDING_SWEETS) selectAndVendItem(12 + (num - 1));
            else selectAndVendItem(num - 1);
        }
        // [L] or [ENTER]: Restock / Load all items back onto shelves
        else if (key == GLFW_KEY_L || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            restockAllItems();
        }
        // [O]: Toggle Collection Door Push / Open & collect resting item
        else if (key == GLFW_KEY_O) {
            g_isDoorManuallyOpened = !g_isDoorManuallyOpened;
            for (auto& s : g_slots) {
                if (s.state == STATE_RESTING_IN_TRAY) {
                    s.state = STATE_DISPENSED_GONE;
                    g_statusMessage = "COLLECTED " + s.name + " - ENJOY!";
                    g_statusTimer = 3.0f;
                    std::cout << "[TAKE] Collected " << s.name << " from delivery tray!" << std::endl;
                }
            }
            std::cout << "[DOOR] Collection Door " << (g_isDoorManuallyOpened ? "OPENED" : "CLOSED") << std::endl;
        }
        // [Numpad 1] or [F1]: Toggle Directional Sunlight (Light 1)
        else if (key == GLFW_KEY_KP_1 || key == GLFW_KEY_F1) {
            g_dirLight.enabled = !g_dirLight.enabled;
            std::cout << "[LIGHT 1] Directional Sunlight (Numpad 1): " << (g_dirLight.enabled ? "ON" : "OFF") << std::endl;
        }
        // [Numpad 2] or [F2]: Toggle Room Downlights (Light 2)
        else if (key == GLFW_KEY_KP_2 || key == GLFW_KEY_F2) {
            for (int i = 0; i < 4; ++i) g_roomPointLights[i].enabled = !g_roomPointLights[i].enabled;
            std::cout << "[LIGHT 2] Architectural Room Downlights (Numpad 2): " << (g_roomPointLights[0].enabled ? "ON" : "OFF") << std::endl;
        }
        // [Numpad 3] or [F3]: Toggle Spot Ceiling Lights (Light 3)
        else if (key == GLFW_KEY_KP_3 || key == GLFW_KEY_F3) {
            g_ceilingSpotLight.enabled = !g_ceilingSpotLight.enabled;
            std::cout << "[LIGHT 3] Spot Ceiling Light (Numpad 3): " << (g_ceilingSpotLight.enabled ? "ON" : "OFF") << std::endl;
        }
        // [Numpad 4] or [F4]: Toggle Vending Cabinet Interior Lamps
        else if (key == GLFW_KEY_KP_4 || key == GLFW_KEY_F4) {
            g_cabinetLightsEnabled = !g_cabinetLightsEnabled;
            std::cout << "[LIGHT 4] Vending Machine Cabinet Lamps (Numpad 4): " << (g_cabinetLightsEnabled ? "ON" : "OFF") << std::endl;
        }
        // [V]: Cycle Preset Camera Views (in Free Mode)
        else if (key == GLFW_KEY_V) {
            g_camera.cycleNextPreset();
            std::cout << "[CAMERA] Preset changed to: " << g_camera.getPresetName() << std::endl;
        }
    }
}

// -----------------------------------------------------------------------------
// Upload Lighting Uniforms to GPU
// -----------------------------------------------------------------------------
void uploadLightUniforms(const Mat4& viewMat, const Mat4& projMat, const Vec3& camPos) {
    glUseProgram(g_shaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "view"), 1, GL_FALSE, viewMat.m);
    glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "projection"), 1, GL_FALSE, projMat.m);
    glUniform3f(glGetUniformLocation(g_shaderProgram, "viewPos"), camPos.x, camPos.y, camPos.z);

    // 1. Directional Sunlight
    glUniform3f(glGetUniformLocation(g_shaderProgram, "dirLight.direction"), g_dirLight.direction.x, g_dirLight.direction.y, g_dirLight.direction.z);
    glUniform3f(glGetUniformLocation(g_shaderProgram, "dirLight.color"), g_dirLight.color.x, g_dirLight.color.y, g_dirLight.color.z);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "dirLight.intensity"), g_dirLight.intensity);
    glUniform1i(glGetUniformLocation(g_shaderProgram, "dirLight.enabled"), g_dirLight.enabled ? 1 : 0);

    // 2. Four Architectural Room Downlights
    for (int i = 0; i < 4; ++i) {
        std::string base = "roomLights[" + std::to_string(i) + "].";
        glUniform3f(glGetUniformLocation(g_shaderProgram, (base + "position").c_str()), g_roomPointLights[i].position.x, g_roomPointLights[i].position.y, g_roomPointLights[i].position.z);
        glUniform3f(glGetUniformLocation(g_shaderProgram, (base + "color").c_str()), g_roomPointLights[i].color.x, g_roomPointLights[i].color.y, g_roomPointLights[i].color.z);
        glUniform1f(glGetUniformLocation(g_shaderProgram, (base + "intensity").c_str()), g_roomPointLights[i].intensity);
        glUniform1f(glGetUniformLocation(g_shaderProgram, (base + "constant").c_str()), g_roomPointLights[i].constant);
        glUniform1f(glGetUniformLocation(g_shaderProgram, (base + "linear").c_str()), g_roomPointLights[i].linear);
        glUniform1f(glGetUniformLocation(g_shaderProgram, (base + "quadratic").c_str()), g_roomPointLights[i].quadratic);
        glUniform1i(glGetUniformLocation(g_shaderProgram, (base + "enabled").c_str()), g_roomPointLights[i].enabled ? 1 : 0);
    }

    // 3. Tray Indicator Light
    glUniform3f(glGetUniformLocation(g_shaderProgram, "trayLight.position"), g_trayIndicatorLight.position.x, g_trayIndicatorLight.position.y, g_trayIndicatorLight.position.z);
    glUniform3f(glGetUniformLocation(g_shaderProgram, "trayLight.color"), g_trayIndicatorLight.color.x, g_trayIndicatorLight.color.y, g_trayIndicatorLight.color.z);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "trayLight.intensity"), g_trayIndicatorLight.intensity);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "trayLight.constant"), g_trayIndicatorLight.constant);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "trayLight.linear"), g_trayIndicatorLight.linear);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "trayLight.quadratic"), g_trayIndicatorLight.quadratic);
    glUniform1i(glGetUniformLocation(g_shaderProgram, "trayLight.enabled"), g_trayIndicatorLight.enabled ? 1 : 0);

    // 4. Spot Light
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
    std::cout << "   GRAND 3D VENDING FACILITY & TRANSIT HUB - OPENGL 3.3 ASSIGNMENT  \n";
    std::cout << "  Student Name: Md. Nayeem | Roll: 2107050                          \n";
    std::cout << "====================================================================\n";
    std::cout << "CONTROLS:\n";
    std::cout << "  [W], [A], [S], [D] : Walk / Move human (in BOTH FPV & Free Orbit!)\n";
    std::cout << "  [SPACE]            : Jump human with gravity physics\n";
    std::cout << "  Mouse Look         : 360-deg Look in FPV | Left-Drag Orbit in Free Mode\n";
    std::cout << "  [TAB] / [F]        : Toggle FIRST PERSON <-> FREE ORBIT CAMERA\n";
    std::cout << "MACHINE CONTROLS (Walk in front of any machine to interact):\n";
    std::cout << "  [1] - [4]          : Interact with nearest Machine (Vend / ATM / Ticket)\n";
    std::cout << "  [L] / [ENTER]      : Load / Restock all 3 Vending Machines\n";
    std::cout << "  [O]                : Push / Open Collection Door flap\n";
    std::cout << "LIGHTING (Numpad / Function Keys):\n";
    std::cout << "  [Numpad 1] / [F1]  : Toggle Directional Sunlight (Light 1)\n";
    std::cout << "  [Numpad 2] / [F2]  : Toggle Room Downlights (Light 2)\n";
    std::cout << "  [Numpad 3] / [F3]  : Toggle Spot Ceiling Lights (Light 3)\n";
    std::cout << "  [Numpad 4] / [F4]  : Toggle Vending Cabinet Lamps (Light 4)\n";
    std::cout << "  [V]                : Cycle Camera Presets (in Free Orbit Mode)\n";
    std::cout << "====================================================================\n\n";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Grand 3D Vending Facility - Md. Nayeem (2107050)", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    int version = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
    if (!version) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL Version: " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version) << std::endl;
    std::cout << "Renderer:       " << glGetString(GL_RENDERER) << std::endl << std::endl;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_CULL_FACE);

    g_shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    g_meshCube     = createCubeMesh();
    g_meshCylinder = createCylinderMesh(32);
    g_meshSphere   = createSphereMesh(24, 28);
    g_meshCoil     = createCoilMesh(5, 24, 0.055f, 0.005f, 0.22f);
    g_meshPlane    = createPlaneMesh();

    // 1. Natural Directional Sunlight (Soft, warm, realistic, no blowout)
    g_dirLight.direction = normalize(Vec3(-0.55f, -1.1f, -0.6f));
    g_dirLight.color = Vec3(1.0f, 0.97f, 0.92f);
    g_dirLight.intensity = 0.68f;
    g_dirLight.enabled = true;

    // 2. Four Architectural Room Downlights (Carefully positioned to avoid glare)
    // Downlight 0: Waiting Hall West
    g_roomPointLights[0].position = Vec3(-4.0f, 3.8f, 3.0f);
    g_roomPointLights[0].color = Vec3(1.0f, 0.98f, 0.92f);
    g_roomPointLights[0].intensity = 0.55f;
    g_roomPointLights[0].constant = 1.0f;
    g_roomPointLights[0].linear = 0.12f;
    g_roomPointLights[0].quadratic = 0.04f;
    g_roomPointLights[0].enabled = true;

    // Downlight 1: Waiting Hall East
    g_roomPointLights[1].position = Vec3(4.0f, 3.8f, 3.0f);
    g_roomPointLights[1].color = Vec3(1.0f, 0.98f, 0.92f);
    g_roomPointLights[1].intensity = 0.55f;
    g_roomPointLights[1].constant = 1.0f;
    g_roomPointLights[1].linear = 0.12f;
    g_roomPointLights[1].quadratic = 0.04f;
    g_roomPointLights[1].enabled = true;

    // Downlight 2: ATM Banking Lounge
    g_roomPointLights[2].position = Vec3(-5.0f, 3.8f, -6.0f);
    g_roomPointLights[2].color = Vec3(0.85f, 0.92f, 1.0f);
    g_roomPointLights[2].intensity = 0.50f;
    g_roomPointLights[2].constant = 1.0f;
    g_roomPointLights[2].linear = 0.14f;
    g_roomPointLights[2].quadratic = 0.05f;
    g_roomPointLights[2].enabled = true;

    // Downlight 3: Ticket & Transit Hub
    g_roomPointLights[3].position = Vec3(5.0f, 3.8f, -6.0f);
    g_roomPointLights[3].color = Vec3(1.0f, 0.95f, 0.85f);
    g_roomPointLights[3].intensity = 0.50f;
    g_roomPointLights[3].constant = 1.0f;
    g_roomPointLights[3].linear = 0.14f;
    g_roomPointLights[3].quadratic = 0.05f;
    g_roomPointLights[3].enabled = true;

    // 3. Track Ceiling Spotlight directed smoothly on vending row
    g_ceilingSpotLight.position = Vec3(-6.0f, 3.9f, 2.0f);
    g_ceilingSpotLight.direction = normalize(Vec3(-0.4f, -1.0f, 0.0f));
    g_ceilingSpotLight.color = Vec3(1.0f, 0.94f, 0.84f);
    g_ceilingSpotLight.intensity = 0.70f;
    g_ceilingSpotLight.cutOff = std::cos(toRadians(45.0f));
    g_ceilingSpotLight.outerCutOff = std::cos(toRadians(60.0f));
    g_ceilingSpotLight.constant = 1.0f;
    g_ceilingSpotLight.linear = 0.10f;
    g_ceilingSpotLight.quadratic = 0.035f;
    g_ceilingSpotLight.enabled = true;

    // 4. Collection Tray Indicator Light
    g_trayIndicatorLight.position = Vec3(-7.3f, 0.45f, 4.5f);
    g_trayIndicatorLight.color = Vec3(0.2f, 0.6f, 1.0f);
    g_trayIndicatorLight.intensity = 1.8f;
    g_trayIndicatorLight.constant = 1.0f;
    g_trayIndicatorLight.linear = 0.7f;
    g_trayIndicatorLight.quadratic = 1.8f;
    g_trayIndicatorLight.enabled = false;

    // Initialize trees and product slots
    initTrees();
    initProductSlots();

    double lastTime = glfwGetTime();
    double titleTimer = 0.0;
    int frameCount = 0;

    while (!glfwWindowShouldClose(window)) {
        double frameStartTime = glfwGetTime();
        float dt = static_cast<float>(frameStartTime - lastTime);
        lastTime = frameStartTime;
        if (dt > 0.05f) dt = 0.05f;

        // Player input works in both FPV and Free Orbit Mode!
        updatePlayerInput(window, dt);
        updateSimulation(dt);

        titleTimer += dt;
        frameCount++;
        if (titleTimer >= 0.25) {
            int fps = (int)(frameCount / titleTimer);
            std::string modeStr = (g_cameraMode == MODE_FIRST_PERSON) ? "[FPV: Walk]" : "[FREE: Orbit Cam (WASD: Move Avatar)]";
            std::string machStr = "";
            if (g_activeMachine == MACHINE_VENDING_DRINKS) machStr = " | AT: DRINKS VENDING (Press 1-4)";
            else if (g_activeMachine == MACHINE_VENDING_SNACKS) machStr = " | AT: SNACKS VENDING (Press 1-3)";
            else if (g_activeMachine == MACHINE_VENDING_SWEETS) machStr = " | AT: SWEETS VENDING (Press 1-3)";
            else if (g_activeMachine == MACHINE_ATM_1 || g_activeMachine == MACHINE_ATM_2) machStr = " | AT: BANK ATM (Press 1-4)";
            else if (g_activeMachine == MACHINE_TICKET_1 || g_activeMachine == MACHINE_TICKET_2) machStr = " | AT: TICKET KIOSK (Press 1-3)";

            std::string title = "3D Facility | " + modeStr + machStr + " | " + g_statusMessage + " | FPS: " + std::to_string(fps);
            glfwSetWindowTitle(window, title.c_str());
            titleTimer = 0.0;
            frameCount = 0;
        }

        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);

        // Soft, aesthetic atmospheric sky blue
        glClearColor(0.48f, 0.68f, 0.88f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (displayH > 0) ? ((float)displayW / (float)displayH) : 1.777f;
        Mat4 projectionMat = perspective(toRadians(50.0f), aspect, 0.20f, 160.0f);

        Mat4 viewMat;
        Vec3 camPos;
        if (g_cameraMode == MODE_FIRST_PERSON) {
            viewMat = g_player.getViewMatrix();
            camPos  = g_player.getEyePos();
        } else {
            viewMat = g_camera.getViewMatrix();
            camPos  = g_camera.getPosition();
        }

        uploadLightUniforms(viewMat, projectionMat, camPos);

        renderScene();

        glfwSwapBuffers(window);
        glfwPollEvents();

        double frameEndTime = glfwGetTime();
        double frameDuration = frameEndTime - frameStartTime;
        if (frameDuration < TARGET_FRAME_TIME) {
            double sleepSeconds = TARGET_FRAME_TIME - frameDuration;
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepSeconds));
        }
    }

    g_meshCube.cleanup();
    g_meshCylinder.cleanup();
    g_meshSphere.cleanup();
    g_meshCoil.cleanup();
    g_meshPlane.cleanup();

    glDeleteProgram(g_shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
