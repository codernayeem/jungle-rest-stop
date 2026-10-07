#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include "Audio.h"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Audio {
namespace {

const int RATE = 44100;
const int BLOCK = 2048;      // samples per buffer (~46 ms)
const int NBUF = 4;

std::vector<float> g_snd[SND_COUNT];
std::vector<float> g_day, g_night;   // looping background sounds

struct Voice { const std::vector<float>* buf; size_t pos; float vol; };
std::vector<Voice> g_voices;
std::mutex g_lock;
std::atomic<bool> g_run{false};
std::atomic<bool> g_mute{false};
std::thread g_thread;
HWAVEOUT g_out = nullptr;
Vec3 g_listener;
float g_dayLevel = 0, g_nightLevel = 0;
size_t g_dayPos = 0, g_nightPos = 0;

// ---------------------------------------------------------------- WAV loading
// Reads a 16-bit PCM WAV file (mono or stereo) into floats in [-1, 1].
std::vector<float> loadWav(const std::string& path) {
    std::vector<float> out;
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "Sound file missing: " << path << std::endl; return out; }
    char id[4];
    uint32_t size;
    f.read(id, 4); f.read((char*)&size, 4); f.read(id, 4);   // "RIFF" <size> "WAVE"
    uint16_t channels = 1, bits = 16;
    while (f.read(id, 4) && f.read((char*)&size, 4)) {
        if (!std::strncmp(id, "fmt ", 4)) {
            std::vector<char> fmt(size);
            f.read(fmt.data(), size);
            std::memcpy(&channels, &fmt[2], 2);
            std::memcpy(&bits, &fmt[14], 2);
        } else if (!std::strncmp(id, "data", 4)) {
            if (bits != 16) break;
            std::vector<int16_t> pcm(size / 2);
            f.read((char*)pcm.data(), size);
            size_t frames = pcm.size() / channels;
            out.resize(frames);
            for (size_t i = 0; i < frames; ++i) {
                float s = 0;
                for (int c = 0; c < channels; ++c) s += pcm[i * channels + c];
                out[i] = s / (32768.0f * channels);
            }
            break;
        } else {
            f.seekg(size + (size & 1), std::ios::cur);   // skip other chunks
        }
    }
    return out;
}

// the sounds sit in assets/sounds next to main.exe
std::string soundDir() {
    char buf[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    size_t k = p.find_last_of("\\/");
    return (k == std::string::npos ? std::string(".") : p.substr(0, k)) + "\\assets\\sounds\\";
}

void loadAll() {
    const char* names[SND_COUNT] = {"click", "beep", "coin", "coin_drop", "thud", "motor", "cash",
                                    "grind", "pour", "door", "car_open", "car_close"};
    std::string dir = soundDir();
    for (int i = 0; i < SND_COUNT; ++i) g_snd[i] = loadWav(dir + names[i] + ".wav");
    g_day = loadWav(dir + "amb_day.wav");
    g_night = loadWav(dir + "amb_night.wav");
    if (g_day.empty()) g_day.assign(1, 0.0f);
    if (g_night.empty()) g_night.assign(1, 0.0f);
}

// ---------------------------------------------------------------- mixer thread
void mixBlock(short* out) {
    float buf[BLOCK] = {0};
    {
        std::lock_guard<std::mutex> lk(g_lock);
        for (int i = 0; i < BLOCK; ++i) {
            buf[i] += g_day[g_dayPos] * g_dayLevel + g_night[g_nightPos] * g_nightLevel;
            g_dayPos = (g_dayPos + 1) % g_day.size();
            g_nightPos = (g_nightPos + 1) % g_night.size();
        }
        for (auto& v : g_voices)
            for (int i = 0; i < BLOCK && v.pos < v.buf->size(); ++i) buf[i] += (*v.buf)[v.pos++] * v.vol;
        size_t k = 0;
        for (auto& v : g_voices)
            if (v.pos < v.buf->size()) g_voices[k++] = v;
        g_voices.resize(k);
    }
    bool mute = g_mute;
    for (int i = 0; i < BLOCK; ++i) {
        float s = mute ? 0.0f : std::tanh(buf[i]);   // soft limit, no harsh clipping
        out[i] = (short)(s * 32000.0f);
    }
}

void threadMain() {
    std::vector<short> data(BLOCK * NBUF);
    WAVEHDR hdr[NBUF] = {};
    for (int b = 0; b < NBUF; ++b) {
        hdr[b].lpData = (LPSTR)&data[b * BLOCK];
        hdr[b].dwBufferLength = BLOCK * sizeof(short);
        waveOutPrepareHeader(g_out, &hdr[b], sizeof(WAVEHDR));
        mixBlock(&data[b * BLOCK]);
        waveOutWrite(g_out, &hdr[b], sizeof(WAVEHDR));
    }
    while (g_run) {
        for (int b = 0; b < NBUF; ++b)
            if (hdr[b].dwFlags & WHDR_DONE) {
                mixBlock(&data[b * BLOCK]);
                waveOutWrite(g_out, &hdr[b], sizeof(WAVEHDR));
            }
        Sleep(5);
    }
    waveOutReset(g_out);
    for (int b = 0; b < NBUF; ++b) waveOutUnprepareHeader(g_out, &hdr[b], sizeof(WAVEHDR));
}

}  // namespace

bool init() {
    loadAll();
    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = RATE;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = RATE * 2;
    if (waveOutOpen(&g_out, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        g_out = nullptr;
        return false;   // no sound card: the program simply runs silent
    }
    g_run = true;
    g_thread = std::thread(threadMain);
    return true;
}

void shutdown() {
    if (!g_out) return;
    g_run = false;
    if (g_thread.joinable()) g_thread.join();
    waveOutClose(g_out);
    g_out = nullptr;
}

void setListener(const Vec3& p) { g_listener = p; }

void play(Sound s, float volume) {
    if (!g_out || g_snd[s].empty()) return;
    std::lock_guard<std::mutex> lk(g_lock);
    if (g_voices.size() < 16) g_voices.push_back({&g_snd[s], 0, volume});
}

void playAt(Sound s, const Vec3& pos, float volume) {
    float d = (pos - g_listener).length();
    float v = volume / (1.0f + (d / 4.0f) * (d / 4.0f));   // half as loud at 4 m, 1/5 at 8 m
    if (v > 0.03f) play(s, v);
}

void setAmbience(float day, float night) {
    std::lock_guard<std::mutex> lk(g_lock);
    g_dayLevel = day;
    g_nightLevel = night;
}

void toggleMute() { g_mute = !g_mute; }
bool muted() { return g_mute; }

}  // namespace Audio
