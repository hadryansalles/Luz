#include "DebugDraw.h"
#include "LuzCommon.h"
#include <glm/gtx/rotate_vector.hpp>
#include <unordered_map>

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

void DebugDraw::Rect(const std::string& name, v3 bl, v3 tl, v3 tr, v3 br) {
    if (impl->gizmos[name].config.update) {
        impl->gizmos[name].points = {bl, tl, tr, br, bl};
    }
    impl->gizmos[name].name = name;
}

void DebugDraw::Line(const std::string& name, v3 p0, v3 p1) {
    if (impl->gizmos[name].config.update) {
        impl->gizmos[name].points = {p0, p1};
    }
    impl->gizmos[name].name = name;
}

void DebugDraw::Box(const std::string& name, v3 p0, v3 p1) {
    if (impl->gizmos[name].config.update) {
        glm::vec3 p000 = glm::min(p0, p1);
        glm::vec3 p111 = glm::max(p0, p1);
        glm::vec3 p001 = glm::vec3(p000.x, p000.y, p111.z);
        glm::vec3 p010 = glm::vec3(p000.x, p111.y, p000.z);
        glm::vec3 p011 = glm::vec3(p000.x, p111.y, p111.z);
        glm::vec3 p100 = glm::vec3(p111.x, p000.y, p000.z);
        glm::vec3 p101 = glm::vec3(p111.x, p000.y, p111.z);
        glm::vec3 p110 = glm::vec3(p111.x, p111.y, p000.z);

        impl->gizmos[name].points = {
            p000, p001, p101, p100, p000, p010, p011, p001,
            p011, p111, p101, p111, p110, p100, p110, p010
        };
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

void DebugDraw::Cone(const std::string& name, v3 base, v3 tip, float radius, int segments) {
    if (impl->gizmos[name].config.update) {
        std::vector<glm::vec3> points;
        
        // Calculate cone axis
        glm::vec3 axis = glm::normalize(tip - base);
        
        // Generate two perpendicular vectors
        glm::vec3 perpA, perpB;
        if (std::abs(axis.y) < 0.99f) {
            perpA = glm::normalize(glm::cross(axis, glm::vec3(0, 1, 0)));
        } else {
            perpA = glm::normalize(glm::cross(axis, glm::vec3(1, 0, 0)));
        }
        perpB = glm::normalize(glm::cross(axis, perpA));

        // Generate base circle points
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)i / segments * glm::two_pi<float>();
            glm::vec3 circlePoint = base + radius * (cos(angle) * perpA + sin(angle) * perpB);
            points.push_back(circlePoint);
            points.push_back(tip);
        }

        // Add base circle
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)i / segments * glm::two_pi<float>();
            glm::vec3 circlePoint = base + radius * (cos(angle) * perpA + sin(angle) * perpB);
            points.push_back(circlePoint);
        }

        impl->gizmos[name].points = points;
    }
    impl->gizmos[name].name = name;
}

void DebugDraw::Sphere(const std::string& name, v3 center, float radius, int segments) {
    if (impl->gizmos[name].config.update) {
        std::vector<glm::vec3> points;
        
        // Generate sphere points
        for (int i = 0; i <= segments; ++i) {
            float lat = glm::pi<float>() * float(i) / float(segments) - glm::half_pi<float>();
            float y = radius * std::sin(lat);
            float r = radius * std::cos(lat);
            
            for (int j = 0; j <= segments; ++j) {
                float lon = glm::two_pi<float>() * float(j) / float(segments);
                float x = r * std::sin(lon);
                float z = r * std::cos(lon);
                
                glm::vec3 point = center + glm::vec3(x, y, z);
                
                // Add lines for latitude
                if (j > 0 && !points.empty()) {
                    points.push_back(points.back());
                    points.push_back(point);
                } else if (j == 0) {
                    points.push_back(point);
                }
                
                // Add lines for longitude
                if (i > 0) {
                    glm::vec3 prevPoint = center + glm::vec3(
                        radius * std::cos(glm::pi<float>() * float(i-1) / float(segments) - glm::half_pi<float>()) * std::sin(lon),
                        radius * std::sin(glm::pi<float>() * float(i-1) / float(segments) - glm::half_pi<float>()),
                        radius * std::cos(glm::pi<float>() * float(i-1) / float(segments) - glm::half_pi<float>()) * std::cos(lon)
                    );
                    points.push_back(prevPoint);
                    points.push_back(point);
                }
            }
        }

        impl->gizmos[name].points = points;
    }
    impl->gizmos[name].name = name;
}

void DebugDraw::ClearNonPersistent() {
    for (auto it = impl->gizmos.begin(); it != impl->gizmos.end();) {
        if (!it->second.config.persist) {
            it = impl->gizmos.erase(it);
        } else {
            ++it;
        }
    }
}