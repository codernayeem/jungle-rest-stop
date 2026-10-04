// =============================================================================
//  Screenshot.h  -  Save the current frame (or any RGBA buffer) as a PNG file.
//  Small self-contained PNG writer (zlib "stored" blocks, no compression lib).
// =============================================================================
#pragma once
#include <string>
#include <vector>
#include <cstdint>

bool writePNG(const std::string& path, int w, int h, const uint8_t* rgb, int channels);
bool saveFramebufferPNG(const std::string& path, int w, int h);
