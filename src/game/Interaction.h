// =============================================================================
//  Interaction.h  -  "Look at it and press E / click" system.
//  Every interactive part (buttons, slots, switches ...) registers an oriented
//  box each frame. A ray from the camera centre is tested against all boxes
//  (ray / OBB slab test in the box's local space); the closest hit is hovered.
// =============================================================================
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "../core/Math3D.h"

class InteractionSystem {
public:
    static InteractionSystem& get() { static InteractionSystem s; return s; }

    // Unique id block for an object (each object reserves a few ids)
    static int reserveIds(int count) { static int next = 1; int r = next; next += count; return r; }

    void beginFrame() { m_items.clear(); }
    // model = matrix of the box centre (rotation + translation), half = half size
    void add(int id, const Mat4& model, const Vec3& half, const std::string& prompt, std::function<void()> onUse);

    // Cast the picking ray. maxDist limits how far the player can reach.
    void update(const Vec3& origin, const Vec3& dir, float maxDist, bool enabled);
    void useHovered();

    bool hovered(int id) const { return m_hoveredId == id; }
    int hoveredId() const { return m_hoveredId; }
    const std::string& prompt() const { return m_prompt; }
    size_t count() const { return m_items.size(); }

private:
    struct Item {
        int id;
        Mat4 model, inv;
        Vec3 half;
        std::string prompt;
        std::function<void()> onUse;
    };
    std::vector<Item> m_items;
    int m_hoveredId = -1;
    std::string m_prompt;
    std::function<void()> m_hoveredAction;
};
