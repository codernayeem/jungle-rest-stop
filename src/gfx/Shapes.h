// =============================================================================
//  Shapes.h  -  The basic shapes (primitives) every object is built from.
//  Each shape is a unit-sized mesh centred at the origin. Final objects are
//  produced only by translating, rotating and scaling these primitives.
// =============================================================================
#pragma once

#include <glad/gl.h>
#include <vector>
#include "../core/Math3D.h"

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

// GPU mesh (vertex + index buffer)
struct Mesh {
    GLuint vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    GLsizei vertexCount = 0;
    float boundRadius = 0.87f;   // radius of a sphere that contains the unit mesh
    Vec3 boundCenter;            // centre of that sphere (local space)
};

// Identifiers of every basic shape
enum MeshId {
    M_CUBE = 0,      // 1 x 1 x 1 box
    M_CYLINDER,      // radius 0.5, height 1, axis = Y, 24 sides
    M_CYLINDER_LOW,  // same, only 8 sides (tree trunks, far objects)
    M_SPHERE,        // radius 0.5, 16 x 20 segments
    M_SPHERE_LOW,    // radius 0.5, 7 x 10 segments (foliage blobs)
    M_CONE,          // base radius 0.5 at y=-0.5, tip at y=+0.5
    M_TCONE,         // truncated cone: bottom r=0.5, top r=0.35 (cups, pots, shades)
    M_TORUS,         // ring radius 0.4, tube radius 0.1, lying in XZ plane
    M_HEMISPHERE,    // top half of a sphere, radius 0.5, flat side at y=0
    M_WEDGE,         // triangular prism: right-angled triangle in YZ, extruded in X
    M_FRUSTUM_BOX,   // tapered box: bottom 1 x 1, top 0.7 x 0.6 (car cabins, roofs)
    M_QUAD,          // vertical 1 x 1 square in the XY plane, facing +Z
    M_CROSS,         // two crossed vertical quads, y in [0,1] (grass, ferns)
    M_LEAVES,        // three crossed quads centred at origin (leaf clusters)
    M_COIL,          // helical spiral (vending machine product spirals)
    M_TERRAIN,       // displaced grid (ground) - generated from the world height map
    M_COUNT
};

// Build all primitives on the GPU. heightFn is used to displace the terrain grid.
void createAllShapes(Mesh meshes[M_COUNT], float (*heightFn)(float, float));
void destroyAllShapes(Mesh meshes[M_COUNT]);
const char* meshName(MeshId id);

// CPU-side generators (also used by the report figures)
void genCube(std::vector<Vertex>& v, std::vector<GLuint>& i);
void genCylinder(std::vector<Vertex>& v, std::vector<GLuint>& i, int segments, float topRadius = 0.5f, bool caps = true);
void genSphere(std::vector<Vertex>& v, std::vector<GLuint>& i, int rings, int sectors, bool hemisphere = false);
