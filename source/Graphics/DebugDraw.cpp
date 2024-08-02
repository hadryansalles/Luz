#include "DebugDraw.h"
#include "LuzCommon.h"

struct DebugDrawImpl {
    struct DrawData {
        std::vector<glm::vec3> points = {};
        DebugDraw::ConfigData config = {};
    };
    std::unordered_map<std::string, DrawData> gizmos;
    LineBlock lineData[LUZ_MAX_LINES];
    uint32_t count = 0;
};

static DebugDrawImpl* impl;

void DebugDraw::Create() {
    impl = new DebugDrawImpl;
}

void DebugDraw::Destroy() {
    delete impl;
}

void DebugDraw::Rect(const std::string& name, const glm::vec3& bl, const glm::vec3& tl, const glm::vec3& tr, const glm::vec3& br) {
    impl->gizmos[name].points = {bl, tl, tr, br, bl};
}

void DebugDraw::Line(const std::string& name, const glm::vec3& p0, const glm::vec3& p1) {
    impl->gizmos[name].points = {p0, p1};
}

void DebugDraw::Clear(const std::string& name) {
    auto it = impl->gizmos.find(name);
    if  (it != impl->gizmos.end()) {
        impl->gizmos.erase(it);
    }
}

void DebugDraw::Config(const std::string& name, const ConfigData& config) {
    impl->gizmos[name].config = config;
}

void DebugDraw::Update() {
    impl->count = 0;
    for (auto& it : impl->gizmos) {
        auto& points = it.second.points;
        auto& config = it.second.config;
        // todo: points
        for (int i = 0; i < points.size() - 1; i++) {
            if (impl->count < LUZ_MAX_LINES) {
                auto& data = impl->lineData[impl->count++];
                data.p0 = points[i];
                data.p1 = points[i + 1];
                data.isPoint = false;
                data.depthAware = config.depthAware;
                data.color = config.color;
                data.thickness = config.thickness;
            }
        }
    }
}

void* DebugDraw::Get(uint32_t& count) {
    count = impl->count;
    return impl->lineData;
}