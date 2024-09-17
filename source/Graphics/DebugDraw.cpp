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
        glm::vec3 p001(p0.x, p0.y, p1.z);
        glm::vec3 p010(p0.x, p1.y, p0.z);
        glm::vec3 p011(p0.x, p1.y, p1.z);
        glm::vec3 p100(p1.x, p0.y, p0.z);
        glm::vec3 p101(p1.x, p0.y, p1.z);
        glm::vec3 p110(p1.x, p1.y, p0.z);

        impl->gizmos[name].points = {
            p0, p001, p101, p100, p0, p010, p011, p001,
            p011, p1, p101, p1, p110, p100, p110, p010
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

void DebugDraw::Cone(const std::string& name, v3 base, v3 tip, float radius) {
    if (impl->gizmos[name].config.update) {
        const int segments = 32;
        const float angleStep = 2 * LUZ_PI / segments;
        
        std::vector<glm::vec3> points;
        points.reserve(segments * 4 + 2);

        glm::vec3 axis = glm::normalize(tip - base);
        glm::vec3 perpendicular = glm::vec3(-axis.y, axis.x, 0);
        if (glm::length(perpendicular) < 0.001f) {
            perpendicular = glm::vec3(0, -axis.z, axis.y);
        }
        perpendicular = glm::normalize(perpendicular);

        // Draw the base circle
        for (int i = 0; i <= segments; ++i) {
            float angle = i * angleStep;
            glm::vec3 direction = glm::rotate(glm::quat(glm::cos(angle / 2), axis * glm::sin(angle / 2)), perpendicular);
            glm::vec3 basePoint = base + direction * radius;
            
            points.push_back(basePoint);
            if (i > 0) points.push_back(basePoint);
        }

        // Draw lines from base to tip
        for (int i = 0; i < segments; ++i) {
            float angle = i * angleStep;
            glm::vec3 direction = glm::rotate(glm::quat(glm::cos(angle / 2), axis * glm::sin(angle / 2)), perpendicular);
            glm::vec3 basePoint = base + direction * radius;
            
            points.push_back(basePoint);
            points.push_back(tip);
        }

        impl->gizmos[name].points = points;
    }
    impl->gizmos[name].name = name;
}

void DebugDraw::Cylinder(const std::string& name, v3 center, float height, float radius) {
    if (impl->gizmos[name].config.update) {
        const int segments = 16;
        const float angleStep = 2 * LUZ_PI / segments;
        
        std::vector<glm::vec3> points;
        points.reserve(segments * 4 + 2);

        glm::vec3 up(0, 1, 0);
        glm::vec3 topCenter = center + up * (height * 0.5f);
        glm::vec3 bottomCenter = center - up * (height * 0.5f);

        for (int i = 0; i <= segments; ++i) {
            float angle = i * angleStep;
            glm::vec3 direction(std::cos(angle), 0, std::sin(angle));
            glm::vec3 topPoint = topCenter + direction * radius;
            glm::vec3 bottomPoint = bottomCenter + direction * radius;
            
            points.push_back(topPoint);
            points.push_back(bottomPoint);
        }

        for (int i = 0; i <= segments; ++i) {
            float angle = i * angleStep;
            glm::vec3 direction(std::cos(angle), 0, std::sin(angle));
            points.push_back(topCenter + direction * radius);
        }

        for (int i = 0; i <= segments; ++i) {
            float angle = i * angleStep;
            glm::vec3 direction(std::cos(angle), 0, std::sin(angle));
            points.push_back(bottomCenter + direction * radius);
        }

        impl->gizmos[name].points = points;
    }
    impl->gizmos[name].name = name;
}

void DebugDraw::Sphere(const std::string& name, v3 center, float radius) {
    if (impl->gizmos[name].config.update) {
        const int segments = 16;
        const float angleStep = LUZ_PI / segments;
        
        std::vector<glm::vec3> points;
        points.reserve(segments * segments * 6);

        for (int i = 0; i < segments; ++i) {
            float phi = i * angleStep;
            for (int j = 0; j <= segments * 2; ++j) {
                float theta = j * angleStep;
                
                glm::vec3 point1(
                    center.x + radius * std::sin(phi) * std::cos(theta),
                    center.y + radius * std::cos(phi),
                    center.z + radius * std::sin(phi) * std::sin(theta)
                );
                
                glm::vec3 point2(
                    center.x + radius * std::sin(phi + angleStep) * std::cos(theta),
                    center.y + radius * std::cos(phi + angleStep),
                    center.z + radius * std::sin(phi + angleStep) * std::sin(theta)
                );
                
                points.push_back(point1);
                points.push_back(point2);
            }
        }

        impl->gizmos[name].points = points;
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
        impl->gizmos[name].config.firstConfig = false;
    }
}

void DebugDraw::Update() {
    impl->count = 0;
    for (auto& it : impl->gizmos) {
        auto& points = it.second.points;
        auto& config = it.second.config;
        if (points.size() == 0) {
            continue;
        }
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
#ifdef LUZ_DEBUG
    std::vector<glm::vec3> allPoints;
    for (auto& it : impl->gizmos) {
        auto& points = it.second.points;
        allPoints.insert(allPoints.end(), points.begin(), points.end());
    }
    return allPoints;
#else
    return std::vector<glm::vec3>();
#endif
}

std::vector<DebugDraw::DrawData> DebugDraw::Get() {
#ifdef LUZ_DEBUG
    std::vector<DebugDraw::DrawData> data;
    for (auto& it : impl->gizmos) {
        data.push_back(it.second);
    }
    return data;
#else
    return std::vector<DebugDraw::DrawData>();
#endif
}