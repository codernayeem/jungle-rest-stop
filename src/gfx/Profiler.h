// =============================================================================
//  Profiler.h  -  Tiny GPU/CPU timer used to find slow parts of a frame.
//  GPU times come from OpenGL timestamp queries (GL 3.3 core).
// =============================================================================
#pragma once
#include <glad/gl.h>
#include <chrono>
#include <map>
#include <string>
#include <vector>

class Profiler {
public:
    static Profiler& get() { static Profiler p; return p; }
    bool enabled = false;

    void beginFrame() {
        if (!enabled) return;
        m_names.clear();
        m_cpu.clear();
        mark("start");
    }
    void mark(const char* name) {
        if (!enabled) return;
        if (m_queries.size() <= m_names.size()) {
            GLuint q;
            glGenQueries(1, &q);
            m_queries.push_back(q);
        }
        glQueryCounter(m_queries[m_names.size()], GL_TIMESTAMP);
        m_names.push_back(name);
        m_cpu.push_back(std::chrono::high_resolution_clock::now());
    }
    // blocks until the GPU finished; accumulates per-section milliseconds
    void endFrame() {
        if (!enabled || m_names.size() < 2) return;
        std::vector<GLuint64> t(m_names.size());
        for (size_t i = 0; i < m_names.size(); ++i) glGetQueryObjectui64v(m_queries[i], GL_QUERY_RESULT, &t[i]);
        for (size_t i = 1; i < m_names.size(); ++i) {
            gpu[m_names[i]] += (t[i] - t[i - 1]) / 1e6;
            cpu[m_names[i]] += std::chrono::duration<double, std::milli>(m_cpu[i] - m_cpu[i - 1]).count();
        }
        frames++;
    }
    void reset() { gpu.clear(); cpu.clear(); frames = 0; }

    std::map<std::string, double> gpu, cpu;
    int frames = 0;

private:
    std::vector<GLuint> m_queries;
    std::vector<std::string> m_names;
    std::vector<std::chrono::high_resolution_clock::time_point> m_cpu;
};
