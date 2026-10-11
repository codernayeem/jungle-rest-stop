// =============================================================================
//  Cinematic.cpp  -  main.exe --video out.mp4
//  Renders a two-minute demonstration film, frame by frame at exactly 30 FPS.
//  The film follows a player through the rest stop: arrival, the ATM, the coffee
//  machine, a vending machine, the TV, the aquarium, the rendering modes and the
//  change from day to night. Visitors (NPCs) keep living their own lives around it.
//    * the player is moved along hand-placed paths; to use a machine the camera
//      turns towards a part and "presses E" (the same picking ray as in the game)
//    * between scenes the world is fast-forwarded (simulated without drawing)
//    * every frame is read back and piped to ffmpeg (H.264 mp4); every sound that
//      plays is recorded with its time and mixed into the soundtrack
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "App.h"
#include "../game/Audio.h"
#include "../game/Interaction.h"
#include "../game/Messages.h"
#include "../scene/Layout.h"
#include "../scene/Lights.h"
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
float angleDiff(float a, float b) {   // b - a, wrapped to [-180, 180]
    float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
    return d;
}
}  // namespace

void App::runCinematic(const std::string& outPath) {
    const float DT = 1.0f / 30.0f;
    int W, H;
    glfwGetFramebufferSize(m_win, &W, &H);
    W &= ~1;
    H &= ~1;
    m_hudVisible = true;
    m_hudCompact = true;
    m_showHelp = false;
    Messages::get().push("", 0.0f);

    const std::string videoTmp = outPath + ".video.mp4", audioTmp = outPath + ".audio.wav";
    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd),
                  "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgb24 -s %dx%d -r 30 -i - -vf vflip,scale=1920:1080 "
                  "-c:v libx264 -preset medium -crf 21 -pix_fmt yuv420p \"%s\"",
                  W, H, videoTmp.c_str());
    FILE* ff = _popen(cmd, "wb");
    if (!ff) { std::cerr << "Could not start ffmpeg (is it installed and on the PATH?)\n"; return; }
    std::vector<unsigned char> px((size_t)W * H * 3);
    Audio::startRecording();
    int totalFrames = 0;
    float cutFade = 0.0f;   // fade in from black after a cut
    float endFade = 0.0f;   // fade out at the very end

    // ---------------- recording ----------------
    auto recFrame = [&](bool hud) {
        update(DT);
        render(W, H, hud);
        float fade = std::max(clampf(cutFade / 0.35f, 0.0f, 1.0f), endFade);
        cutFade -= DT;
        if (fade > 0.0f) {
            m_text.begin2D(W, H);
            m_text.rect(0, 0, (float)W, (float)H, Vec4(0, 0, 0, fade));
            m_text.end2D();
        }
        glFinish();
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, px.data());
        fwrite(px.data(), 1, px.size(), ff);
        Audio::recordFrame(DT);
        glfwSwapBuffers(m_win);
        glfwPollEvents();
        ++totalFrames;
    };
    auto cut = [&]() {
        cutFade = 0.35f;
        Messages::get().push("", 0.0f);
        std::cout << "  [" << totalFrames / 30 << " s]" << std::endl;
    };
    auto hour = [&](float h) { m_day.hour = h; m_day.paused = true; };
    // fast-forward (no drawing, no sound) until the condition is true
    auto simUntil = [&](const std::function<bool()>& cond, float maxSeconds) {
        Audio::stopRecording(false);
        for (int i = 0; i < (int)(maxSeconds / DT); ++i) {
            update(DT);
            if (cond && cond()) return true;
        }
        if (cond) std::cout << "  (event did not happen)" << std::endl;
        return false;
    };

    // ---------------- free camera (establishing shots, NPC shots) ----------------
    auto setCam = [&](const Cam& c) {
        m_player.mode = CAM_FREE;
        m_player.scripted = true;
        m_player.scriptWalking = false;
        m_player.pos = Vec3(60.0f, 0.0f, -60.0f);   // the player's body stays out of the way
        m_player.freePos = c.pos;
        Vec3 d = normalize(c.target - c.pos);
        m_player.freeYaw = toDegrees(std::atan2(d.x, d.z));
        m_player.freePitch = toDegrees(std::asin(clampf(d.y, -1.0f, 1.0f)));
        m_player.fov = c.fov;
    };
    auto shot = [&](float seconds, const std::function<Cam(float)>& camAt, bool hud = false) {
        int frames = (int)(seconds * 30.0f);
        for (int k = 0; k < frames; ++k) {
            setCam(camAt(frames > 1 ? (float)k / (frames - 1) : 0.0f));
            recFrame(hud);
        }
    };
    Vec3 sPos, sTgt;
    bool sInit = false;
    auto follow = [&](const Vec3& pos, const Vec3& tgt, float k) {
        if (!sInit) { sPos = pos; sTgt = tgt; sInit = true; }
        sPos = lerpv(sPos, pos, k);
        sTgt = lerpv(sTgt, tgt, k);
    };

    // ---------------- the player ----------------
    auto place = [&](CameraMode mode, const Vec3& feet, float yaw, float pitch, float fov) {
        m_player.mode = mode;
        m_player.scripted = true;
        m_player.scriptWalking = false;
        m_player.pos = feet;
        m_player.yaw = yaw;
        m_player.pitch = pitch;
        m_player.fov = fov;
    };
    // walk through the points; the body turns towards where it walks
    auto walk = [&](const std::vector<Vec3>& pts, float speed, float pitch = -4.0f) {
        m_player.scriptWalking = true;
        for (const Vec3& target : pts) {
            for (int guard = 0; guard < 900; ++guard) {
                Vec3 d = target - m_player.pos;
                d.y = 0;
                float dist = d.length();
                if (dist < 0.03f) break;
                Vec3 dir = d / dist;
                m_player.pos += dir * std::min(dist, speed * DT);
                float want = toDegrees(std::atan2(dir.x, dir.z));
                m_player.yaw += clampf(angleDiff(m_player.yaw, want), -220.0f * DT, 220.0f * DT);
                m_player.pitch += (pitch - m_player.pitch) * 0.08f;
                recFrame(true);
            }
        }
        m_player.scriptWalking = false;
    };
    // turn the head smoothly until the crosshair is on a point
    auto aim = [&](const Vec3& target, float seconds) {
        Vec3 eye = m_player.pos + Vec3(0, m_player.eyeHeight, 0);
        Vec3 d = normalize(target - eye);
        float y0 = m_player.yaw, p0 = m_player.pitch;
        float y1 = y0 + angleDiff(y0, toDegrees(std::atan2(d.x, d.z)));
        float p1 = toDegrees(std::asin(clampf(d.y, -1.0f, 1.0f)));
        int n = std::max(1, (int)(seconds * 30.0f));
        for (int k = 1; k <= n; ++k) {
            float t = ease((float)k / n);
            m_player.yaw = y0 + (y1 - y0) * t;
            m_player.pitch = p0 + (p1 - p0) * t;
            recFrame(true);
        }
    };
    auto wait = [&](float seconds) {
        for (int k = 0; k < (int)(seconds * 30.0f); ++k) recFrame(true);
    };
    auto press = [&](float after) {
        std::cout << "    press: " << InteractionSystem::get().prompt() << std::endl;
        InteractionSystem::get().useHovered();
        wait(after);
    };
    auto pressAt = [&](const Vec3& target, float turn, float after) { aim(target, turn); press(after); };

    // ---------------- setup: a busy rest stop on a clear morning ----------------
    hour(9.4f);
    setCam(Cam{Vec3(0, 30, 60), Vec3(0, 0, 0), 55});
    m_visitors.maxVisitors = 6;
    m_visitors.spawnMin = 4.0f;
    m_visitors.spawnMax = 8.0f;
    simUntil(nullptr, 80.0f);

    // 1. from the sky down to the rest stop in its jungle clearing
    cutFade = 1.0f;
    shot(7.0f, [&](float t) {
        // stay high above the tree tops until the camera is over the open parking lot
        float e = ease(t);
        Vec3 p = lerpv(Vec3(-70, 0, 100), Vec3(-4, 0, 24.5f), e);
        p.y = 60.0f - 49.0f * e * e * e;
        return Cam{p, lerpv(Vec3(0, 0, -5), Vec3(1.5f, 1.2f, 1.0f), e), 60};
    });

    // 2. a visitor's car comes along the road and parks (visitors only arrive by car)
    int car = -1;
    simUntil([&]() {
        if (m_visitors.visitorsActive() < m_visitors.slots()) m_visitors.forceSpawn();
        for (int i = 0; i < m_visitors.slots(); ++i) {
            VisitorSystem::Info in = m_visitors.info(i);
            if (in.active && in.state == VisitorSystem::ST_DRIVE_IN && in.carPos.x > 38.0f && in.carPos.x < 48.0f) { car = i; return true; }
        }
        return false;
    }, 240.0f);
    if (car >= 0) {
        cut();
        sInit = false;
        int frames = 0;
        // follow it until it has parked (at most 10 s)
        while (frames < 300 && m_visitors.info(car).state != VisitorSystem::ST_EXIT_CAR) {
            VisitorSystem::Info in = m_visitors.info(car);
            Vec3 c = in.carPos;
            Vec3 want = in.state == VisitorSystem::ST_DRIVE_IN || in.state == VisitorSystem::ST_WAIT_LOT ? c + Vec3(-7.0f, 2.6f, 6.5f)
                                                                                                       : Vec3(c.x + 1.5f, 4.2f, 25.0f);
            follow(want, c + Vec3(0, 0.6f, 0), 0.06f);
            setCam(Cam{sPos, sTgt, 50});
            recFrame(false);
            ++frames;
        }
        // the driver gets out
        for (int k = 0; k < 75; ++k) {
            VisitorSystem::Info in = m_visitors.info(car);
            follow(Vec3(in.carPos.x + 2.5f, 3.2f, 24.5f), (in.inCar ? in.carPos : in.pos) + Vec3(0, 0.9f, 0), 0.06f);
            setCam(Cam{sPos, sTgt, 50});
            recFrame(false);
        }
    }

    // 3. the player (third person) walks from the parking lot to the entrance; the doors slide open
    cut();
    place(CAM_THIRD, Vec3(-1.2f, 0, 14.0f), 170.0f, -6.0f, 62);
    walk({Vec3(0.0f, 0, 11.0f), Vec3(0.0f, 0, 7.6f)}, 1.7f, -6.0f);
    // 4. first person: inside the lounge, a look around
    m_player.mode = CAM_FIRST;
    m_player.fov = 72;
    walk({Vec3(0.0f, 0, 5.2f), Vec3(0.0f, 0, 3.4f)}, 1.7f, -4.0f);
    aim(Vec3(-4.0f, 1.2f, -1.0f), 1.2f);
    aim(Vec3(3.5f, 1.2f, -3.0f), 1.6f);

    // 5. the ATM: card, PIN 1234, withdraw Tk 1,000, card, cash and receipt
    simUntil([&]() { return m_atm.m_state == ATM::WELCOME && !m_atm.inUseByNpc(); }, 60.0f);
    cut();
    place(CAM_FIRST, Vec3(-7.6f, 0, -2.4f), 250.0f, -8.0f, 62);
    walk({Vec3(-9.2f, 0, -3.0f), m_atm.standPos() + Vec3(0.12f, 0, 0.05f)}, 1.5f, -12.0f);
    Mat4 ar = m_atm.root();
    Mat4 kf = rotate(translate(ar, Vec3(0.0f, 1.035f, 0.40f)), toRadians(12.0f), Vec3(1, 0, 0));
    auto keyAt = [&](int i) { return kf.transformPoint(Vec3(-0.135f + (i % 4) * 0.075f + (i % 4 == 3 ? 0.02f : 0.0f), 0.03f, -0.085f + (i / 4) * 0.052f)); };
    pressAt(ar.transformPoint({0.34f, 1.30f, 0.21f}), 0.7f, 1.5f);          // insert the card
    pressAt(keyAt(0), 0.5f, 0.15f);                                          // 1
    pressAt(keyAt(1), 0.25f, 0.15f);                                         // 2
    pressAt(keyAt(2), 0.25f, 0.15f);                                         // 3
    pressAt(keyAt(4), 0.3f, 0.15f);                                          // 4
    pressAt(keyAt(11), 0.35f, 1.6f);                                         // ENTER -> verify -> menu
    pressAt(ar.transformPoint({-0.245f, 1.53f, 0.20f}), 0.6f, 0.9f);         // Withdraw
    pressAt(ar.transformPoint({-0.245f, 1.47f, 0.20f}), 0.4f, 3.1f);         // Tk 1,000 -> processing
    pressAt(ar.transformPoint({0.34f, 1.30f, 0.21f}), 0.5f, 0.9f);           // take the card
    pressAt(ar.transformPoint({0.0f, 0.88f, 0.37f}), 0.6f, 1.3f);            // take the cash
    pressAt(ar.transformPoint({0.34f, 1.20f, 0.21f}), 0.6f, 1.2f);           // take the receipt

    // 6. the coffee machine: choose a cappuccino, grinding, pouring, steam, take the cup
    simUntil([&]() { return m_coffee.m_state == CoffeeMachine::IDLE && m_coffee.available(); }, 60.0f);
    cut();
    Vec3 cs = m_coffee.standPos();
    place(CAM_FIRST, cs + Vec3(1.4f, 0, 1.0f), 225.0f, -10.0f, 60);
    walk({cs + Vec3(0.15f, 0, 0.1f)}, 1.5f, -14.0f);
    Vec3 cp = Layout::COFFEE_POS;
    pressAt(cp + Vec3(0.0f, 0.497f, 0.21f), 0.7f, 0.3f);                    // cappuccino button
    aim(cp + Vec3(0.0f, 0.25f, 0.1f), 0.8f);
    for (int k = 0; k < 400 && m_coffee.m_state != CoffeeMachine::READY; ++k) recFrame(true);
    wait(0.6f);
    pressAt(cp + Vec3(0.0f, 0.1f, 0.09f), 0.4f, 1.0f);                      // take the cup

    // 7. a vending machine: Tk 50 note, code A1, OK, the spiral turns, the can drops, push the flap
    VendingMachine& vm = m_vend[0];
    simUntil([&]() { return vm.available(); }, 60.0f);
    cut();
    Vec3 vs = vm.standPos() + Vec3(0.12f, 0, 0.12f);
    place(CAM_FIRST, vs + Vec3(-1.6f, 0, 1.6f), 135.0f, -8.0f, 62);
    walk({vs}, 1.6f, -10.0f);
    Mat4 vr = vm.root();
    auto vp = [&](float x, float y) { return vr.transformPoint({x, y, 0.41f}); };
    pressAt(vp(0.33f, 1.12f), 0.7f, 0.7f);                                  // Tk 50 note
    pressAt(vp(0.255f, 1.50f), 0.5f, 0.2f);                                 // A
    pressAt(vp(0.325f, 1.50f), 0.3f, 0.5f);                                 // 1
    pressAt(vp(0.395f, 1.44f), 0.4f, 0.2f);                                 // OK
    aim(vp(-0.15f, 1.1f), 0.6f);                                             // watch the spiral
    for (int k = 0; k < 150 && vm.m_state != VendingMachine::READY; ++k) recFrame(true);
    pressAt(vp(-0.15f, 0.38f), 0.7f, 0.9f);                                 // push the flap: the item goes to the bag
    pressAt(vr.transformPoint({0.33f, 0.86f, 0.41f}), 0.5f, 0.9f);          // take the change

    // 8. the TV: change the channel twice
    cut();
    place(CAM_FIRST, Vec3(0.7f, 0, -2.6f), 195.0f, 4.0f, 70);
    pressAt(Layout::TV_POS, 0.6f, 1.6f);
    press(1.8f);

    // 9. visitors living their lives in the lounge (seen from above the sofas)
    m_hudVisible = false;
    cut();
    shot(5.0f, [&](float t) {
        float e = ease(t);
        return Cam{lerpv(Vec3(5.6f, 2.5f, 4.2f), Vec3(-4.6f, 2.5f, 4.2f), e), lerpv(Vec3(1.5f, 0.9f, -2.5f), Vec3(-2.5f, 0.9f, -2.5f), e), 68};
    });
    m_hudVisible = true;

    // 10. the library: feed the fish, they swim up to the food
    cut();
    Vec3 av = m_aquarium.viewPos();
    place(CAM_FIRST, Vec3(8.4f, 0, 0.2f), 30.0f, -6.0f, 62);
    walk({av + Vec3(0, 0, -0.3f)}, 1.6f, -6.0f);
    pressAt(Layout::AQUARIUM_POS + Vec3(0, 1.0f, 0), 0.7f, 0.3f);
    aim(Layout::AQUARIUM_POS + Vec3(0, 1.15f, 0), 0.5f);
    wait(4.0f);

    // 11. Basic <-> Enhanced rendering (key B), seen from above the parking lot
    cut();
    m_player.scripted = true;
    {
        int frames = 0;
        shot(7.0f, [&](float t) {
            if (frames == 60) onKey(GLFW_KEY_B, GLFW_PRESS, 0);
            if (frames == 140) onKey(GLFW_KEY_B, GLFW_PRESS, 0);
            ++frames;
            float e = ease(t);
            return Cam{lerpv(Vec3(-6.0f, 10.0f, 25.0f), Vec3(-2.0f, 9.0f, 23.0f), e), Vec3(1.5f, 1.2f, 1.0f), 62};
        }, true);
    }

    // 12. time-lapse: the day turns into night
    cut();
    hour(17.0f);
    m_day.paused = false;
    float oldSpeed = m_day.hoursPerSecond;
    m_day.hoursPerSecond = 3.6f / 8.0f;
    shot(8.0f, [&](float t) {
        float a = -0.6f + 1.2f * t;
        return Cam{Vec3(1.5f + std::sin(a) * 21.0f, 9.5f, 3.0f + std::cos(a) * 21.0f), Vec3(1.5f, 1.5f, 1.0f), 60};
    });
    m_day.hoursPerSecond = oldSpeed;
    hour(20.8f);

    // 13. night inside: the player switches the lounge lights off and on again
    cut();
    place(CAM_FIRST, Vec3(-1.0f, 0, 2.9f), 200.0f, -2.0f, 72);
    aim(Vec3(-1.75f, 1.3f, 4.89f), 0.9f);
    press(0.5f);
    aim(Vec3(1.0f, 1.2f, -3.0f), 1.0f);   // the dark room, only the machines glow
    wait(0.8f);
    aim(Vec3(-1.75f, 1.3f, 4.89f), 0.8f);
    press(0.3f);
    aim(Vec3(1.0f, 1.2f, -3.0f), 0.9f);
    wait(0.6f);

    // 14. a car leaves at night with its headlights on
    m_hudVisible = false;
    int leaving = -1;
    if (simUntil([&]() {
            for (int i = 0; i < m_visitors.slots(); ++i)
                if (m_visitors.info(i).active && m_visitors.info(i).state == VisitorSystem::ST_UNPARKING) { leaving = i; return true; }
            return false;
        }, 150.0f)) {
        cut();
        sInit = false;
        shot(5.0f, [&](float) {
            Vec3 c = m_visitors.info(leaving).carPos;
            follow(Vec3(c.x + 9.0f, 3.2f, Layout::AISLE_Z - 1.0f), c + Vec3(0, 0.6f, 0), 0.1f);
            return Cam{sPos, sTgt, 55};
        });
    }

    // 15. the end: up into the night sky, roof hidden so the lit rooms can be seen
    cut();
    m_cutaway = true;
    {
        int frames = 0, total = (int)(7.0f * 30.0f);
        shot(7.0f, [&](float t) {
            ++frames;
            endFade = clampf((frames - (total - 45)) / 45.0f, 0.0f, 1.0f);
            float e = ease(t);
            return Cam{lerpv(Vec3(1.5f, 13.0f, 15.0f), Vec3(-30.0f, 48.0f, 70.0f), e), Vec3(1.5f, 0.5f, 0.0f), 58};
        });
    }
    m_cutaway = false;

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
    m_player.scripted = false;
    std::cout << "Video written to " << outPath << " (" << totalFrames / 30 << " s, " << W << "x" << H << ")" << std::endl;
}
