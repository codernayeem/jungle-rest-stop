#include "VendingMachine.h"
#include "../game/Audio.h"
#include "../game/Interaction.h"
#include "../game/Inventory.h"
#include "../game/Messages.h"
#include "../scene/Lights.h"
#include "../scene/Materials.h"
#include <cmath>
#include <cstdio>

namespace {
// local layout (machine faces +Z, origin at floor centre)
const float SHELF_Y[4] = {1.555f, 1.285f, 1.015f, 0.745f};   // row A (top) .. D
const float COL_X[3] = {-0.36f, -0.15f, 0.06f};
const float ITEM_GAP = 0.075f;
const float ITEM_FRONT_Z = 0.0525f;
const float FRONT_Z = 0.40f;
const float BAY_FLOOR_Y = 0.235f;
const float VEND_TIME = 1.6f;
const char* KEY_LABELS[9] = {"A", "B", "C", "D", "1", "2", "3", "CLR", "OK"};

Vec3 keyLocal(int k) {
    if (k < 4) return Vec3(0.255f, 1.50f - k * 0.06f, FRONT_Z);
    if (k < 7) return Vec3(0.325f, 1.50f - (k - 4) * 0.06f, FRONT_Z);
    return Vec3(0.395f, 1.50f - (k - 7) * 0.06f, FRONT_Z);
}

std::string tk(int v) {
    char b[32];
    std::snprintf(b, sizeof(b), "Tk %d", v);
    return b;
}

std::vector<Product> catalogue(VendKind k) {
    if (k == VEND_DRINKS)
        return {{"Cola", 30, P_CAN, {0.80f, 0.08f, 0.10f}, {0.95f, 0.95f, 0.95f}},
                {"Lemon Lime", 30, P_CAN, {0.20f, 0.75f, 0.25f}, {0.95f, 0.92f, 0.20f}},
                {"Orange Soda", 30, P_CAN, {0.98f, 0.50f, 0.08f}, {1.0f, 0.85f, 0.25f}},
                {"Energy Drink", 60, P_CAN, {0.10f, 0.25f, 0.70f}, {0.75f, 0.78f, 0.82f}},
                {"Iced Coffee", 55, P_CAN, {0.35f, 0.20f, 0.10f}, {0.92f, 0.85f, 0.70f}},
                {"Mango Juice", 40, P_JUICE, {0.98f, 0.72f, 0.10f}, {0.25f, 0.60f, 0.20f}},
                {"Mineral Water", 20, P_BOTTLE, {0.60f, 0.82f, 0.95f}, {0.15f, 0.40f, 0.85f}},
                {"Apple Juice", 45, P_BOTTLE, {0.90f, 0.75f, 0.20f}, {0.25f, 0.65f, 0.20f}},
                {"Green Tea", 35, P_BOTTLE, {0.55f, 0.75f, 0.30f}, {0.95f, 0.95f, 0.90f}},
                {"Sports Drink", 50, P_BOTTLE, {0.15f, 0.55f, 0.95f}, {0.95f, 0.30f, 0.10f}},
                {"Lychee Drink", 40, P_JUICE, {0.95f, 0.55f, 0.65f}, {0.95f, 0.95f, 0.95f}},
                {"Milk", 35, P_JUICE, {0.95f, 0.95f, 0.98f}, {0.20f, 0.45f, 0.85f}}};
    if (k == VEND_SNACKS)
        return {{"BBQ Chips", 30, P_BAG, {0.85f, 0.18f, 0.12f}, {1.0f, 0.80f, 0.15f}},
                {"Salted Chips", 30, P_BAG, {0.95f, 0.85f, 0.15f}, {0.85f, 0.15f, 0.12f}},
                {"Sour Cream", 30, P_BAG, {0.18f, 0.62f, 0.30f}, {0.95f, 0.95f, 0.95f}},
                {"Cheese Puffs", 35, P_BAG, {0.98f, 0.55f, 0.05f}, {0.20f, 0.30f, 0.80f}},
                {"Chanachur", 25, P_BAG, {0.75f, 0.10f, 0.45f}, {1.0f, 0.85f, 0.25f}},
                {"Nachos", 40, P_BAG, {0.95f, 0.70f, 0.10f}, {0.15f, 0.15f, 0.15f}},
                {"Crackers", 25, P_BOX, {0.90f, 0.80f, 0.55f}, {0.80f, 0.15f, 0.12f}},
                {"Peanuts", 20, P_BAG, {0.55f, 0.35f, 0.20f}, {0.95f, 0.90f, 0.70f}},
                {"Pretzels", 30, P_BOX, {0.20f, 0.40f, 0.75f}, {0.95f, 0.80f, 0.30f}},
                {"Popcorn", 35, P_BAG, {0.95f, 0.95f, 0.90f}, {0.85f, 0.12f, 0.12f}},
                {"Spicy Chips", 30, P_BAG, {0.60f, 0.05f, 0.05f}, {0.98f, 0.55f, 0.10f}},
                {"Rice Crisps", 25, P_BOX, {0.95f, 0.65f, 0.75f}, {0.40f, 0.20f, 0.55f}}};
    return {{"Milk Chocolate", 40, P_BAR, {0.40f, 0.22f, 0.12f}, {0.90f, 0.75f, 0.20f}},
            {"Dark Chocolate", 50, P_BAR, {0.18f, 0.10f, 0.06f}, {0.85f, 0.15f, 0.15f}},
            {"Caramel Bar", 35, P_BAR, {0.85f, 0.55f, 0.15f}, {0.40f, 0.20f, 0.10f}},
            {"Wafer", 25, P_BAR, {0.85f, 0.10f, 0.20f}, {0.95f, 0.95f, 0.95f}},
            {"Nougat", 30, P_BAR, {0.95f, 0.90f, 0.75f}, {0.20f, 0.45f, 0.80f}},
            {"Coconut Bar", 30, P_BAR, {0.95f, 0.95f, 0.98f}, {0.30f, 0.60f, 0.25f}},
            {"Butter Cookies", 45, P_BOX, {0.15f, 0.30f, 0.70f}, {0.95f, 0.85f, 0.35f}},
            {"Choco Cookies", 45, P_BOX, {0.35f, 0.18f, 0.10f}, {0.95f, 0.95f, 0.90f}},
            {"Biscuits", 20, P_BOX, {0.95f, 0.75f, 0.15f}, {0.80f, 0.10f, 0.10f}},
            {"Gummy Bears", 35, P_BAG, {0.95f, 0.35f, 0.60f}, {0.30f, 0.85f, 0.30f}},
            {"Lollipops", 20, P_BAG, {0.55f, 0.25f, 0.85f}, {0.98f, 0.85f, 0.20f}},
            {"Mint Candy", 15, P_BOX, {0.20f, 0.75f, 0.70f}, {0.95f, 0.95f, 0.95f}}};
}
}  // namespace

// =============================================================================
void VendingMachine::init(int index, VendKind kind, const Vec3& pos, float yawDeg) {
    m_index = index;
    m_kind = kind;
    m_pos = pos;
    m_yaw = yawDeg;
    m_root = rotateYXZ(translate(Mat4(), pos), {0, yawDeg, 0});
    auto cat = catalogue(kind);
    for (int i = 0; i < ROWS * COLS; ++i) {
        m_slots[i].p = cat[i];
        m_slots[i].stock = DEPTH;
    }
    m_idBase = InteractionSystem::reserveIds(16);
}

const char* VendingMachine::title() const {
    switch (m_kind) {
        case VEND_DRINKS: return "COLD DRINKS";
        case VEND_SNACKS: return "CRISPY SNACKS";
        default: return "SWEET TREATS";
    }
}

Vec3 VendingMachine::standPos() const { return m_root.transformPoint(Vec3(-0.1f, 0, 1.05f)); }

Vec3 VendingMachine::slotLocal(int row, int col) const { return Vec3(COL_X[col], SHELF_Y[row] + 0.01f, ITEM_FRONT_Z); }

void VendingMachine::message(const std::string& l1, const std::string& l2, float t) {
    m_line1 = l1;
    m_line2 = l2;
    m_msgTimer = t;
}

// -----------------------------------------------------------------------------
// Keypad logic
// -----------------------------------------------------------------------------
void VendingMachine::press(const std::string& key) {
    for (int k = 0; k < 9; ++k)
        if (key == KEY_LABELS[k]) m_keyAnim[k] = 1.0f;
    if (m_state != IDLE) { message("PLEASE WAIT", "MACHINE BUSY", 1.5f); return; }
    if (key == "CLR") { m_entry.clear(); message("CODE CLEARED", "", 1.2f); return; }
    if (key == "OK") {
        if (m_entry.size() != 2) { message("ENTER A CODE", "e.g. B2", 2.0f); return; }
        int row = m_entry[0] - 'A', col = m_entry[1] - '1';
        int idx = row * COLS + col;
        Slot& s = m_slots[idx];
        if (s.stock <= 0) { message("SOLD OUT", "CHOOSE ANOTHER", 2.5f); m_entry.clear(); return; }
        if (m_credit < s.p.price) { message("ADD " + tk(s.p.price - m_credit), "PRICE " + tk(s.p.price), 2.5f); return; }
        startVend(idx, false);
        return;
    }
    if (key[0] >= 'A' && key[0] <= 'D') { m_entry = key; return; }
    if (key[0] >= '1' && key[0] <= '3') {
        if (m_entry.size() == 1) m_entry += key;
        else message("PRESS A LETTER", "FIRST (A-D)", 1.8f);
    }
}

void VendingMachine::startVend(int idx, bool byNpc) {
    m_active = idx;
    m_vendT = 0.0f;
    m_state = VENDING;
    Audio::playAt(SND_MOTOR, m_pos + Vec3(0, 1.2f, 0));
    m_byNpc = byNpc;
    Slot& s = m_slots[idx];
    if (!byNpc) {
        int change = m_credit - s.p.price;
        m_credit = 0;
        if (change > 0) {
            m_cupCash += change;
            for (int c = 0; c < std::min(6, change / 5); ++c) m_coins.push_back({Vec3(0.33f + 0.01f * (c % 3), 1.0f + c * 0.03f, 0.37f), 0.0f, false});
            notify("Change " + tk(change) + " dropped in the coin cup");
        }
    }
    m_lastName = s.p.name;
    m_lastShape = s.p.shape;
    m_entry.clear();
    m_msgTimer = 0.0f;
    totalSold++;
}

void VendingMachine::returnCredit() {
    if (m_credit <= 0) { message("NO CREDIT", "", 1.5f); return; }
    m_cupCash += m_credit;
    for (int c = 0; c < std::min(6, m_credit / 5); ++c) m_coins.push_back({Vec3(0.33f + 0.01f * (c % 3), 1.0f + c * 0.03f, 0.37f), 0.0f, false});
    message("RETURNED", tk(m_credit), 2.0f);
    m_credit = 0;
    m_entry.clear();
}

bool VendingMachine::npcStartPurchase(unsigned seed) {
    if (!available()) return false;
    Rng r(seed * 7919u + 13u);
    for (int tries = 0; tries < 20; ++tries) {
        int idx = r.irange(0, ROWS * COLS - 1);
        if (m_slots[idx].stock > 0) {
            message("CARD PAYMENT", "APPROVED", 1.5f);
            m_keyAnim[8] = 1.0f;
            startVend(idx, true);
            return true;
        }
    }
    return false;
}

void VendingMachine::takeItem(bool byPlayer) {
    if (m_state != READY) return;
    m_state = IDLE;
    m_flapTarget = 50.0f;
    m_flapHold = 1.0f;
    if (byPlayer) {
        Inventory::player().addItem(m_lastName);
        notify("You got: " + m_lastName);
        message("THANK YOU!", "ENJOY", 2.5f);
    } else {
        message("THANK YOU!", "", 2.0f);
    }
}

void VendingMachine::npcTakeItem() { takeItem(false); }

// -----------------------------------------------------------------------------
void VendingMachine::update(float dt) {
    m_time += dt;
    if (m_msgTimer > 0) m_msgTimer -= dt;
    for (float& k : m_keyAnim) k = std::max(0.0f, k - dt * 4.0f);
    for (float& k : m_slotAnim) k = std::max(0.0f, k - dt * 3.0f);

    if (m_state == VENDING) {
        m_vendT += dt;
        Slot& s = m_slots[m_active];
        float t = std::min(1.0f, m_vendT / VEND_TIME);
        s.advance = t;
        s.spin = t * 360.0f;
        if (t >= 1.0f) {
            // the front item leaves the spiral and starts falling
            int row = m_active / COLS, col = m_active % COLS;
            m_dropPos = slotLocal(row, col) + Vec3(0, 0, ITEM_GAP);
            m_dropVel = Vec3(0, 0.0f, 0.35f);
            m_dropRot = Vec3(0, 0, 0);
            m_dropSpin = Vec3(220.0f + 60.0f * (m_index % 2), 0, 90.0f);
            s.stock--;
            s.advance = 0.0f;
            s.spin = 0.0f;
            m_state = DROPPING;
        }
    } else if (m_state == DROPPING) {
        m_dropVel.y -= 9.81f * dt;
        m_dropPos += m_dropVel * dt;
        m_dropPos.z = clampf(m_dropPos.z, 0.14f, 0.30f);
        m_dropRot += m_dropSpin * dt;
        const float rest = BAY_FLOOR_Y + 0.035f;
        if (m_dropPos.y <= rest) {
            m_dropPos.y = rest;
            Audio::playAt(SND_THUD, m_pos + Vec3(0, 0.5f, 0), std::min(1.0f, std::fabs(m_dropVel.y) * 0.5f));
            m_dropVel.y = -m_dropVel.y * 0.32f;   // bounce with energy loss
            m_dropVel.z = 0.0f;
            m_dropSpin *= 0.4f;
            if (std::fabs(m_dropVel.y) < 0.35f) {
                m_state = READY;
                // lie flat on its side in the tray
                m_dropRot = Vec3(90.0f, 15.0f * (m_index - 1), 0);
                m_dropPos.y = rest;
                m_dropPos.z = 0.22f;
                if (!m_byNpc) message("TAKE YOUR ITEM", "PUSH FLAP BELOW", 6.0f);
            }
        }
    }

    // collection flap swings back closed
    if (m_flapHold > 0) m_flapHold -= dt;
    else m_flapTarget = 0.0f;
    m_flap = approach(m_flap, m_flapTarget, dt * 160.0f);

    // returning coins fall into the cup
    for (auto& c : m_coins) {
        if (c.landed) continue;
        c.vy -= 9.81f * dt;
        c.pos.y += c.vy * dt;
        if (c.pos.y < 0.84f) { c.pos.y = 0.84f; c.landed = true; Audio::playAt(SND_COIN_DROP, m_pos + Vec3(0, 0.9f, 0), 0.6f); }
    }

    // slots restock some time after they become empty
    for (auto& s : m_slots) {
        if (s.stock > 0) continue;
        s.restock += dt;
        if (s.restock > 40.0f) {
            s.stock = DEPTH;
            s.restock = 0.0f;
            notify(std::string(title()) + ": " + s.p.name + " restocked");
        }
    }
}

// -----------------------------------------------------------------------------
// Interactions (each key, money slots, lever, cup and flap)
// -----------------------------------------------------------------------------
void VendingMachine::registerInteractions() {
    auto& is = InteractionSystem::get();
    for (int k = 0; k < 9; ++k) {
        std::string label = KEY_LABELS[k];
        is.add(m_idBase + k, translate(m_root, keyLocal(k) + Vec3(0, 0, 0.01f)), Vec3(0.027f, 0.025f, 0.02f),
               "Press [" + label + "]", [this, label]() { press(label); });
    }
    Inventory& inv = Inventory::player();
    is.add(m_idBase + 9, translate(m_root, {0.30f, 1.23f, FRONT_Z + 0.01f}), Vec3(0.04f, 0.04f, 0.02f),
           "Insert Tk 10 coin  (you have " + tk(inv.cash) + ")", [this]() {
               if (m_state != IDLE) { message("PLEASE WAIT", "", 1.0f); return; }
               if (!Inventory::player().pay(10)) { notify("Not enough cash - use the ATM!"); return; }
               m_credit += 10;
               m_slotAnim[0] = 1.0f;
               Audio::play(SND_COIN);
           });
    is.add(m_idBase + 10, translate(m_root, {0.33f, 1.12f, FRONT_Z + 0.01f}), Vec3(0.09f, 0.03f, 0.02f),
           "Insert Tk 50 note  (you have " + tk(inv.cash) + ")", [this]() {
               if (m_state != IDLE) { message("PLEASE WAIT", "", 1.0f); return; }
               if (!Inventory::player().pay(50)) { notify("Not enough cash - use the ATM!"); return; }
               m_credit += 50;
               m_slotAnim[1] = 1.0f;
               Audio::play(SND_COIN, 0.4f);
           });
    is.add(m_idBase + 11, translate(m_root, {0.42f, 1.23f, FRONT_Z + 0.01f}), Vec3(0.025f, 0.035f, 0.02f), "Coin return lever",
           [this]() { m_slotAnim[2] = 1.0f; returnCredit(); });
    if (m_cupCash > 0)
        is.add(m_idBase + 12, translate(m_root, {0.33f, 0.86f, FRONT_Z}), Vec3(0.07f, 0.05f, 0.04f),
               "Take change (" + tk(m_cupCash) + ")", [this]() {
                   Inventory::player().cash += m_cupCash;
                   notify("Collected " + tk(m_cupCash) + " change");
                   m_cupCash = 0;
                   m_coins.clear();
               });
    is.add(m_idBase + 13, translate(m_root, {-0.15f, 0.38f, FRONT_Z}), Vec3(0.30f, 0.17f, 0.03f),
           m_state == READY ? "Push flap - take " + m_lastName : "Collection flap", [this]() {
               if (m_state == READY) takeItem(true);
               else { m_flapTarget = 45.0f; m_flapHold = 0.6f; }
           });
}

void VendingMachine::registerLights() const {
    LightDef l;
    l.pos = m_root.transformPoint(Vec3(-0.15f, 1.6f, 0.75f));
    l.color = Vec3(0.85f, 0.95f, 1.0f);
    l.intensity = 0.9f;
    l.range = 2.6f;
    l.group = LG_MACHINES;
    LightManager::get().addStatic(l);
}

// -----------------------------------------------------------------------------
// Static cabinet: shell, display cavity, shelves, panel plates, tray frame
// -----------------------------------------------------------------------------
void VendingMachine::drawStatic(DrawSink& s) const {
    Node n(s, m_root);
    const Vec3 bodyCols[3] = {Vec3(0.72f, 0.10f, 0.12f), Vec3(0.88f, 0.45f, 0.10f), Vec3(0.16f, 0.32f, 0.68f)};
    Material body = Mat::paint(bodyCols[m_kind]);
    Material trim = Mat::darkMetal();
    Material interior = Material(Vec3(0.86f, 0.87f, 0.90f), 0.3f, 24.0f);
    Material shelf = Mat::steel();
    Material chrome = Mat::chrome();
    Material dark = Mat::black();

    // shell
    partTag("Back panel");
    n.box({0, 1.0f, -0.38f}, {1.0f, 1.86f, 0.04f}, body);
    partTag("Side panels");
    n.box({-0.48f, 1.0f, 0}, {0.04f, 1.86f, 0.8f}, body);
    n.box({0.48f, 1.0f, 0}, {0.04f, 1.86f, 0.8f}, body);
    partTag("Top panel");
    n.box({0, 1.93f, 0}, {1.0f, 0.06f, 0.8f}, body);
    partTag("Base");
    n.box({0, 0.11f, 0}, {1.0f, 0.14f, 0.8f}, body);
    partTag("Levelling feet");
    for (int i = -1; i <= 1; i += 2)
        for (int j = -1; j <= 1; j += 2) {
            n.cyl({i * 0.42f, 0.025f, j * 0.32f}, {0.06f, 0.05f, 0.06f}, chrome);
        }
    // marquee header (always lit) and trims around the glass
    partTag("Lit marquee");
    n.box({0, 1.86f, FRONT_Z - 0.002f}, {0.94f, 0.12f, 0.02f}, Mat::glow(bodyCols[m_kind] * 0.5f + Vec3(0.4f), 0.7f));
    partTag("Glass frame");
    n.box({-0.15f, 1.785f, FRONT_Z - 0.016f}, {0.66f, 0.03f, 0.04f}, trim);
    n.box({-0.15f, 0.645f, FRONT_Z - 0.016f}, {0.66f, 0.03f, 0.04f}, trim);
    n.box({-0.47f, 1.215f, FRONT_Z - 0.016f}, {0.03f, 1.11f, 0.04f}, trim);
    n.box({0.175f, 1.215f, FRONT_Z - 0.016f}, {0.03f, 1.11f, 0.04f}, trim);
    // display cavity + interior tube light (emissive)
    partTag("Display cavity");
    n.box({-0.15f, 1.22f, -0.35f}, {0.64f, 1.14f, 0.02f}, interior);
    n.box({-0.455f, 1.22f, 0}, {0.01f, 1.14f, 0.7f}, interior);
    n.box({0.16f, 1.22f, 0}, {0.01f, 1.14f, 0.7f}, interior);
    n.box({-0.15f, 1.785f, 0}, {0.64f, 0.02f, 0.7f}, interior);
    partTag("Tube light");
    n.part(M_CYLINDER, {-0.15f, 1.76f, 0.30f}, {0.022f, 0.6f, 0.022f}, Mat::glow(Vec3(0.92f, 0.97f, 1.0f), 2.2f), {0, 0, 90});
    // shelves with price rails
    partTag("Shelves + rails");
    for (int r = 0; r < ROWS; ++r) {
        n.box({-0.15f, SHELF_Y[r] - 0.01f, -0.06f}, {0.62f, 0.015f, 0.38f}, shelf);
        n.box({-0.15f, SHELF_Y[r] - 0.02f, 0.13f}, {0.62f, 0.03f, 0.01f}, Material(Vec3(0.12f, 0.12f, 0.14f), 0.3f, 32.0f));
    }
    // glass front (transparent)
    partTag("Front glass");
    n.box({-0.15f, 1.22f, FRONT_Z - 0.015f}, {0.62f, 1.14f, 0.008f}, Mat::glass(0.18f));
    // control panel plate
    partTag("Controls/Control panel");
    n.box({0.33f, 1.185f, FRONT_Z - 0.02f}, {0.27f, 1.27f, 0.04f}, trim);
    partTag("Lower front panel");
    n.box({0.33f, 0.36f, FRONT_Z - 0.02f}, {0.27f, 0.38f, 0.04f}, body);          // panel below the controls
    n.box({0.33f, 0.36f, FRONT_Z + 0.001f}, {0.20f, 0.12f, 0.004f}, Material(Vec3(0.92f), 0.3f, 32.0f));   // service label
    partTag("Controls/LCD screen");
    n.box({0.33f, 1.66f, FRONT_Z + 0.001f}, {0.24f, 0.11f, 0.004f}, Material(Vec3(0.02f, 0.05f, 0.04f), 0.8f, 96.0f));
    partTag("Controls/Keypad plate");
    n.box({0.325f, 1.41f, FRONT_Z + 0.001f}, {0.22f, 0.27f, 0.004f}, Material(Vec3(0.12f, 0.12f, 0.14f), 0.3f, 32.0f));
    // coin slot plate, note acceptor, card pad, coin cup recess
    partTag("Controls/Coin slot");
    n.box({0.30f, 1.23f, FRONT_Z + 0.004f}, {0.07f, 0.09f, 0.008f}, chrome);
    n.box({0.30f, 1.235f, FRONT_Z + 0.009f}, {0.008f, 0.045f, 0.004f}, dark);
    partTag("Controls/Note acceptor");
    n.box({0.33f, 1.12f, FRONT_Z + 0.004f}, {0.19f, 0.06f, 0.008f}, dark);
    n.box({0.33f, 1.12f, FRONT_Z + 0.009f}, {0.15f, 0.008f, 0.004f}, Mat::glow(Vec3(0.2f, 0.9f, 0.3f), 1.0f));
    partTag("Controls/Card pad");
    n.box({0.33f, 1.0f, FRONT_Z + 0.004f}, {0.14f, 0.10f, 0.008f}, Material(Vec3(0.15f, 0.18f, 0.25f), 0.4f, 32.0f));
    n.box({0.33f, 1.0f, FRONT_Z + 0.009f}, {0.05f, 0.035f, 0.003f}, Mat::glow(Vec3(0.2f, 0.6f, 1.0f), 0.6f));
    partTag("Coin cup");
    n.box({0.33f, 0.86f, FRONT_Z - 0.01f}, {0.15f, 0.11f, 0.04f}, chrome);
    n.box({0.33f, 0.86f, FRONT_Z + 0.006f}, {0.11f, 0.07f, 0.01f}, dark);
    // collection bay
    partTag("Collection bay");
    n.box({-0.15f, BAY_FLOOR_Y - 0.01f, 0.22f}, {0.6f, 0.02f, 0.34f}, Mat::black());
    n.box({-0.15f, 0.40f, 0.06f}, {0.6f, 0.36f, 0.02f}, Mat::black());
    n.box({-0.15f, 0.60f, FRONT_Z - 0.018f}, {0.66f, 0.06f, 0.04f}, trim);
    n.box({-0.15f, 0.19f, FRONT_Z - 0.018f}, {0.66f, 0.04f, 0.04f}, trim);
    n.box({-0.465f, 0.40f, FRONT_Z - 0.012f}, {0.04f, 0.34f, 0.04f}, trim);   // 8 mm proud of the side panel
    n.box({0.165f, 0.40f, FRONT_Z - 0.019f}, {0.04f, 0.34f, 0.04f}, trim);
}

// -----------------------------------------------------------------------------
// Dynamic parts: spirals, products, keys, LEDs, flap, dropping item, coins
// -----------------------------------------------------------------------------
void VendingMachine::drawDynamic(DrawSink& dyn, const Vec3& cam) const {
    Node n(dyn, m_root);
    auto& is = InteractionSystem::get();
    float dist = (cam - m_pos).length();
    bool detail = dist < 22.0f;

    if (detail) {
        Material coil = Mat::chrome();
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c) {
                const Slot& s = m_slots[r * COLS + c];
                Vec3 sp = slotLocal(r, c);
                // spiral (rotates about its own axis while vending)
                partTag("Spirals");
                n.at({sp.x, sp.y + 0.055f, -0.06f}).rotZ(-s.spin).part(M_COIL, {0, 0, 0}, {1, 1, 1}, coil);
                for (int k = 0; k < s.stock; ++k) {
                    float z = sp.z - k * ITEM_GAP + s.advance * ITEM_GAP;
                    partTag("Products");
                    drawProduct(n.at({sp.x, sp.y, z}), s.p);
                }
            }
        // falling / waiting item
        if (m_state == DROPPING || m_state == READY) {
            const Slot& s = m_slots[m_active];
            Node it = n.at(m_dropPos, m_dropRot).at({0, -productHeight(s.p.shape) * 0.5f, 0});
            drawProduct(it, s.p);
        }
    }
    partTag("Controls/Keys");
    // keypad buttons (pushed in when pressed, glow when hovered)
    for (int k = 0; k < 9; ++k) {
        Vec3 p = keyLocal(k) + Vec3(0, 0, 0.008f - m_keyAnim[k] * 0.007f);
        Material km = Mat::plastic(k == 7 ? Vec3(0.85f, 0.2f, 0.15f) : (k == 8 ? Vec3(0.15f, 0.75f, 0.25f) : Vec3(0.82f, 0.83f, 0.86f)));
        if (is.hovered(m_idBase + k)) km.emission = Vec3(0.25f, 0.45f, 0.6f);
        n.box(p, {0.05f, 0.045f, 0.014f}, km);
    }
    // hover highlights for slots / lever / cup / flap
    auto hl = [&](int id, const Vec3& p, const Vec3& sz) {
        if (is.hovered(m_idBase + id)) n.box(p, sz, Mat::glow(Vec3(0.2f, 0.6f, 1.0f), 0.8f));
    };
    hl(9, {0.30f, 1.23f, FRONT_Z + 0.012f}, {0.075f, 0.095f, 0.002f});
    hl(10, {0.33f, 1.12f, FRONT_Z + 0.012f}, {0.20f, 0.065f, 0.002f});
    hl(12, {0.33f, 0.86f, FRONT_Z + 0.012f}, {0.16f, 0.12f, 0.002f});
    partTag("Controls/Coin-return lever");
    // coin return lever
    n.at({0.42f, 1.25f, FRONT_Z + 0.01f}).rotX(m_slotAnim[2] * 40.0f).part(M_CUBE, {0, -0.02f, 0.01f}, {0.02f, 0.05f, 0.02f},
                                                                          is.hovered(m_idBase + 11) ? Mat::glow(Vec3(0.6f, 0.8f, 1.0f), 0.6f) : Mat::chrome());
    partTag("Controls/Status LEDs");
    // status LEDs
    bool busy = m_state == VENDING || m_state == DROPPING;
    n.sph({0.26f, 1.585f, FRONT_Z + 0.01f}, Vec3(0.022f), Mat::glow(Vec3(0.2f, 1.0f, 0.3f), busy ? 0.15f : 1.6f));
    n.sph({0.33f, 1.585f, FRONT_Z + 0.01f}, Vec3(0.022f), Mat::glow(Vec3(1.0f, 0.65f, 0.1f), busy ? (std::fmod(m_time, 0.4f) < 0.2f ? 2.0f : 0.4f) : 0.12f));
    n.sph({0.40f, 1.585f, FRONT_Z + 0.01f}, Vec3(0.022f), Mat::glow(Vec3(0.2f, 0.6f, 1.0f), m_state == READY ? 2.0f : 0.12f));
    partTag("Collection flap");
    // collection flap (hinged at the top, swings inward)
    Node flap = n.at({-0.15f, 0.57f, FRONT_Z - 0.01f}).rotX(m_flap);
    Material flapM = Material(Vec3(0.18f, 0.19f, 0.22f), 0.5f, 48.0f);
    if (is.hovered(m_idBase + 13)) flapM.emission = Vec3(0.1f, 0.25f, 0.35f);
    flap.box({0, -0.17f, 0}, {0.58f, 0.34f, 0.015f}, flapM);
    flap.box({0, -0.17f, 0.01f}, {0.22f, 0.06f, 0.004f}, Material(Vec3(0.9f), 0.4f, 32.0f));
    partTag("Change coins");
    // coins resting in the cup
    for (const auto& c : m_coins) n.cyl(c.pos, {0.024f, 0.004f, 0.024f}, Material(Vec3(0.85f, 0.75f, 0.35f), 0.9f, 96.0f));
    partTag("Controls/LCD screen");
    // LCD glow surface
    n.box({0.33f, 1.66f, FRONT_Z + 0.004f}, {0.22f, 0.09f, 0.002f}, Mat::glow(Vec3(0.02f, 0.10f, 0.06f), 1.0f));
}

void VendingMachine::drawScreen(TextRenderer& t) const {
    Canvas c;
    c.model = translate(m_root, Vec3(0.33f, 1.66f, FRONT_Z + 0.007f));
    c.w = 0.22f;
    c.h = 0.09f;
    std::string l1, l2;
    if (m_msgTimer > 0) { l1 = m_line1; l2 = m_line2; }
    else if (m_state == VENDING || m_state == DROPPING) { l1 = "VENDING..."; l2 = m_lastName; }
    else if (m_state == READY) { l1 = "TAKE YOUR ITEM"; l2 = "PUSH FLAP"; }
    else if (m_entry.size() == 2) {
        const Slot& s = m_slots[(m_entry[0] - 'A') * COLS + (m_entry[1] - '1')];
        l1 = m_entry + " " + s.p.name;
        l2 = tk(s.p.price) + (m_credit >= s.p.price ? " PRESS OK" : " CR " + std::to_string(m_credit));
    } else if (m_credit > 0) { l1 = "CREDIT " + tk(m_credit); l2 = m_entry.empty() ? "ENTER CODE" : "CODE " + m_entry + "_"; }
    else { l1 = "INSERT MONEY"; l2 = m_entry.empty() ? "THEN CODE + OK" : "CODE " + m_entry + "_"; }
    Vec4 green(0.35f, 1.0f, 0.55f, 1.0f);
    t.ctext(c, 0.008f, 0.050f, 0.030f, l1.substr(0, 14), green);
    t.ctext(c, 0.008f, 0.012f, 0.030f, l2.substr(0, 14), green);

    // marquee title
    Canvas m;
    m.model = translate(m_root, Vec3(0.0f, 1.86f, FRONT_Z + 0.011f));
    m.w = 0.94f;
    m.h = 0.12f;
    t.ctext(m, 0.47f, 0.025f, 0.075f, title(), Vec4(1, 1, 1, 1), 1);

    // price tags along each shelf rail + key labels
    for (int r = 0; r < ROWS; ++r)
        for (int col = 0; col < COLS; ++col) {
            const Slot& s = m_slots[r * COLS + col];
            Canvas tag;
            tag.model = translate(m_root, Vec3(COL_X[col], SHELF_Y[r] - 0.02f, 0.136f));
            tag.w = 0.18f;
            tag.h = 0.03f;
            std::string code = std::string(1, char('A' + r)) + char('1' + col);
            std::string txt = s.stock > 0 ? code + " " + std::to_string(s.p.price) : code + " SOLD";
            t.ctext(tag, 0.09f, 0.004f, 0.022f, txt, s.stock > 0 ? Vec4(1, 1, 1, 1) : Vec4(1, 0.3f, 0.3f, 1), 1);
        }
    for (int k = 0; k < 9; ++k) {
        Canvas kc;
        kc.model = translate(m_root, keyLocal(k) + Vec3(0, 0, 0.016f - m_keyAnim[k] * 0.007f));
        kc.w = 0.05f;
        kc.h = 0.045f;
        float sz = k >= 7 ? 0.016f : 0.026f;
        t.ctext(kc, 0.025f, 0.0225f - sz * 0.5f, sz, KEY_LABELS[k], Vec4(0.08f, 0.08f, 0.1f, 1), 1);
    }
    Canvas lbl;
    lbl.model = translate(m_root, Vec3(0.33f, 1.30f, FRONT_Z + 0.003f));
    lbl.w = 0.24f;
    lbl.h = 0.03f;
    t.ctext(lbl, 0.12f, 0.0f, 0.018f, "10    RETURN", Vec4(0.9f, 0.9f, 0.9f, 1), 1);
    Canvas lbl2;
    lbl2.model = translate(m_root, Vec3(0.33f, 1.165f, FRONT_Z + 0.003f));
    lbl2.w = 0.24f;
    lbl2.h = 0.02f;
    t.ctext(lbl2, 0.12f, 0.0f, 0.016f, "NOTES Tk 50", Vec4(0.9f, 0.9f, 0.9f, 1), 1);
}
