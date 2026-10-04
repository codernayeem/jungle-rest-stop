// =============================================================================
//  ATM.h  -  Automated Teller Machine with a step-by-step on-screen process
//
//  Steps shown on the ATM screen AND in the HUD side panel:
//    1 Insert card  2 Enter PIN  3 Choose service  4 Choose amount
//    5 Processing   6 Take card  7 Take cash       8 Take receipt
//  Uses: 4 + 4 side function keys beside the screen, a 4x4 keypad,
//  card slot, cash dispenser and receipt printer (all clickable).
//  NPC visitors run the same state machine automatically.
// =============================================================================
#pragma once
#include <string>
#include <vector>
#include "../gfx/Draw.h"
#include "../gfx/Text.h"

class ATM {
    friend class App;   // the report showcase drives the machines directly
public:
    void init(const Vec3& pos, float yawDeg);
    void update(float dt);
    void drawStatic(DrawSink& s) const;
    void drawDynamic(DrawSink& dyn) const;
    void drawScreen(TextRenderer& t) const;
    void registerInteractions();
    void registerLights() const;

    // HUD helpers
    bool playerSession() const { return m_state != WELCOME && !m_npc; }
    int currentStep() const;                // 1..8, 0 = not started
    static const char* stepName(int i);     // 1..8
    static const int STEP_COUNT = 8;

    // NPC
    bool available() const { return m_state == WELCOME && !m_playerNear; }
    bool npcStart();
    bool npcFinished() const { return m_npc && m_state == WELCOME; }
    bool inUseByNpc() const { return m_npc; }
    Vec3 standPos() const;
    float standYaw() const { return m_yaw + 180.0f; }
    void setPlayerNear(bool b) { m_playerNear = b; }
    Mat4 root() const { return m_root; }

private:
    enum State {
        WELCOME, CARD_IN, PIN, VERIFY, PIN_WRONG, MENU, AMOUNT, OTHER_AMOUNT, PROCESSING,
        TAKE_CARD, TAKE_CASH, RECEIPT, BALANCE, NO_FUNDS, CANCELLED, THANKS
    };
    void go(State s);
    void sideKey(int i);        // 0-3 left (top..bottom), 4-7 right
    void keypad(const std::string& k);
    void cardSlot();
    void cashSlot();
    void receiptSlot();
    void npcThink(float dt);

    Vec3 m_pos;
    float m_yaw = 0;
    Mat4 m_root;
    State m_state = WELCOME;
    float m_t = 0.0f;           // time in current state
    float m_time = 0.0f;
    std::string m_pin, m_amountText;
    int m_amount = 0;
    int m_tries = 0;
    bool m_wantReceipt = true;
    bool m_cashOut = false, m_cardOut = false, m_receiptOut = false;
    float m_cardAnim = 0, m_cashAnim = 0, m_receiptAnim = 0;   // 0..1 how far out
    float m_keyAnim[16] = {}, m_sideAnim[8] = {};
    bool m_npc = false;
    float m_npcTimer = 0.0f;
    int m_npcStep = 0;
    bool m_playerNear = false;
    int m_idBase = 0;
};
