#include "DebugDraw.h"
#include "LuzCommon.h"

struct DebugDrawImpl {
    std::unordered_map<std::string, DebugDraw::DrawData> gizmos;
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
    if (impl->gizmos[name].config.update) {
        impl->gizmos[name].points = {bl, tl, tr, br, bl};
    }
    impl->gizmos[name].name = name;
}

void DebugDraw::Line(const std::string& name, const glm::vec3& p0, const glm::vec3& p1) {
    if (impl->gizmos[name].config.update) {
        impl->gizmos[name].points = {p0, p1};
    }
    impl->gizmos[name].name = name;
}

void DebugDraw::Box(const std::string& name, v3 p0, v3 p1) {
    if (impl->gizmos[name].config.update) {
        // todo: complete
    }
    impl->gizmos[name].name = name;
}

void DebugDraw::Box(const std::string& name, v3 p000, v3 p001, v3 p010, v3 p011, v3 p100, v3 p101, v3 p110, v3 p111) {
    if (impl->gizmos[name].config.update) {
        impl->gizmos[name].points = {
            p000, p001, p101, p100, p000, p010, p011, p001,
            p011, p111, p101, p111, p110, p100, p110, p010
        };
    }
    impl->gizmos[name].name = name;
}

void DebugDraw::Clear(const std::string& name) {
    auto it = impl->gizmos.find(name);
    if  (it != impl->gizmos.end()) {
        impl->gizmos.erase(it);
    }
}

void DebugDraw::Config(const std::string& name, const ConfigData& config, bool onCreation) {
    if (!onCreation || impl->gizmos.find(name) == impl->gizmos.end()) {
        impl->gizmos[name].config = config;
    }
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

std::vector<glm::vec3> DebugDraw::GetPoints() {
    std::vector<glm::vec3> allPoints;
    for (auto& it : impl->gizmos) {
        auto& points = it.second.points;
        allPoints.insert(allPoints.end(), points.begin(), points.end());
    }
    return allPoints;
}

std::vector<DebugDraw::DrawData> DebugDraw::Get() {
    std::vector<DebugDraw::DrawData> data;
    for (auto& it : impl->gizmos) {
        data.push_back(it.second);
    }
    return data;
}