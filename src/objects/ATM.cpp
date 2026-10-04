#include "ATM.h"
#include "../game/Interaction.h"
#include "../game/Inventory.h"
#include "../game/Messages.h"
#include "../scene/Lights.h"
#include "../scene/Materials.h"
#include <cmath>
#include <cstdio>

namespace {
const char* KEYS[16] = {"1", "2", "3", "CANCEL", "4", "5", "6", "CLEAR", "7", "8", "9", "ENTER", "", "0", "", ""};
const float SCREEN_Y = 1.46f, SCREEN_Z = 0.19f;
const float SCREEN_W = 0.40f, SCREEN_H = 0.30f;
const float SIDE_Y[4] = {1.53f, 1.47f, 1.41f, 1.35f};

std::string money(int v) {
    char b[48];
    if (v >= 1000) std::snprintf(b, sizeof(b), "Tk %d,%03d", v / 1000, v % 1000);
    else std::snprintf(b, sizeof(b), "Tk %d", v);
    return b;
}
Mat4 keypadFrame(const Mat4& root) { return rotate(translate(root, Vec3(0.0f, 1.035f, 0.40f)), toRadians(12.0f), Vec3(1, 0, 0)); }
Vec3 keyPos(int i) { return Vec3(-0.135f + (i % 4) * 0.075f + (i % 4 == 3 ? 0.02f : 0.0f), 0.03f, -0.085f + (i / 4) * 0.052f); }
const int AMOUNTS[4] = {500, 1000, 2000, 5000};
}  // namespace

void ATM::init(const Vec3& pos, float yawDeg) {
    m_pos = pos;
    m_yaw = yawDeg;
    m_root = rotateYXZ(translate(Mat4(), pos), {0, yawDeg, 0});
    m_idBase = InteractionSystem::reserveIds(32);
}

Vec3 ATM::standPos() const { return m_root.transformPoint(Vec3(0.0f, 0, 0.95f)); }

const char* ATM::stepName(int i) {
    static const char* n[] = {"", "Insert your card", "Enter 4-digit PIN", "Choose a service", "Choose the amount",
                              "Processing", "Take your card", "Take your cash", "Take your receipt"};
    return (i >= 1 && i <= 8) ? n[i] : "";
}

int ATM::currentStep() const {
    switch (m_state) {
        case WELCOME: case CARD_IN: return 1;
        case PIN: case VERIFY: case PIN_WRONG: return 2;
        case MENU: case BALANCE: return 3;
        case AMOUNT: case OTHER_AMOUNT: case NO_FUNDS: return 4;
        case PROCESSING: return 5;
        case TAKE_CARD: case CANCELLED: return 6;
        case TAKE_CASH: return 7;
        case RECEIPT: return 8;
        case THANKS: return 9;
    }
    return 0;
}

void ATM::go(State s) {
    m_state = s;
    m_t = 0.0f;
    if (s == WELCOME) {
        m_pin.clear();
        m_amountText.clear();
        m_amount = 0;
        m_tries = 0;
        m_npc = false;
        m_cashOut = m_cardOut = m_receiptOut = false;
    }
    if (s == TAKE_CARD) m_cardOut = true;
    if (s == TAKE_CASH) m_cashOut = true;
    if (s == RECEIPT) m_receiptOut = true;
}

// -----------------------------------------------------------------------------
// Inputs
// -----------------------------------------------------------------------------
void ATM::cardSlot() {
    if (m_state == WELCOME) {
        go(CARD_IN);
        if (!m_npc) notify("Card inserted");
    } else if (m_state == TAKE_CARD) {
        m_cardOut = false;
        if (!m_npc) notify("Card taken");
        if (m_amount > 0) go(TAKE_CASH);
        else go(THANKS);
    }
}

void ATM::cashSlot() {
    if (m_state != TAKE_CASH) return;
    m_cashOut = false;
    if (!m_npc) {
        Inventory& inv = Inventory::player();
        inv.cash += m_amount;
        inv.bank -= m_amount;
        notify("You took " + money(m_amount) + " cash");
    }
    if (m_wantReceipt) go(RECEIPT);
    else go(THANKS);
}

void ATM::receiptSlot() {
    if (m_state != RECEIPT || m_t < 1.2f) return;
    m_receiptOut = false;
    if (!m_npc) notify("Receipt taken");
    go(THANKS);
}

void ATM::sideKey(int i) {
    m_sideAnim[i] = 1.0f;
    switch (m_state) {
        case MENU:
            if (i == 0) go(AMOUNT);                       // withdraw
            else if (i == 1) go(BALANCE);                 // balance
            else if (i == 4) { m_amount = 1000; go(PROCESSING); }   // fast cash
            else if (i == 7) go(CANCELLED);
            break;
        case AMOUNT:
            if (i < 4) { m_amount = AMOUNTS[i]; go(PROCESSING); }
            else if (i == 4) { m_amount = 10000; go(PROCESSING); }
            else if (i == 6) { m_amountText.clear(); go(OTHER_AMOUNT); }
            else if (i == 7) go(CANCELLED);
            break;
        case BALANCE:
            if (i == 3) go(MENU);
            else if (i == 7) { m_amount = 0; go(TAKE_CARD); }
            break;
        case NO_FUNDS:
            if (i == 7) { m_amount = 0; go(TAKE_CARD); }
            else go(AMOUNT);
            break;
        default: break;
    }
}

void ATM::keypad(const std::string& k) {
    for (int i = 0; i < 16; ++i)
        if (k == KEYS[i] && !k.empty()) m_keyAnim[i] = 1.0f;
    if (k == "CANCEL") {
        if (m_state == PIN || m_state == MENU || m_state == AMOUNT || m_state == OTHER_AMOUNT || m_state == BALANCE) go(CANCELLED);
        return;
    }
    if (m_state == PIN) {
        if (k == "CLEAR") m_pin.clear();
        else if (k == "ENTER") { if (m_pin.size() == 4) go(VERIFY); }
        else if (k.size() == 1 && m_pin.size() < 4) m_pin += k;
    } else if (m_state == OTHER_AMOUNT) {
        if (k == "CLEAR") m_amountText.clear();
        else if (k == "ENTER") {
            int v = m_amountText.empty() ? 0 : std::atoi(m_amountText.c_str());
            if (v >= 100 && v % 100 == 0 && v <= 20000) { m_amount = v; go(PROCESSING); }
            else { m_amountText.clear(); if (!m_npc) notify("Amount must be a multiple of 100 (max 20,000)"); }
        } else if (k.size() == 1 && m_amountText.size() < 5) m_amountText += k;
    }
}

bool ATM::npcStart() {
    if (!available()) return false;
    m_npc = true;
    m_npcStep = 0;
    m_npcTimer = 0.0f;
    cardSlot();
    return true;
}

// The NPC "presses" the same buttons a player would, with small pauses
void ATM::npcThink(float dt) {
    m_npcTimer += dt;
    switch (m_state) {
        case PIN:
            if (m_npcTimer > 0.45f) {
                m_npcTimer = 0.0f;
                if (m_pin.size() < 4) keypad(std::string(1, "1234"[m_pin.size()]));
                else keypad("ENTER");
            }
            break;
        case MENU: if (m_npcTimer > 1.4f) { m_npcTimer = 0; sideKey(0); } break;
        case AMOUNT: if (m_npcTimer > 1.2f) { m_npcTimer = 0; sideKey((int)(m_time * 10.0f) % 3); } break;
        case TAKE_CARD: if (m_t > 1.2f) cardSlot(); break;
        case TAKE_CASH: if (m_t > 1.0f) cashSlot(); break;
        case RECEIPT: if (m_t > 1.8f) receiptSlot(); break;
        default: m_npcTimer = 0.0f; break;
    }
}

// -----------------------------------------------------------------------------
void ATM::update(float dt) {
    m_t += dt;
    m_time += dt;
    for (float& k : m_keyAnim) k = std::max(0.0f, k - dt * 5.0f);
    for (float& k : m_sideAnim) k = std::max(0.0f, k - dt * 5.0f);

    switch (m_state) {
        case CARD_IN: if (m_t > 1.4f) go(PIN); break;
        case VERIFY:
            if (m_t > 1.3f) {
                if (m_pin == "1234") go(MENU);
                else if (++m_tries >= 3) { notify("Too many wrong PINs"); go(CANCELLED); }
                else go(PIN_WRONG);
            }
            break;
        case PIN_WRONG: if (m_t > 2.0f) { m_pin.clear(); go(PIN); } break;
        case PROCESSING:
            if (m_t > 2.4f) {
                int bank = m_npc ? 100000 : Inventory::player().bank;
                if (m_amount > bank) go(NO_FUNDS);
                else go(TAKE_CARD);
            }
            break;
        case CANCELLED: if (m_t > 1.8f) { m_amount = 0; go(TAKE_CARD); } break;
        case THANKS: if (m_t > 2.5f) go(WELCOME); break;
        default: break;
    }
    if (m_npc) npcThink(dt);

    // slot animations: card slides in/out, cash & receipt come out
    float cardTarget = (m_state == CARD_IN) ? (m_t < 0.3f ? 1.0f : 0.0f) : (m_cardOut ? 1.0f : 0.0f);
    if (m_state == CARD_IN && m_t < 0.05f) m_cardAnim = 1.0f;
    m_cardAnim = approach(m_cardAnim, cardTarget, dt * 1.3f);
    m_cashAnim = approach(m_cashAnim, m_cashOut ? 1.0f : 0.0f, dt * 1.2f);
    m_receiptAnim = approach(m_receiptAnim, m_receiptOut ? std::min(1.0f, m_t / 1.2f) : 0.0f, dt * 2.0f);
}

// -----------------------------------------------------------------------------
void ATM::registerInteractions() {
    auto& is = InteractionSystem::get();
    if (m_npc) {
        is.add(m_idBase + 31, translate(m_root, {0, SCREEN_Y, SCREEN_Z}), Vec3(0.3f, 0.25f, 0.05f), "ATM in use - please wait", [] {});
        return;
    }
    // side keys
    for (int i = 0; i < 8; ++i) {
        float x = i < 4 ? -0.245f : 0.245f;
        is.add(m_idBase + i, translate(m_root, {x, SIDE_Y[i % 4], SCREEN_Z + 0.01f}), Vec3(0.022f, 0.018f, 0.02f),
               std::string("Side key ") + (i < 4 ? "L" : "R") + std::to_string(i % 4 + 1), [this, i]() { sideKey(i); });
    }
    // keypad
    Mat4 kf = keypadFrame(m_root);
    for (int i = 0; i < 16; ++i) {
        std::string k = KEYS[i];
        if (k.empty()) continue;
        is.add(m_idBase + 8 + i, translate(kf, keyPos(i)), Vec3(0.032f, 0.015f, 0.022f), "Keypad [" + k + "]",
               [this, k]() { keypad(k); });
    }
    is.add(m_idBase + 24, translate(m_root, {0.34f, 1.30f, 0.20f}), Vec3(0.06f, 0.03f, 0.04f),
           m_state == WELCOME ? "Insert bank card" : (m_state == TAKE_CARD ? "Take your card" : "Card slot"), [this]() { cardSlot(); });
    is.add(m_idBase + 25, translate(m_root, {0.0f, 0.88f, 0.36f}), Vec3(0.18f, 0.04f, 0.05f),
           m_state == TAKE_CASH ? "Take cash (" + money(m_amount) + ")" : "Cash dispenser", [this]() { cashSlot(); });
    is.add(m_idBase + 26, translate(m_root, {0.34f, 1.20f, 0.20f}), Vec3(0.05f, 0.02f, 0.04f),
           m_state == RECEIPT ? "Take receipt" : "Receipt printer", [this]() { receiptSlot(); });
}

void ATM::registerLights() const {
    LightDef l;
    l.pos = m_root.transformPoint(Vec3(0, 1.5f, 0.7f));
    l.color = Vec3(0.45f, 0.70f, 1.0f);
    l.intensity = 0.9f;
    l.range = 2.8f;
    l.group = LG_MACHINES;
    LightManager::get().addStatic(l);
}

// -----------------------------------------------------------------------------
void ATM::drawStatic(DrawSink& s) const {
    Node n(s, m_root);
    Material body = Mat::paint(Vec3(0.22f, 0.24f, 0.28f));
    Material fascia = Mat::paint(Vec3(0.75f, 0.77f, 0.80f));
    Material blue = Mat::paint(Vec3(0.08f, 0.28f, 0.62f));
    Material dark = Mat::black();
    n.box({0, 0.04f, 0}, {1.0f, 0.08f, 0.72f}, Mat::darkMetal());
    n.box({0, 0.54f, 0}, {0.96f, 0.92f, 0.66f}, body);
    n.box({0, 1.45f, -0.08f}, {0.96f, 0.80f, 0.50f}, fascia);
    n.box({0, 1.96f, 0}, {1.04f, 0.24f, 0.72f}, blue);
    n.box({0, 1.835f, 0.33f}, {1.0f, 0.02f, 0.05f}, Mat::glow(Vec3(0.75f, 0.88f, 1.0f), 1.6f));
    // privacy wings
    n.box({-0.50f, 1.45f, 0.28f}, {0.04f, 0.62f, 0.30f}, body);
    n.box({0.50f, 1.45f, 0.28f}, {0.04f, 0.62f, 0.30f}, body);
    // keypad shelf (sloped)
    Node shelf(s, keypadFrame(m_root));
    shelf.box({0, 0, 0}, {0.96f, 0.05f, 0.30f}, body);
    shelf.box({0, 0.027f, -0.01f}, {0.36f, 0.006f, 0.24f}, Material(Vec3(0.55f, 0.57f, 0.60f), 0.6f, 64.0f).tx(TEX_METAL, 3.0f));
    // screen bezel and recess
    n.box({0, SCREEN_Y, SCREEN_Z - 0.012f}, {0.54f, 0.40f, 0.02f}, dark);
    // card slot, receipt slot
    n.box({0.34f, 1.30f, 0.18f}, {0.13f, 0.06f, 0.03f}, dark);
    n.box({0.34f, 1.30f, 0.196f}, {0.10f, 0.008f, 0.004f}, Mat::glow(Vec3(0.2f, 1.0f, 0.4f), 1.2f));
    n.box({0.34f, 1.20f, 0.18f}, {0.11f, 0.04f, 0.03f}, dark);
    n.box({0.34f, 1.20f, 0.196f}, {0.08f, 0.004f, 0.004f}, Material(Vec3(0.3f)));
    // cash dispenser shutter
    n.box({0, 0.88f, 0.335f}, {0.40f, 0.10f, 0.02f}, Mat::chrome());
    n.box({0, 0.88f, 0.346f}, {0.32f, 0.03f, 0.004f}, dark);
    // camera dome, speaker
    n.part(M_HEMISPHERE, {0, 1.73f, SCREEN_Z - 0.01f}, {0.05f, 0.025f, 0.05f}, dark, {90, 0, 0});
    for (int i = 0; i < 5; ++i) n.box({-0.34f, 1.24f + i * 0.015f, 0.172f}, {0.08f, 0.005f, 0.004f}, dark);
    // kick plate + logo stripe
    n.box({0, 0.15f, 0.335f}, {0.94f, 0.1f, 0.01f}, Mat::steel());
    n.box({0, 0.60f, 0.334f}, {0.94f, 0.04f, 0.01f}, blue);
}

void ATM::drawDynamic(DrawSink& dyn) const {
    Node n(dyn, m_root);
    auto& is = InteractionSystem::get();
    // screen glow surface
    n.box({0, SCREEN_Y, SCREEN_Z - 0.001f}, {SCREEN_W + 0.02f, SCREEN_H + 0.02f, 0.002f}, Mat::glow(Vec3(0.04f, 0.10f, 0.22f), 1.0f));
    // side keys
    for (int i = 0; i < 8; ++i) {
        float x = i < 4 ? -0.245f : 0.245f;
        Material m = Mat::plastic(Vec3(0.80f, 0.82f, 0.85f));
        if (is.hovered(m_idBase + i)) m.emission = Vec3(0.2f, 0.4f, 0.6f);
        n.box({x, SIDE_Y[i % 4], SCREEN_Z + 0.004f - m_sideAnim[i] * 0.006f}, {0.04f, 0.03f, 0.016f}, m);
    }
    // keypad keys
    Node kf(dyn, keypadFrame(m_root));
    for (int i = 0; i < 16; ++i) {
        std::string k = KEYS[i];
        Vec3 col = Vec3(0.78f, 0.79f, 0.82f);
        if (k == "CANCEL") col = Vec3(0.85f, 0.15f, 0.12f);
        else if (k == "CLEAR") col = Vec3(0.95f, 0.78f, 0.15f);
        else if (k == "ENTER") col = Vec3(0.15f, 0.72f, 0.25f);
        else if (k.empty()) col = Vec3(0.35f, 0.36f, 0.38f);
        Material m = Mat::plastic(col);
        if (is.hovered(m_idBase + 8 + i)) m.emission = Vec3(0.2f, 0.4f, 0.6f);
        kf.box(keyPos(i) - Vec3(0, m_keyAnim[i] * 0.006f, 0), {0.062f, 0.018f, 0.042f}, m);
    }
    // card (slides in / out of the slot)
    if (m_cardAnim > 0.01f) {
        Material card = Material(Vec3(0.10f, 0.45f, 0.85f), 0.6f, 64.0f);
        n.box({0.34f, 1.30f, 0.17f + m_cardAnim * 0.05f}, {0.054f, 0.004f, 0.086f}, card);
        n.box({0.34f, 1.3025f, 0.19f + m_cardAnim * 0.05f}, {0.012f, 0.001f, 0.01f}, Material(Vec3(0.9f, 0.75f, 0.3f), 0.9f, 96.0f));
    }
    if (is.hovered(m_idBase + 24) || m_state == WELCOME || m_state == TAKE_CARD) {
        float blink = (std::fmod(m_time, 0.8f) < 0.4f) ? 2.0f : 0.5f;
        n.box({0.34f, 1.30f, 0.197f}, {0.12f, 0.05f, 0.002f}, Mat::glow(Vec3(0.2f, 1.0f, 0.4f), is.hovered(m_idBase + 24) ? 1.5f : blink * 0.4f));
    }
    // cash bills
    if (m_cashAnim > 0.01f) {
        for (int i = 0; i < 4; ++i)
            n.box({0, 0.88f + i * 0.004f, 0.33f + m_cashAnim * 0.05f}, {0.15f, 0.003f, 0.07f},
                  Material(i % 2 ? Vec3(0.35f, 0.65f, 0.40f) : Vec3(0.55f, 0.45f, 0.70f), 0.1f, 8.0f));
    }
    if (m_state == TAKE_CASH) {
        float blink = (std::fmod(m_time, 0.8f) < 0.4f) ? 1.5f : 0.3f;
        n.box({0, 0.835f, 0.347f}, {0.32f, 0.008f, 0.004f}, Mat::glow(Vec3(0.2f, 1.0f, 0.4f), blink));
    }
    // receipt paper
    if (m_receiptAnim > 0.01f) {
        Node r = n.at({0.34f, 1.20f, 0.19f}).rotX(25.0f * m_receiptAnim);
        r.box({0, 0, m_receiptAnim * 0.06f}, {0.06f, 0.002f, m_receiptAnim * 0.12f}, Material(Vec3(0.97f, 0.97f, 0.94f), 0.1f, 8.0f));
    }
}

// -----------------------------------------------------------------------------
// Screen content (what the customer sees in every step)
// -----------------------------------------------------------------------------
void ATM::drawScreen(TextRenderer& t) const {
    Canvas c;
    c.model = translate(m_root, Vec3(0, SCREEN_Y, SCREEN_Z + 0.001f));
    c.w = SCREEN_W;
    c.h = SCREEN_H;
    const Vec4 white(1, 1, 1, 1), yellow(1.0f, 0.88f, 0.3f, 1), cyan(0.5f, 0.9f, 1.0f, 1), dim(0.7f, 0.8f, 0.95f, 1);
    // header bar
    t.crect(c, 0, SCREEN_H - 0.04f, SCREEN_W, 0.04f, Vec4(0.08f, 0.28f, 0.62f, 1));
    t.ctext(c, 0.01f, SCREEN_H - 0.031f, 0.024f, "KUET BANK", white);
    int step = currentStep();
    if (step >= 1 && step <= 8) t.ctext(c, SCREEN_W - 0.01f, SCREEN_H - 0.031f, 0.02f, "STEP " + std::to_string(step) + "/8", yellow, 2);

    auto title = [&](const std::string& s, float y = 0.20f) { t.ctext(c, SCREEN_W * 0.5f, y, 0.026f, s, white, 1); };
    auto smallTitle = [&](const std::string& s) { t.ctext(c, SCREEN_W * 0.5f, 0.2345f, 0.02f, s, yellow, 1); };
    auto line = [&](const std::string& s, float y, const Vec4& col) { t.ctext(c, SCREEN_W * 0.5f, y, 0.02f, s, col, 1); };
    auto leftOpt = [&](int row, const std::string& s) {
        float y = (SIDE_Y[row] - SCREEN_Y) + SCREEN_H * 0.5f - 0.01f;
        t.ctext(c, 0.008f, y, 0.019f, "< " + s, cyan);
    };
    auto rightOpt = [&](int row, const std::string& s) {
        float y = (SIDE_Y[row] - SCREEN_Y) + SCREEN_H * 0.5f - 0.01f;
        t.ctext(c, SCREEN_W - 0.008f, y, 0.019f, s + " >", cyan, 2);
    };
    auto bar = [&](float f) {
        t.crect(c, 0.06f, 0.08f, SCREEN_W - 0.12f, 0.018f, Vec4(0.15f, 0.2f, 0.3f, 1));
        t.crect(c, 0.06f, 0.08f, (SCREEN_W - 0.12f) * clampf(f, 0, 1), 0.018f, Vec4(0.25f, 0.85f, 0.45f, 1));
    };

    switch (m_state) {
        case WELCOME:
            title("Welcome!", 0.205f);
            line("Please insert your card", 0.15f, dim);
            t.crect(c, 0.16f, 0.05f, 0.08f, 0.05f, Vec4(0.1f, 0.45f, 0.85f, 1));
            t.crect(c, 0.17f, 0.075f, 0.015f, 0.012f, Vec4(0.9f, 0.75f, 0.3f, 1));
            break;
        case CARD_IN: title("Reading card...", 0.17f); bar(m_t / 1.4f); break;
        case PIN: {
            title("Enter your PIN", 0.205f);
            std::string stars;
            for (int i = 0; i < 4; ++i) stars += (i < (int)m_pin.size()) ? "* " : "_ ";
            t.ctext(c, SCREEN_W * 0.5f, 0.12f, 0.04f, stars, yellow, 1);
            line("Press ENTER to confirm", 0.075f, dim);
            line("(demo PIN: 1234)", 0.035f, Vec4(0.6f, 0.65f, 0.7f, 1));
            break;
        }
        case VERIFY: title("Verifying PIN...", 0.17f); bar(m_t / 1.3f); break;
        case PIN_WRONG:
            title("Incorrect PIN", 0.18f);
            line("Tries left: " + std::to_string(3 - m_tries), 0.12f, yellow);
            break;
        case MENU:
            smallTitle("Select a service");
            leftOpt(0, "Withdraw");
            leftOpt(1, "Balance");
            rightOpt(0, "Fast Tk 1000");
            rightOpt(3, "Exit");
            break;
        case AMOUNT:
            smallTitle("Select the amount");
            for (int i = 0; i < 4; ++i) leftOpt(i, money(AMOUNTS[i]));
            rightOpt(0, money(10000));
            rightOpt(2, "Other");
            rightOpt(3, "Cancel");
            break;
        case OTHER_AMOUNT:
            title("Type the amount", 0.20f);
            t.ctext(c, SCREEN_W * 0.5f, 0.12f, 0.035f, "Tk " + (m_amountText.empty() ? std::string("_") : m_amountText), yellow, 1);
            line("Multiples of 100, ENTER", 0.07f, dim);
            break;
        case PROCESSING:
            title("Processing...", 0.18f);
            line("Withdraw " + money(m_amount), 0.135f, yellow);
            bar(m_t / 2.4f);
            break;
        case NO_FUNDS:
            title("Insufficient funds", 0.15f);
            leftOpt(3, "Try again");
            rightOpt(3, "Exit");
            break;
        case BALANCE: {
            title("Available balance", 0.20f);
            int bal = m_npc ? 48250 : Inventory::player().bank;
            t.ctext(c, SCREEN_W * 0.5f, 0.13f, 0.034f, money(bal), yellow, 1);
            leftOpt(3, "Back");
            rightOpt(3, "Exit");
            break;
        }
        case CANCELLED: title("Transaction cancelled", 0.17f); break;
        case TAKE_CARD:
            title("Please take your card", 0.18f);
            line("Card is returned first", 0.12f, dim);
            break;
        case TAKE_CASH:
            title("Please take your cash", 0.19f);
            t.ctext(c, SCREEN_W * 0.5f, 0.12f, 0.034f, money(m_amount), yellow, 1);
            break;
        case RECEIPT:
            title(m_t < 1.2f ? "Printing receipt..." : "Take your receipt", 0.18f);
            if (m_t < 1.2f) bar(m_t / 1.2f);
            break;
        case THANKS:
            title("Thank you!", 0.19f);
            line("Have a safe journey", 0.13f, dim);
            break;
    }

    // header text + keypad labels
    Canvas h;
    h.model = translate(m_root, Vec3(0, 1.96f, 0.362f));
    h.w = 1.0f;
    h.h = 0.24f;
    t.ctext(h, 0.5f, 0.06f, 0.13f, "ATM", Vec4(1, 1, 1, 1), 1);
    Mat4 kf = keypadFrame(m_root);
    for (int i = 0; i < 16; ++i) {
        if (!KEYS[i][0]) continue;
        Canvas kc;
        Vec3 p = keyPos(i) + Vec3(0, 0.0095f - m_keyAnim[i] * 0.006f, 0);
        kc.model = rotate(translate(kf, p), toRadians(-90.0f), Vec3(1, 0, 0));
        kc.w = 0.062f;
        kc.h = 0.042f;
        std::string k = KEYS[i];
        float sz = k.size() > 1 ? 0.011f : 0.024f;
        t.ctext(kc, 0.031f, 0.021f - sz * 0.5f, sz, k, Vec4(0.05f, 0.05f, 0.06f, 1), 1);
    }
}
