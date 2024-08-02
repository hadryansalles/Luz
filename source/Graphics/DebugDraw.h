#pragma once

#include "Base.hpp"

struct DebugDraw {
    struct ConfigData {
        glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        float thickness = 1.0f;
        bool depthAware = true;
        bool isPoint = false;
    };

    static void Create();
    static void Destroy();
    static void Update();
    static void* Get(uint32_t& count);

    static void Rect(const std::string& name, const glm::vec3& bl, const glm::vec3& tl, const glm::vec3& tr, const glm::vec3& br);
    static void Line(const std::string& name, const glm::vec3& p0, const glm::vec3& p1);
    static void Clear(const std::string& name);
    static void Config(const std::string& name, const ConfigData& config);
};