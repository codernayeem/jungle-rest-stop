// =============================================================================
//  Textures.h  -  Procedural textures (generated in code, no image files).
//  All textures live in one GL_TEXTURE_2D_ARRAY; every instance chooses a layer.
//  RGB stores a "detail" multiplier (0.5 = no change) and A stores a cut-out mask.
// =============================================================================
#pragma once
#include <glad/gl.h>
#include <vector>
#include <cstdint>

enum TexLayer {
    TEX_NONE = 0,
    TEX_GRASS,
    TEX_FOREST_FLOOR,
    TEX_ASPHALT,
    TEX_CONCRETE,
    TEX_BRICK,
    TEX_WOOD,
    TEX_TILE,
    TEX_FABRIC,
    TEX_BARK,
    TEX_LEAVES,        // alpha: broad leaf cluster
    TEX_GRASS_BLADES,  // alpha: grass tuft
    TEX_FERN,          // alpha: fern frond
    TEX_PALM,          // alpha: palm frond
    TEX_METAL,
    TEX_PLASTER,
    TEX_CARPET,
    TEX_ROOF,
    TEX_ROCK,
    TEX_BOOKS,
    TEX_FLOWERS,       // alpha: small flowers
    TEX_CEILING,
    TEX_MARBLE,
    TEX_LEATHER,
    TEX_SAND,
    TEX_VINE,          // alpha: hanging vine strip
    TEX_COUNT
};

class TextureArray {
public:
    static const int SIZE = 256;
    bool create();
    void bind(int unit) const;
    void destroy();
    GLuint id() const { return m_tex; }
    // Raw RGBA pixels of a layer (for saving texture swatches for the report)
    const std::vector<uint8_t>& layerPixels(int layer) const { return m_pixels[layer]; }
    static const char* layerName(int layer);
private:
    GLuint m_tex = 0;
    std::vector<uint8_t> m_pixels[TEX_COUNT];
};
