// =============================================================================
//  Inventory.h  -  The player's cash, bank account and collected items
// =============================================================================
#pragma once
#include <string>
#include <vector>

struct Inventory {
    int cash = 120;              // Taka in the player's pocket
    int bank = 25000;            // Taka in the bank account (ATM)
    std::vector<std::string> items;

    static Inventory& player() { static Inventory inv; return inv; }

    bool pay(int amount) {
        if (cash < amount) return false;
        cash -= amount;
        return true;
    }
    void addItem(const std::string& s) {
        items.push_back(s);
        if (items.size() > 6) items.erase(items.begin());
    }
};
