#include "Profiler.hpp"

static std::unordered_map<std::string, float> _timings;

TimeScope::~TimeScope() {
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() / 1000.0f;
    if (logged) {
        LOG_INFO("{} took {} seconds", title.c_str(), elapsed / 1000.0f);
    } else {
        auto it = _timings.find(title);
        if (it == _timings.end()) {
            _timings[title] = elapsed;
        } else {
            it->second = (it->second + elapsed) / 2.0f;
        }
    }
}
const std::unordered_map<std::string, float>& TimeScope::GetCPUTimes() {
    return _timings;
}
