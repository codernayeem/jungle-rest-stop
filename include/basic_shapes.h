#ifndef BASIC_SHAPES_H
#define BASIC_SHAPES_H

#include <glad/gl.h>
#include "math3d.h"
#include <vector>
#include <cmath>

// -----------------------------------------------------------------------------
// Vertex Data Layout
// -----------------------------------------------------------------------------
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
};

// -----------------------------------------------------------------------------
// OpenGL Mesh Encapsulation
// -----------------------------------------------------------------------------
struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;

    void cleanup() {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ebo) glDeleteBuffers(1, &ebo);
        vao = vbo = ebo = 0;
        indexCount = 0;
    }

    void draw() const {
        if (vao && indexCount > 0) {
            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
    }
};

// Helper to upload vertex and index arrays to GPU
inline Mesh uploadMesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices) {
    Mesh mesh;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // Position (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // Normal (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(Vec3)));

    // TexCoord (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(Vec3)));

    glBindVertexArray(0);
    mesh.indexCount = static_cast<GLsizei>(indices.size());
    return mesh;
}

// -----------------------------------------------------------------------------
// Basic Shape: Unit Cube (1.0 x 1.0 x 1.0, centered at origin)
// -----------------------------------------------------------------------------
inline Mesh createCubeMesh() {
    std::vector<Vertex> vertices = {
        // Front face (+Z)
        { Vec3(-0.5f, -0.5f,  0.5f), Vec3(0.0f, 0.0f, 1.0f), Vec2(0.0f, 0.0f) },
        { Vec3( 0.5f, -0.5f,  0.5f), Vec3(0.0f, 0.0f, 1.0f), Vec2(1.0f, 0.0f) },
        { Vec3( 0.5f,  0.5f,  0.5f), Vec3(0.0f, 0.0f, 1.0f), Vec2(1.0f, 1.0f) },
        { Vec3(-0.5f,  0.5f,  0.5f), Vec3(0.0f, 0.0f, 1.0f), Vec2(0.0f, 1.0f) },

        // Back face (-Z)
        { Vec3( 0.5f, -0.5f, -0.5f), Vec3(0.0f, 0.0f, -1.0f), Vec2(0.0f, 0.0f) },
        { Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.0f, 0.0f, -1.0f), Vec2(1.0f, 0.0f) },
        { Vec3(-0.5f,  0.5f, -0.5f), Vec3(0.0f, 0.0f, -1.0f), Vec2(1.0f, 1.0f) },
        { Vec3( 0.5f,  0.5f, -0.5f), Vec3(0.0f, 0.0f, -1.0f), Vec2(0.0f, 1.0f) },

        // Top face (+Y)
        { Vec3(-0.5f,  0.5f,  0.5f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.0f, 0.0f) },
        { Vec3( 0.5f,  0.5f,  0.5f), Vec3(0.0f, 1.0f, 0.0f), Vec2(1.0f, 0.0f) },
        { Vec3( 0.5f,  0.5f, -0.5f), Vec3(0.0f, 1.0f, 0.0f), Vec2(1.0f, 1.0f) },
        { Vec3(-0.5f,  0.5f, -0.5f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.0f, 1.0f) },

        // Bottom face (-Y)
        { Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.0f, -1.0f, 0.0f), Vec2(0.0f, 0.0f) },
        { Vec3( 0.5f, -0.5f, -0.5f), Vec3(0.0f, -1.0f, 0.0f), Vec2(1.0f, 0.0f) },
        { Vec3( 0.5f, -0.5f,  0.5f), Vec3(0.0f, -1.0f, 0.0f), Vec2(1.0f, 1.0f) },
        { Vec3(-0.5f, -0.5f,  0.5f), Vec3(0.0f, -1.0f, 0.0f), Vec2(0.0f, 1.0f) },

        // Right face (+X)
        { Vec3( 0.5f, -0.5f,  0.5f), Vec3(1.0f, 0.0f, 0.0f), Vec2(0.0f, 0.0f) },
        { Vec3( 0.5f, -0.5f, -0.5f), Vec3(1.0f, 0.0f, 0.0f), Vec2(1.0f, 0.0f) },
        { Vec3( 0.5f,  0.5f, -0.5f), Vec3(1.0f, 0.0f, 0.0f), Vec2(1.0f, 1.0f) },
        { Vec3( 0.5f,  0.5f,  0.5f), Vec3(1.0f, 0.0f, 0.0f), Vec2(0.0f, 1.0f) },

        // Left face (-X)
        { Vec3(-0.5f, -0.5f, -0.5f), Vec3(-1.0f, 0.0f, 0.0f), Vec2(0.0f, 0.0f) },
        { Vec3(-0.5f, -0.5f,  0.5f), Vec3(-1.0f, 0.0f, 0.0f), Vec2(1.0f, 0.0f) },
        { Vec3(-0.5f,  0.5f,  0.5f), Vec3(-1.0f, 0.0f, 0.0f), Vec2(1.0f, 1.0f) },
        { Vec3(-0.5f,  0.5f, -0.5f), Vec3(-1.0f, 0.0f, 0.0f), Vec2(0.0f, 1.0f) }
    };

    std::vector<GLuint> indices;
    for (int i = 0; i < 6; ++i) {
        GLuint base = i * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    return uploadMesh(vertices, indices);
}

// -----------------------------------------------------------------------------
// Basic Shape: Unit Cylinder (radius = 0.5, height = 1.0, along Y-axis)
// -----------------------------------------------------------------------------
inline Mesh createCylinderMesh(int segments = 32) {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    float halfH = 0.5f;
    float radius = 0.5f;

    // Tube side
    for (int i = 0; i <= segments; ++i) {
        float theta = (2.0f * (float)M_PI * i) / segments;
        float x = std::cos(theta);
        float z = std::sin(theta);
        float u = (float)i / segments;

        // Top rim
        vertices.push_back({ Vec3(x * radius, halfH, z * radius), Vec3(x, 0.0f, z), Vec2(u, 1.0f) });
        // Bottom rim
        vertices.push_back({ Vec3(x * radius, -halfH, z * radius), Vec3(x, 0.0f, z), Vec2(u, 0.0f) });
    }

    for (int i = 0; i < segments; ++i) {
        GLuint tl = i * 2;
        GLuint bl = i * 2 + 1;
        GLuint tr = (i + 1) * 2;
        GLuint br = (i + 1) * 2 + 1;

        indices.push_back(tl);
        indices.push_back(bl);
        indices.push_back(tr);

        indices.push_back(tr);
        indices.push_back(bl);
        indices.push_back(br);
    }

    // Top Cap
    GLuint topCenterIdx = (GLuint)vertices.size();
    vertices.push_back({ Vec3(0.0f, halfH, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.5f, 0.5f) });
    for (int i = 0; i <= segments; ++i) {
        float theta = (2.0f * (float)M_PI * i) / segments;
        float x = std::cos(theta);
        float z = std::sin(theta);
        vertices.push_back({ Vec3(x * radius, halfH, z * radius), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.5f + 0.5f * x, 0.5f + 0.5f * z) });
    }
    for (int i = 0; i < segments; ++i) {
        indices.push_back(topCenterIdx);
        indices.push_back(topCenterIdx + 1 + i);
        indices.push_back(topCenterIdx + 2 + i);
    }

    // Bottom Cap
    GLuint bottomCenterIdx = (GLuint)vertices.size();
    vertices.push_back({ Vec3(0.0f, -halfH, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec2(0.5f, 0.5f) });
    for (int i = 0; i <= segments; ++i) {
        float theta = (2.0f * (float)M_PI * i) / segments;
        float x = std::cos(theta);
        float z = std::sin(theta);
        vertices.push_back({ Vec3(x * radius, -halfH, z * radius), Vec3(0.0f, -1.0f, 0.0f), Vec2(0.5f + 0.5f * x, 0.5f + 0.5f * z) });
    }
    for (int i = 0; i < segments; ++i) {
        indices.push_back(bottomCenterIdx);
        indices.push_back(bottomCenterIdx + 2 + i);
        indices.push_back(bottomCenterIdx + 1 + i);
    }

    return uploadMesh(vertices, indices);
}

// -----------------------------------------------------------------------------
// Basic Shape: Unit Sphere (radius = 0.5, centered at origin)
// -----------------------------------------------------------------------------
inline Mesh createSphereMesh(int rings = 20, int sectors = 24) {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    float radius = 0.5f;

    for (int r = 0; r <= rings; ++r) {
        float phi = (float)M_PI * (float)r / rings; // 0 to PI
        float y = radius * std::cos(phi);
        float sinPhi = std::sin(phi);

        for (int s = 0; s <= sectors; ++s) {
            float theta = 2.0f * (float)M_PI * (float)s / sectors; // 0 to 2PI
            float x = radius * sinPhi * std::cos(theta);
            float z = radius * sinPhi * std::sin(theta);

            Vec3 pos(x, y, z);
            Vec3 norm = normalize(pos);
            Vec2 uv((float)s / sectors, (float)r / rings);

            vertices.push_back({ pos, norm, uv });
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            GLuint cur = r * (sectors + 1) + s;
            GLuint next = cur + (sectors + 1);

            indices.push_back(cur);
            indices.push_back(next);
            indices.push_back(cur + 1);

            indices.push_back(cur + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    return uploadMesh(vertices, indices);
}

// -----------------------------------------------------------------------------
// Basic Shape: 3D Helical Coil / Spiral (for vending machine mechanism)
// -----------------------------------------------------------------------------
inline Mesh createCoilMesh(int turns = 5, int pointsPerTurn = 24, float coilRadius = 0.055f, float wireThickness = 0.005f, float length = 0.28f) {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    int totalSteps = turns * pointsPerTurn;
    int tubeSegments = 8;

    for (int i = 0; i <= totalSteps; ++i) {
        float t = (float)i / totalSteps;
        float angle = t * turns * 2.0f * (float)M_PI;
        float z = -length * 0.5f + t * length;
        float cx = coilRadius * std::cos(angle);
        float cy = coilRadius * std::sin(angle);

        // Tangent vector along spiral path
        float dx = -coilRadius * std::sin(angle) * (turns * 2.0f * (float)M_PI);
        float dy =  coilRadius * std::cos(angle) * (turns * 2.0f * (float)M_PI);
        float dz = length;
        Vec3 tangent = normalize(Vec3(dx, dy, dz));
        Vec3 normalDir = normalize(Vec3(cx, cy, 0.0f));
        Vec3 binormal = normalize(cross(tangent, normalDir));

        for (int j = 0; j < tubeSegments; ++j) {
            float phi = (2.0f * (float)M_PI * j) / tubeSegments;
            float cosPhi = std::cos(phi);
            float sinPhi = std::sin(phi);

            Vec3 offset = (normalDir * cosPhi + binormal * sinPhi) * wireThickness;
            Vec3 pos = Vec3(cx, cy, z) + offset;
            Vec3 norm = normalize(offset);

            vertices.push_back({ pos, norm, Vec2(t, (float)j / tubeSegments) });
        }
    }

    for (int i = 0; i < totalSteps; ++i) {
        for (int j = 0; j < tubeSegments; ++j) {
            int nextJ = (j + 1) % tubeSegments;

            GLuint curRing = i * tubeSegments;
            GLuint nextRing = (i + 1) * tubeSegments;

            indices.push_back(curRing + j);
            indices.push_back(nextRing + j);
            indices.push_back(curRing + nextJ);

            indices.push_back(curRing + nextJ);
            indices.push_back(nextRing + j);
            indices.push_back(nextRing + nextJ);
        }
    }

    return uploadMesh(vertices, indices);
}

// -----------------------------------------------------------------------------
// Basic Shape: Unit Quad / Plane (1.0 x 1.0, lying on XZ plane, normal +Y)
// -----------------------------------------------------------------------------
inline Mesh createPlaneMesh() {
    std::vector<Vertex> vertices = {
        { Vec3(-0.5f, 0.0f,  0.5f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.0f, 0.0f) },
        { Vec3( 0.5f, 0.0f,  0.5f), Vec3(0.0f, 1.0f, 0.0f), Vec2(1.0f, 0.0f) },
        { Vec3( 0.5f, 0.0f, -0.5f), Vec3(0.0f, 1.0f, 0.0f), Vec2(1.0f, 1.0f) },
        { Vec3(-0.5f, 0.0f, -0.5f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.0f, 1.0f) }
    };
    std::vector<GLuint> indices = { 0, 1, 2, 0, 2, 3 };
    return uploadMesh(vertices, indices);
}

#endif // BASIC_SHAPES_H
