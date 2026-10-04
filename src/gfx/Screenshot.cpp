#include "Screenshot.h"
#include <glad/gl.h>
#include <cstdio>

namespace {
uint32_t crcTable[256];
bool crcReady = false;

void initCrc() {
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crcTable[n] = c;
    }
    crcReady = true;
}
uint32_t crc(const uint8_t* buf, size_t len, uint32_t c = 0xFFFFFFFFu) {
    if (!crcReady) initCrc();
    for (size_t i = 0; i < len; ++i) c = crcTable[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c;
}
void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((x >> 24) & 255); v.push_back((x >> 16) & 255); v.push_back((x >> 8) & 255); v.push_back(x & 255);
}
void chunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> buf;
    put32(buf, (uint32_t)data.size());
    buf.insert(buf.end(), type, type + 4);
    buf.insert(buf.end(), data.begin(), data.end());
    uint32_t c = crc(buf.data() + 4, buf.size() - 4) ^ 0xFFFFFFFFu;
    put32(buf, c);
    fwrite(buf.data(), 1, buf.size(), f);
}
}  // namespace

bool writePNG(const std::string& path, int w, int h, const uint8_t* px, int ch) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr;
    put32(ihdr, w);
    put32(ihdr, h);
    ihdr.push_back(8);
    ihdr.push_back(ch == 4 ? 6 : 2);
    ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(f, "IHDR", ihdr);

    // raw scanlines (filter byte 0) wrapped in zlib stored blocks
    std::vector<uint8_t> raw;
    raw.reserve((size_t)h * (w * ch + 1));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), px + (size_t)y * w * ch, px + (size_t)(y + 1) * w * ch);
    }
    std::vector<uint8_t> z;
    z.push_back(0x78); z.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back(n & 255); z.push_back((n >> 8) & 255);
        z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    put32(z, (b << 16) | a);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", {});
    std::fclose(f);
    return true;
}

bool saveFramebufferPNG(const std::string& path, int w, int h) {
    std::vector<uint8_t> px((size_t)w * h * 3), flipped((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h; ++y)
        std::copy(px.begin() + (size_t)(h - 1 - y) * w * 3, px.begin() + (size_t)(h - y) * w * 3, flipped.begin() + (size_t)y * w * 3);
    return writePNG(path, w, h, flipped.data(), 3);
}
