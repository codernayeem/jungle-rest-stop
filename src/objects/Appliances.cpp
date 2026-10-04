#include "Appliances.h"
#include "../game/Interaction.h"
#include "../game/Inventory.h"
#include "../game/Messages.h"
#include "../scene/Lights.h"
#include "../scene/Materials.h"
#include <cmath>
#include <cstdio>

namespace {
const char* DRINKS[3] = {"Espresso", "Cappuccino", "Masala Tea"};
const Vec3 DRINK_COL[3] = {Vec3(0.22f, 0.11f, 0.05f), Vec3(0.62f, 0.42f, 0.27f), Vec3(0.60f, 0.33f, 0.10f)};
const Vec3 BTN_COL[3] = {Vec3(0.35f, 0.18f, 0.08f), Vec3(0.85f, 0.70f, 0.50f), Vec3(0.85f, 0.45f, 0.10f)};
}  // namespace

// =============================================================================
//  COFFEE MACHINE
// =============================================================================
void CoffeeMachine::init(const Vec3& pos, float yawDeg) {
    m_pos = pos;
    m_yaw = yawDeg;
    m_root = rotateYXZ(translate(Mat4(), pos), {0, yawDeg, 0});
    m_idBase = InteractionSystem::reserveIds(8);
}

Vec3 CoffeeMachine::standPos() const {
    Vec3 p = m_root.transformPoint(Vec3(0, 0, 0.85f));
    p.y = 0.0f;
    return p;
}

void CoffeeMachine::start(int drink, bool npc) {
    if (m_state != IDLE) return;
    m_drink = drink;
    m_npc = npc;
    m_state = CUP_DROP;
    m_t = 0;
    m_btnAnim[drink] = 1.0f;
}

bool CoffeeMachine::npcBrew(int drink) {
    if (!available()) return false;
    start(drink % 3, true);
    return true;
}

void CoffeeMachine::take(bool player) {
    if (m_state != READY) return;
    m_state = IDLE;
    m_steam.clear();
    cupsServed++;
    if (player) {
        Inventory::player().addItem(DRINKS[m_drink]);
        notify(std::string("You took a hot ") + DRINKS[m_drink]);
    }
}

void CoffeeMachine::npcTakeCup() { take(false); }

void CoffeeMachine::update(float dt) {
    m_time += dt;
    m_t += dt;
    for (float& b : m_btnAnim) b = std::max(0.0f, b - dt * 4.0f);
    if (m_state == CUP_DROP && m_t > 0.5f) { m_state = GRINDING; m_t = 0; }
    else if (m_state == GRINDING && m_t > 1.6f) { m_state = BREWING; m_t = 0; }
    else if (m_state == BREWING && m_t > 4.5f) { m_state = READY; m_t = 0; }
    if (m_state == READY || m_state == BREWING) {
        if (std::fmod(m_time, 0.12f) < dt) m_steam.push_back({Vec3(((int)(m_time * 37) % 7 - 3) * 0.006f, 0.15f, 0.09f), 1.6f});
    }
    for (auto& p : m_steam) {
        p.life -= dt;
        p.p.y += dt * 0.07f;
        p.p.x += std::sin(m_time * 3.0f + p.life * 5.0f) * dt * 0.01f;
    }
    while (!m_steam.empty() && m_steam.front().life <= 0) m_steam.erase(m_steam.begin());
}

void CoffeeMachine::registerInteractions() {
    auto& is = InteractionSystem::get();
    if (m_npc && m_state != IDLE) return;
    for (int i = 0; i < 3; ++i)
        is.add(m_idBase + i, translate(m_root, {-0.11f + i * 0.11f, 0.497f, 0.205f}), Vec3(0.03f, 0.03f, 0.02f),
               std::string("Make ") + DRINKS[i] + " (free)", [this, i]() { start(i, false); });
    if (m_state == READY && !m_npc)
        is.add(m_idBase + 3, translate(m_root, {0, 0.1f, 0.09f}), Vec3(0.06f, 0.07f, 0.06f), std::string("Take ") + DRINKS[m_drink],
               [this]() { take(true); });
}

void CoffeeMachine::drawStatic(DrawSink& s) const {
    Node n(s, m_root);
    Material steel = Mat::steel();
    Material black = Material(Vec3(0.06f, 0.06f, 0.07f), 0.6f, 64.0f);
    n.box({0, 0.02f, 0}, {0.40f, 0.04f, 0.40f}, black);
    n.box({0, 0.32f, -0.10f}, {0.40f, 0.56f, 0.20f}, steel);
    n.box({-0.195f, 0.32f, 0.0f}, {0.012f, 0.56f, 0.38f}, black);
    n.box({0.195f, 0.32f, 0.0f}, {0.012f, 0.56f, 0.38f}, black);
    n.box({0, 0.53f, 0.0f}, {0.40f, 0.16f, 0.40f}, black);
    n.box({0, 0.575f, 0.201f}, {0.19f, 0.062f, 0.004f}, Material(Vec3(0.02f, 0.03f, 0.05f), 0.8f, 96.0f));
    for (int i = -1; i <= 1; i += 2) n.cyl({i * 0.025f, 0.43f, 0.09f}, {0.022f, 0.035f, 0.022f}, Mat::chrome());
    // drip tray with grill bars
    n.box({0, 0.05f, 0.08f}, {0.28f, 0.02f, 0.18f}, Mat::chrome());
    for (int i = 0; i < 6; ++i) n.box({-0.11f + i * 0.044f, 0.062f, 0.08f}, {0.008f, 0.004f, 0.16f}, black);
    // bean hopper (transparent) with beans inside, lid
    n.tcone({0.0f, 0.70f, -0.06f}, {0.24f, 0.18f, 0.24f}, Mat::glass(0.35f, Vec3(0.6f, 0.5f, 0.4f)), {180, 0, 0});
    n.cyl({0.0f, 0.66f, -0.06f}, {0.17f, 0.09f, 0.17f}, Material(Vec3(0.28f, 0.16f, 0.08f), 0.3f, 32.0f).tx(TEX_ROCK, 20.0f));
    n.cyl({0.0f, 0.80f, -0.06f}, {0.25f, 0.02f, 0.25f}, black);
    // water tank at the back
    n.box({0.12f, 0.33f, -0.24f}, {0.12f, 0.45f, 0.08f}, Mat::glass(0.4f, Vec3(0.6f, 0.8f, 1.0f)));
    n.box({0.0f, 0.538f, 0.205f}, {0.40f, 0.008f, 0.004f}, Mat::chrome());
}

void CoffeeMachine::drawDynamic(DrawSink& dyn) const {
    Node n(dyn, m_root);
    auto& is = InteractionSystem::get();
    for (int i = 0; i < 3; ++i) {
        Material b = Mat::plastic(BTN_COL[i]);
        if (is.hovered(m_idBase + i)) b.emission = Vec3(0.3f, 0.5f, 0.7f);
        else if (m_state == IDLE) b.emission = BTN_COL[i] * 0.25f;
        n.part(M_CYLINDER, {-0.11f + i * 0.11f, 0.497f, 0.205f - m_btnAnim[i] * 0.005f}, {0.04f, 0.012f, 0.04f}, b, {90, 0, 0});
        n.part(M_TORUS, {-0.11f + i * 0.11f, 0.497f, 0.201f}, {0.062f, 0.04f, 0.062f}, Mat::chrome(), {90, 0, 0});
    }
    n.box({0, 0.575f, 0.2045f}, {0.18f, 0.055f, 0.002f}, Mat::glow(Vec3(0.05f, 0.12f, 0.20f), 1.0f));
    n.sph({0.17f, 0.59f, 0.202f}, Vec3(0.012f), Mat::glow(m_state == IDLE ? Vec3(0.2f, 1.0f, 0.3f) : Vec3(1.0f, 0.6f, 0.1f), 1.5f));
    if (m_state == IDLE) return;
    // cup (drops in, then fills)
    float cupY = 0.064f;
    if (m_state == CUP_DROP) cupY = 0.40f - (0.40f - 0.064f) * std::min(1.0f, (m_t / 0.5f) * (m_t / 0.5f));
    Node cup = n.at({0, cupY, 0.09f});
    Material ceramic = Material(Vec3(0.96f, 0.96f, 0.94f), 0.6f, 80.0f);
    cup.tcone({0, 0.045f, 0}, {0.085f, 0.09f, 0.085f}, ceramic, {180, 0, 0});
    cup.part(M_TORUS, {0.05f, 0.05f, 0}, {0.06f, 0.06f, 0.06f}, ceramic, {90, 0, 0});
    float fill = m_state == BREWING ? m_t / 4.5f : (m_state == READY ? 1.0f : 0.0f);
    if (fill > 0.01f) {
        // only the liquid surface is visible (the cup is opaque): a disc whose radius
        // follows the cup's taper at the current height
        float hgt = 0.006f + 0.075f * fill;
        float rad = (0.0298f + (0.0425f - 0.0298f) * (hgt / 0.09f)) * 2.0f - 0.006f;
        bool foam = m_drink == 1 && fill > 0.75f;
        cup.cyl({0, hgt, 0}, {rad, 0.003f, rad}, Material(foam ? Vec3(0.93f, 0.88f, 0.78f) : DRINK_COL[m_drink], 0.7f, 96.0f));
    }
    if (m_state == BREWING) {
        float top = 0.41f, bottom = cupY + 0.006f + 0.075f * fill;
        n.cyl({0, (top + bottom) * 0.5f, 0.09f}, {0.006f, top - bottom, 0.006f}, Material(DRINK_COL[m_drink], 0.8f, 96.0f));
    }
    for (const auto& p : m_steam)
        n.sph(p.p + Vec3(0, cupY - 0.064f, 0), Vec3(0.02f + (1.6f - p.life) * 0.025f),
              Material(Vec3(0.95f), 0.0f, 1.0f).a(0.25f * std::min(1.0f, p.life)).fl(MF_UNLIT));
}

void CoffeeMachine::drawScreen(TextRenderer& t) const {
    Canvas c;
    c.model = translate(m_root, Vec3(0, 0.575f, 0.2068f));
    c.w = 0.18f;
    c.h = 0.055f;
    std::string l1, l2;
    char buf[32];
    switch (m_state) {
        case IDLE: l1 = "SELECT DRINK"; l2 = "COMPLIMENTARY"; break;
        case CUP_DROP: l1 = "PLACING CUP"; l2 = DRINKS[m_drink]; break;
        case GRINDING: l1 = "GRINDING..."; l2 = DRINKS[m_drink]; break;
        case BREWING:
            std::snprintf(buf, sizeof(buf), "BREWING %d%%", (int)(m_t / 4.5f * 100));
            l1 = buf; l2 = DRINKS[m_drink];
            break;
        case READY: l1 = "ENJOY!"; l2 = "TAKE YOUR CUP"; break;
    }
    t.ctext(c, 0.09f, 0.029f, 0.019f, l1, Vec4(0.6f, 0.9f, 1.0f, 1), 1);
    t.ctext(c, 0.09f, 0.006f, 0.015f, l2, Vec4(0.9f, 0.8f, 0.5f, 1), 1);
    // button captions
    const char* cap[3] = {"ESP", "CAP", "TEA"};
    for (int i = 0; i < 3; ++i) {
        Canvas b;
        b.model = translate(m_root, Vec3(-0.11f + i * 0.11f, 0.466f, 0.202f));
        b.w = 0.08f; b.h = 0.012f;
        t.ctext(b, 0.04f, 0.0f, 0.012f, cap[i], Vec4(0.9f, 0.9f, 0.9f, 1), 1);
    }
}

// =============================================================================
//  TELEVISION
// =============================================================================
namespace {
const char* CH_NAMES[Television::CHANNELS] = {"Off", "News", "Nature", "Sports", "Weather"};
// filled circle drawn as horizontal strips (the canvas only has rectangles)
void circle(TextRenderer& t, const Canvas& c, float cx, float cy, float r, const Vec4& col, int strips = 12) {
    for (int i = 0; i < strips; ++i) {
        float y0 = -r + 2 * r * i / strips, y1 = -r + 2 * r * (i + 1) / strips;
        float ym = (y0 + y1) * 0.5f;
        float hw = std::sqrt(std::max(0.0f, r * r - ym * ym));
        t.crect(c, cx - hw, cy + y0, 2 * hw, y1 - y0, col);
    }
}
// triangle (mountain) drawn as strips
void mountain(TextRenderer& t, const Canvas& c, float cx, float base, float w, float h, const Vec4& col) {
    const int strips = 10;
    for (int i = 0; i < strips; ++i) {
        float f = (float)i / strips;
        float hw = w * 0.5f * (1.0f - f);
        t.crect(c, cx - hw, base + h * f, 2 * hw, h / strips + 0.001f, col);
    }
}
}  // namespace

void Television::init(const Vec3& pos, float yawDeg) {
    m_pos = pos;
    m_yaw = yawDeg;
    m_root = rotateYXZ(translate(Mat4(), pos), {0, yawDeg, 0});
    m_idBase = InteractionSystem::reserveIds(2);
}

void Television::update(float dt) {
    m_time += dt;
    if (m_switchFx > 0) m_switchFx -= dt;
}

void Television::registerInteractions() {
    InteractionSystem::get().add(m_idBase, translate(m_root, {0, 0, 0.03f}), Vec3(0.9f, 0.52f, 0.04f),
                                 std::string("Change TV channel (now: ") + CH_NAMES[channel] + ")", [this]() {
                                     channel = (channel + 1) % CHANNELS;
                                     m_switchFx = 0.35f;
                                     notify(std::string("TV: ") + CH_NAMES[channel]);
                                 });
}

void Television::drawStatic(DrawSink& s) const {
    Node n(s, m_root);
    n.box({0, 0, -0.06f}, {0.45f, 0.32f, 0.06f}, Mat::darkMetal());     // wall bracket
    n.box({0, 0, -0.015f}, {1.86f, 1.08f, 0.045f}, Mat::black());
    n.box({0, -0.548f, 0.0f}, {1.86f, 0.016f, 0.05f}, Mat::darkMetal());
}

void Television::drawDynamic(DrawSink& dyn) const {
    Node n(dyn, m_root);
    bool hov = InteractionSystem::get().hovered(m_idBase);
    Material scr = channel == 0 ? Material(Vec3(0.02f, 0.02f, 0.025f), 1.0f, 200.0f) : Mat::glow(Vec3(0.05f), 1.0f);
    if (hov) scr.emission = scr.emission + Vec3(0.05f, 0.08f, 0.12f);
    n.box({0, 0, 0.0085f}, {1.78f, 1.0f, 0.002f}, scr);
    n.sph({0.86f, -0.535f, 0.02f}, Vec3(0.012f), Mat::glow(channel ? Vec3(0.2f, 1.0f, 0.3f) : Vec3(1.0f, 0.1f, 0.1f), 1.5f));
}

void Television::addFrameLight() const {
    if (channel == 0) return;
    const Vec3 cols[CHANNELS] = {Vec3(0), Vec3(0.45f, 0.55f, 1.0f), Vec3(0.5f, 0.9f, 0.6f), Vec3(0.4f, 1.0f, 0.4f), Vec3(0.4f, 0.7f, 1.0f)};
    LightDef l;
    l.pos = m_root.transformPoint(Vec3(0, -0.1f, 0.7f));
    l.color = cols[channel];
    l.intensity = 0.9f + 0.25f * std::sin(m_time * 7.0f) * std::sin(m_time * 2.3f);
    l.range = 4.5f;
    l.group = LG_MACHINES;
    LightManager::get().addFrame(l);
}

void Television::drawScreen(TextRenderer& t, float hour, const std::string& clock) const {
    if (channel == 0) return;
    Canvas c;
    c.model = translate(m_root, Vec3(0, 0, 0.011f));
    c.w = 1.76f;
    c.h = 0.98f;
    const float W = c.w, H = c.h, T = m_time;
    const Vec4 white(1, 1, 1, 1);
    if (m_switchFx > 0) {  // static noise when switching channel
        for (int y = 0; y < 14; ++y)
            for (int x = 0; x < 24; ++x) {
                float v = hash2(x, y, (uint32_t)(T * 60)) * 0.8f;
                t.crect(c, x * W / 24, y * H / 14, W / 24, H / 14, Vec4(v, v, v, 1));
            }
        return;
    }
    if (channel == 1) {  // ---------------- NEWS ----------------
        t.crect(c, 0, 0, W, H, Vec4(0.05f, 0.09f, 0.22f, 1));
        t.crect(c, 0, H * 0.35f, W, H * 0.65f, Vec4(0.08f, 0.14f, 0.32f, 1));
        // video wall behind the presenter: jungle picture
        t.crect(c, W * 0.55f, H * 0.42f, W * 0.38f, H * 0.42f, Vec4(0.35f, 0.62f, 0.85f, 1));
        t.crect(c, W * 0.55f, H * 0.42f, W * 0.38f, H * 0.12f, Vec4(0.15f, 0.45f, 0.18f, 1));
        for (int i = 0; i < 4; ++i) {
            float px = W * (0.6f + i * 0.09f);
            t.crect(c, px, H * 0.50f, 0.012f, H * 0.18f, Vec4(0.35f, 0.25f, 0.15f, 1));
            circle(t, c, px + 0.006f, H * 0.70f, 0.05f, Vec4(0.12f, 0.40f, 0.14f, 1), 8);
        }
        t.ctext(c, W * 0.74f, H * 0.86f, 0.035f, "JUNGLE REST STOP", white, 1);
        // presenter: shoulders, neck, head, hair
        float px = W * 0.28f;
        t.crect(c, px - 0.17f, H * 0.22f, 0.34f, 0.24f, Vec4(0.15f, 0.15f, 0.2f, 1));
        t.crect(c, px - 0.03f, H * 0.40f, 0.06f, 0.08f, Vec4(0.95f, 0.95f, 0.95f, 1));
        t.crect(c, px - 0.025f, H * 0.46f, 0.05f, 0.05f, Vec4(0.78f, 0.58f, 0.44f, 1));
        circle(t, c, px, H * 0.58f, 0.075f, Vec4(0.80f, 0.60f, 0.46f, 1));
        circle(t, c, px, H * 0.63f, 0.07f, Vec4(0.12f, 0.08f, 0.06f, 1), 6);
        float mouth = (std::sin(T * 12.0f) > 0) ? 0.012f : 0.004f;
        t.crect(c, px - 0.02f, H * 0.545f, 0.04f, mouth, Vec4(0.45f, 0.15f, 0.15f, 1));
        // desk
        t.crect(c, 0.0f, H * 0.18f, W * 0.55f, H * 0.12f, Vec4(0.55f, 0.35f, 0.18f, 1));
        // lower third + ticker
        t.crect(c, 0, H * 0.10f, W * 0.26f, 0.07f, Vec4(0.80f, 0.08f, 0.08f, 1));
        t.ctext(c, 0.02f, H * 0.10f + 0.015f, 0.045f, "BREAKING", white);
        t.crect(c, W * 0.26f, H * 0.10f, W * 0.74f, 0.07f, Vec4(0.95f, 0.95f, 0.95f, 1));
        t.ctext(c, W * 0.28f, H * 0.10f + 0.017f, 0.04f, "New rest stop opens deep in the jungle", Vec4(0.1f, 0.1f, 0.15f, 1));
        t.crect(c, 0, 0, W, H * 0.09f, Vec4(0.02f, 0.03f, 0.08f, 1));
        std::string ticker = "  WEATHER: warm & humid, 31 C  ***  TRAFFIC: jungle road clear  ***  ATM, coffee & snacks open 24/7  ***  Drive safe!  ";
        std::string vis = ticker + ticker;
        size_t startCh = (size_t)(T * 7.0f) % ticker.size();
        t.ctext(c, 0.02f, 0.02f, 0.035f, vis.substr(startCh, 70), Vec4(1.0f, 0.85f, 0.3f, 1));
        t.ctext(c, 0.03f, H - 0.07f, 0.045f, "KTV NEWS 24", white);
        t.ctext(c, W - 0.03f, H - 0.07f, 0.045f, clock, white, 2);
    } else if (channel == 2) {  // ---------------- NATURE ----------------
        const Vec4 bands[4] = {Vec4(0.35f, 0.60f, 0.92f, 1), Vec4(0.45f, 0.68f, 0.93f, 1), Vec4(0.60f, 0.78f, 0.95f, 1), Vec4(0.78f, 0.86f, 0.92f, 1)};
        for (int i = 0; i < 4; ++i) t.crect(c, 0, H * (0.85f - i * 0.15f), W, H * 0.15f + 0.002f, bands[i]);
        t.crect(c, 0, H * 0.0f, W, H * 0.40f, Vec4(0.78f, 0.86f, 0.92f, 1));
        circle(t, c, W * 0.78f, H * 0.78f, 0.08f, Vec4(1.0f, 0.92f, 0.55f, 1));
        mountain(t, c, W * 0.25f, H * 0.35f, 0.9f, 0.42f, Vec4(0.38f, 0.45f, 0.55f, 1));
        mountain(t, c, W * 0.65f, H * 0.35f, 1.1f, 0.34f, Vec4(0.32f, 0.40f, 0.50f, 1));
        t.crect(c, 0, 0, W, H * 0.36f, Vec4(0.22f, 0.50f, 0.22f, 1));
        t.crect(c, 0, H * 0.10f, W, H * 0.06f, Vec4(0.30f, 0.55f, 0.80f, 1));   // river
        for (int i = 0; i < 9; ++i) {
            float x = std::fmod(i * 0.23f + 0.05f, W);
            t.crect(c, x, H * 0.18f, 0.015f, 0.1f, Vec4(0.35f, 0.24f, 0.14f, 1));
            circle(t, c, x + 0.0075f, H * 0.18f + 0.13f, 0.06f + 0.01f * (i % 3), Vec4(0.10f, 0.38f + 0.03f * (i % 3), 0.12f, 1), 8);
        }
        for (int i = 0; i < 3; ++i) {
            float x = std::fmod(T * (0.03f + i * 0.01f) + i * 0.6f, W + 0.4f) - 0.2f;
            circle(t, c, x, H * (0.80f - i * 0.07f), 0.05f, Vec4(1, 1, 1, 0.95f), 6);
            circle(t, c, x + 0.06f, H * (0.80f - i * 0.07f), 0.04f, Vec4(1, 1, 1, 0.95f), 6);
        }
        float bx = std::fmod(T * 0.12f, W + 0.2f) - 0.1f, by = H * 0.62f + std::sin(T * 2.0f) * 0.02f;
        float flap = std::sin(T * 10.0f) * 0.012f;
        t.crect(c, bx - 0.03f, by + flap, 0.03f, 0.006f, Vec4(0.1f, 0.1f, 0.1f, 1));
        t.crect(c, bx, by - flap, 0.03f, 0.006f, Vec4(0.1f, 0.1f, 0.1f, 1));
        t.crect(c, 0, H - 0.09f, W, 0.09f, Vec4(0, 0, 0, 0.35f));
        t.ctext(c, 0.04f, H - 0.07f, 0.05f, "WILD BANGLADESH  -  Sundarbans", white);
    } else if (channel == 3) {  // ---------------- SPORTS ----------------
        for (int i = 0; i < 8; ++i) t.crect(c, i * W / 8, 0, W / 8 + 0.001f, H, (i % 2) ? Vec4(0.20f, 0.55f, 0.22f, 1) : Vec4(0.24f, 0.60f, 0.25f, 1));
        const Vec4 line(0.95f, 0.95f, 0.95f, 1);
        t.crect(c, 0.06f, 0.06f, W - 0.12f, 0.008f, line);
        t.crect(c, 0.06f, H - 0.068f, W - 0.12f, 0.008f, line);
        t.crect(c, 0.06f, 0.06f, 0.008f, H - 0.12f, line);
        t.crect(c, W - 0.068f, 0.06f, 0.008f, H - 0.12f, line);
        t.crect(c, W * 0.5f - 0.004f, 0.06f, 0.008f, H - 0.12f, line);
        circle(t, c, W * 0.5f, H * 0.5f, 0.13f, line, 16);
        circle(t, c, W * 0.5f, H * 0.5f, 0.122f, Vec4(0.22f, 0.58f, 0.24f, 1), 16);
        t.crect(c, 0.06f, H * 0.35f, 0.15f, H * 0.30f, Vec4(1, 1, 1, 0.15f));
        t.crect(c, W - 0.21f, H * 0.35f, 0.15f, H * 0.30f, Vec4(1, 1, 1, 0.15f));
        Vec2 ball(W * 0.5f + std::sin(T * 0.7f) * 0.55f, H * 0.5f + std::sin(T * 1.3f) * 0.3f);
        for (int i = 0; i < 12; ++i) {
            bool red = i < 6;
            float bx = (red ? W * 0.3f : W * 0.7f) + std::sin(T * (0.6f + i * 0.07f) + i) * 0.3f;
            float by = H * (0.15f + 0.14f * (i % 6)) + std::cos(T * (0.5f + i * 0.05f) + i * 2.0f) * 0.05f;
            bx = lerpf(bx, ball.x, 0.25f);
            by = lerpf(by, ball.y, 0.15f);
            circle(t, c, bx, by, 0.022f, red ? Vec4(0.85f, 0.12f, 0.12f, 1) : Vec4(0.15f, 0.3f, 0.85f, 1), 6);
        }
        circle(t, c, ball.x, ball.y, 0.012f, Vec4(1, 1, 1, 1), 6);
        int minute = 52 + (int)(T / 5.0f) % 38;
        char sb[64];
        std::snprintf(sb, sizeof(sb), "KUET 2 - 1 RUET   %02d:%02d", minute, (int)(T * 4) % 60);
        t.crect(c, 0.08f, H - 0.15f, 0.82f, 0.075f, Vec4(0.05f, 0.05f, 0.08f, 0.85f));
        t.ctext(c, 0.1f, H - 0.135f, 0.045f, sb, white);
        t.ctext(c, W - 0.1f, 0.1f, 0.04f, "LIVE", Vec4(1, 0.3f, 0.3f, 1), 2);
    } else {  // ---------------- WEATHER ----------------
        t.crect(c, 0, 0, W, H, Vec4(0.12f, 0.35f, 0.62f, 1));
        circle(t, c, W * 0.42f, H * 0.48f, 0.28f, Vec4(0.30f, 0.62f, 0.30f, 1), 16);
        circle(t, c, W * 0.55f, H * 0.62f, 0.2f, Vec4(0.30f, 0.62f, 0.30f, 1), 12);
        circle(t, c, W * 0.30f, H * 0.30f, 0.16f, Vec4(0.28f, 0.58f, 0.28f, 1), 12);
        bool night = hour < 6.0f || hour > 18.5f;
        if (night) {
            circle(t, c, W * 0.82f, H * 0.70f, 0.09f, Vec4(0.95f, 0.95f, 0.85f, 1));
            circle(t, c, W * 0.85f, H * 0.73f, 0.08f, Vec4(0.12f, 0.35f, 0.62f, 1));
        } else {
            circle(t, c, W * 0.82f, H * 0.70f, 0.09f, Vec4(1.0f, 0.85f, 0.25f, 1));
        }
        t.ctext(c, W * 0.44f, H * 0.50f, 0.04f, "Khulna  31 C", white, 1);
        t.ctext(c, W * 0.30f, H * 0.28f, 0.035f, "Sundarbans 29 C", white, 1);
        t.ctext(c, W * 0.58f, H * 0.66f, 0.035f, "Jessore 30 C", white, 1);
        t.crect(c, 0, H - 0.1f, W, 0.1f, Vec4(0.03f, 0.1f, 0.25f, 1));
        t.ctext(c, 0.04f, H - 0.075f, 0.05f, "WEATHER", white);
        t.ctext(c, W - 0.04f, H - 0.075f, 0.05f, clock, Vec4(1, 0.9f, 0.4f, 1), 2);
        t.ctext(c, W * 0.78f, H * 0.52f, 0.04f, night ? "Clear night" : "Sunny", white, 1);
        t.ctext(c, W * 0.78f, H * 0.45f, 0.032f, "Humidity 78%", white, 1);
    }
}

// =============================================================================
//  AQUARIUM
// =============================================================================
void Aquarium::init(const Vec3& pos, float yawDeg) {
    m_pos = pos;
    m_yaw = yawDeg;
    m_root = rotateYXZ(translate(Mat4(), pos), {0, yawDeg, 0});
    m_idBase = InteractionSystem::reserveIds(2);
    Rng r(31);
    const Vec3 cols[] = {Vec3(1.0f, 0.45f, 0.05f), Vec3(0.95f, 0.85f, 0.15f), Vec3(0.2f, 0.45f, 0.95f), Vec3(0.95f, 0.2f, 0.3f),
                         Vec3(0.9f, 0.9f, 0.95f), Vec3(0.3f, 0.85f, 0.75f), Vec3(1.0f, 0.55f, 0.2f)};
    for (int i = 0; i < 7; ++i) {
        Fish f;
        f.phase = r.range(0, 6.28f);
        f.speed = r.range(0.35f, 0.7f) * (r.chance(0.5f) ? 1.0f : -1.0f);
        f.radiusX = r.range(0.35f, 0.72f);
        f.radiusZ = r.range(0.06f, 0.15f);
        f.y = r.range(0.88f, 1.22f);
        f.size = r.range(0.05f, 0.085f);
        f.color = cols[i];
        f.fin = cols[(i + 3) % 7] * 0.8f + Vec3(0.2f);
        f.x = 0; f.z = 0; f.yaw = 0; f.cy = f.y;
        m_fish.push_back(f);
    }
    for (int i = 0; i < 14; ++i) m_bubbles.push_back({Vec3(0.68f + r.range(-0.02f, 0.02f), r.range(0.82f, 1.3f), -0.12f), r.range(0.12f, 0.22f)});
}

Vec3 Aquarium::viewPos() const {
    Vec3 p = m_root.transformPoint(Vec3(0, 0, 1.4f));
    p.y = 0;
    return p;
}

void Aquarium::update(float dt) {
    m_time += dt;
    for (auto& f : m_fish) {
        f.phase += f.speed * dt;
        float nx = std::cos(f.phase) * f.radiusX, nz = std::sin(f.phase) * f.radiusZ;
        float dx = nx - f.x, dz = nz - f.z;
        if (std::fabs(dx) + std::fabs(dz) > 1e-5f) f.yaw = toDegrees(std::atan2(dx, dz));
        f.x = nx;
        f.z = nz;
        float targetY = f.y + std::sin(m_time * 0.7f + f.phase) * 0.03f;
        if (!m_food.empty()) targetY = 1.26f;   // swim up to eat
        f.cy = approach(f.cy, targetY, dt * 0.25f);
    }
    for (auto& b : m_bubbles) {
        b.p.y += b.speed * dt;
        b.p.x += std::sin(m_time * 4.0f + b.p.y * 20.0f) * dt * 0.02f;
        if (b.p.y > 1.30f) { b.p.y = 0.82f; b.p.x = 0.68f; }
    }
    for (auto& f : m_food) { f.life -= dt; f.p.y = std::max(0.84f, f.p.y - dt * 0.04f); }
    while (!m_food.empty() && m_food.front().life <= 0) m_food.erase(m_food.begin());
}

void Aquarium::registerInteractions() {
    InteractionSystem::get().add(m_idBase, translate(m_root, {0, 1.06f, 0.0f}), Vec3(0.9f, 0.32f, 0.26f), "Feed the fish", [this]() {
        Rng r((uint32_t)(m_time * 1000));
        for (int i = 0; i < 12; ++i) m_food.push_back({Vec3(r.range(-0.6f, 0.6f), 1.30f, r.range(-0.15f, 0.15f)), r.range(5.0f, 8.0f)});
        notify("You sprinkle some fish food");
    });
}

void Aquarium::registerLights() const {
    LightDef l;
    l.pos = m_root.transformPoint(Vec3(0, 1.35f, 0.5f));
    l.color = Vec3(0.55f, 0.85f, 1.0f);
    l.intensity = 1.1f;
    l.range = 3.2f;
    l.group = LG_MACHINES;
    LightManager::get().addStatic(l);
}

void Aquarium::drawStatic(DrawSink& s) const {
    Node n(s, m_root);
    Material wood = Mat::wood(Vec3(0.30f, 0.19f, 0.10f), 1.5f);
    n.box({0, 0.375f, 0}, {1.8f, 0.75f, 0.5f}, wood);
    for (int i = 0; i < 3; ++i) {
        n.box({-0.6f + i * 0.6f, 0.37f, 0.252f}, {0.56f, 0.64f, 0.01f}, Mat::wood(Vec3(0.36f, 0.23f, 0.13f), 1.5f));
        n.box({-0.6f + i * 0.6f + 0.22f, 0.45f, 0.262f}, {0.02f, 0.12f, 0.02f}, Mat::chrome());
    }
    n.box({0, 0.79f, 0}, {1.76f, 0.06f, 0.46f}, Material(Vec3(0.90f, 0.80f, 0.60f), 0.05f, 8.0f).tx(TEX_SAND, 4.0f));
    Rng r(5);
    for (int i = 0; i < 6; ++i)
        n.part(M_SPHERE_LOW, {r.range(-0.75f, 0.75f), 0.83f, r.range(-0.15f, 0.12f)}, {r.range(0.08f, 0.18f), r.range(0.06f, 0.12f), r.range(0.08f, 0.14f)},
               Material(Vec3(0.45f, 0.42f, 0.40f), 0.2f, 16.0f).tx(TEX_ROCK, 4.0f), {0, r.range(0.0f, 180.0f), 0});
    n.part(M_CYLINDER_LOW, {-0.3f, 0.86f, -0.08f}, {0.05f, 0.6f, 0.05f}, Mat::bark(Vec3(0.35f, 0.25f, 0.15f)), {0, 30, 75});
    n.part(M_TCONE, {0.68f, 0.83f, -0.12f}, {0.04f, 0.03f, 0.04f}, Mat::matte(Vec3(0.3f, 0.3f, 0.35f)));
    // hood with light strip
    n.box({0, 1.41f, 0}, {1.84f, 0.08f, 0.54f}, Mat::black());
    n.box({0, 1.366f, 0.1f}, {1.7f, 0.01f, 0.05f}, Mat::glow(Vec3(0.75f, 0.92f, 1.0f), 2.0f));
}

void Aquarium::drawDynamic(DrawSink& dyn) const {
    Node n(dyn, m_root);
    // swaying water plants (wind flag reused as water current)
    Material plant = Mat::leafCard(Vec3(0.25f, 0.70f, 0.30f), TEX_GRASS_BLADES);
    for (int i = 0; i < 7; ++i)
        n.part(M_CROSS, {-0.78f + i * 0.26f, 0.81f, -0.14f + 0.05f * (i % 3)}, {0.22f, 0.25f + 0.08f * (i % 3), 0.22f}, plant, {0, i * 25.0f, 0});
    // fish: body + tail (wiggles) + dorsal fin + eyes
    for (const auto& f : m_fish) {
        float s = f.size;
        Node fn = n.at({f.x, f.cy, f.z}, {0, f.yaw, 0});
        Material body = Material(f.color, 0.7f, 64.0f);
        fn.sph({0, 0, 0}, {0.45f * s, 0.75f * s, 1.25f * s}, body);
        Node tail = fn.at({0, 0, -0.55f * s}).rotY(std::sin(m_time * 9.0f + f.phase * 3.0f) * 25.0f);
        tail.part(M_CONE, {0, 0, -0.25f * s}, {0.08f * s, 0.5f * s, 0.7f * s}, Material(f.fin, 0.4f, 32.0f), {90, 0, 0});
        fn.part(M_WEDGE, {0, 0.42f * s, -0.1f * s}, {0.05f * s, 0.3f * s, 0.5f * s}, Material(f.fin, 0.4f, 32.0f), {0, 180, 0});
        for (int e = -1; e <= 1; e += 2) fn.sph({e * 0.17f * s, 0.12f * s, 0.42f * s}, Vec3(0.13f * s), Mat::black());
    }
    for (const auto& b : m_bubbles) n.sph(b.p, Vec3(0.018f), Material(Vec3(0.9f, 0.95f, 1.0f), 1.0f, 128.0f).a(0.45f));
    for (const auto& f : m_food) n.sph(f.p, Vec3(0.008f), Mat::matte(Vec3(0.6f, 0.35f, 0.15f)));
    // water volume and glass (transparent, drawn last by the renderer)
    n.box({0, 1.03f, 0}, {1.76f, 0.5f, 0.46f}, Material(Vec3(0.35f, 0.65f, 0.80f), 0.9f, 96.0f).a(0.16f).fl(MF_NO_SHADOW));
    n.box({0, 1.06f, 0}, {1.8f, 0.62f, 0.5f}, Mat::glass(0.12f));
}
