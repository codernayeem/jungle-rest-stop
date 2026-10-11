// =============================================================================
//  Messages.h  -  Short on-screen notifications ("toasts") shown by the HUD
// =============================================================================
#pragma once
#include <string>
#include <deque>

struct Toast {
    std::string text;
    float time;
};

class Messages {
public:
    static Messages& get() { static Messages m; return m; }
    void push(const std::string& t, float seconds = 3.5f) {
        if (!m_list.empty() && m_list.back().text == t) { m_list.back().time = seconds; return; }
        m_list.push_back({t, seconds});
        while (m_list.size() > 5) m_list.pop_front();
    }
    void update(float dt) {
        for (auto& t : m_list) t.time -= dt;
        while (!m_list.empty() && m_list.front().time <= 0.0f) m_list.pop_front();
    }
    const std::deque<Toast>& list() const { return m_list; }
    void clear() { m_list.clear(); }
private:
    std::deque<Toast> m_list;
};

inline void notify(const std::string& t, float seconds = 3.5f) { Messages::get().push(t, seconds); }
