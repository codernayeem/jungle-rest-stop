// =============================================================================
//  VendingMachine.h  -  Fully working vending machine
//
//  How to buy (player):
//    1. insert money   - coin slot (+Tk 10) or note slot (+Tk 50)
//    2. type a code    - letter (row A-D) + number (column 1-3), e.g. "B2"
//    3. press OK       - the spiral motor turns, the item moves forward,
//                        falls (gravity + bounce) into the bottom tray
//    4. push the flap  - take the item; change drops into the coin cup
//  CLR clears the code, the coin-return lever gives back unused credit.
//  Empty slots are restocked automatically after a while.
// =============================================================================
#pragma once
#include <string>
#include <vector>
#include "Products.h"
#include "../gfx/Text.h"

enum VendKind { VEND_DRINKS = 0, VEND_SNACKS, VEND_SWEETS };

class VendingMachine {
    friend class App;   // the report showcase drives the machines directly
public:
    static const int ROWS = 4, COLS = 3, DEPTH = 4;

    void init(int index, VendKind kind, const Vec3& pos, float yawDeg);
    void update(float dt);
    void drawStatic(DrawSink& s) const;                       // cabinet (never changes)
    void drawDynamic(DrawSink& dyn, const Vec3& cam) const;   // products, spirals, buttons, lights
    void drawScreen(TextRenderer& t) const;
    void registerInteractions();
    void registerLights() const;

    // ---- used by NPC visitors ----
    bool available() const { return m_state == IDLE && m_credit == 0 && m_entry.empty() && !m_playerNear; }
    bool npcStartPurchase(unsigned seed);
    bool itemWaiting() const { return m_state == READY; }
    void npcTakeItem();
    std::string lastItemName() const { return m_lastName; }
    ProductShape lastItemShape() const { return m_lastShape; }
    Vec3 standPos() const;          // where a customer stands
    float standYaw() const { return m_yaw + 180.0f; }
    void setPlayerNear(bool b) { m_playerNear = b; }
    const char* title() const;
    Mat4 root() const { return m_root; }

    // statistics for the report
    int totalSold = 0;

private:
    enum State { IDLE, VENDING, DROPPING, READY };
    struct Slot { Product p; int stock = DEPTH; float spin = 0.0f; float advance = 0.0f; float restock = 0.0f; };
    struct Coin { Vec3 pos; float vy; bool landed; };

    void press(const std::string& key);
    void startVend(int slot, bool byNpc);
    void message(const std::string& l1, const std::string& l2, float t = 3.0f);
    void returnCredit();
    Vec3 slotLocal(int row, int col) const;   // local position of a slot's front
    void takeItem(bool byPlayer);

    int m_index = 0;
    VendKind m_kind = VEND_DRINKS;
    Vec3 m_pos;
    float m_yaw = 0.0f;
    Mat4 m_root;
    Slot m_slots[ROWS * COLS];

    State m_state = IDLE;
    int m_credit = 0;
    std::string m_entry;
    std::string m_line1, m_line2;
    float m_msgTimer = 0.0f;
    int m_active = -1;
    float m_vendT = 0.0f;
    Vec3 m_dropPos, m_dropVel, m_dropRot, m_dropSpin;
    float m_flap = 0.0f, m_flapTarget = 0.0f, m_flapHold = 0.0f;
    bool m_byNpc = false;
    bool m_playerNear = false;
    int m_cupCash = 0;
    std::vector<Coin> m_coins;
    float m_keyAnim[9] = {};
    float m_slotAnim[4] = {};
    float m_time = 0.0f;
    int m_idBase = 0;
    std::string m_lastName;
    ProductShape m_lastShape = P_CAN;
};
