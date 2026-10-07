// =============================================================================
//  Cinematic.cpp  -  main.exe --video out.mp4
//  Renders a short film of the scene, frame by frame at exactly 30 FPS:
//    * a list of shots; each shot has a camera move (position + target over
//      time t = 0..1, with smooth easing) and a short caption
//    * between shots the world is fast-forwarded (simulated without drawing)
//      until a visitor really does the thing the next shot shows
//    * every frame is read back and piped to ffmpeg (H.264 mp4); every sound
//      that plays is recorded with its time and mixed into the soundtrack
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "App.h"
#include "../game/Audio.h"
#include "../game/Messages.h"
#include "../scene/Layout.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct Cam { Vec3 pos, target; float fov; };
float ease(float t) { t = clampf(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }
Vec3 lerpv(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
}  // namespace

void App::runCinematic(const std::string& outPath) {
    const float DT = 1.0f / 30.0f;
    int W, H;
    glfwGetFramebufferSize(m_win, &W, &H);
    W &= ~1;
    H &= ~1;
    m_hudVisible = false;
    m_showHelp = false;
    Messages::get().push("", 0.0f);

    const std::string videoTmp = outPath + ".video.mp4", audioTmp = outPath + ".audio.wav";
    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd),
                  "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgb24 -s %dx%d -r 30 -i - -vf vflip "
                  "-c:v libx264 -preset medium -crf 20 -pix_fmt yuv420p \"%s\"",
                  W, H, videoTmp.c_str());
    FILE* ff = _popen(cmd, "wb");
    if (!ff) { std::cerr << "Could not start ffmpeg (is it installed and on the PATH?)\n"; return; }
    std::vector<unsigned char> px((size_t)W * H * 3);
    Audio::startRecording();
    int totalFrames = 0;

    // ---------------- helpers ----------------
    auto setCam = [&](const Cam& c) {
        m_player.mode = CAM_FREE;
        m_player.pos = Vec3(60.0f, 0.0f, -60.0f);   // the player's (unseen) body stays out of the visitors' way
        m_player.freePos = c.pos;
        Vec3 d = normalize(c.target - c.pos);
        m_player.freeYaw = toDegrees(std::atan2(d.x, d.z));
        m_player.freePitch = toDegrees(std::asin(clampf(d.y, -1.0f, 1.0f)));
        m_player.fov = c.fov;
    };
    auto hour = [&](float h) { m_day.hour = h; m_day.paused = true; };
    // fast-forward (no drawing, no sound) until the condition is true
    auto simUntil = [&](const std::function<bool()>& cond, float maxSeconds) {
        Audio::stopRecording(false);
        for (int i = 0; i < (int)(maxSeconds / DT); ++i) {
            update(DT);
            if (cond && cond()) { std::cout << "  waited " << (int)(i * DT) << " s" << std::endl; return true; }
        }
        if (cond) std::cout << "  (event did not happen, shot skipped)" << std::endl;
        return false;
    };
    // finds a visitor whose description and stage match
    auto findVisitor = [&](const char* what, int minStage, int maxStage) {
        for (int i = 0; i < m_visitors.slots(); ++i) {
            VisitorSystem::Info in = m_visitors.info(i);
            if (in.active && std::strcmp(m_visitors.describe(i), what) == 0 && in.actStage >= minStage && in.actStage <= maxStage) return i;
        }
        return -1;
    };
    // records one shot
    auto shot = [&](float seconds, const std::function<Cam(float)>& camAt, const std::string& caption, bool title = false,
                    const std::string& sub = "") {
        int frames = (int)(seconds * 30.0f);
        for (int k = 0; k < frames; ++k) {
            float t = frames > 1 ? (float)k / (frames - 1) : 0.0f;
            setCam(camAt(t));
            update(DT);
            render(W, H, false);
            // overlay: fade from / to black, caption
            float time = k * DT, left = seconds - time;
            float fade = std::max(0.0f, std::max(1.0f - time / 0.35f, 1.0f - left / 0.35f));
            float ca = clampf(std::min((time - 0.4f) / 0.5f, (left - 0.3f) / 0.5f), 0.0f, 1.0f);
            m_text.begin2D(W, H);
            float s = H / 1080.0f;
            if (title) {
                m_text.text(W * 0.5f, H * 0.40f, 92 * s, caption, Vec4(1, 1, 1, ca), 1);
                if (!sub.empty()) m_text.text(W * 0.5f, H * 0.40f + 110 * s, 40 * s, sub, Vec4(0.92f, 0.95f, 0.85f, ca), 1);
            } else if (!caption.empty()) {
                float tw = m_text.width(caption, 38 * s) + 60 * s;
                m_text.rect(60 * s, H - 150 * s, tw, 70 * s, Vec4(0.0f, 0.0f, 0.0f, 0.45f * ca));
                m_text.text(90 * s, H - 133 * s, 38 * s, caption, Vec4(1, 1, 1, ca));
            }
            if (fade > 0.0f) m_text.rect(0, 0, (float)W, (float)H, Vec4(0, 0, 0, fade));
            m_text.end2D();
            glFinish();
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            fwrite(px.data(), 1, px.size(), ff);
            Audio::recordFrame(DT);
            glfwSwapBuffers(m_win);
            glfwPollEvents();
            ++totalFrames;
        }
        std::cout << "  shot done (" << totalFrames / 30 << " s)" << std::endl;
    };
    // smooth follow: the camera glides after a moving point instead of jumping
    Vec3 sPos, sTgt;
    bool sInit = false;
    auto follow = [&](const Vec3& pos, const Vec3& tgt, float k) {
        if (!sInit) { sPos = pos; sTgt = tgt; sInit = true; }
        sPos = lerpv(sPos, pos, k);
        sTgt = lerpv(sTgt, tgt, k);
    };

    // ---------------- setup: a busy rest stop on a clear morning ----------------
    hour(9.6f);
    setCam(Cam{Vec3(0, 30, 60), Vec3(0, 0, 0), 55});
    m_visitors.maxVisitors = 6;
    m_visitors.spawnMin = 4.0f;
    m_visitors.spawnMax = 8.0f;
    simUntil(nullptr, 70.0f);

    // 1. establishing shot: down from the sky to the rest stop
    shot(5.5f, [&](float t) {
        float e = ease(t);
        return Cam{lerpv(Vec3(-70, 60, 100), Vec3(-12, 18, 38), e), lerpv(Vec3(0, 0, -5), Vec3(1.5f, 2, 3), e), 55};
    }, "Jungle Rest Stop", true, "a rest stop in the middle of the jungle");

    // 2. a visitor's car on the road (as soon as a parking slot is free, a car is called)
    int car = -1;
    simUntil([&]() {
        if (m_visitors.visitorsActive() < m_visitors.slots()) m_visitors.forceSpawn();
        for (int i = 0; i < m_visitors.slots(); ++i) {
            VisitorSystem::Info in = m_visitors.info(i);
            if (in.active && in.state == VisitorSystem::ST_DRIVE_IN && in.carPos.x > 58.0f && in.carPos.x < 72.0f) { car = i; return true; }
        }
        return false;
    }, 240.0f);
    if (car >= 0) {
        sInit = false;
        shot(4.5f, [&](float t) {
            VisitorSystem::Info in = m_visitors.info(car);
            Vec3 c = in.carPos;
            follow(c + Vec3(-9.0f + 3.0f * t, 2.0f, -5.4f), c + Vec3(0, 0.7f, 0), 0.2f);
            return Cam{sPos, sTgt, 52};
        }, "Visitors arrive only by car");

        // 3. it parks
        if (simUntil([&]() { return m_visitors.info(car).state == VisitorSystem::ST_PARKING; }, 40.0f)) {
            Vec3 c0 = m_visitors.info(car).carPos;
            float cx = c0.x - 7.0f;
            for (float p : {-20.0f, -10.0f, 10.0f, 20.0f})
                if (std::fabs(cx - p) < 2.5f) cx += 3.0f;   // keep the ornamental palms out of the picture
            sInit = false;
            shot(5.5f, [&](float t) {
                follow(Vec3(cx, 4.6f, 24.0f), m_visitors.info(car).carPos + Vec3(0, 0.6f, 0), 0.12f);
                return Cam{sPos, sTgt, 50};
            }, "It parks along a smooth Bezier curve");
        }
        // 4. the driver gets out (seen from the aisle, next to the car)
        if (simUntil([&]() { return m_visitors.info(car).state == VisitorSystem::ST_EXIT_CAR; }, 20.0f)) {
            Vec3 c0 = m_visitors.info(car).carPos;
            Vec3 camP(c0.x - 5.0f, 2.0f, Layout::AISLE_Z);
            sInit = false;
            shot(3.5f, [&](float t) {
                VisitorSystem::Info in = m_visitors.info(car);
                follow(camP, (in.inCar ? in.carPos : in.pos) + Vec3(0, 1.0f, 0), 0.15f);
                return Cam{sPos, sTgt, 50};
            }, "The driver gets out...");
        }
        // 5. ...and walks in: seen from inside, through the glass doors as they slide open
        if (simUntil([&]() {
                VisitorSystem::Info in = m_visitors.info(car);
                return in.state == VisitorSystem::ST_INSIDE && !in.inCar && in.pos.z < 10.5f && std::fabs(in.pos.x) < 6.0f;
            }, 30.0f)) {
            sInit = false;
            shot(6.0f, [&](float t) {
                VisitorSystem::Info in = m_visitors.info(car);
                Vec3 tg = in.pos + Vec3(0, 1.1f, 0);
                tg.z = std::max(tg.z, 3.0f);
                follow(lerpv(Vec3(1.9f, 1.7f, 0.6f), Vec3(1.6f, 1.65f, 1.2f), ease(t)), tg, 0.1f);
                return Cam{sPos, sTgt, 58};
            }, "...and walks inside");
        }
    }

    // 6. buying from a vending machine
    int v = -1;
    if (simUntil([&]() { v = findVisitor("at a vending machine", 1, 2); return v >= 0; }, 300.0f)) {
        Vec3 p = m_visitors.info(v).pos;
        shot(7.0f, [&](float t) {
            float e = ease(t);
            return Cam{lerpv(Vec3(p.x + 1.7f, 1.6f, p.z + 1.9f), Vec3(p.x + 1.2f, 1.5f, p.z + 1.5f), e), Vec3(p.x - 0.2f, 1.05f, p.z - 0.9f), 55};
        }, "Type a code, the spiral turns, the item drops");
    }
    // 7. the ATM
    if (simUntil([&]() { v = findVisitor("using the ATM", 1, 2); return v >= 0; }, 150.0f)) {
        Vec3 p = m_visitors.info(v).pos;
        const Vec3 atm = Layout::ATM_POS;
        shot(5.5f, [&](float t) {
            return Cam{lerpv(Vec3(p.x + 1.4f, 1.75f, p.z + 1.6f), Vec3(p.x + 1.0f, 1.7f, p.z + 1.3f), ease(t)), Vec3(atm.x, 1.25f, atm.z), 50};
        }, "The ATM shows every step on its screen");
    }
    // 8. coffee
    if (simUntil([&]() { v = findVisitor("making coffee", 1, 2); return v >= 0; }, 300.0f)) {
        Vec3 p = m_visitors.info(v).pos;
        const Vec3 cm = Layout::COFFEE_POS;
        shot(6.5f, [&](float t) {
            return Cam{lerpv(Vec3(p.x + 1.8f, 1.75f, p.z + 0.7f), Vec3(p.x + 1.5f, 1.65f, p.z + 0.5f), ease(t)), Vec3(cm.x, 1.15f, cm.z), 50};
        }, "Fresh coffee: grind, pour, take the cup");
    }
    // 9. relaxing: watching TV (or reading in the library)
    if (simUntil([&]() { v = findVisitor("watching TV", 2, 2); return v >= 0; }, 90.0f))
        shot(4.5f, [&](float t) { return Cam{lerpv(Vec3(3.6f, 1.9f, 3.2f), Vec3(2.2f, 1.8f, 3.4f), ease(t)), Vec3(0.0f, 1.0f, -3.2f), 60}; },
             "Some sit down and watch TV");
    else if (simUntil([&]() { v = findVisitor("reading", 2, 2); return v >= 0; }, 60.0f)) {
        Vec3 p = m_visitors.info(v).pos;
        shot(4.5f, [&](float t) { return Cam{lerpv(p + Vec3(1.8f, 1.5f, 1.8f), p + Vec3(1.4f, 1.4f, 1.6f), ease(t)), p + Vec3(0, 0.8f, 0), 55}; },
             "Some sit down and read");
    }
    // 10. the aquarium
    if (simUntil([&]() { v = findVisitor("watching fish", 1, 9); return v >= 0; }, 90.0f)) {
        Vec3 p = m_visitors.info(v).pos;
        const Vec3 aq = Layout::AQUARIUM_POS;
        shot(4.5f, [&](float t) { return Cam{lerpv(p + Vec3(-1.2f, 1.7f, -2.0f), p + Vec3(-0.8f, 1.6f, -1.6f), ease(t)), Vec3(aq.x, 1.0f, aq.z), 55}; },
             "...or watch the fish");
    }

    // 11. time-lapse: day turns into night
    hour(17.2f);
    m_day.paused = false;
    float oldSpeed = m_day.hoursPerSecond;
    m_day.hoursPerSecond = 2.8f / 7.0f;
    shot(7.0f, [&](float t) {
        float a = -0.55f + 1.1f * t;
        return Cam{Vec3(1.5f + std::sin(a) * 20.0f, 10.0f, 3.0f + std::cos(a) * 20.0f), Vec3(1.5f, 1.5f, 1.0f), 60};
    }, "Day turns into night");
    m_day.hoursPerSecond = oldSpeed;
    hour(20.2f);

    // 12. night: a car leaves with its headlights on
    int leaving = -1;
    if (simUntil([&]() {
            for (int i = 0; i < m_visitors.slots(); ++i)
                if (m_visitors.info(i).active && m_visitors.info(i).state == VisitorSystem::ST_UNPARKING) { leaving = i; return true; }
            return false;
        }, 150.0f)) {
        sInit = false;
        shot(5.5f, [&](float t) {
            Vec3 c = m_visitors.info(leaving).carPos;
            follow(Vec3(c.x + 9.0f, 3.2f, Layout::AISLE_Z - 1.0f), c + Vec3(0, 0.6f, 0), 0.1f);   // from the open aisle
            return Cam{sPos, sTgt, 55};
        }, "At night: headlights, street lamps and fireflies");
    }

    // 13. end: pull back into the night sky
    shot(5.0f, [&](float t) {
        float e = ease(t);
        return Cam{lerpv(Vec3(-4, 9, 26), Vec3(-40, 42, 80), e), Vec3(1.5f, 1.5f, 2.0f), 55};
    }, "Jungle Rest Stop", true, "Md. Nayeem  |  2107050  |  CSE 4102");

    // ---------------- finish: close the video, add the soundtrack ----------------
    _pclose(ff);
    Audio::writeRecording(audioTmp);
    std::snprintf(cmd, sizeof(cmd), "ffmpeg -y -loglevel error -i \"%s\" -i \"%s\" -c:v copy -af \"volume=1.8,alimiter=limit=0.9\" -c:a aac -b:a 160k -shortest \"%s\"",
                  videoTmp.c_str(), audioTmp.c_str(), outPath.c_str());
    int rc = std::system(cmd);
    if (rc == 0) {
        std::remove(videoTmp.c_str());
        std::remove(audioTmp.c_str());
    }
    std::cout << "Video written to " << outPath << " (" << totalFrames / 30 << " s, " << W << "x" << H << ")" << std::endl;
}
