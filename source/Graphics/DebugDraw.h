#pragma once

#include "Base.hpp"

struct DebugDraw {
    struct ConfigData {
        glm::vec4 color = {1.0f, 0.0f, 0.0f, 1.0f};
        float thickness = 1.0f;
        bool depthAware = true;
        bool isPoint = false;
        bool update = true;
        bool hide = false;
        bool drawTitle = true;
        bool firstConfig = true;
    };
    struct DrawData {
        std::vector<glm::vec3> points = {};
        DebugDraw::ConfigData config = {};
        std::string name = "";
    };

    static void Create();
    static void Destroy();
    static void Update();

    static std::vector<glm::vec3> GetPoints();
    static std::vector<DrawData> Get();

    using v3 = const glm::vec3&;
    static void Rect(const std::string& name, const glm::vec3& bl, const glm::vec3& tl, const glm::vec3& tr, const glm::vec3& br);
    static void Line(const std::string& name, const glm::vec3& p0, const glm::vec3& p1);
    static void Box(const std::string& name, v3 p0, v3 p1);
    static void Box(const std::string& name, v3 p000, v3 p001, v3 p010, v3 p011, v3 p100, v3 p101, v3 p110, v3 p111);
    static void Cone(const std::string& name, v3 base, v3 tip, float radius);
    static void Cylinder(const std::string& name, v3 center, float height, float radius);
    static void Sphere(const std::string& name, v3 center, float radius);
    static void Clear(const std::string& name);
    static void Config(const std::string& name, const ConfigData& config, bool onCreation = true);
};