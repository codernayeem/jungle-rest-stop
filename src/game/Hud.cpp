#include "Hud.h"
#include "Messages.h"
#include "../objects/ATM.h"
#include <cstdio>

namespace {
const Vec4 PANEL(0.03f, 0.05f, 0.08f, 0.62f);
const Vec4 WHITE(1, 1, 1, 1);
const Vec4 DIM(0.75f, 0.82f, 0.9f, 1);
const Vec4 ACCENT(0.35f, 0.85f, 0.65f, 1);
const Vec4 GOLD(1.0f, 0.82f, 0.35f, 1);
}  // namespace

bool Hud::click(float x, float y) {
    for (auto& b : buttons)
        if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) {
            if (b.onClick) b.onClick();
            return true;
        }
    return false;
}

void Hud::draw(TextRenderer& t, int W, int H, const HudData& d) {
    t.begin2D(W, H);
    float s = std::max(0.75f, H / 900.0f);   // UI scale
    float fs = 17.0f * s, lh = fs * 1.25f;

    // ---------------- status panel (top-left) ----------------
    float px = 12, py = 12, pw = 330 * s;
    int lines = 6;
    t.rect(px, py, pw, lh * lines + 16, PANEL);
    float y = py + 8;
    char buf[160];
    t.text(px + 10, y, fs * 1.1f, "JUNGLE REST STOP", GOLD); y += lh * 1.1f;
    std::snprintf(buf, sizeof(buf), "%s  |  %s", d.clock.c_str(), d.phase.c_str());
    t.text(px + 10, y, fs, buf, WHITE); y += lh;
    std::snprintf(buf, sizeof(buf), "Camera: %s  FOV %d", d.mode.c_str(), (int)d.fov);
    t.text(px + 10, y, fs, buf, DIM); y += lh;
    std::snprintf(buf, sizeof(buf), "Render: %s", d.renderMode.c_str());
    t.text(px + 10, y, fs, buf, d.enhanced ? ACCENT : Vec4(1, 0.7f, 0.4f, 1)); y += lh;
    std::snprintf(buf, sizeof(buf), "FPS %d (cap %s)  calls %d", d.fps, d.fpsCapText.c_str(), d.drawCalls);
    t.text(px + 10, y, fs * 0.9f, buf, DIM); y += lh;
    std::snprintf(buf, sizeof(buf), "Visitors %d  parked %d", d.visitors, d.parked);
    t.text(px + 10, y, fs * 0.9f, buf, DIM);

    // ---------------- wallet + items (top-right) ----------------
    float rw = 250 * s, rx = W - rw - 12, ry = 12;
    int il = (int)d.items.size();
    t.rect(rx, ry, rw, lh * (2 + (il ? il + 1 : 0)) + 14, PANEL);
    float yy = ry + 8;
    std::snprintf(buf, sizeof(buf), "Cash: Tk %d", d.cash);
    t.text(rx + 10, yy, fs, buf, GOLD); yy += lh;
    std::snprintf(buf, sizeof(buf), "Bank: Tk %d", d.bank);
    t.text(rx + 10, yy, fs * 0.9f, buf, DIM); yy += lh;
    if (il) {
        t.text(rx + 10, yy, fs * 0.9f, "Bag:", ACCENT); yy += lh;
        for (const auto& it : d.items) { t.text(rx + 20, yy, fs * 0.9f, "- " + it, WHITE); yy += lh; }
    }

    // ---------------- buttons (bottom-right, clickable when cursor is free) ----------------
    float bw = 190 * s, bh = 30 * s, bx = W - bw - 12, by = H - 12 - bh;
    for (int i = (int)buttons.size() - 1; i >= 0; --i) {
        auto& b = buttons[i];
        b.x = bx; b.y = by; b.w = bw; b.h = bh;
        t.rect(bx, by, bw, bh, b.active ? Vec4(0.10f, 0.45f, 0.35f, 0.85f) : Vec4(0.15f, 0.17f, 0.22f, 0.85f));
        t.rect(bx, by, 4, bh, b.active ? ACCENT : Vec4(0.5f, 0.5f, 0.55f, 1));
        t.text(bx + 12, by + (bh - fs * 0.95f) * 0.5f, fs * 0.95f, b.label, WHITE);
        by -= bh + 6;
    }
    if (!d.cursorFree) t.text(bx, by + 4, fs * 0.8f, "[Alt] free mouse to click", DIM);

    // ---------------- ATM steps (left) ----------------
    if (d.showAtmSteps) {
        float ax = 12, ay = py + lh * lines + 30, aw = 300 * s;
        t.rect(ax, ay, aw, lh * 10 + 10, PANEL);
        t.text(ax + 10, ay + 6, fs * 1.05f, "ATM - STEP BY STEP", GOLD);
        for (int i = 1; i <= ATM::STEP_COUNT; ++i) {
            float ly = ay + 8 + lh * (i + 0.2f);
            bool done = d.atmStep > i, cur = d.atmStep == i;
            if (cur) t.rect(ax + 4, ly - 2, aw - 8, lh, Vec4(0.15f, 0.45f, 0.75f, 0.6f));
            std::snprintf(buf, sizeof(buf), "%s %d. %s", done ? "[x]" : (cur ? "[>]" : "[ ]"), i, ATM::stepName(i));
            t.text(ax + 10, ly, fs * 0.92f, buf, done ? ACCENT : (cur ? WHITE : DIM));
        }
        if (d.atmStep >= 9) t.text(ax + 10, ay + 8 + lh * 9.2f, fs * 0.92f, "Done - thank you!", ACCENT);
    }

    // ---------------- crosshair + prompt ----------------
    if (d.crosshair) {
        float cx = W * 0.5f, cy = H * 0.5f;
        Vec4 c = d.prompt.empty() ? Vec4(1, 1, 1, 0.7f) : Vec4(0.4f, 1.0f, 0.7f, 1.0f);
        t.rect(cx - 9, cy - 1, 18, 2, c);
        t.rect(cx - 1, cy - 9, 2, 18, c);
        if (!d.prompt.empty()) {
            std::string p = "[E / Click]  " + d.prompt;
            float tw = t.width(p, fs * 1.05f);
            t.rect(cx - tw * 0.5f - 12, cy + 30, tw + 24, fs * 1.05f + 14, PANEL);
            t.text(cx, cy + 37, fs * 1.05f, p, WHITE, 1);
        }
    }

    // ---------------- notifications ----------------
    float ty = H * 0.68f;
    for (const auto& m : Messages::get().list()) {
        float a = std::min(1.0f, m.time);
        float tw = t.width(m.text, fs);
        t.rect(W * 0.5f - tw * 0.5f - 10, ty, tw + 20, fs + 10, Vec4(0.05f, 0.08f, 0.12f, 0.7f * a));
        t.text(W * 0.5f, ty + 5, fs, m.text, Vec4(1, 0.95f, 0.75f, a), 1);
        ty += fs + 14;
    }

    // ---------------- help ----------------
    if (d.showHelp) {
        const char* help[] = {
            "CONTROLS",
            "W A S D  move     Shift  run     Space  jump",
            "Mouse  look       E / Left click  use the thing you look at",
            "V  camera: first person -> third person -> free camera",
            "Mouse wheel / + -  change FOV",
            "B  BASIC <-> ENHANCED rendering   T textures   G shadows   F fog",
            "N  day <-> night    [ ]  time speed    M  pause time",
            "R  roof cut-away    L  FPS cap 30/60    K  call a visitor car    O  sound on/off",
            "F1-F4  lounge / spot / room / outdoor lights",
            "F12  screenshot     Alt  free the mouse     H  hide help     Esc  quit",
            "",
            "Vending: insert coin/note, type code (e.g. B2), press OK, push flap",
            "ATM: insert card, PIN 1234, choose service with the side keys",
        };
        int n = sizeof(help) / sizeof(help[0]);
        float hw = 700 * s, hx = W * 0.5f - hw * 0.5f, hy = H * 0.5f - (n * lh + 20) * 0.5f;
        t.rect(hx, hy, hw, n * lh + 20, Vec4(0.02f, 0.03f, 0.05f, 0.85f));
        for (int i = 0; i < n; ++i) t.text(hx + 16, hy + 10 + i * lh, fs, help[i], i == 0 ? GOLD : WHITE);
    } else {
        t.text(12, H - 12 - fs, fs * 0.85f, "H = help", DIM);
    }
    t.end2D();
}
