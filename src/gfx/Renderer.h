// =============================================================================
//  Renderer.h  -  Instanced renderer for basic shapes.
//
//  * Static world: shapes are collected once, grouped into 20 m chunks and
//    stored in GPU instance buffers. Each frame only visible chunks are drawn
//    (frustum + distance culling) -> very few draw calls (good for weak GPUs).
//  * Dynamic objects (people, cars, machines, doors ...) are re-submitted every
//    frame into streaming instance buffers.
//  * Enhanced mode: textures, shadow mapping (PCF), fog, sky with clouds/stars,
//    wind animation, tone mapping.  Basic mode: plain coloured Phong shading.
// =============================================================================
#pragma once
#include <vector>
#include <map>
#include <string>
#include "Shader.h"
#include "Shapes.h"
#include "Textures.h"
#include "Draw.h"

struct RenderSettings {
    bool enhanced = true;   // master switch:  Enhanced  <->  Basic
    bool textures = true;
    bool shadows = true;
    bool fog = true;
    bool wind = true;
    bool occlusion = true;   // GPU occlusion queries skip hidden chunks
    int shadowRes = 2048;
    float shadowRadius = 32.0f;
};

struct PointLightGPU { Vec3 pos; Vec3 color; float range; };
struct SpotLightGPU { Vec3 pos; Vec3 dir; Vec3 color; float range; float cosInner; float cosOuter; };

struct Environment {
    Vec3 lightDir = normalize(Vec3(0.4f, 0.8f, 0.3f));  // direction TO the sun (or moon)
    Vec3 lightColor = Vec3(1.0f);                        // colour * intensity
    Vec3 sunDir = Vec3(0, 1, 0), moonDir = Vec3(0, -1, 0);
    Vec3 skyAmbient = Vec3(0.35f), groundAmbient = Vec3(0.2f);
    Vec3 zenith = Vec3(0.25f, 0.45f, 0.8f), horizon = Vec3(0.7f, 0.8f, 0.9f), groundSky = Vec3(0.3f, 0.3f, 0.3f);
    Vec3 sunGlow = Vec3(1.0f, 0.9f, 0.7f);
    Vec3 cloudColor = Vec3(1.0f);
    Vec3 fogColor = Vec3(0.7f, 0.8f, 0.9f);
    float fogDensity = 0.010f;
    float night = 0.0f;      // 0 = full day, 1 = full night
    float exposure = 1.25f;
    bool castShadows = true; // light high enough for shadows
};

struct FrameInfo {
    Mat4 view, proj;
    Vec3 camPos;
    float time = 0;
    float farDist = 220.0f;
    Environment env;
    std::vector<PointLightGPU> points;
    std::vector<SpotLightGPU> spots;
    Vec3 shadowFocus;
    Vec4 indoorBox;        // xmin, zmin, xmax, zmax of the building
    float indoorRoofY = 3.6f;
    Vec3 terrainColor2 = Vec3(0.30f, 0.24f, 0.15f);
    // Lights are sorted by region (0..2 = rooms, 3 = outdoors). A pixel only loops
    // over the lights of its own region: pointStart[r] .. pointStart[r+1]-1
    static const int REGIONS = 4;
    int pointStart[REGIONS + 1] = {0, 0, 0, 0, 0};
    int spotStart[REGIONS + 1] = {0, 0, 0, 0, 0};
    Vec4 regionSplit = Vec4(-7.0f, 7.0f, 0, 0);   // x of the two partition walls
    bool drawStatic = true;   // false = studio render of a single object
    bool drawSky = true;
    Vec3 clearColor = Vec3(0.5f);
};

class Renderer {
public:
    static const int MAX_POINT = 16;
    static const int MAX_SPOT = 8;

    bool init(float (*heightFn)(float, float));
    void shutdown();

    RenderSettings settings;

    // Static world building
    DrawSink& staticSink() { return m_staticSink; }
    void finalizeStatic();
    void clearStatic();

    // Per-frame dynamic objects
    DrawSink& dynamicSink() { return m_dynSink; }

    // Frame passes
    void beginFrame(const FrameInfo& f, int width, int height);  // uploads + shadow + sky + opaque
    void drawTransparent();                                      // sorted blended objects
    void endFrame();                                             // clears dynamic lists

    const Mesh& mesh(MeshId id) const { return m_meshes[id]; }
    TextureArray& textures() { return m_textures; }
    const Mat4& viewProj() const { return m_viewProj; }
    bool saveShadowMap(const std::string& path) const;   // depth image as seen from the sun (report figure)

    // statistics for the HUD / report
    int statDrawCalls = 0, statInstances = 0, statChunksVisible = 0, statChunksTotal = 0, statChunksOccluded = 0;
    long long statTriangles = 0;
    size_t staticInstanceCount() const { return m_staticCount; }

private:
    struct InstanceData {
        float model[16];
        float nrm[9];
        float color[4];
        float emit[4];
        float params[4];
    };
    static void packInstance(InstanceData& d, const Mat4& model, const Material& m);
    void setupInstanceAttribs(GLsizeiptr baseOffset);
    GLuint makeVao(MeshId mesh, GLuint instanceVbo, GLsizeiptr baseOffset);

    struct Batch { MeshId mesh; int lod; bool cast; GLuint vao; GLsizei count; };
    struct Chunk {
        Vec3 mn, mx;
        GLuint vbo = 0;
        std::vector<Batch> batches;
        // occlusion culling: a GPU query tells if any pixel of the chunk was visible
        GLuint query = 0;
        bool pending = false, occluded = false, global = false;
    };

    struct StaticCollector : DrawSink {
        Renderer* r;
        void add(MeshId mesh, const Mat4& model, const Material& mat) override;
    };
    struct DynamicCollector : DrawSink {
        Renderer* r;
        void add(MeshId mesh, const Mat4& model, const Material& mat) override;
    };
    struct PendingStatic { int key; int chunk; InstanceData d; Vec3 c; float r; };
    struct TransInst { MeshId mesh; float dist; InstanceData d; };

    void drawChunks(bool shadowPass, const Frustum& fr, const Vec3& cam);   // shadow pass or depth pre-pass
    void drawChunkList();                                                  // shading pass (chunks found visible)
    void drawDynamicOpaque();
    void renderShadowMap(const FrameInfo& f);
    void renderSky(const FrameInfo& f);
    void setMainUniforms(const FrameInfo& f);

    Mesh m_meshes[M_COUNT];
    TextureArray m_textures;
    Shader m_main, m_depth, m_sky, m_proxy;
    GLuint m_skyVao = 0, m_proxyVao = 0;
    std::vector<int> m_visibleChunks, m_occludedChunks, m_drawList;
    std::vector<float> m_drawDist;

    StaticCollector m_staticSink;
    DynamicCollector m_dynSink;
    std::vector<PendingStatic> m_pending;
    std::vector<TransInst> m_staticTrans;
    std::vector<Chunk> m_chunks;
    size_t m_staticCount = 0;

    std::vector<InstanceData> m_dyn[M_COUNT];
    GLuint m_dynVbo[M_COUNT] = {}, m_dynVao[M_COUNT] = {};
    std::vector<TransInst> m_trans;
    GLuint m_transVbo = 0, m_transVao[M_COUNT] = {};

    GLuint m_shadowFbo = 0, m_shadowTex = 0;
    int m_shadowResAllocated = 0;
    Mat4 m_lightVP;
    Vec3 m_shadowDir = Vec3(0, 1, 0);   // light direction used for the shadow map (moves in small steps)
    bool m_shadowValid = false;

    Mat4 m_viewProj;
    Vec3 m_camPos;
    Vec3 m_lastCamPos = Vec3(1e9f), m_lastCamDir = Vec3(0, 0, 1);   // to notice camera jumps (occlusion reset)
    FrameInfo m_frame;
};
