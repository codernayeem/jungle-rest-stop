#include "Renderer.h"
#include "Screenshot.h"
#include "Profiler.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

// =============================================================================
//  GLSL SHADERS
// =============================================================================
namespace {

const char* MAIN_VS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 iM0;
layout(location=4) in vec4 iM1;
layout(location=5) in vec4 iM2;
layout(location=6) in vec4 iM3;
layout(location=7) in vec3 iN0;
layout(location=8) in vec3 iN1;
layout(location=9) in vec3 iN2;
layout(location=10) in vec4 iColor;
layout(location=11) in vec4 iEmit;
layout(location=12) in vec4 iParams;

uniform mat4 uViewProj;
uniform float uTime;
uniform int uWind;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vLocalPos;
out vec3 vLocalNormal;
out vec2 vUV;
flat out vec4 vColor;
flat out vec4 vEmit;
flat out vec4 vParams;
invariant gl_Position;

void main() {
    mat4 M = mat4(iM0, iM1, iM2, iM3);
    mat3 N = mat3(iN0, iN1, iN2);
    vec4 wp = M * vec4(aPos, 1.0);
    int flags = int(iParams.w + 0.5);
    if ((flags & 2) != 0 && uWind != 0) {
        // wind sway: the higher the vertex inside the shape, the more it moves
        float h = clamp(aPos.y + 0.5, 0.0, 1.0);
        float ph = uTime * 1.7 + wp.x * 0.35 + wp.z * 0.27;
        float amp = 0.05 * h * min(length(iM1.xyz), 2.5);
        wp.x += sin(ph) * amp;
        wp.z += cos(ph * 0.83) * amp * 0.7;
    }
    vWorldPos = wp.xyz;
    vNormal = N * aNormal;
    vLocalPos = aPos * vec3(length(iM0.xyz), length(iM1.xyz), length(iM2.xyz));
    vLocalNormal = aNormal;
    vUV = aUV;
    vColor = iColor;
    vEmit = iEmit;
    vParams = iParams;
    gl_Position = uViewProj * wp;
}
)GLSL";

const char* MAIN_FS = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vLocalPos;
in vec3 vLocalNormal;
in vec2 vUV;
flat in vec4 vColor;
flat in vec4 vEmit;
flat in vec4 vParams;
out vec4 FragColor;

#define MAX_POINT 16
#define MAX_SPOT 8

uniform sampler2DArray uTex;
uniform sampler2DShadow uShadowMap;
uniform mat4 uLightVP;
uniform float uShadowTexel;

uniform int uEnhanced;
uniform int uUseTex;
uniform int uUseShadow;
uniform int uUseFog;

uniform vec3 uCamPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uSkyAmb;
uniform vec3 uGroundAmb;
uniform vec4 uIndoor;
uniform float uIndoorRoofY;
uniform vec3 uTerrainColor2;

uniform int uNumPL;
uniform int uPLStart[5];            // per region first light index (regions: ATM, lounge, library, outdoor)
uniform int uSLStart[5];
uniform vec2 uRegionSplit;          // x positions of the partition walls
uniform vec3 uPLPos[MAX_POINT];
uniform vec4 uPLColor[MAX_POINT];   // rgb * intensity, w = range
uniform int uNumSL;
uniform vec3 uSLPos[MAX_SPOT];
uniform vec4 uSLDir[MAX_SPOT];      // xyz = direction, w = range
uniform vec3 uSLColor[MAX_SPOT];
uniform vec2 uSLCone[MAX_SPOT];     // cos(inner), cos(outer)

uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uExposure;

// What a mirror-like surface reflects in direction R (no real reflection is
// traced; this is a cheap painted surrounding with strong contrast):
//   outside: bright sky above, a dark band of jungle at the horizon, dark ground below,
//            plus the glow of the sun;  inside: ceiling, walls and the light floor
vec3 envColor(vec3 R, bool inside) {
    float y = R.y;
    if (inside) {
        vec3 c = y > 0.45 ? vec3(0.22, 0.24, 0.22) : (y > -0.15 ? vec3(0.55, 0.50, 0.42) : vec3(0.75, 0.73, 0.70));
        return c * (0.35 + uSkyAmb.g * 1.4);
    }
    vec3 sky = mix(uFogColor * 1.25 + 0.1, uSkyAmb * 2.6, smoothstep(0.1, 0.7, y));
    vec3 trees = uGroundAmb * 0.3 + vec3(0.01, 0.025, 0.01);                // thin dark band at the horizon
    vec3 ground = uGroundAmb * 0.7 + uLightColor * 0.3;                    // sunlit pavement and grass
    vec3 c = mix(trees, sky, smoothstep(0.06, 0.12, y));
    c = mix(c, ground, smoothstep(-0.02, -0.12, y));
    c += uLightColor * pow(max(dot(R, uLightDir), 0.0), 48.0) * 3.0;   // the sun seen in the mirror
    return c;
}

float shadowFactor(vec3 wp, vec3 n, float ndl) {
    vec4 lp = uLightVP * vec4(wp + n * 0.05, 1.0);
    vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    float bias = 0.0006 + 0.0012 * (1.0 - ndl);
    float s = texture(uShadowMap, vec3(p.xy, p.z - bias)) * 2.0;
    s += texture(uShadowMap, vec3(p.xy + vec2( 1.2,  0.6) * uShadowTexel, p.z - bias));
    s += texture(uShadowMap, vec3(p.xy + vec2(-0.6,  1.2) * uShadowTexel, p.z - bias));
    s += texture(uShadowMap, vec3(p.xy + vec2(-1.2, -0.6) * uShadowTexel, p.z - bias));
    s += texture(uShadowMap, vec3(p.xy + vec2( 0.6, -1.2) * uShadowTexel, p.z - bias));
    return s / 6.0;
}

void main() {
    int flags = int(vParams.w + 0.5);
    int layer = int(vEmit.w + 0.5);

    // ---------- texture lookup (object-space box mapping or mesh UVs) ----------
    vec4 tex = vec4(0.5, 0.5, 0.5, 1.0);
    if (layer > 0) {
        vec2 uv;
        if ((flags & 1) != 0) uv = vUV;
        else {
            vec3 an = abs(vLocalNormal);
            if (an.y >= an.x && an.y >= an.z) uv = vLocalPos.xz;
            else if (an.x >= an.z) uv = vLocalPos.zy;
            else uv = vLocalPos.xy;
            uv *= vParams.z;
        }
        tex = texture(uTex, vec3(uv, float(layer)));
    }
    if ((flags & 16) != 0) {   // cut-out (leaves, grass); same rule as the depth pre-pass
        float cut = 0.5 - 0.22 * clamp(distance(vWorldPos, uCamPos) * (1.0 / 60.0), 0.0, 1.0);
        if (tex.a < cut) discard;
    }

    vec3 albedo = vColor.rgb;
    if ((flags & 32) != 0) {
        // terrain: mix lawn grass with jungle floor using a large-scale noise
        if (uUseTex != 0) {
            float n = texture(uTex, vec3(vWorldPos.xz * 0.012, 18.0)).r * 2.0;
            float jungle = smoothstep(26.0, 40.0, length(vec2(vWorldPos.x * 0.8, (vWorldPos.z - 6.0) * 1.1)));
            float m = clamp(jungle * 0.75 + (n - 0.9) * 1.4, 0.0, 1.0);
            vec3 g = texture(uTex, vec3(vWorldPos.xz * 0.35, 1.0)).rgb * 2.0 * vColor.rgb;
            vec3 f = texture(uTex, vec3(vWorldPos.xz * 0.28, 2.0)).rgb * 2.0 * uTerrainColor2;
            albedo = mix(g, f, m);
        }
    } else if (uUseTex != 0 && layer > 0) {
        albedo *= tex.rgb * 2.0;
    }

    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    bool foliage = (flags & 8) != 0;
    if (foliage && dot(N, V) < 0.0) N = -N;
    float specS = vParams.x, shin = vParams.y;

    vec3 result;
    if ((flags & 4) != 0) {
        result = albedo + vEmit.rgb;
    } else {
        // ---- ambient (sky above, ground below) ----
        vec3 amb = mix(uGroundAmb, uSkyAmb, N.y * 0.5 + 0.5);
        bool indoor = vWorldPos.x > uIndoor.x && vWorldPos.x < uIndoor.z &&
                      vWorldPos.z > uIndoor.y && vWorldPos.z < uIndoor.w && vWorldPos.y < uIndoorRoofY;
        if (indoor) amb *= 0.45;
        result = amb * albedo;

        // ---- shiny materials ----
        // metal: little diffuse light, a strong highlight in the metal's own colour,
        //        and a mirror reflection of the surroundings (sky outside, room inside)
        // gloss: normal paint underneath + a clear coat that reflects more at grazing angles
        bool metal = (flags & 128) != 0, gloss = (flags & 256) != 0;
        vec3 specCol = vec3(1.0);
        vec3 baseCol = albedo;
        if (metal) { specCol = albedo * 3.0; baseCol = albedo * 0.25; result = amb * baseCol; }
        if (gloss) specCol = vec3(2.5);

        // ---- directional light: sun (day) or moon (night) ----
        float ndl = dot(N, uLightDir);
        float diff = foliage ? clamp(ndl * 0.6 + 0.4, 0.0, 1.0) : max(ndl, 0.0);
        float sh = 1.0;
        if (uUseShadow != 0 && diff > 0.0) sh = shadowFactor(vWorldPos, N, max(ndl, 0.0));
        vec3 H = normalize(uLightDir + V);
        float sp = ndl > 0.0 ? pow(max(dot(N, H), 0.0), shin) * specS : 0.0;
        if ((metal || gloss) && ndl > 0.0) sp += pow(max(dot(N, H), 0.0), 24.0) * 0.25;   // soft sheen around the sharp highlight
        result += (baseCol * diff + specCol * sp) * uLightColor * sh;
        if (metal || gloss) {
            vec3 Rv = reflect(-V, N);
            vec3 env = envColor(Rv, indoor);
            float fres = pow(1.0 - max(dot(N, V), 0.0), 5.0);           // Schlick: more reflection at grazing angles
            if (metal) result += env * albedo * (0.85 + 0.15 * fres);
            else result += env * (0.08 + 0.75 * fres);
        }
        if (foliage) {
            // light shining through thin leaves when looking towards the sun
            float through = pow(max(dot(-V, uLightDir), 0.0), 3.0);
            result += albedo * vec3(0.9, 1.0, 0.55) * uLightColor * sh * through * 0.45;
            // cards are darker near their base (leaves shade each other)
            if ((flags & 1) != 0) result *= mix(0.72, 1.08, clamp(vUV.y, 0.0, 1.0));
        }

        // which region is this pixel in? (each room only sees its own lamps)
        int region = 3;
        if (indoor && vWorldPos.x > uIndoor.x + 0.1 && vWorldPos.x < uIndoor.z - 0.1 &&
            vWorldPos.z > uIndoor.y + 0.1 && vWorldPos.z < uIndoor.w - 0.1)
            region = vWorldPos.x < uRegionSplit.x ? 0 : (vWorldPos.x < uRegionSplit.y ? 1 : 2);
        // ---- point lights (bulbs, lamps) ----
        for (int i = uPLStart[region]; i < uPLStart[region + 1]; ++i) {
            vec3 L = uPLPos[i] - vWorldPos;
            float d = length(L);
            float range = uPLColor[i].w;
            if (d > range) continue;
            L /= d;
            float att = pow(clamp(1.0 - pow(d / range, 4.0), 0.0, 1.0), 2.0) / (1.0 + d * d);
            float nl = foliage ? clamp(dot(N, L) * 0.6 + 0.4, 0.0, 1.0) : max(dot(N, L), 0.0);
            vec3 h = normalize(L + V);
            float s = pow(max(dot(N, h), 0.0), shin) * specS;
            result += (baseCol + specCol * s) * nl * uPLColor[i].rgb * att;
        }
        // ---- spot lights (downlights, head lights, reading lamp) ----
        for (int i = uSLStart[region]; i < uSLStart[region + 1]; ++i) {
            vec3 L = uSLPos[i] - vWorldPos;
            float d = length(L);
            float range = uSLDir[i].w;
            if (d > range) continue;
            L /= d;
            float cone = smoothstep(uSLCone[i].y, uSLCone[i].x, dot(-L, uSLDir[i].xyz));
            if (cone <= 0.0) continue;
            float att = pow(clamp(1.0 - pow(d / range, 4.0), 0.0, 1.0), 2.0) / (1.0 + 0.5 * d * d);
            float nl = foliage ? clamp(dot(N, L) * 0.6 + 0.4, 0.0, 1.0) : max(dot(N, L), 0.0);
            vec3 h = normalize(L + V);
            float s = pow(max(dot(N, h), 0.0), shin) * specS;
            result += (baseCol + specCol * s) * nl * uSLColor[i] * att * cone;
        }
        result += vEmit.rgb;
    }

    if (uUseFog != 0) {
        float d = length(uCamPos - vWorldPos);
        float f = 1.0 - exp(-pow(d * uFogDensity, 1.5));
        result = mix(result, uFogColor, clamp(f, 0.0, 1.0));
    }
    if (uEnhanced != 0) result = vec3(1.0) - exp(-result * uExposure);   // tone mapping
    else result = clamp(result, 0.0, 1.0);
    FragColor = vec4(result, vColor.a);
}
)GLSL";

const char* DEPTH_VS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 iM0;
layout(location=4) in vec4 iM1;
layout(location=5) in vec4 iM2;
layout(location=6) in vec4 iM3;
layout(location=11) in vec4 iEmit;
layout(location=12) in vec4 iParams;
uniform mat4 uLightVP;
uniform float uTime;
uniform int uWind;
out vec2 vUV;
out vec3 vWorld;
flat out float vLayer;
flat out float vFlags;
invariant gl_Position;
void main() {
    mat4 M = mat4(iM0, iM1, iM2, iM3);
    vec4 wp = M * vec4(aPos, 1.0);
    int flags = int(iParams.w + 0.5);
    if ((flags & 2) != 0 && uWind != 0) {
        float h = clamp(aPos.y + 0.5, 0.0, 1.0);
        float ph = uTime * 1.7 + wp.x * 0.35 + wp.z * 0.27;
        float amp = 0.05 * h * min(length(iM1.xyz), 2.5);
        wp.x += sin(ph) * amp;
        wp.z += cos(ph * 0.83) * amp * 0.7;
    }
    vUV = aUV;
    vWorld = wp.xyz;
    vLayer = iEmit.w;
    vFlags = iParams.w;
    gl_Position = uLightVP * wp;
}
)GLSL";

const char* DEPTH_FS = R"GLSL(
#version 330 core
in vec2 vUV;
in vec3 vWorld;
flat in float vLayer;
flat in float vFlags;
uniform sampler2DArray uTex;
uniform vec3 uCutCam;
uniform float uCutFade;    // 0 in the shadow pass
void main() {
    int flags = int(vFlags + 0.5);
    if ((flags & 16) != 0) {
        // far away the mip-maps blur the alpha, so lower the threshold: leaves stay full
        float cut = 0.5 - 0.22 * clamp(distance(vWorld, uCutCam) * uCutFade, 0.0, 1.0);
        if (texture(uTex, vec3(vUV, vLayer)).a < cut) discard;
    }
}
)GLSL";

const char* SKY_VS = R"GLSL(
#version 330 core
out vec2 vNdc;
void main() {
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0, (gl_VertexID == 2) ? 3.0 : -1.0);
    vNdc = p;
    gl_Position = vec4(p, 1.0, 1.0);
}
)GLSL";

const char* SKY_FS = R"GLSL(
#version 330 core
in vec2 vNdc;
out vec4 FragColor;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform vec3 uZenith, uHorizon, uGround, uSunGlow, uCloudColor;
uniform vec3 uSunDir, uMoonDir;
uniform float uNight, uTime, uExposure;
uniform int uEnhanced;

float hash(vec3 p) { p = fract(p * 0.3183099 + 0.1); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }
float hash2(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i), hash2(i + vec2(1, 0)), u.x), mix(hash2(i + vec2(0, 1)), hash2(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) { float s = 0.0, a = 0.5; for (int i = 0; i < 4; ++i) { s += a * noise(p); p *= 2.03; a *= 0.5; } return s; }

void main() {
    vec4 w = uInvViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 d = normalize(w.xyz / w.w - uCamPos);
    float h = d.y;
    vec3 col = h > 0.0 ? mix(uHorizon, uZenith, pow(h, 0.55)) : mix(uHorizon, uGround, pow(min(-h * 4.0, 1.0), 0.6));

    float sd = max(dot(d, uSunDir), 0.0);
    float sunVis = smoothstep(-0.12, 0.05, uSunDir.y);
    col += uSunGlow * (pow(sd, 6.0) * 0.35 + pow(sd, 900.0) * 12.0) * sunVis;

    float md = max(dot(d, uMoonDir), 0.0);
    col += vec3(0.75, 0.82, 1.0) * (pow(md, 1500.0) * 3.0 + pow(md, 60.0) * 0.08) * uNight;

    if (uEnhanced != 0 && h > 0.0) {
        // twinkling stars
        vec3 p = d * 260.0;
        vec3 cell = floor(p);
        float r = hash(cell);
        if (r > 0.9965) {
            vec3 c = fract(p) - 0.5;
            float s = smoothstep(0.22, 0.0, length(c));
            col += vec3(s) * uNight * (0.6 + 0.4 * sin(uTime * 3.0 + r * 200.0)) * smoothstep(0.0, 0.2, h);
        }
        // drifting clouds
        vec2 uv = d.xz / (h + 0.12) * 0.9 + vec2(uTime * 0.006, uTime * 0.002);
        float c = smoothstep(0.48, 0.80, fbm(uv * 1.6));
        col = mix(col, uCloudColor, c * 0.85 * smoothstep(0.0, 0.25, h));
    }
    if (uEnhanced != 0) col = vec3(1.0) - exp(-col * uExposure);
    FragColor = vec4(col, 1.0);
}
)GLSL";

const char* PROXY_VS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uViewProj;
uniform vec3 uMin, uMax;
void main() { gl_Position = uViewProj * vec4(mix(uMin, uMax, aPos + 0.5), 1.0); }
)GLSL";
const char* PROXY_FS = R"GLSL(
#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(1.0); }
)GLSL";

const GLsizei INSTANCE_STRIDE = sizeof(float) * 37;
const float CHUNK_SIZE = 32.0f;
const float LOD_DIST[3] = {1e9f, 85.0f, 45.0f};

}  // namespace

// =============================================================================
//  Instance packing
// =============================================================================
void Renderer::packInstance(InstanceData& d, const Mat4& model, const Material& m) {
    std::memcpy(d.model, model.m, sizeof(d.model));
    normalMatrix(model, d.nrm);
    d.color[0] = m.color.x; d.color[1] = m.color.y; d.color[2] = m.color.z; d.color[3] = m.alpha;
    d.emit[0] = m.emission.x; d.emit[1] = m.emission.y; d.emit[2] = m.emission.z; d.emit[3] = (float)m.tex;
    d.params[0] = m.spec; d.params[1] = m.shininess; d.params[2] = m.texScale; d.params[3] = (float)m.flags;
}

void Renderer::setupInstanceAttribs(GLsizeiptr base) {
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, INSTANCE_STRIDE, (void*)(base + i * 16));
        glVertexAttribDivisor(3 + i, 1);
    }
    for (int i = 0; i < 3; ++i) {
        glEnableVertexAttribArray(7 + i);
        glVertexAttribPointer(7 + i, 3, GL_FLOAT, GL_FALSE, INSTANCE_STRIDE, (void*)(base + 64 + i * 12));
        glVertexAttribDivisor(7 + i, 1);
    }
    const GLsizeiptr offs[3] = {100, 116, 132};
    for (int i = 0; i < 3; ++i) {
        glEnableVertexAttribArray(10 + i);
        glVertexAttribPointer(10 + i, 4, GL_FLOAT, GL_FALSE, INSTANCE_STRIDE, (void*)(base + offs[i]));
        glVertexAttribDivisor(10 + i, 1);
    }
}

GLuint Renderer::makeVao(MeshId mesh, GLuint instanceVbo, GLsizeiptr base) {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_meshes[mesh].vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(Vec3)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(Vec3)));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_meshes[mesh].ebo);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    setupInstanceAttribs(base);
    glBindVertexArray(0);
    return vao;
}

// =============================================================================
//  Init / shutdown
// =============================================================================
bool Renderer::init(float (*heightFn)(float, float)) {
    createAllShapes(m_meshes, heightFn);
    m_textures.create();
    bool ok = m_main.build(MAIN_VS, MAIN_FS, "main");
    ok &= m_depth.build(DEPTH_VS, DEPTH_FS, "depth");
    ok &= m_sky.build(SKY_VS, SKY_FS, "sky");
    ok &= m_proxy.build(PROXY_VS, PROXY_FS, "proxy");
    glGenVertexArrays(1, &m_skyVao);
    glGenVertexArrays(1, &m_proxyVao);
    glBindVertexArray(m_proxyVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_meshes[M_CUBE].vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_meshes[M_CUBE].ebo);
    glBindVertexArray(0);

    m_staticSink.r = this;
    m_dynSink.r = this;
    for (int i = 0; i < M_COUNT; ++i) {
        glGenBuffers(1, &m_dynVbo[i]);
        glBindBuffer(GL_ARRAY_BUFFER, m_dynVbo[i]);
        glBufferData(GL_ARRAY_BUFFER, INSTANCE_STRIDE, nullptr, GL_STREAM_DRAW);
        m_dynVao[i] = makeVao((MeshId)i, m_dynVbo[i], 0);
    }
    glGenBuffers(1, &m_transVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_transVbo);
    glBufferData(GL_ARRAY_BUFFER, INSTANCE_STRIDE, nullptr, GL_STREAM_DRAW);
    for (int i = 0; i < M_COUNT; ++i) m_transVao[i] = makeVao((MeshId)i, m_transVbo, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return ok;
}

void Renderer::clearStatic() {
    for (auto& c : m_chunks) {
        for (auto& b : c.batches) glDeleteVertexArrays(1, &b.vao);
        glDeleteBuffers(1, &c.vbo);
        if (c.query) glDeleteQueries(1, &c.query);
    }
    m_chunks.clear();
    m_pending.clear();
    m_staticTrans.clear();
    m_staticCount = 0;
}

void Renderer::shutdown() {
    clearStatic();
    for (int i = 0; i < M_COUNT; ++i) {
        glDeleteVertexArrays(1, &m_dynVao[i]);
        glDeleteBuffers(1, &m_dynVbo[i]);
        glDeleteVertexArrays(1, &m_transVao[i]);
    }
    glDeleteBuffers(1, &m_transVbo);
    glDeleteVertexArrays(1, &m_skyVao);
    if (m_shadowFbo) glDeleteFramebuffers(1, &m_shadowFbo);
    if (m_shadowTex) glDeleteTextures(1, &m_shadowTex);
    m_main.destroy();
    m_depth.destroy();
    m_sky.destroy();
    m_textures.destroy();
    destroyAllShapes(m_meshes);
}

// =============================================================================
//  Static world collection -> chunks
// =============================================================================
void Renderer::StaticCollector::add(MeshId mesh, const Mat4& model, const Material& mat) {
    InstanceData d;
    packInstance(d, model, mat);
    const Mesh& ms = r->m_meshes[mesh];
    Vec3 c = model.transformPoint(ms.boundCenter);
    float rad = ms.boundRadius * model.maxAxisScale();
    if (mat.alpha < 0.999f) {
        r->m_staticTrans.push_back({mesh, 0.0f, d});
        return;
    }
    bool cast = (mat.flags & MF_NO_SHADOW) == 0;
    int key = (int)mesh * 8 + mat.lod * 2 + (cast ? 1 : 0);
    r->m_pending.push_back({key, 0, d, c, rad});
}

void Renderer::finalizeStatic() {
    // group by chunk cell; very large objects go to one "global" chunk
    std::map<long long, std::vector<size_t>> cells;
    const long long GLOBAL = (long long)1 << 40;
    for (size_t i = 0; i < m_pending.size(); ++i) {
        const auto& p = m_pending[i];
        long long key;
        if (p.r > CHUNK_SIZE * 1.5f) key = GLOBAL;
        else {
            long long cx = (long long)std::floor(p.c.x / CHUNK_SIZE) + 1000;
            long long cz = (long long)std::floor(p.c.z / CHUNK_SIZE) + 1000;
            key = cx * 4096 + cz;
        }
        cells[key].push_back(i);
    }
    for (auto& kv : cells) {
        Chunk ch;
        ch.global = kv.first == GLOBAL;
        glGenQueries(1, &ch.query);
        ch.mn = Vec3(1e9f);
        ch.mx = Vec3(-1e9f);
        std::map<int, std::vector<InstanceData>> groups;
        for (size_t idx : kv.second) {
            const auto& p = m_pending[idx];
            groups[p.key].push_back(p.d);
            ch.mn = Vec3(std::min(ch.mn.x, p.c.x - p.r), std::min(ch.mn.y, p.c.y - p.r), std::min(ch.mn.z, p.c.z - p.r));
            ch.mx = Vec3(std::max(ch.mx.x, p.c.x + p.r), std::max(ch.mx.y, p.c.y + p.r), std::max(ch.mx.z, p.c.z + p.r));
        }
        std::vector<InstanceData> all;
        std::vector<std::pair<int, size_t>> starts;
        for (auto& g : groups) {
            starts.push_back({g.first, all.size()});
            all.insert(all.end(), g.second.begin(), g.second.end());
        }
        glGenBuffers(1, &ch.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, ch.vbo);
        glBufferData(GL_ARRAY_BUFFER, all.size() * sizeof(InstanceData), all.data(), GL_STATIC_DRAW);
        for (auto& s : starts) {
            int key = s.first;
            Batch b;
            b.mesh = (MeshId)(key / 8);
            b.lod = (key % 8) / 2;
            b.cast = (key % 2) == 1;
            b.count = (GLsizei)groups[key].size();
            b.vao = makeVao(b.mesh, ch.vbo, (GLsizeiptr)(s.second * sizeof(InstanceData)));
            ch.batches.push_back(b);
        }
        m_staticCount += all.size();
        m_chunks.push_back(ch);
    }
    m_pending.clear();
    m_pending.shrink_to_fit();
    statChunksTotal = (int)m_chunks.size();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// =============================================================================
//  Dynamic objects
// =============================================================================
void Renderer::DynamicCollector::add(MeshId mesh, const Mat4& model, const Material& mat) {
    if (mat.alpha < 0.999f) {
        TransInst t;
        t.mesh = mesh;
        packInstance(t.d, model, mat);
        t.dist = 0.0f;  // computed in drawTransparent() once the camera is known
        r->m_trans.push_back(t);
        return;
    }
    r->m_dyn[mesh].emplace_back();
    packInstance(r->m_dyn[mesh].back(), model, mat);
}

// =============================================================================
//  Frame
// =============================================================================
void Renderer::setMainUniforms(const FrameInfo& f) {
    Shader& s = m_main;
    bool enh = settings.enhanced;
    s.set("uViewProj", m_viewProj);
    s.set("uTime", f.time);
    s.set("uWind", (enh && settings.wind) ? 1 : 0);
    s.set("uTex", 0);
    s.set("uShadowMap", 1);
    s.set("uLightVP", m_lightVP);
    s.set("uShadowTexel", 1.0f / (float)std::max(1, m_shadowResAllocated));
    s.set("uEnhanced", enh ? 1 : 0);
    s.set("uUseTex", (enh && settings.textures) ? 1 : 0);
    s.set("uUseShadow", (enh && settings.shadows && m_shadowValid) ? 1 : 0);
    s.set("uUseFog", (enh && settings.fog) ? 1 : 0);
    s.set("uCamPos", f.camPos);
    s.set("uLightDir", f.env.lightDir);
    s.set("uLightColor", f.env.lightColor);
    s.set("uSkyAmb", f.env.skyAmbient);
    s.set("uGroundAmb", f.env.groundAmbient);
    s.set("uIndoor", f.indoorBox);
    s.set("uIndoorRoofY", f.indoorRoofY);
    s.set("uTerrainColor2", f.terrainColor2);
    s.set("uFogColor", f.env.fogColor);
    s.set("uFogDensity", f.env.fogDensity);
    s.set("uExposure", f.env.exposure);

    int np = std::min((int)f.points.size(), MAX_POINT);
    float pp[MAX_POINT * 3], pc[MAX_POINT * 4];
    for (int i = 0; i < np; ++i) {
        const auto& L = f.points[i];
        pp[i * 3] = L.pos.x; pp[i * 3 + 1] = L.pos.y; pp[i * 3 + 2] = L.pos.z;
        pc[i * 4] = L.color.x; pc[i * 4 + 1] = L.color.y; pc[i * 4 + 2] = L.color.z; pc[i * 4 + 3] = L.range;
    }
    s.set("uNumPL", np);
    glUniform1iv(s.loc("uPLStart[0]"), 5, f.pointStart);
    glUniform1iv(s.loc("uSLStart[0]"), 5, f.spotStart);
    s.set("uRegionSplit", Vec2(f.regionSplit.x, f.regionSplit.y));
    if (np) {
        glUniform3fv(s.loc("uPLPos[0]"), np, pp);
        glUniform4fv(s.loc("uPLColor[0]"), np, pc);
    }
    int ns = std::min((int)f.spots.size(), MAX_SPOT);
    float sp[MAX_SPOT * 3], sd[MAX_SPOT * 4], sc[MAX_SPOT * 3], cn[MAX_SPOT * 2];
    for (int i = 0; i < ns; ++i) {
        const auto& L = f.spots[i];
        sp[i * 3] = L.pos.x; sp[i * 3 + 1] = L.pos.y; sp[i * 3 + 2] = L.pos.z;
        Vec3 d = normalize(L.dir);
        sd[i * 4] = d.x; sd[i * 4 + 1] = d.y; sd[i * 4 + 2] = d.z; sd[i * 4 + 3] = L.range;
        sc[i * 3] = L.color.x; sc[i * 3 + 1] = L.color.y; sc[i * 3 + 2] = L.color.z;
        cn[i * 2] = L.cosInner; cn[i * 2 + 1] = L.cosOuter;
    }
    s.set("uNumSL", ns);
    if (ns) {
        glUniform3fv(s.loc("uSLPos[0]"), ns, sp);
        glUniform4fv(s.loc("uSLDir[0]"), ns, sd);
        glUniform3fv(s.loc("uSLColor[0]"), ns, sc);
        glUniform2fv(s.loc("uSLCone[0]"), ns, cn);
    }
}

void Renderer::drawChunks(bool shadowPass, const Frustum& fr, const Vec3& cam) {
    // 1) frustum + distance test, 2) sort front-to-back (nearer chunks fill the
    // depth buffer first, so hidden pixels behind them are rejected early),
    // 3) skip chunks that the GPU reported as fully hidden last frame.
    m_visibleChunks.clear();
    std::vector<float> dist(m_chunks.size(), 0.0f);
    for (size_t i = 0; i < m_chunks.size(); ++i) {
        Chunk& ch = m_chunks[i];
        if (!fr.boxVisible(ch.mn, ch.mx)) continue;
        float dx = std::max(std::max(ch.mn.x - cam.x, 0.0f), cam.x - ch.mx.x);
        float dz = std::max(std::max(ch.mn.z - cam.z, 0.0f), cam.z - ch.mx.z);
        dist[i] = std::sqrt(dx * dx + dz * dz);
        if (!shadowPass && dist[i] > m_frame.farDist) continue;
        m_visibleChunks.push_back((int)i);
    }
    std::sort(m_visibleChunks.begin(), m_visibleChunks.end(), [&](int a, int b) { return dist[a] < dist[b]; });

    auto drawBatches = [&](Chunk& ch, float d) {
        for (auto& b : ch.batches) {
            if (shadowPass && (!b.cast || b.lod == 2)) continue;
            if (d > LOD_DIST[b.lod]) continue;
            glBindVertexArray(b.vao);
            glDrawElementsInstanced(GL_TRIANGLES, m_meshes[b.mesh].indexCount, GL_UNSIGNED_INT, nullptr, b.count);
            statDrawCalls++;
            statInstances += b.count;
            statTriangles += (long long)b.count * m_meshes[b.mesh].indexCount / 3;
        }
    };
    if (shadowPass) {
        for (int i : m_visibleChunks) drawBatches(m_chunks[i], dist[i]);
        return;
    }

    m_occludedChunks.clear();
    m_drawList.clear();
    m_drawDist.clear();
    bool useQueries = settings.occlusion && m_frame.drawStatic;
    for (int i : m_visibleChunks) {
        Chunk& ch = m_chunks[i];
        statChunksVisible++;
        if (ch.pending) {   // read last frame's answer without waiting for the GPU
            GLuint ready = 0;
            glGetQueryObjectuiv(ch.query, GL_QUERY_RESULT_AVAILABLE, &ready);
            if (ready) {
                GLuint any = 1;
                glGetQueryObjectuiv(ch.query, GL_QUERY_RESULT, &any);
                ch.occluded = any == 0;
                ch.pending = false;
            }
        }
        bool inside = cam.x > ch.mn.x - 2 && cam.x < ch.mx.x + 2 && cam.z > ch.mn.z - 2 && cam.z < ch.mx.z + 2;
        if (!useQueries || inside || ch.global) ch.occluded = false;
        if (ch.occluded) { m_occludedChunks.push_back(i); continue; }
        bool ask = useQueries && !inside && !ch.global && !ch.pending;
        if (ask) glBeginQuery(GL_ANY_SAMPLES_PASSED, ch.query);
        drawBatches(ch, dist[i]);
        m_drawList.push_back(i);
        m_drawDist.push_back(dist[i]);
        if (ask) { glEndQuery(GL_ANY_SAMPLES_PASSED); ch.pending = true; }
    }
    statChunksOccluded = (int)m_occludedChunks.size();
    if (m_occludedChunks.empty()) return;
    // hidden chunks: draw only their bounding box (no colour, no depth) to ask
    // the GPU whether they became visible again
    m_proxy.use();
    m_proxy.set("uViewProj", m_viewProj);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glBindVertexArray(m_proxyVao);
    for (int i : m_occludedChunks) {
        Chunk& ch = m_chunks[i];
        if (ch.pending) continue;
        m_proxy.set("uMin", ch.mn);
        m_proxy.set("uMax", ch.mx);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, ch.query);
        glDrawElements(GL_TRIANGLES, m_meshes[M_CUBE].indexCount, GL_UNSIGNED_INT, nullptr);
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        ch.pending = true;
    }
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);   // still inside the depth pre-pass
    glDepthMask(GL_TRUE);
    m_depth.use();
}

void Renderer::drawChunkList() {
    for (size_t k = 0; k < m_drawList.size(); ++k) {
        Chunk& ch = m_chunks[m_drawList[k]];
        for (auto& b : ch.batches) {
            if (m_drawDist[k] > LOD_DIST[b.lod]) continue;
            glBindVertexArray(b.vao);
            glDrawElementsInstanced(GL_TRIANGLES, m_meshes[b.mesh].indexCount, GL_UNSIGNED_INT, nullptr, b.count);
            statDrawCalls++;
            statInstances += b.count;
            statTriangles += (long long)b.count * m_meshes[b.mesh].indexCount / 3;
        }
    }
}

void Renderer::drawDynamicOpaque() {
    for (int i = 0; i < M_COUNT; ++i) {
        if (m_dyn[i].empty()) continue;
        glBindVertexArray(m_dynVao[i]);
        glDrawElementsInstanced(GL_TRIANGLES, m_meshes[i].indexCount, GL_UNSIGNED_INT, nullptr, (GLsizei)m_dyn[i].size());
        statDrawCalls++;
        statInstances += (int)m_dyn[i].size();
        statTriangles += (long long)m_dyn[i].size() * m_meshes[i].indexCount / 3;
    }
}

void Renderer::renderShadowMap(const FrameInfo& f) {
    int res = settings.shadowRes;
    if (res != m_shadowResAllocated) {
        if (m_shadowFbo) glDeleteFramebuffers(1, &m_shadowFbo);
        if (m_shadowTex) glDeleteTextures(1, &m_shadowTex);
        glGenTextures(1, &m_shadowTex);
        glBindTexture(GL_TEXTURE_2D, m_shadowTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, res, res, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float border[4] = {1, 1, 1, 1};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glGenFramebuffers(1, &m_shadowFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowTex, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_shadowResAllocated = res;
    }
    // Orthographic light camera centred on the focus point. Two things keep the
    // shadow edges still (no crawling / shimmering):
    //  1) the sun direction used here only changes in steps of 0.5 degrees,
    //  2) the centre is snapped to whole shadow-map texels in the light's own plane.
    float R = settings.shadowRadius;
    float texel = 2.0f * R / res;
    Vec3 Lnow = normalize(f.env.lightDir);
    if (dot(Lnow, m_shadowDir) < std::cos(toRadians(0.5f))) m_shadowDir = Lnow;
    Vec3 L = m_shadowDir;
    Vec3 up = std::fabs(L.y) > 0.98f ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
    Vec3 c = f.shadowFocus;
    c.y = 0.0f;
    Vec3 right = normalize(cross(up, L)), upL = cross(L, right);   // light-plane axes
    float a = dot(c, right), b = dot(c, upL);
    c = c + right * (std::floor(a / texel) * texel - a) + upL * (std::floor(b / texel) * texel - b);
    Mat4 lv = lookAt(c + L * 120.0f, c, up);
    Mat4 lp = ortho(-R, R, -R, R, 1.0f, 260.0f);
    m_lightVP = lp * lv;

    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
    glViewport(0, 0, res, res);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 3.0f);
    m_depth.use();
    m_depth.set("uLightVP", m_lightVP);
    m_depth.set("uTime", f.time);
    m_depth.set("uWind", settings.wind ? 1 : 0);
    m_depth.set("uTex", 0);
    m_depth.set("uCutCam", f.camPos);
    m_depth.set("uCutFade", 0.0f);
    Frustum lf;
    lf.fromMatrix(m_lightVP);
    if (f.drawStatic) drawChunks(true, lf, c);
    drawDynamicOpaque();
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_shadowValid = true;
}

void Renderer::renderSky(const FrameInfo& f) {
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    m_sky.use();
    Mat4 viewNoT = f.view;
    m_sky.set("uInvViewProj", inverse(m_viewProj));
    m_sky.set("uCamPos", f.camPos);
    m_sky.set("uZenith", f.env.zenith);
    m_sky.set("uHorizon", f.env.horizon);
    m_sky.set("uGround", f.env.groundSky);
    m_sky.set("uSunGlow", f.env.sunGlow);
    m_sky.set("uCloudColor", f.env.cloudColor);
    m_sky.set("uSunDir", f.env.sunDir);
    m_sky.set("uMoonDir", f.env.moonDir);
    m_sky.set("uNight", f.env.night);
    m_sky.set("uTime", f.time);
    m_sky.set("uExposure", f.env.exposure);
    m_sky.set("uEnhanced", settings.enhanced ? 1 : 0);
    glBindVertexArray(m_skyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    (void)viewNoT;
}

void Renderer::beginFrame(const FrameInfo& f, int width, int height) {
    m_frame = f;
    m_camPos = f.camPos;
    m_viewProj = f.proj * f.view;
    // Occlusion results are one frame old. After a camera jump or a fast turn they no
    // longer fit the view, so forget them (every chunk is drawn again and re-tested).
    Vec3 camDir = normalize(Vec3(-f.view.m[2], -f.view.m[6], -f.view.m[10]));
    if ((f.camPos - m_lastCamPos).length() > 2.0f || dot(camDir, m_lastCamDir) < 0.94f)
        for (auto& ch : m_chunks) { ch.occluded = false; ch.pending = false; }
    m_lastCamPos = f.camPos;
    m_lastCamDir = camDir;
    statDrawCalls = statInstances = statChunksVisible = 0;
    statTriangles = 0;

    // ---- upload dynamic instance data ----
    for (int i = 0; i < M_COUNT; ++i) {
        if (m_dyn[i].empty()) continue;
        glBindBuffer(GL_ARRAY_BUFFER, m_dynVbo[i]);
        GLsizeiptr bytes = (GLsizeiptr)(m_dyn[i].size() * sizeof(InstanceData));
        glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, m_dyn[i].data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    Profiler::get().mark("upload");

    m_textures.bind(0);

    // ---- shadow pass ----
    m_shadowValid = false;
    if (settings.enhanced && settings.shadows && f.env.castShadows) renderShadowMap(f);
    Profiler::get().mark("shadow");

    // ---- main pass ----
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    Vec3 cc = f.drawSky ? f.env.horizon : f.clearColor;
    glClearColor(cc.x, cc.y, cc.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (f.drawSky) renderSky(f);
    Profiler::get().mark("sky");

    // ---- depth pre-pass: only depth, very cheap shader. Afterwards every pixel
    //      is lit exactly once (no work wasted on surfaces that get covered) ----
    Frustum fr;
    fr.fromMatrix(m_viewProj);
    m_textures.bind(0);
    m_depth.use();
    m_depth.set("uLightVP", m_viewProj);
    m_depth.set("uTime", f.time);
    m_depth.set("uWind", (settings.enhanced && settings.wind) ? 1 : 0);
    m_depth.set("uTex", 0);
    m_depth.set("uCutCam", f.camPos);
    m_depth.set("uCutFade", 1.0f / 60.0f);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    m_drawList.clear();
    m_drawDist.clear();
    if (f.drawStatic) drawChunks(false, fr, f.camPos);
    drawDynamicOpaque();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    Profiler::get().mark("depth_prepass");

    // ---- shading pass ----
    m_main.use();
    m_textures.bind(0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_shadowTex);
    glActiveTexture(GL_TEXTURE0);
    setMainUniforms(f);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    drawChunkList();
    Profiler::get().mark("static_world");
    drawDynamicOpaque();
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glBindVertexArray(0);
    Profiler::get().mark("dynamic_opaque");
}

void Renderer::drawTransparent() {
    if (m_frame.drawStatic)
    for (auto& t : m_staticTrans) {
        Vec3 p(t.d.model[12], t.d.model[13], t.d.model[14]);
        if ((p - m_camPos).length() < m_frame.farDist) m_trans.push_back(t);
    }
    if (m_trans.empty()) return;
    for (auto& t : m_trans) {
        Vec3 p(t.d.model[12], t.d.model[13], t.d.model[14]);
        t.dist = (p - m_camPos).length();
    }
    std::sort(m_trans.begin(), m_trans.end(), [](const TransInst& a, const TransInst& b) { return a.dist > b.dist; });
    std::vector<InstanceData> data(m_trans.size());
    for (size_t i = 0; i < m_trans.size(); ++i) data[i] = m_trans[i].d;
    glBindBuffer(GL_ARRAY_BUFFER, m_transVbo);
    GLsizeiptr bytes = (GLsizeiptr)(data.size() * sizeof(InstanceData));
    glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, data.data());

    m_main.use();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    size_t i = 0;
    while (i < m_trans.size()) {
        size_t j = i;
        MeshId mesh = m_trans[i].mesh;
        while (j < m_trans.size() && m_trans[j].mesh == mesh) ++j;
        glBindVertexArray(m_transVao[mesh]);
        glBindBuffer(GL_ARRAY_BUFFER, m_transVbo);
        setupInstanceAttribs((GLsizeiptr)(i * sizeof(InstanceData)));
        glDrawElementsInstanced(GL_TRIANGLES, m_meshes[mesh].indexCount, GL_UNSIGNED_INT, nullptr, (GLsizei)(j - i));
        statDrawCalls++;
        i = j;
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    Profiler::get().mark("transparent");
}

bool Renderer::saveShadowMap(const std::string& path) const {
    if (!m_shadowTex || m_shadowResAllocated <= 0) return false;
    int n = m_shadowResAllocated;
    std::vector<float> depth((size_t)n * n);
    glBindTexture(GL_TEXTURE_2D, m_shadowTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    float lo = 1.0f, hi = 0.0f;
    for (float d : depth) if (d < 1.0f) { lo = std::min(lo, d); hi = std::max(hi, d); }
    std::vector<uint8_t> rgb((size_t)n * n * 3);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            float d = depth[(size_t)(n - 1 - y) * n + x];
            float v = d >= 1.0f ? 1.0f : (d - lo) / std::max(1e-6f, hi - lo);
            uint8_t c = (uint8_t)(clampf(v, 0, 1) * 255.0f);
            uint8_t* o = &rgb[((size_t)y * n + x) * 3];
            o[0] = o[1] = o[2] = c;
        }
    return writePNG(path, n, n, rgb.data(), 3);
}

void Renderer::endFrame() {
    for (int i = 0; i < M_COUNT; ++i) m_dyn[i].clear();
    m_trans.clear();
}
