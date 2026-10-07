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
#include <algorithm>
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
// recording (video mode)
bool g_loaded = false, g_recording = false, g_recOn = false;
double g_recTime = 0;
struct RecEvent { double t; int snd; float vol; };
std::vector<RecEvent> g_events;
std::vector<float> g_recDay, g_recNight;   // background level per video frame
float g_frameLen = 1.0f / 30.0f;

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
    g_loaded = true;
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
    if (g_recording) {
        if (g_recOn && !g_snd[s].empty()) g_events.push_back({g_recTime, (int)s, volume});
        return;
    }
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

void startRecording() {
    if (!g_loaded) { loadAll(); g_loaded = true; }
    g_recording = true;
    g_recOn = false;
    g_recTime = 0;
    g_events.clear();
    g_recDay.clear();
    g_recNight.clear();
}

void recordFrame(float seconds) {
    g_recOn = true;
    g_frameLen = seconds;
    g_recDay.push_back(g_dayLevel);
    g_recNight.push_back(g_nightLevel);
    g_recTime += seconds;
}

void stopRecording(bool keep) { g_recOn = keep; }

bool writeRecording(const std::string& path) {
    size_t n = (size_t)(g_recTime * RATE) + RATE;   // one extra second for sounds that ring out
    std::vector<float> mix(n, 0.0f);
    for (size_t i = 0; i < n; ++i) {   // background, level taken from the video frame
        size_t f = std::min(g_recDay.size() - 1, (size_t)(i / (g_frameLen * RATE)));
        float d = g_recDay.empty() ? 0.0f : g_recDay[f], ng = g_recNight.empty() ? 0.0f : g_recNight[f];
        mix[i] += g_day[i % g_day.size()] * d + g_night[i % g_night.size()] * ng;
    }
    for (const auto& e : g_events) {
        size_t s0 = (size_t)(e.t * RATE);
        const auto& b = g_snd[e.snd];
        for (size_t k = 0; k < b.size() && s0 + k < n; ++k) mix[s0 + k] += b[k] * e.vol;
    }
    std::vector<int16_t> pcm(n);
    for (size_t i = 0; i < n; ++i) pcm[i] = (int16_t)(std::tanh(mix[i]) * 32000.0f);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    uint32_t dataBytes = (uint32_t)(n * 2), riff = 36 + dataBytes, fmtLen = 16, rate = RATE, byteRate = RATE * 2;
    uint16_t pcmFmt = 1, ch = 1, align = 2, bits = 16;
    f.write("RIFF", 4); f.write((char*)&riff, 4); f.write("WAVE", 4);
    f.write("fmt ", 4); f.write((char*)&fmtLen, 4); f.write((char*)&pcmFmt, 2); f.write((char*)&ch, 2);
    f.write((char*)&rate, 4); f.write((char*)&byteRate, 4); f.write((char*)&align, 2); f.write((char*)&bits, 2);
    f.write("data", 4); f.write((char*)&dataBytes, 4); f.write((char*)pcm.data(), dataBytes);
    g_recording = false;
    return true;
}
bool muted() { return g_mute; }

}  // namespace Audio
